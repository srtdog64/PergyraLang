#include "parser_internal.h"

Token
consume_name_token(Parser *parser, const char *message)
{
    if (parser_check_name_token(parser))
        return parser_advance(parser);
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}

Token
consume_decl_name_token(Parser *parser, const char *message)
{
    if (parser_check_decl_name_token(parser))
        return parser_advance(parser);
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}

Token
consume_binding_name_token(Parser *parser, const char *message)
{
    if (parser_check_binding_name_token(parser))
        return parser_advance(parser);
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}

bool
parser_append_destructure_name(Parser *parser, ASTNode *node, const char *name)
{
    char **grown;
    char *owned_name;

    if (parser == NULL || node == NULL)
        return false;

    owned_name = pergyra_strdup(name);
    if (owned_name == NULL) {
        parser_error(parser, "Out of memory while parsing destructuring name");
        return false;
    }

    if (node->data.let_destructure.name_count == node->data.let_destructure.name_capacity) {
        size_t next_capacity = node->data.let_destructure.name_capacity == 0
            ? 4
            : node->data.let_destructure.name_capacity * 2;
        if (next_capacity < node->data.let_destructure.name_capacity
            || next_capacity > SIZE_MAX / sizeof(char *)) {
            free(owned_name);
            parser_error(parser, "Out of memory while parsing destructuring names");
            return false;
        }
        grown = realloc(node->data.let_destructure.names, next_capacity * sizeof(char *));
        if (grown == NULL) {
            free(owned_name);
            parser_error(parser, "Out of memory while parsing destructuring names");
            return false;
        }
        node->data.let_destructure.names = grown;
        node->data.let_destructure.name_capacity = next_capacity;
    }

    node->data.let_destructure.names[node->data.let_destructure.name_count++] = owned_name;
    return true;
}

bool
parser_check_name_token(Parser *parser)
{
    return parser_check_decl_name_token(parser);
}

bool
parser_check_decl_name_token(Parser *parser)
{
    if (parser == NULL)
        return false;

    switch (parser->current_token.type) {
    case TOKEN_IDENTIFIER:
        return true;
    default:
        return false;
    }
}

bool
parser_check_binding_name_token(Parser *parser)
{
    if (parser == NULL)
        return false;

    switch (parser->current_token.type) {
    case TOKEN_IDENTIFIER:
    case TOKEN_SLOT:
    case TOKEN_EVENT:
    case TOKEN_WORLD:
    case TOKEN_ZONE:
    case TOKEN_ROSTER:
    case TOKEN_RELATION:
    case TOKEN_EFFECT:
    case TOKEN_ROLE:
    case TOKEN_PARTY:
        return true;
    default:
        return false;
    }
}

bool
parser_match_name_token(Parser *parser)
{
    return parser_match_expr_name_token(parser);
}

bool
parser_check_expr_name_token(Parser *parser)
{
    return parser_check_binding_name_token(parser);
}

bool
parser_match_expr_name_token(Parser *parser)
{
    if (!parser_check_expr_name_token(parser))
        return false;
    parser_advance(parser);
    return true;
}

Token
consume_member_name_token(Parser *parser, const char *message)
{
    if (parser_check_expr_name_token(parser))
        return parser_advance(parser);
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}
