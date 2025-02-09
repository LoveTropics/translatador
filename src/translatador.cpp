#include <translatador.h>
#include "tokenization.h"

#include <common/options.h>
#include <data/types.h>
#include <marian.h>
#include <memory>
#include <mutex>
#include <string>
#include <translator/beam_search.h>
#include <utility>
#include <vector>
#ifdef USE_WHATLANG
#include <whatlang.h>
#endif
#ifdef __unix__
#include <csignal>
#endif

static std::mutex init_mutex;
static bool initialized;

static thread_local std::string last_error;

static void initialize() {
    std::lock_guard guard(init_mutex);
    if (!initialized) {
#ifdef __unix__
        // Capture and restore signal handlers that are otherwise overridden by Marian
        // This is particularly important when running inside the JVM, which uses these handlers in normal operation
        struct sigaction sigsegv = {};
        struct sigaction sigfpe = {};
        sigaction(SIGSEGV, nullptr, &sigsegv);
        sigaction(SIGFPE, nullptr, &sigfpe);
#endif
        marian::setThrowExceptionOnAbort(true);
        for (const Logger& logger : createLoggers()) {
            logger->set_level(spdlog::level::level_enum::off);
        }
#ifdef __unix__
        sigaction(SIGSEGV, &sigsegv, nullptr);
        sigaction(SIGFPE, &sigfpe, nullptr);
#endif
        initialized = true;
    }
}

struct OwnedBuffer {
    char* data;
    const size_t size;

    OwnedBuffer(const OwnedBuffer&) = delete;

    OwnedBuffer& operator=(const OwnedBuffer&) = delete;

    ~OwnedBuffer() {
        if (data) {
#ifdef _WIN32
            _aligned_free(data);
#else
            std::free(data);
#endif
        }
    }

    explicit operator bool() const {
        return data && size > 0;
    }
};

struct BufferRef {
    const char* data;
    const size_t size;

    explicit operator bool() const {
        return data && size > 0;
    }

    bool operator!=(const BufferRef& buffer) const {
        return data != buffer.data || size != buffer.size;
    }

    [[nodiscard]] OwnedBuffer aligned_copy(const size_t alignment) const {
        if (data && size > 0) {
            const size_t aligned_size = (size + alignment - 1) / alignment * alignment;
#ifdef _WIN32
            void* new_data = _aligned_malloc(aligned_size, alignment);
#else
            void* new_data = std::aligned_alloc(alignment, aligned_size);
#endif
            std::memcpy(new_data, data, size);
            return OwnedBuffer{static_cast<char *>(new_data), size};
        }
        return {};
    }
};

struct Vocabs {
    std::shared_ptr<marian::Vocab> source;
    std::shared_ptr<marian::Vocab> target;

    explicit Vocabs(
        const std::shared_ptr<marian::Options>& options,
        const BufferRef buffer
    ): source(load_vocab(options, buffer)),
       target(source) {
    }

    explicit Vocabs(
        const std::shared_ptr<marian::Options>& options,
        const BufferRef source_buffer,
        const BufferRef target_buffer
    ): source(load_vocab(options, source_buffer)),
       target(load_vocab(options, target_buffer)) {
    }

    static std::shared_ptr<marian::Vocab> load_vocab(const std::shared_ptr<marian::Options>& options, const BufferRef buffer) {
        const OwnedBuffer aligned_buffer = buffer.aligned_copy(64);
        std::shared_ptr<marian::Vocab> vocab = std::make_shared<marian::Vocab>(options, 0);
        vocab->loadFromSerialized(marian::string_view(aligned_buffer.data, aligned_buffer.size));
        return vocab;
    }
};

struct ModelData {
    const std::shared_ptr<marian::Options> options;
    const OwnedBuffer model_memory;
    // short_list_generator holds a raw reference to this memory
    const OwnedBuffer short_list_memory;
    const Vocabs vocabs;
    const size_t max_segment_length;
    const SsplitMode segment_split_mode;
    std::shared_ptr<marian::data::BinaryShortlistGenerator> short_list_generator;

    [[nodiscard]] Vocabs create_vocabs(const BufferRef source_vocab, const BufferRef target_vocab) const {
        if (source_vocab != target_vocab && target_vocab) {
            return Vocabs(options, source_vocab, target_vocab);
        }
        return Vocabs(options, source_vocab);
    }

    [[nodiscard]] std::shared_ptr<marian::data::BinaryShortlistGenerator> create_short_list_generator() const {
        if (short_list_memory) {
            bool shared = vocabs.source == vocabs.target;
            return std::make_shared<marian::data::BinaryShortlistGenerator>(
                short_list_memory.data, short_list_memory.size,
                vocabs.source, vocabs.target,
                0, 1, shared, false
            );
        }
        return {};
    }

    ModelData(
        std::shared_ptr<marian::Options> options,
        const BufferRef model,
        const BufferRef source_vocab,
        const BufferRef target_vocab,
        const BufferRef short_list
    ): options(std::move(options)),
       model_memory(model.aligned_copy(256)),
       short_list_memory(short_list.aligned_copy(64)),
       vocabs(create_vocabs(source_vocab, target_vocab)),
       max_segment_length(this->options->get<size_t>("max-length-break")),
       segment_split_mode(parse_ssplit_mode(this->options->get<std::string>("ssplit-mode"))),
       short_list_generator(create_short_list_generator()) {
    }

    ModelData(const ModelData&) = delete;

    ModelData(const ModelData&& data) = delete;

    ModelData& operator=(const ModelData&) = delete;

    ModelData& operator=(const ModelData&&) = delete;

    ~ModelData() {
        // Ensure that short_list_generator cannot outlive this struct: short_list_generator holds a raw reference to short_list_memory, but we need to pass it as a shared_ptr to Marian
        if (short_list_generator) {
            short_list_generator.reset();
        }
    }
};

struct TrlString {
    const std::shared_ptr<std::string> plain;

    mutable std::mutex tokenized_mutex;
    mutable std::optional<std::shared_ptr<TokenizedString>> tokenized;

    explicit TrlString(std::string&& plain): plain(std::make_shared<std::string>(plain)) {
    }

    explicit TrlString(std::shared_ptr<TokenizedString>&& tokenized): plain(tokenized->plain),
                                                                      tokenized(std::optional(std::move(tokenized))) {
    }

    [[nodiscard]] std::shared_ptr<TokenizedString> get_tokenized(
        const std::shared_ptr<marian::Vocab const>& vocab,
        const size_t max_segment_length,
        const SsplitMode split_mode
    ) const {
        std::lock_guard guard(tokenized_mutex);
        TokenizationParameters parameters{vocab, max_segment_length, split_mode};
        if (!tokenized.has_value() || tokenized.value()->parameters != parameters) {
            // Could be wasteful if only vocabulary changed and not splitting mode - but this should be rare
            tokenized.emplace(tokenize(plain, std::move(parameters)));
        }
        return tokenized.value();
    }
};

struct TrlModel {
    const std::shared_ptr<ModelData> data;
    const std::shared_ptr<marian::ExpressionGraph> graph;
    const std::vector<std::shared_ptr<marian::Scorer>> scorers;

    TrlModel(
        std::shared_ptr<ModelData> data,
        std::shared_ptr<marian::ExpressionGraph>&& graph,
        std::vector<std::shared_ptr<marian::Scorer>>&& scorers
    ): data(std::move(data)),
       graph(std::move(graph)),
       scorers(std::move(scorers)) {
    }

    TrlModel(const TrlModel&) = delete;

    TrlModel& operator=(const TrlModel&) = delete;

    template<typename F>
    void evaluate(const std::vector<std::shared_ptr<TokenizedString>>&& batch, F handler) const;
};

char* trl_get_last_error() {
    if (!last_error.empty()) {
        char* result = strdup(last_error.c_str());
        last_error = "";
        return result;
    }
    return nullptr;
}

template<typename T, typename F>
static T* create_fallible(const F function) {
    try {
        return function();
    } catch (std::exception& e) {
        last_error = std::string(e.what());
        return nullptr;
    }
}

template<typename F>
static TrlError run_fallible(const F function) {
    try {
        function();
        return TRL_OK;
    } catch (std::exception& e) {
        last_error = std::string(e.what());
        return TRL_ERROR;
    }
}

static std::shared_ptr<marian::Options> parse_options(const char* yaml) {
    std::shared_ptr<marian::Options> options = std::make_shared<marian::Options>();

    const marian::ConfigParser parser(marian::cli::mode::translation);
    options->merge(parser.getConfig());

    // Default properties, based on ones used for Firefox translation models: https://github.com/mozilla/firefox-translations-models/blob/main/evals/translators/bergamot.config.yml
    options->set<size_t>("max-length-break", 128);
    options->set<float>("max-length-factor", 2.0);
    options->set<size_t>("beam-size", 1);
    options->set<float>("normalize", 1.0);
    options->set<float>("word-penalty", 0.0);
    options->set<bool>("skip-cost", true);
    options->set<size_t>("workspace", 128);
    options->set<std::string>("alignment", "soft");
    options->set<std::string>("ssplit-mode", "paragraph");
    options->set<std::string>("gemm-precision", "int8shiftAlphaAll");
    options->set<bool>("quiet", true);
    options->set<bool>("quiet-translation", true);

    if (yaml) {
        options->parse(std::string(yaml));
    }

    // Dummy values, should not be overridden
    options->set<std::vector<std::string>>("vocabs", {"source", "target"});

    return options;
}

static TrlModel instantiate_model(const std::shared_ptr<ModelData>& data) {
    const marian::DeviceId device(0, marian::DeviceType::cpu);
    std::shared_ptr<marian::ExpressionGraph> graph = std::make_shared<marian::ExpressionGraph>(true);
    graph->setDefaultElementType(marian::typeFromString(data->options->get<std::vector<std::string>>("precision", {"float32"})[0]));
    graph->setDevice(device);
    graph->getBackend()->configureDevice(data->options);
    graph->reserveWorkspaceMB(data->options->get<size_t>("workspace"));

    std::vector<std::shared_ptr<marian::Scorer>> scorers = marian::createScorers(data->options, std::vector<const void *>{data->model_memory.data});
    for (const std::shared_ptr<marian::Scorer>& scorer : scorers) {
        scorer->init(graph);
        if (data->short_list_generator) {
            scorer->setShortlistGenerator(data->short_list_generator);
        }
    }

    graph->forward();

    return {data, std::move(graph), std::move(scorers)};
}

const TrlModel* trl_create_model(const char* yaml_config, const char* model, const size_t model_size, const char* source_vocab, const size_t source_vocab_size, const char* target_vocab, const size_t target_vocab_size, const char* short_list, const size_t short_list_size) {
    initialize();
    return create_fallible<TrlModel>([=] {
        std::shared_ptr<marian::Options> options = parse_options(yaml_config);
        const std::shared_ptr<ModelData> data = std::make_shared<ModelData>(
            options,
            BufferRef{model, model_size},
            BufferRef{source_vocab, source_vocab_size},
            BufferRef{target_vocab, target_vocab_size},
            BufferRef{short_list, short_list_size}
        );
        return new TrlModel(instantiate_model(data));
    });
}

const TrlModel* trl_clone_model(const TrlModel* model) {
    // We already initialized `model`, so we should hope that this should not fail
    return new TrlModel(instantiate_model(model->data));
}

void trl_destroy_model(const TrlModel* model) {
    delete model;
}

const TrlString* trl_create_string(const char* utf) {
    return new TrlString(std::string(utf));
}

const char* trl_get_string_utf(const TrlString* string) {
    return string->plain->c_str();
}

void trl_destroy_string(const TrlString* string) {
    delete string;
}

template<typename F>
void TrlModel::evaluate(const std::vector<std::shared_ptr<TokenizedString>>&& batch, const F handler) const {
    const std::shared_ptr<marian::data::CorpusBatch> corpus_batch = generate_corpus_batch(batch, data->vocabs.source);

    const std::shared_ptr<marian::Vocab const> target_vocab = data->vocabs.target;
    marian::BeamSearch search(data->options, scorers, target_vocab);
    const marian::Histories histories = search.search(graph, corpus_batch);

    size_t segment_id = 0;
    for (size_t i = 0; i < batch.size(); i++) {
        const std::shared_ptr<TokenizedString>& source = batch[i];
        std::shared_ptr<TokenizedString> target = decode_string(source, target_vocab, &histories[segment_id]);
        segment_id += source->segments.size();

        handler(i, std::move(target));
    }
}

TrlError trl_translate(const TrlModel* model, const TrlString* const* source, const TrlString** target, const size_t count) {
    return run_fallible([model, source, target, count] {
        const std::shared_ptr<const marian::Vocab> source_vocab = model->data->vocabs.source;
        const size_t max_segment_length = model->data->max_segment_length;
        const SsplitMode segment_split_mode = model->data->segment_split_mode;

        std::vector<std::shared_ptr<TokenizedString>> batch;
        batch.reserve(count);
        for (size_t i = 0; i < count; i++) {
            batch.push_back(source[i]->get_tokenized(source_vocab, max_segment_length, segment_split_mode));
        }

        model->evaluate(std::move(batch), [&target](const int i, std::shared_ptr<TokenizedString>&& string) {
            target[i] = new TrlString(std::move(string));
        });
    });
}

TrlError trl_detect_language(const char* string, TrlDetectedLangInfo* result) {
#ifdef USE_WHATLANG
    WlInfo info;
    switch (wl_detect(string, &info)) {
        case WL_MALFORMED_STRING:
            last_error = std::string("Malformed string");
            return TRL_ERROR;
        case WL_DETECTION_FAILED:
            last_error = std::string("Internal language detection error");
            return TRL_ERROR;
        default:
            result->lang = static_cast<TrlDetectedLang>(info.lang);
            result->confidence = info.confidence;
            return TRL_OK;
    }
#else
    last_error = std::string("Language detection is disabled for this build");
    return TRL_ERROR;
#endif
}
