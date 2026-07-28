#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "lexer/lexer_keywords.h"

int
main(void)
{
    size_t index;
    size_t reserved_count = 0;
    size_t contextual_count = 0;
    size_t soft_count = 0;
    bool seen_word_ids[PGY_LANGUAGE_WORD_COUNT] = { false };
    const uint32_t known_contexts =
        PGY_KEYWORD_CONTEXT_DECLARATION | PGY_KEYWORD_CONTEXT_STATEMENT |
        PGY_KEYWORD_CONTEXT_EXPRESSION | PGY_KEYWORD_CONTEXT_TYPE |
        PGY_KEYWORD_CONTEXT_CLAUSE | PGY_KEYWORD_CONTEXT_MODULE |
        PGY_KEYWORD_CONTEXT_INTENT_STEP | PGY_KEYWORD_CONTEXT_ZONE_BODY |
        PGY_KEYWORD_CONTEXT_NAME | PGY_KEYWORD_CONTEXT_PARAMETER;

    if (lexer_keyword_registry_count() != 144
        || PGY_LANGUAGE_WORD_COUNT != 144) {
        fprintf(stderr, "registry/word-id count is not 144\n");
        return 1;
    }

    for (index = 0; index < lexer_keyword_registry_count(); index++) {
        const PgyLanguageKeywordRow *row =
            lexer_keyword_registry_row(index);
        PgyTokenType looked_up;

        if (row == NULL || row->spelling == NULL) {
            fprintf(stderr, "missing registry row %zu\n", index);
            return 2;
        }
        if (row->word_id < 0 || row->word_id >= PGY_LANGUAGE_WORD_COUNT
            || row->word_id != (PgyLanguageWordId)index
            || seen_word_ids[row->word_id]
            || lexer_language_word_row(row->word_id) != row
            || lexer_lookup_language_word(row->spelling, strlen(row->spelling))
                != row->word_id) {
            fprintf(stderr, "language-word identity drift: %s\n", row->spelling);
            return 3;
        }
        seen_word_ids[row->word_id] = true;
        if (row->context_mask == 0
            || (row->context_mask & ~known_contexts) != 0
            || (row->implementation_support & PGY_KEYWORD_SUPPORT_NATIVE) == 0
            || row->highlight_scope < PGY_KEYWORD_HIGHLIGHT_NONE
            || row->highlight_scope > PGY_KEYWORD_HIGHLIGHT_INTENT
            || ((row->tooling_flags & PGY_KEYWORD_TOOLING_HIGHLIGHT) != 0)
                != (row->highlight_scope != PGY_KEYWORD_HIGHLIGHT_NONE)) {
            fprintf(stderr, "language-word metadata drift: %s\n", row->spelling);
            return 4;
        }
        looked_up = lexer_lookup_keyword(row->spelling, strlen(row->spelling));
        if (row->keyword_class == PGY_KEYWORD_CLASS_RESERVED) {
            const char *debug_identity;

            reserved_count++;
            if (looked_up != row->token_type) {
                fprintf(stderr, "reserved lookup drift: %s\n", row->spelling);
                return 5;
            }
            debug_identity = lexer_keyword_debug_name(row->token_type);
            if (debug_identity == NULL
                || strcmp(debug_identity, row->debug_identity) != 0) {
                fprintf(stderr, "reserved debug drift: %s\n", row->spelling);
                return 6;
            }
        } else {
            if (row->keyword_class == PGY_KEYWORD_CLASS_CONTEXTUAL)
                contextual_count++;
            else if (row->keyword_class == PGY_KEYWORD_CLASS_SOFT)
                soft_count++;
            else
                return 7;
            if (row->token_type != PGY_KEYWORD_TOKEN_NONE
                || looked_up != TOKEN_IDENTIFIER) {
                fprintf(stderr, "non-reserved token drift: %s\n", row->spelling);
                return 8;
            }
        }
    }

    if (reserved_count != 70 || contextual_count != 71 || soft_count != 3)
        return 9;
    if (lexer_keyword_debug_name(PGY_KEYWORD_TOKEN_NONE) != NULL) {
        fprintf(stderr, "non-reserved debug identity leaked into token lookup\n");
        return 10;
    }
    if (lexer_language_word_row(PGY_LANGUAGE_WORD_UNKNOWN) != NULL
        || lexer_language_word_row(PGY_LANGUAGE_WORD_COUNT) != NULL
        || lexer_lookup_language_word(NULL, 0) != PGY_LANGUAGE_WORD_UNKNOWN
        || lexer_lookup_language_word("not_a_language_word", 19)
            != PGY_LANGUAGE_WORD_UNKNOWN) {
        fprintf(stderr, "unknown language-word lookup did not fail closed\n");
        return 11;
    }
    if (lexer_language_word_row(PGY_LANGUAGE_WORD_OWN)->context_mask
            != PGY_KEYWORD_CONTEXT_PARAMETER
        || lexer_language_word_row(PGY_LANGUAGE_WORD_REF)->context_mask
            != PGY_KEYWORD_CONTEXT_PARAMETER
        || lexer_language_word_row(PGY_LANGUAGE_WORD_TYPE)->context_mask
            != PGY_KEYWORD_CONTEXT_DECLARATION
        || lexer_language_word_row(PGY_LANGUAGE_WORD_IMPL)->context_mask
            != (PGY_KEYWORD_CONTEXT_DECLARATION | PGY_KEYWORD_CONTEXT_TYPE)) {
        fprintf(stderr, "own/ref/type/impl grammar context drift\n");
        return 12;
    }
    return 0;
}
