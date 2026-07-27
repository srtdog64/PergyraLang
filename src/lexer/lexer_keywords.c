#include "lexer_keywords.h"

#include <string.h>

#define PGY_LANGUAGE_KEYWORD(                                           \
    spelling_value, class_value, token_value, debug_value,              \
    context_value, axis_value, support_value, tooling_value,             \
    highlight_scope_value)                                              \
    {                                                                    \
        PGY_LANGUAGE_WORD_##debug_value, spelling_value, class_value,    \
        token_value, #debug_value, context_value, axis_value,            \
        support_value, tooling_value, highlight_scope_value              \
    },

static const PgyLanguageKeywordRow kLanguageKeywordRegistry[] = {
#include "language_keyword_registry.def"
};

#undef PGY_LANGUAGE_KEYWORD

size_t
lexer_keyword_registry_count(void)
{
    return sizeof(kLanguageKeywordRegistry)
        / sizeof(kLanguageKeywordRegistry[0]);
}

const PgyLanguageKeywordRow *
lexer_keyword_registry_row(size_t index)
{
    if (index >= lexer_keyword_registry_count())
        return NULL;
    return &kLanguageKeywordRegistry[index];
}

const PgyLanguageKeywordRow *
lexer_language_word_row(PgyLanguageWordId word_id)
{
    size_t index = (size_t)word_id;

    if (word_id < 0 || word_id >= PGY_LANGUAGE_WORD_COUNT ||
        index >= lexer_keyword_registry_count() ||
        kLanguageKeywordRegistry[index].word_id != word_id)
        return NULL;
    return &kLanguageKeywordRegistry[index];
}

const char *
lexer_keyword_debug_name(PgyTokenType type)
{
    size_t row = 0;
    size_t count = lexer_keyword_registry_count();

    while (row < count) {
        if (kLanguageKeywordRegistry[row].keyword_class ==
                PGY_KEYWORD_CLASS_RESERVED &&
            kLanguageKeywordRegistry[row].token_type == type)
            return kLanguageKeywordRegistry[row].debug_identity;
        row++;
    }
    return NULL;
}

static int
keyword_compare_slice(const char *text, size_t length, const char *keyword)
{
    size_t keyword_len = strlen(keyword);
    size_t limit = length < keyword_len ? length : keyword_len;
    int cmp = strncmp(text, keyword, limit);

    if (cmp != 0)
        return cmp;
    if (length < keyword_len)
        return -1;
    if (length > keyword_len)
        return 1;
    return 0;
}

PgyLanguageWordId
lexer_lookup_language_word(const char *text, size_t length)
{
    size_t lo = 0;
    size_t hi = lexer_keyword_registry_count();

    if (text == NULL)
        return PGY_LANGUAGE_WORD_UNKNOWN;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const PgyLanguageKeywordRow *row = &kLanguageKeywordRegistry[mid];
        int cmp = keyword_compare_slice(text, length, row->spelling);
        if (cmp == 0)
            return row->word_id;
        if (cmp < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return PGY_LANGUAGE_WORD_UNKNOWN;
}

PgyTokenType
lexer_lookup_keyword(const char *text, size_t length)
{
    PgyLanguageWordId word_id = lexer_lookup_language_word(text, length);
    const PgyLanguageKeywordRow *row = lexer_language_word_row(word_id);

    if (row == NULL || row->keyword_class != PGY_KEYWORD_CLASS_RESERVED)
        return TOKEN_IDENTIFIER;
    return row->token_type;
}
