#ifndef PGY_LEXER_KEYWORDS_H
#define PGY_LEXER_KEYWORDS_H

#include <stddef.h>
#include <stdint.h>

#include "lexer.h"

typedef enum {
    PGY_KEYWORD_CLASS_RESERVED = 1,
    PGY_KEYWORD_CLASS_CONTEXTUAL,
    PGY_KEYWORD_CLASS_SOFT
} PgyLanguageKeywordClass;

/* Stable language-word identity is projected from the registry's existing
 * canonical debug symbol. UNKNOWN is never a row identity; COUNT is the
 * checked exclusive upper bound. */
typedef enum {
    PGY_LANGUAGE_WORD_UNKNOWN = -1,
#define PGY_LANGUAGE_KEYWORD(                                           \
    spelling_value, class_value, token_value, debug_value,              \
    context_value, axis_value, support_value, tooling_value,            \
    highlight_scope_value)                                              \
    PGY_LANGUAGE_WORD_##debug_value,
#include "language_keyword_registry.def"
#undef PGY_LANGUAGE_KEYWORD
    PGY_LANGUAGE_WORD_COUNT
} PgyLanguageWordId;

/* Contextual and soft rows have no lexer token identity. */
#define PGY_KEYWORD_TOKEN_NONE ((PgyTokenType)-1)

typedef enum {
    PGY_KEYWORD_CONTEXT_DECLARATION = 1u << 0,
    PGY_KEYWORD_CONTEXT_STATEMENT   = 1u << 1,
    PGY_KEYWORD_CONTEXT_EXPRESSION  = 1u << 2,
    PGY_KEYWORD_CONTEXT_TYPE        = 1u << 3,
    PGY_KEYWORD_CONTEXT_CLAUSE      = 1u << 4,
    PGY_KEYWORD_CONTEXT_MODULE      = 1u << 5,
    PGY_KEYWORD_CONTEXT_INTENT_STEP = 1u << 6,
    PGY_KEYWORD_CONTEXT_ZONE_BODY   = 1u << 7,
    PGY_KEYWORD_CONTEXT_NAME        = 1u << 8,
    PGY_KEYWORD_CONTEXT_PARAMETER   = 1u << 9
} PgyLanguageKeywordContext;

typedef enum {
    PGY_KEYWORD_AXIS_GENERAL = 0,
    PGY_KEYWORD_AXIS_RESOURCE,
    PGY_KEYWORD_AXIS_EXECUTION,
    PGY_KEYWORD_AXIS_DOMAIN,
    PGY_KEYWORD_AXIS_TYPE_CONTRACT
} PgyLanguageKeywordAxis;

typedef enum {
    PGY_KEYWORD_SUPPORT_NATIVE    = 1u << 0,
    PGY_KEYWORD_SUPPORT_SELF_HOST = 1u << 1
} PgyLanguageKeywordSupport;

typedef enum {
    PGY_KEYWORD_TOOLING_COMPLETION = 1u << 0,
    PGY_KEYWORD_TOOLING_HOVER      = 1u << 1,
    PGY_KEYWORD_TOOLING_HIGHLIGHT  = 1u << 2
} PgyLanguageKeywordTooling;

typedef enum {
    PGY_KEYWORD_HIGHLIGHT_NONE = 0,
    PGY_KEYWORD_HIGHLIGHT_CONTROL,
    PGY_KEYWORD_HIGHLIGHT_DECLARATION,
    PGY_KEYWORD_HIGHLIGHT_MODIFIER,
    PGY_KEYWORD_HIGHLIGHT_CONSTANT,
    PGY_KEYWORD_HIGHLIGHT_TYPE,
    PGY_KEYWORD_HIGHLIGHT_DOMAIN,
    PGY_KEYWORD_HIGHLIGHT_INTENT
} PgyLanguageKeywordHighlightScope;

typedef struct {
    PgyLanguageWordId word_id;
    const char *spelling;
    PgyLanguageKeywordClass keyword_class;
    PgyTokenType token_type;
    const char *debug_identity;
    uint32_t context_mask;
    PgyLanguageKeywordAxis axis;
    uint32_t implementation_support;
    uint32_t tooling_flags;
    PgyLanguageKeywordHighlightScope highlight_scope;
} PgyLanguageKeywordRow;

PgyTokenType lexer_lookup_keyword(const char *text, size_t length);
PgyLanguageWordId lexer_lookup_language_word(const char *text, size_t length);
size_t lexer_keyword_registry_count(void);
const PgyLanguageKeywordRow *lexer_keyword_registry_row(size_t index);
const PgyLanguageKeywordRow *lexer_language_word_row(PgyLanguageWordId word_id);
const char *lexer_keyword_debug_name(PgyTokenType type);

#endif /* PGY_LEXER_KEYWORDS_H */
