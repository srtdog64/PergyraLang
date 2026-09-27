#include "parser_internal.h"
#include "../lexer/lexer_keywords.h"

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

static Token
consume_unreserved_name_token(Parser *parser, const char *message,
                              const char *role)
{
    if (parser_check_binding_name_token(parser))
        return parser_advance(parser);
    /* A reserved word the registry does not admit as a name: say so at the
     * word, as the self-host parser does (binding_name_reserved,
     * field_name_reserved), instead of only naming what was expected. */
    if (parser != NULL && parser->current_token.text != NULL
        && lexer_reserved_keyword_row(parser->current_token.type) != NULL) {
        parser_error(parser,
            "'%s' is a reserved language word and cannot name a %s; "
            "rename it, for example by adding a suffix",
            parser->current_token.text, role);
        return parser->current_token;
    }
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}

Token
consume_binding_name_token(Parser *parser, const char *message)
{
    return consume_unreserved_name_token(parser, message, "binding");
}

Token
consume_field_name_token(Parser *parser, const char *message)
{
    Token name = consume_unreserved_name_token(parser, message, "field");
    /* A refused reserved word still stands in the name position; step past
     * it so the field's ':' and type parse and the body recovers at its end
     * instead of reporting the closing brace as a second error. */
    if (parser != NULL && lexer_reserved_keyword_row(name.type) != NULL
        && parser->current_token.line == name.line
        && parser->current_token.column == name.column)
        parser_advance(parser);
    return name;
}

/* A field name position: a name token, or a reserved word the field
 * consumer will refuse by name, followed by ':'. */
bool
parser_check_field_name_ahead(Parser *parser)
{
    if (parser == NULL || parser_peek_next(parser).type != TOKEN_COLON)
        return false;
    return parser_check_binding_name_token(parser)
        || lexer_reserved_keyword_row(parser->current_token.type) != NULL;
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

/* A reserved word names a binding only when its registry row carries the
 * NAME context; the self-hosted parser reads the same rows. */
bool
parser_check_binding_name_token(Parser *parser)
{
    const PgyLanguageKeywordRow *row;

    if (parser == NULL)
        return false;
    if (parser->current_token.type == TOKEN_IDENTIFIER)
        return true;
    row = lexer_reserved_keyword_row(parser->current_token.type);
    return row != NULL
        && (row->context_mask & PGY_KEYWORD_CONTEXT_NAME) != 0;
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
    /* No field can carry a reserved word (consume_field_name_token), so a
     * member access naming one gets the same reason. */
    if (parser != NULL && parser->current_token.text != NULL
        && lexer_reserved_keyword_row(parser->current_token.type) != NULL) {
        parser_error(parser,
            "'%s' is a reserved language word and cannot name a field; "
            "rename the field, for example by adding a suffix",
            parser->current_token.text);
        return parser_advance(parser);
    }
    return parser_consume(parser, TOKEN_IDENTIFIER, message);
}
