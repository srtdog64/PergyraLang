#include "parser_internal.h"

static bool
parser_grow_map_entry_arrays(Parser *parser, ASTNode *map, size_t *capacity)
{
    size_t next = (*capacity == 0) ? 4 : *capacity * 2;
    ASTNode **grown_keys;
    ASTNode **grown_values;

    if (next < *capacity || next > SIZE_MAX / sizeof(ASTNode *)) {
        parser_error(parser, "Map literal has too many entries");
        return false;
    }
    grown_keys = realloc(map->data.map_literal.keys,
                         next * sizeof(ASTNode *));
    if (grown_keys == NULL) {
        parser_error(parser, "Out of memory while parsing map literal");
        return false;
    }
    map->data.map_literal.keys = grown_keys;
    grown_values = realloc(map->data.map_literal.values,
                           next * sizeof(ASTNode *));
    if (grown_values == NULL) {
        parser_error(parser, "Out of memory while parsing map literal");
        return false;
    }
    map->data.map_literal.values = grown_values;
    *capacity = next;
    return true;
}

static bool
parser_append_map_entry(Parser *parser, ASTNode *map, size_t *capacity,
                        ASTNode *key, ASTNode *value)
{
    if (map->data.map_literal.count == *capacity
        && !parser_grow_map_entry_arrays(parser, map, capacity)) {
        return false;
    }
    map->data.map_literal.keys[map->data.map_literal.count] = key;
    map->data.map_literal.values[map->data.map_literal.count] = value;
    map->data.map_literal.count++;
    return true;
}

/* Bare brace literal in expression position: a map `{ k: v, ... }`, a set
 * `{ a, b, ... }`, or an empty form. The colon is the map marker: `{:}` is an
 * empty map, `{}` is an empty set; for a non-empty brace we parse the first
 * element and peek -- a following ':' means map, otherwise set. (Object
 * initializers `Type { field: value }` are handled in the postfix path, so an
 * opening brace reaching primary is a map/set literal.) */
ASTNode *
parse_map_literal_expression(Parser *parser)
{
    int line = parser->current_token.line;
    int col = parser->current_token.column;
    ASTNode *first;

    parser_consume(parser, TOKEN_LBRACE,
                   "Expected '{' to begin map or set literal");

    if (parser_check(parser, TOKEN_RBRACE)) {
        ASTNode *set = calloc(1, sizeof(ASTNode));
        if (set == NULL) {
            parser_error(parser, "Out of memory while parsing set literal");
            return NULL;
        }
        set->type = AST_SET_LITERAL;
        set->line = line;
        set->column = col;
        parser_advance(parser);
        return set;
    }
    if (parser_check(parser, TOKEN_COLON)) {
        ASTNode *map = calloc(1, sizeof(ASTNode));
        if (map == NULL) {
            parser_error(parser, "Out of memory while parsing map literal");
            return NULL;
        }
        map->type = AST_MAP_LITERAL;
        map->line = line;
        map->column = col;
        parser_advance(parser);
        parser_consume(parser, TOKEN_RBRACE,
                       "Expected '}' after empty map literal '{:}'");
        return map;
    }

    first = parser_parse_expression(parser);

    if (parser_check(parser, TOKEN_COLON)) {
        ASTNode *map = calloc(1, sizeof(ASTNode));
        size_t capacity = 0;
        ASTNode *value;
        if (map == NULL) {
            parser_error(parser, "Out of memory while parsing map literal");
            ast_destroy(first);
            return NULL;
        }
        map->type = AST_MAP_LITERAL;
        map->line = line;
        map->column = col;
        parser_advance(parser);
        value = parser_parse_expression(parser);
        if (!parser_append_map_entry(parser, map, &capacity, first, value)) {
            ast_destroy(first);
            ast_destroy(value);
        } else if (parser_match(parser, TOKEN_COMMA)) {
            while (!parser_check(parser, TOKEN_RBRACE)
                   && !parser_is_at_end(parser)) {
                ASTNode *key = parser_parse_expression(parser);
                ASTNode *val;
                parser_consume(parser, TOKEN_COLON,
                               "Expected ':' after map literal key");
                val = parser_parse_expression(parser);
                if (!parser_append_map_entry(parser, map, &capacity,
                                             key, val)) {
                    ast_destroy(key);
                    ast_destroy(val);
                    break;
                }
                if (!parser_match(parser, TOKEN_COMMA))
                    break;
            }
        }
        parser_consume(parser, TOKEN_RBRACE, "Expected '}' after map literal");
        return map;
    }

    {
        ASTNode *set = calloc(1, sizeof(ASTNode));
        size_t capacity = 0;
        if (set == NULL) {
            parser_error(parser, "Out of memory while parsing set literal");
            ast_destroy(first);
            return NULL;
        }
        set->type = AST_SET_LITERAL;
        set->line = line;
        set->column = col;
        if (!parser_append_expr_node_with_capacity(parser,
                &set->data.set_literal.elements, &set->data.set_literal.count,
                &capacity, first)) {
            ast_destroy(first);
        } else if (parser_match(parser, TOKEN_COMMA)) {
            while (!parser_check(parser, TOKEN_RBRACE)
                   && !parser_is_at_end(parser)) {
                ASTNode *elem = parser_parse_expression(parser);
                if (!parser_append_expr_node_with_capacity(parser,
                        &set->data.set_literal.elements,
                        &set->data.set_literal.count,
                        &capacity, elem)) {
                    ast_destroy(elem);
                    break;
                }
                if (!parser_match(parser, TOKEN_COMMA))
                    break;
            }
        }
        parser_consume(parser, TOKEN_RBRACE,
                       "Expected '}' after set literal");
        return set;
    }
}
