#include "parser_internal.h"
#include "../semantic/type_system.h"

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
parser_effect_mask_from_token(Token tok, uint32_t *mask_out)
{
    if (mask_out == NULL)
        return false;

    if (tok.type == TOKEN_SECURE) {
        *mask_out = EFFECT_SECURE;
        return true;
    }
    if (tok.type == TOKEN_REMOTE) {
        *mask_out = EFFECT_REMOTE;
        return true;
    }
    if (tok.type == TOKEN_NONDETERMINISTIC) {
        *mask_out = EFFECT_NONDETERMINISTIC;
        return true;
    }
    if (tok.type == TOKEN_COLLAPSE) {
        *mask_out = EFFECT_COLLAPSE;
        return true;
    }
    if (tok.type == TOKEN_LOCAL) {
        *mask_out = EFFECT_NONE;
        return true;
    }
    if (tok.type == TOKEN_UNSAFE) {
        *mask_out = EFFECT_UNSAFE;
        return true;
    }

    if (tok.text == NULL)
        return false;
    if (strcmp(tok.text, "secure") == 0) {
        *mask_out = EFFECT_SECURE;
        return true;
    }
    if (strcmp(tok.text, "remote") == 0) {
        *mask_out = EFFECT_REMOTE;
        return true;
    }
    if (strcmp(tok.text, "nondeterministic") == 0) {
        *mask_out = EFFECT_NONDETERMINISTIC;
        return true;
    }
    if (strcmp(tok.text, "collapse") == 0) {
        *mask_out = EFFECT_COLLAPSE;
        return true;
    }
    if (strcmp(tok.text, "io") == 0) {
        *mask_out = EFFECT_IO;
        return true;
    }
    if (strcmp(tok.text, "alloc") == 0) {
        *mask_out = EFFECT_ALLOC;
        return true;
    }
    if (strcmp(tok.text, "authority") == 0) {
        *mask_out = EFFECT_AUTHORITY;
        return true;
    }
    if (strcmp(tok.text, "local") == 0) {
        *mask_out = EFFECT_NONE;
        return true;
    }

    return false;
}

void
parse_optional_effect_clause(Parser *parser, bool *has_clause_out,
                             uint32_t *mask_out)
{
    uint32_t mask = 0;

    if (has_clause_out == NULL || mask_out == NULL || !parser_match(parser, TOKEN_WITH))
        return;

    *has_clause_out = true;

    if (!parser_check(parser, TOKEN_IDENTIFIER)
        || parser->current_token.text == NULL
        || strcmp(parser->current_token.text, "effects") != 0) {
        parser_error(parser,
                     "Expected 'effects' after 'with' in function/action clause; use 'with effects ...'");
        return;
    }
    parser_advance(parser);

    while (!parser_is_at_end(parser)) {
        uint32_t effect = EFFECT_NONE;
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

        if (!parser_effect_mask_from_token(tok, &effect)) {
            parser_error(parser, "Unknown effect '%s' in 'with effects' clause",
                         tok.text != NULL ? tok.text : "<token>");
            return;
        }

        mask |= effect;
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }

    *mask_out = mask;
}
