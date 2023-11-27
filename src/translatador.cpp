#include <translatador.h>

#include <marian.h>
#include <common/options.h>
#include <data/types.h>
#include <translator/annotation.h>
#include <translator/beam_search.h>
#include <translator/definitions.h>
#include <translator/logging.h>
#include <translator/text_processor.h>
#include <variant>

namespace bergamot = marian::bergamot;

static std::mutex init_mutex;
static bool initialized;

static thread_local std::string last_error;

static void initialize() {
    std::lock_guard guard(init_mutex);
    if (!initialized) {
        marian::setThrowExceptionOnAbort(true);
        for (const Logger& logger : createLoggers()) {
            logger->set_level(spdlog::level::level_enum::off);
        }
        initialized = true;
    }
}

struct Buffer {
    const char* data;
    const size_t size;

    explicit operator bool() const {
        return data && size > 0;
    }

    bool operator!=(const Buffer& buffer) const {
        return data != buffer.data || size != buffer.size;
    }

    [[nodiscard]] bergamot::AlignedMemory aligned_copy(const size_t alignment) const {
        if (data && size > 0) {
            bergamot::AlignedMemory result(size, alignment);
            std::memcpy(result.begin(), data, size);
            return result;
        }
        return {};
    }
};

struct Batch {
    std::vector<bergamot::AnnotatedText> sources;
    std::vector<size_t> sentence_counts;

    std::vector<marian::Words> sentences;
    std::vector<size_t> sentence_ids;

    size_t sentence_id;
    size_t max_length;

    explicit Batch(const size_t expectedCount): sentence_id(0), max_length(0) {
        sources.reserve(expectedCount);
        sentence_counts.reserve(expectedCount);
        sentences.reserve(expectedCount * 2);
        sentence_ids.reserve(expectedCount * 2);
    }

    void push(bergamot::Segments&& segments, bergamot::AnnotatedText&& source) {
        sources.push_back(std::move(source));
        sentence_counts.push_back(segments.size());

        for (bergamot::Segment& sentence : segments) {
            if (sentence.size() > max_length) {
                max_length = sentence.size();
            }
            sentences.push_back(std::move(sentence));
            sentence_ids.push_back(sentence_id++);
        }
    }
};

struct ModelData {
    std::shared_ptr<marian::Options> options;
    bergamot::AlignedMemory model_memory;
    // short_list_generator holds a raw reference to this memory
    bergamot::AlignedMemory short_list_memory;
    bergamot::Vocabs vocabs;
    bergamot::TextProcessor text_processor;
    std::shared_ptr<marian::data::BinaryShortlistGenerator> short_list_generator;

    [[nodiscard]] bergamot::Vocabs create_vocabs(const Buffer source_vocab, const Buffer target_vocab) const {
        static constexpr size_t ALIGNMENT = 64;
        if (source_vocab != target_vocab && target_vocab) {
            return bergamot::Vocabs(options, {
                std::make_shared<bergamot::AlignedMemory>(source_vocab.aligned_copy(ALIGNMENT)),
                std::make_shared<bergamot::AlignedMemory>(target_vocab.aligned_copy(ALIGNMENT))
            });
        } else {
            std::shared_ptr<bergamot::AlignedMemory> vocab_memory = std::make_shared<bergamot::AlignedMemory>(source_vocab.aligned_copy(ALIGNMENT));
            return bergamot::Vocabs(options, {vocab_memory, vocab_memory});
        }
    }

    std::shared_ptr<marian::data::BinaryShortlistGenerator> create_short_list_generator() {
        if (short_list_memory.begin() && short_list_memory.size() > 0) {
            bool shared = vocabs.sources().front() == vocabs.target();
            return std::make_shared<marian::data::BinaryShortlistGenerator>(
                short_list_memory.begin(), short_list_memory.size(),
                vocabs.sources().front(), vocabs.target(),
                0, 1, shared, false
            );
        }
        return {};
    }

    ModelData(
        std::shared_ptr<marian::Options> options,
        const Buffer model,
        const Buffer source_vocab,
        const Buffer target_vocab,
        const Buffer short_list
    ): options(std::move(options)),
       model_memory(model.aligned_copy(256)),
       short_list_memory(short_list.aligned_copy(64)),
       vocabs(create_vocabs(source_vocab, target_vocab)),
       text_processor(bergamot::TextProcessor(this->options, this->vocabs, bergamot::AlignedMemory())),
       short_list_generator(create_short_list_generator()) {
    }

    ModelData(const ModelData&) = delete;

    ModelData(const ModelData&& data) = delete;

    ModelData& operator=(const ModelData&) = delete;

    ModelData& operator=(const ModelData&&) = delete;

    ~ModelData() {
        // Ensure that short_list_generator cannot outlive this struct: short_list_generator holds a raw reference to short_list_memory, but we need to pass it as a shared_ptr to Bergamot
        if (short_list_generator) {
            short_list_generator.reset();
        }
    }
};

struct TrlString {
    std::variant<std::string, bergamot::AnnotatedText> plain_or_annotated;

    [[nodiscard]] const std::string& as_plain() const {
        if (std::holds_alternative<bergamot::AnnotatedText>(plain_or_annotated)) {
            return std::get<bergamot::AnnotatedText>(plain_or_annotated).text;
        }
        return std::get<std::string>(plain_or_annotated);
    }
};

struct TrlModel {
    std::shared_ptr<ModelData> data;
    std::shared_ptr<marian::ExpressionGraph> graph;
    std::vector<std::shared_ptr<marian::Scorer>> scorers;

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
    void evaluate(const Batch&& batch, F handler) const;
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

    std::vector<std::shared_ptr<marian::Scorer>> scorers = marian::createScorers(data->options, std::vector<const void *>{data->model_memory.begin()});
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
            Buffer{model, model_size},
            Buffer{source_vocab, source_vocab_size},
            Buffer{target_vocab, target_vocab_size},
            Buffer{short_list, short_list_size}
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
    return new TrlString{std::string(utf)};
}

const char* trl_get_string_utf(const TrlString* string) {
    return string->as_plain().c_str();
}

void trl_destroy_string(const TrlString* string) {
    delete string;
}

static bergamot::AnnotatedText decode_sentences(const bergamot::AnnotatedText& source, const std::shared_ptr<marian::Vocab const>& vocab, const std::shared_ptr<marian::History>* histories, const size_t sentence_count) {
    bergamot::AnnotatedText target;
    target.text.reserve(source.text.size());

    for (int i = 0; i < sentence_count; i++) {
        const std::shared_ptr<marian::History>& history = histories[i];

        const marian::NBestList results = history->nBest(1);
        const marian::Words& words = std::get<0>(results[0]);

        std::string decoded;
        std::vector<marian::string_view> mappings;
        vocab->decodeWithByteRanges(words, decoded, mappings, false);
        target.appendSentence(source.gap(i), mappings.begin(), mappings.end());

        if (i + 1 == sentence_count) {
            target.appendEndingWhitespace(source.gap(i + 1));
        }
    }

    return target;
}

template<typename F>
void TrlModel::evaluate(const Batch&& batch, const F handler) const {
    size_t batch_size = batch.sentences.size();

    const std::shared_ptr<marian::data::SubBatch> sub_batch = std::make_shared<marian::data::SubBatch>(batch_size, batch.max_length, data->vocabs.sources().at(0));

    size_t word_count = 0;
    for (size_t i = 0; i < batch_size; i++) {
        const marian::Words sentence = batch.sentences[i];
        for (size_t j = 0; j < sentence.size(); j++) {
            sub_batch->data()[j * batch_size + i] = sentence[j];
            sub_batch->mask()[j * batch_size + i] = 1.0f;
            word_count++;
        }
    }

    sub_batch->setWords(word_count);

    const std::shared_ptr<marian::data::CorpusBatch> corpus_batch = std::make_shared<marian::data::CorpusBatch>(std::vector{sub_batch});
    corpus_batch->setSentenceIds(batch.sentence_ids);

    const std::shared_ptr<marian::Vocab const> target_vocab = data->vocabs.target();
    marian::BeamSearch search(data->options, scorers, target_vocab);
    const marian::Histories histories = search.search(graph, corpus_batch);

    size_t sentence_id = 0;
    for (int i = 0; i < batch.sources.size(); i++) {
        const bergamot::AnnotatedText source = batch.sources[i];
        const size_t sentence_count = batch.sentence_counts[i];

        bergamot::AnnotatedText target = decode_sentences(source, target_vocab, &histories[sentence_id], sentence_count);
        sentence_id += sentence_count;

        handler(i, std::move(target));
    }
}

TrlError trl_translate(const TrlModel* model, const TrlString* const* source, const TrlString** target, const size_t count) {
    std::vector builder(count, bergamot::AnnotatedText());

    return run_fallible([&builder, model, source, target, count] {
        Batch batch(count);

        // TODO: Split batches based on mini-batch-words (see BatchingPool)
        for (size_t i = 0; i < count; i++) {
            const TrlString* string = source[i];
            bergamot::Segments segments;
            bergamot::AnnotatedText annotated;
            if (std::holds_alternative<bergamot::AnnotatedText>(string->plain_or_annotated)) {
                annotated = std::get<bergamot::AnnotatedText>(string->plain_or_annotated);
                model->data->text_processor.processFromAnnotation(annotated, segments);
            } else {
                model->data->text_processor.process(std::string(string->as_plain()), annotated, segments);
            }
            batch.push(std::move(segments), std::move(annotated));
        }

        model->evaluate(std::move(batch), [&builder](const int i, bergamot::AnnotatedText&& string) {
            builder[i] = bergamot::AnnotatedText(string);
        });

        for (int i = 0; i < count; i++) {
            target[i] = new TrlString{std::move(builder[i])};
        }
    });
}
