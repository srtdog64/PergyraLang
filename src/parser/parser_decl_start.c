#include "parser_internal.h"

bool
parser_starts_named_declaration(Parser *parser, PgyTokenType keyword)
{
    Token next;

    if (parser == NULL || !parser_check(parser, keyword))
        return false;

    next = parser_peek_next(parser);

    switch (next.type) {
    case TOKEN_IDENTIFIER:
    case TOKEN_SLOT:
    case TOKEN_SUBJECT:
    case TOKEN_CLASS:
    case TOKEN_STRUCT:
    case TOKEN_OBJECT:
    case TOKEN_TOBJECT:
    case TOKEN_VESSEL:
    case TOKEN_INTENT:
    case TOKEN_ROSTER:
    case TOKEN_WORLD:
        return true;
    default:
        return false;
    }
}
