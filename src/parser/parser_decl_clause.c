#include "parser_internal.h"
#include "../lexer/lexer_keywords.h"
#include "../semantic/callable_contract_vocabulary.h"

bool
parser_decl_match_contextual_keyword(Parser *parser, const char *keyword)
{
    if (parser == NULL || keyword == NULL
        || !parser_check(parser, TOKEN_IDENTIFIER)
        || parser->current_token.text == NULL
        || strcmp(parser->current_token.text, keyword) != 0) {
        return false;
    }

    parser_advance(parser);
    return true;
}

bool
parser_decl_check_contextual_keyword(Parser *parser, const char *keyword)
{
    return parser != NULL && keyword != NULL
        && parser_check(parser, TOKEN_IDENTIFIER)
        && parser->current_token.text != NULL
        && strcmp(parser->current_token.text, keyword) == 0;
}

static bool
parser_decl_token_is_language_word(Token tok, PgyLanguageWordId word_id)
{
    return tok.text != NULL &&
        lexer_lookup_language_word(tok.text, strlen(tok.text)) == word_id;
}

void
parse_optional_effect_clause(Parser *parser, bool *has_clause_out,
                             uint32_t *mask_out)
{
    uint32_t mask = 0;
    uint32_t seen_words = 0;
    bool saw_zero_exclusive = false;

    if (has_clause_out == NULL || mask_out == NULL || !parser_match(parser, TOKEN_WITH))
        return;

    *has_clause_out = true;

    if (!parser_decl_token_is_language_word(
            parser->current_token, PGY_LANGUAGE_WORD_EFFECTS)) {
        parser_error(parser,
                     "Expected 'effects' after 'with' in function/action clause; use 'with effects ...'");
        return;
    }
    parser_advance(parser);

    while (!parser_is_at_end(parser)) {
        const PgyCallableContractWordSpec *effect;
        Token tok;

        if (parser_check(parser, TOKEN_IDENTIFIER)
            || parser_check(parser, TOKEN_SECURE)
            || parser_check(parser, TOKEN_REMOTE)
            || parser_check(parser, TOKEN_NONDETERMINISTIC)
            || parser_check(parser, TOKEN_COLLAPSE)
            || parser_check(parser, TOKEN_LOCAL)
            || parser_check(parser, TOKEN_UNSAFE))
            tok = parser_advance(parser);
        else {
            parser_error(parser,
                         "Expected effect name after 'with effects' in function/action clause");
            return;
        }

        effect = pgy_callable_contract_vocabulary_find(
            PGY_CALLABLE_CONTRACT_AXIS_EFFECT, tok.text);
        if (effect == NULL) {
            parser_error(parser, "Unknown effect '%s' in 'with effects' clause",
                         tok.text != NULL ? tok.text : "<token>");
            return;
        }
        if ((seen_words & (1u << (uint32_t)effect->id)) != 0) {
            parser_error(parser, "Duplicate effect '%s' in 'with effects' clause",
                         effect->spelling);
            return;
        }
        if ((effect->zero_policy == PGY_CALLABLE_CONTRACT_ZERO_EXCLUSIVE &&
             seen_words != 0) ||
            (effect->zero_policy != PGY_CALLABLE_CONTRACT_ZERO_EXCLUSIVE &&
             saw_zero_exclusive)) {
            parser_error(parser,
                         "Effect 'local' cannot be combined with other effects");
            return;
        }

        seen_words |= 1u << (uint32_t)effect->id;
        saw_zero_exclusive = effect->zero_policy ==
            PGY_CALLABLE_CONTRACT_ZERO_EXCLUSIVE;
        mask |= effect->mask;
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }

    *mask_out = mask;
}

void
parse_optional_caps_clause(Parser *parser, bool *has_clause_out,
                           uint32_t *mask_out)
{
    uint32_t mask = 0;
    uint32_t seen_words = 0;

    if (has_clause_out == NULL || mask_out == NULL
        || !parser_match(parser, TOKEN_WITH))
        return;

    *has_clause_out = true;

    if (!parser_decl_token_is_language_word(
            parser->current_token, PGY_LANGUAGE_WORD_CAPS)) {
        parser_error(parser,
                     "Expected 'caps' after 'with' in function/action clause; use 'with caps ...'");
        return;
    }
    parser_advance(parser);

    while (!parser_is_at_end(parser)) {
        const PgyCallableContractWordSpec *cap;
        Token tok;

        if (!parser_check(parser, TOKEN_IDENTIFIER)) {
            parser_error(parser,
                         "Expected capability name after 'with caps' in function/action clause");
            return;
        }
        tok = parser_advance(parser);

        cap = pgy_callable_contract_vocabulary_find(
            PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY, tok.text);
        if (cap == NULL) {
            parser_error(parser, "Unknown capability '%s' in 'with caps' clause; "
                                 "expected a registered callable capability",
                         tok.text != NULL ? tok.text : "<token>");
            return;
        }
        if ((seen_words & (1u << (uint32_t)cap->id)) != 0) {
            parser_error(parser,
                         "Duplicate capability '%s' in 'with caps' clause",
                         cap->spelling);
            return;
        }

        seen_words |= 1u << (uint32_t)cap->id;
        mask |= cap->mask;
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }

    *mask_out = mask;
}
