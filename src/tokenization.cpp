#include "tokenization.h"
#include <cassert>
#include <data/types.h>
#include <string>
#include <vector>

static thread_local ug::ssplit::SentenceSplitter SENTENCE_SPLITTER = ug::ssplit::SentenceSplitter();

SsplitMode parse_ssplit_mode(const std::string& mode) {
    if (mode == "sentence") {
        return SsplitMode::one_sentence_per_line;
    } else if (mode == "paragraph") {
        return SsplitMode::one_paragraph_per_line;
    } else if (mode == "wrapped_text") {
        return SsplitMode::wrapped_text;
    } else {
        throw std::runtime_error("Unrecognized ssplit-mode: " + mode);
    }
}

[[nodiscard]] std::string_view gap_before(const TokenizedString& string, const size_t segment_index) {
    size_t segment_start = 0;
    size_t last_segment_end = 0;
    if (segment_index > 0) {
        last_segment_end = string.segments[segment_index - 1].tokens.back().end;
    }
    if (segment_index < string.segments.size()) {
        segment_start = string.segments[segment_index].tokens.front().begin;
    }
    return std::string_view(*string.plain).substr(last_segment_end, segment_start - last_segment_end);
}

std::shared_ptr<TokenizedString> tokenize(const std::shared_ptr<std::string>& plain, TokenizationParameters&& parameters) {
    std::vector<TokenizedSegment> tokenized_segments;

    ug::ssplit::SentenceStream segment_stream(
        std::string_view{*plain},
        SENTENCE_SPLITTER,
        parameters.segment_split_mode
    );

    std::string_view segment_view;
    while (segment_stream >> segment_view) {
        std::vector<marian::string_view> token_ranges;
        marian::Words segment_tokens = parameters.vocab->encodeWithByteRanges(
            marian::string_view(segment_view.data(), segment_view.size()),
            token_ranges,
            false,
            true
        );
        if (segment_tokens.empty()) {
            continue;
        }

        // We should generate segments of at most `max_segment_length` tokens, so they might need to be split
        for (size_t segment_start = 0; segment_start < segment_tokens.size(); segment_start += parameters.max_segment_length) {
            const size_t wrapped_segment_length = std::min(parameters.max_segment_length, segment_tokens.size() - segment_start);

            TokenizedSegment& wrapped_segment = tokenized_segments.emplace_back();
            wrapped_segment.tokens.reserve(wrapped_segment_length);
            for (size_t i = 0; i < wrapped_segment_length; i++) {
                const size_t token_index = segment_start + i;
                wrapped_segment.tokens.emplace_back(
                    *plain,
                    token_ranges[token_index],
                    segment_tokens[token_index]
                );
            }
        }
    }

    return std::make_shared<TokenizedString>(
        std::move(parameters),
        std::shared_ptr(plain),
        std::move(tokenized_segments)
    );
}

std::shared_ptr<TokenizedString> decode_string(const std::shared_ptr<TokenizedString>& source, const std::shared_ptr<marian::Vocab const>& vocab, const std::shared_ptr<marian::History>* histories) {
    std::string target_plain;
    std::vector<TokenizedSegment> target_segments;
    target_plain.reserve(source->plain->size());
    target_segments.reserve(source->segments.size());

    for (int i = 0; i < source->segments.size(); i++) {
        const std::shared_ptr<marian::History>& history = histories[i];

        const marian::NBestList results = history->nBest(1);
        const marian::Words& tokens = std::get<0>(results[0]);

        std::string plain_segment;
        std::vector<marian::string_view> token_ranges;
        vocab->decodeWithByteRanges(tokens, plain_segment, token_ranges, true);

        // Tokens between segments might not include the whitespace/punctuation, so we need to reinsert these
        target_plain += gap_before(*source, i);

        // Note: `tokens` contains an additional entry for the EOS marker, but we want to discard this
        const size_t token_count = token_ranges.size();

        TokenizedSegment& segment = target_segments.emplace_back();
        segment.tokens.reserve(token_count);
        for (size_t token_index = 0; token_index < token_count; token_index++) {
            Token& token = segment.tokens.emplace_back(
                plain_segment,
                token_ranges[token_index],
                tokens[token_index]
            );
            token.begin += target_plain.length();
            token.end += target_plain.length();
        }

        target_plain += plain_segment;
    }

    target_plain += gap_before(*source, source->segments.size());

    return std::make_shared<TokenizedString>(
        TokenizationParameters{vocab, source->parameters.max_segment_length, source->parameters.segment_split_mode},
        std::make_shared<std::string>(target_plain),
        std::move(target_segments)
    );
}

std::shared_ptr<marian::data::CorpusBatch> generate_corpus_batch(const std::vector<std::shared_ptr<TokenizedString>>& batch, const std::shared_ptr<const marian::Vocab>& source_vocab) {
    size_t batch_size = 0;
    size_t max_segment_length = 0;
    for (const std::shared_ptr<TokenizedString>& source : batch) {
        assert(source->parameters.vocab == source_vocab);
        for (const TokenizedSegment& segment : source->segments) {
            if (segment.tokens.size() > max_segment_length) {
                max_segment_length = segment.tokens.size();
            }
            batch_size++;
        }
    }

    // +1 for EOS token
    const std::shared_ptr<marian::data::SubBatch> sub_batch = std::make_shared<marian::data::SubBatch>(batch_size, max_segment_length + 1, source_vocab);

    size_t segment_id = 0;
    size_t token_count = 0;
    std::vector<size_t> segment_ids;
    segment_ids.reserve(batch_size);

    const marian::Word eos_token = source_vocab->getEosId();

    for (const std::shared_ptr<TokenizedString>& source : batch) {
        for (const TokenizedSegment& segment : source->segments) {
            for (size_t token_index = 0; token_index < segment.tokens.size(); token_index++) {
                const size_t index = token_index * batch_size + segment_id;
                sub_batch->data()[index] = segment.tokens[token_index].id;
                sub_batch->mask()[index] = 1.0f;
                token_count++;
            }

            const size_t eos_index = segment.tokens.size() * batch_size + segment_id;
            sub_batch->data()[eos_index] = eos_token;
            sub_batch->mask()[eos_index] = 1.0f;
            token_count++;

            segment_ids.push_back(segment_id);
            segment_id++;
        }
    }

    sub_batch->setWords(token_count);

    std::shared_ptr<marian::data::CorpusBatch> corpus_batch = std::make_shared<marian::data::CorpusBatch>(std::vector{sub_batch});
    corpus_batch->setSentenceIds(segment_ids);
    return corpus_batch;
}
