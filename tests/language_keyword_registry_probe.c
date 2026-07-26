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

    if (lexer_keyword_registry_count() != 145) {
        fprintf(stderr, "registry row count is not 145\n");
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
        looked_up = lexer_lookup_keyword(row->spelling, strlen(row->spelling));
        if (row->keyword_class == PGY_KEYWORD_CLASS_RESERVED) {
            const char *debug_identity;

            reserved_count++;
            if (looked_up != row->token_type) {
                fprintf(stderr, "reserved lookup drift: %s\n", row->spelling);
                return 3;
            }
            debug_identity = lexer_keyword_debug_name(row->token_type);
            if (debug_identity == NULL
                || strcmp(debug_identity, row->debug_identity) != 0) {
                fprintf(stderr, "reserved debug drift: %s\n", row->spelling);
                return 4;
            }
        } else {
            if (row->keyword_class == PGY_KEYWORD_CLASS_CONTEXTUAL)
                contextual_count++;
            else if (row->keyword_class == PGY_KEYWORD_CLASS_SOFT)
                soft_count++;
            else
                return 5;
            if (row->token_type != PGY_KEYWORD_TOKEN_NONE
                || looked_up != TOKEN_IDENTIFIER) {
                fprintf(stderr, "non-reserved token drift: %s\n", row->spelling);
                return 6;
            }
        }
    }

    if (reserved_count != 71 || contextual_count != 71 || soft_count != 3)
        return 7;
    if (lexer_keyword_debug_name(PGY_KEYWORD_TOKEN_NONE) != NULL) {
        fprintf(stderr, "non-reserved debug identity leaked into token lookup\n");
        return 8;
    }
    return 0;
}
