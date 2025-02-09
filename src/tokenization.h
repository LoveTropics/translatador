#ifndef TOKENIZATION_H
#define TOKENIZATION_H

#include <marian.h>
#include <translator/beam_search.h>
#include <ssplit.h>

typedef ug::ssplit::SentenceStream::splitmode SsplitMode;

SsplitMode parse_ssplit_mode(const std::string& mode);

struct TokenizationParameters {
    std::shared_ptr<marian::Vocab const> vocab;
    size_t max_segment_length;
    SsplitMode segment_split_mode;

    bool operator!=(const TokenizationParameters& parameters) const {
        return vocab != parameters.vocab
               || max_segment_length != parameters.max_segment_length
               || segment_split_mode != parameters.segment_split_mode;
    }
};

struct Token {
    marian::Word id;
    size_t begin;
    size_t end;

    explicit Token(
        const std::string& source,
        const marian::string_view token_view,
        const marian::Word id
    ): id(id),
       begin(token_view.data() - source.data()),
       end(token_view.data() - source.data() + token_view.length()) {
    }
};

struct TokenizedSegment {
    std::vector<Token> tokens;
};

struct TokenizedString {
    const TokenizationParameters parameters;
    const std::shared_ptr<std::string> plain;
    const std::vector<TokenizedSegment> segments;

    explicit TokenizedString(
        TokenizationParameters&& parameters,
        std::shared_ptr<std::string>&& plain,
        std::vector<TokenizedSegment>&& segments
    ): parameters(std::move(parameters)),
       plain(std::move(plain)),
       segments(std::move(segments)) {
    }
};

std::shared_ptr<TokenizedString> tokenize(const std::shared_ptr<std::string>& plain, TokenizationParameters&& parameters);

std::shared_ptr<marian::data::CorpusBatch> generate_corpus_batch(const std::vector<std::shared_ptr<TokenizedString>>& batch, const std::shared_ptr<const marian::Vocab>& source_vocab);

std::shared_ptr<TokenizedString> decode_string(const std::shared_ptr<TokenizedString>& source, const std::shared_ptr<marian::Vocab const>& vocab, const std::shared_ptr<marian::History>* histories);

#endif
