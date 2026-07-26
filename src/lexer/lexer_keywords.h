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
    PGY_KEYWORD_CONTEXT_NAME        = 1u << 8
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

typedef struct {
    const char *spelling;
    PgyLanguageKeywordClass keyword_class;
    PgyTokenType token_type;
    const char *debug_identity;
    uint32_t context_mask;
    PgyLanguageKeywordAxis axis;
    uint32_t implementation_support;
    uint32_t tooling_flags;
} PgyLanguageKeywordRow;

PgyTokenType lexer_lookup_keyword(const char *text, size_t length);
size_t lexer_keyword_registry_count(void);
const PgyLanguageKeywordRow *lexer_keyword_registry_row(size_t index);
const char *lexer_keyword_debug_name(PgyTokenType type);

#endif /* PGY_LEXER_KEYWORDS_H */
