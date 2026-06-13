#include "parser_internal.h"
#include "../common/match_variant_policy.h"

ASTNode* parse_pipe(Parser* parser);

/*
 * Operator-chain cap. Left-associative binary chains (a < b < c < ...) build a
 * left-deep AST iteratively; the later recursive tree walks (semantic check,
 * codegen, destroy) would overflow the native stack on a pathologically long
 * chain. parser->binary_op_count is reset per top-level expression and capped
 * here so the deep node is never built.
 */
#define PARSER_MAX_EXPR_OPERATORS 4096

static ASTNode*
parser_chain_binary(Parser* parser, ASTNode* left, Token op, ASTNode* right)
{
    if (parser->binary_op_count >= PARSER_MAX_EXPR_OPERATORS) {
        if (!parser->has_error) {
            parser_error(parser,
                "Expression has too many chained operators (limit is 4096); "
                "split it into smaller subexpressions");
        }
        return left;
    }
    parser->binary_op_count++;
    return ast_create_binary(left, op, right);
}
static int
parser_name_table_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const char *const *candidate = (const char *const *)entry;

    return strcmp(name, *candidate);
}

static bool
parser_name_in_sorted_table(const char *name, const char *const *names,
                            size_t count)
{
    if (name == NULL || names == NULL || count == 0)
        return false;

    return bsearch(name, names, count, sizeof(names[0]),
                   parser_name_table_compare) != NULL;
}

/* Non-destructive lookahead: with the current token at `<`, scan a copy of the
 * lexer for a balanced type-argument list closed by `>` and immediately
 * followed by `(`. Only type-shaped tokens may appear inside, so a comparison
 * expression like `a < b` is not mistaken for a generic call. */
bool
parser_generic_call_args_ahead(Parser *parser)
{
    Lexer scan;
    int depth = 1;

    if (parser == NULL || parser->lexer == NULL)
        return false;
    scan = *parser->lexer;
    for (;;) {
        Token t = lexer_next_token(&scan);
        switch (t.type) {
        case TOKEN_LESS:
            depth++;
            break;
        case TOKEN_GREATER:
            depth--;
            if (depth == 0) {
                Token after = lexer_next_token(&scan);
                return after.type == TOKEN_LPAREN;
            }
            break;
        case TOKEN_IDENTIFIER:
        case TOKEN_COMMA:
        case TOKEN_DOT:
        case TOKEN_SLOT:
        case TOKEN_AMP:
            break;  /* type-shaped; keep scanning */
        default:
            return false;
        }
        if (depth <= 0 || depth > 32)
            return false;
    }
}

static bool
parser_name_accepts_call_type_arguments(const char *name)
{
    static const char *const names[] = {
        "ClaimSecureSlot",
        "ClaimSlot",
    };
    return parser_name_in_sorted_table(name, names,
                                       sizeof(names) / sizeof(names[0]));
}

static bool
parser_name_is_builtin_like_identifier(const char *name)
{
    static const char *const names[] = {
        "Channel",
        "ClaimSecureSlot",
        "ClaimSlot",
        "Log",
        "Read",
        "Release",
        "Write",
    };
    return parser_name_in_sorted_table(name, names,
                                       sizeof(names) / sizeof(names[0]));
}

ASTNode* parser_parse_expression(Parser* parser) {
    ASTNode* result;
    if (parser->recursion_depth == 0)
        parser->binary_op_count = 0;
    if (!parser_enter_recursion(parser))
        return NULL;
    result = parser_parse_assignment(parser);
    parser_leave_recursion(parser);
    return result;
}

ASTNode* parser_parse_assignment(Parser* parser) {
    ASTNode* expr = parse_pipe(parser);

    if (parser_match(parser, TOKEN_ASSIGN)) {
        ASTNode* value = parser_parse_assignment(parser);
        ASTNode* assign = ast_create_assignment(expr, value);
        return assign;
    }

    if (parser_match(parser, TOKEN_SUBSCRIBE)) {
        ASTNode* handler = parser_parse_assignment(parser);
        return ast_create_event_subscribe(expr, handler);
    }

    if (parser_match(parser, TOKEN_UNSUBSCRIBE)) {
        ASTNode* handler = parser_parse_assignment(parser);
        return ast_create_event_unsubscribe(expr, handler);
    }

    return expr;
}

static ASTNode*
parse_coalescing(Parser *parser)
{
    ASTNode *expr = parse_logical_or(parser);

    while (parser_match(parser, TOKEN_COALESCE)) {
        Token op = parser->previous_token;
        ASTNode *fallback = parse_logical_or(parser);
        expr = parser_chain_binary(parser, expr, op, fallback);
    }

    return expr;
}

ASTNode* parse_pipe(Parser* parser) {
    ASTNode* expr = parse_coalescing(parser);

    while (parser_match(parser, TOKEN_PIPE_ARROW)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_coalescing(parser);
        if (right == NULL) {
            ast_destroy(expr);
            return NULL;
        }
        if (right->type == AST_CALL) {
            if (!parser_prepend_call_argument(parser, right, expr)) {
                ast_destroy(expr);
                ast_destroy(right);
                return NULL;
            }
            expr = right;
        } else if (right->type == AST_IDENTIFIER) {
            ASTNode *call = ast_create_call(right);
            call->data.call.arguments = calloc(1, sizeof(ASTNode *));
            if (call->data.call.arguments == NULL) {
                parser_error(parser, "Out of memory while lowering pipe expression");
                ast_destroy(expr);
                ast_destroy(call);
                return NULL;
            }
            call->data.call.arguments[0] = expr;
            call->data.call.arg_count = 1;
            call->data.call.arg_capacity = 1;
            expr = call;
        } else {
            expr = parser_chain_binary(parser, expr, op, right);
        }
    }

    return expr;
}

ASTNode* parse_logical_or(Parser* parser) {
    ASTNode* expr = parse_logical_and(parser);

    while (parser_match(parser, TOKEN_OR)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_logical_and(parser);
        expr = parser_chain_binary(parser, expr, op, right);
    }

    return expr;
}

ASTNode* parse_logical_and(Parser* parser) {
    ASTNode* expr = parse_equality(parser);

    while (parser_match(parser, TOKEN_AND)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_equality(parser);
        expr = parser_chain_binary(parser, expr, op, right);
    }

    return expr;
}

ASTNode* parse_equality(Parser* parser) {
    ASTNode* expr = parse_comparison(parser);

    while (parser_match(parser, TOKEN_EQUAL) ||
           parser_match(parser, TOKEN_NOT_EQUAL)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_comparison(parser);
        expr = parser_chain_binary(parser, expr, op, right);
    }

    return expr;
}

ASTNode* parse_comparison(Parser* parser) {
    ASTNode* expr = parse_addition(parser);

    while (parser_match(parser, TOKEN_LESS) ||
           parser_match(parser, TOKEN_LESS_EQUAL) ||
           parser_match(parser, TOKEN_GREATER) ||
           parser_match(parser, TOKEN_GREATER_EQUAL)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_addition(parser);
        expr = parser_chain_binary(parser, expr, op, right);
    }

    return expr;
}

ASTNode* parse_addition(Parser* parser) {
    ASTNode* expr = parse_multiplication(parser);

    while (parser_match(parser, TOKEN_PLUS) ||
           parser_match(parser, TOKEN_MINUS)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_multiplication(parser);
        expr = parser_chain_binary(parser, expr, op, right);
    }

    return expr;
}

/* `expr as Type` builds an AST_CAST node carrying the target type name.
 * Numeric (Int<->Float) and identity casts are lowered by both backends;
 * other targets are rejected during semantic analysis. */
static ASTNode*
parser_build_cast(Parser* parser, ASTNode* expr, ASTNode* type_node)
{
    const char* type_name = type_node != NULL ? ast_type_name(type_node) : NULL;
    ASTNode* cast;

    if (type_name == NULL) {
        parser_error(parser, "Expected a type name after 'as'");
        ast_destroy(type_node);
        return expr;
    }
    /* `expr as String` reuses the parity-tested ToString builtin; numeric
     * Int/Float casts carry an AST_CAST node lowered by both backends. */
    if (strcmp(type_name, "String") == 0) {
        ASTNode* callee = ast_create_identifier("ToString");
        ASTNode* call = ast_create_call(callee);
        ast_destroy(type_node);
        if (call == NULL || callee == NULL
            || !parser_append_call_argument(parser, call, NULL, expr)) {
            parser_error(parser, "Out of memory while lowering cast to String");
            return expr;
        }
        return call;
    }
    cast = ast_create_cast(expr, type_name);
    ast_destroy(type_node);
    if (cast == NULL) {
        parser_error(parser, "Out of memory while parsing cast expression");
        return expr;
    }
    return cast;
}

ASTNode* parse_cast(Parser* parser) {
    ASTNode* expr = parse_unary(parser);

    while (!parser->no_cast && parser_match(parser, TOKEN_AS)) {
        ASTNode* type_node = parse_type(parser);
        expr = parser_build_cast(parser, expr, type_node);
    }

    return expr;
}

ASTNode* parse_multiplication(Parser* parser) {
    ASTNode* expr = parse_cast(parser);

    while (parser_match(parser, TOKEN_STAR) ||
           parser_match(parser, TOKEN_SLASH) ||
           parser_match(parser, TOKEN_PERCENT)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_cast(parser);
        expr = parser_chain_binary(parser, expr, op, right);
    }

    return expr;
}

ASTNode* parse_unary(Parser* parser) {
    if (parser_match(parser, TOKEN_NOT) ||
        parser_match(parser, TOKEN_MINUS) ||
        parser_match(parser, TOKEN_AMP)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_unary(parser);
        return ast_create_unary(op, right);
    }

    return parser_parse_call(parser);
}

static bool
parser_grow_map_entry_arrays(Parser* parser, ASTNode* map, size_t* capacity)
{
    size_t next = (*capacity == 0) ? 4 : *capacity * 2;
    ASTNode** grown_keys;
    ASTNode** grown_values;

    if (next < *capacity || next > SIZE_MAX / sizeof(ASTNode*)) {
        parser_error(parser, "Map literal has too many entries");
        return false;
    }
    grown_keys = realloc(map->data.map_literal.keys, next * sizeof(ASTNode*));
    if (grown_keys == NULL) {
        parser_error(parser, "Out of memory while parsing map literal");
        return false;
    }
    map->data.map_literal.keys = grown_keys;
    grown_values = realloc(map->data.map_literal.values, next * sizeof(ASTNode*));
    if (grown_values == NULL) {
        parser_error(parser, "Out of memory while parsing map literal");
        return false;
    }
    map->data.map_literal.values = grown_values;
    *capacity = next;
    return true;
}

static bool
parser_append_map_entry(Parser* parser, ASTNode* map, size_t* capacity,
                        ASTNode* key, ASTNode* value)
{
    if (map->data.map_literal.count == *capacity
        && !parser_grow_map_entry_arrays(parser, map, capacity))
    {
        return false;
    }
    map->data.map_literal.keys[map->data.map_literal.count] = key;
    map->data.map_literal.values[map->data.map_literal.count] = value;
    map->data.map_literal.count++;
    return true;
}

/* Bare brace map literal `{ key: value, ... }` in expression position.
 * Object initializers `Type { field: value }` are handled in the postfix
 * path, so an opening brace reaching primary is unambiguously a map. */
static ASTNode*
parse_map_literal_expression(Parser* parser)
{
    ASTNode* map = calloc(1, sizeof(ASTNode));
    size_t capacity = 0;

    if (map == NULL) {
        parser_error(parser, "Out of memory while parsing map literal");
        return NULL;
    }
    map->type = AST_MAP_LITERAL;
    map->line = parser->current_token.line;
    map->column = parser->current_token.column;
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' to begin map literal");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode* key = parser_parse_expression(parser);
        ASTNode* value;
        parser_consume(parser, TOKEN_COLON, "Expected ':' after map literal key");
        value = parser_parse_expression(parser);
        if (!parser_append_map_entry(parser, map, &capacity, key, value)) {
            ast_destroy(key);
            ast_destroy(value);
            break;
        }
        if (!parser_match(parser, TOKEN_COMMA))
            break;
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after map literal");
    return map;
}

ASTNode* parser_parse_primary(Parser* parser) {
    /* Leading-dot enum/union variant shorthand:
     * .Some(v), .None, .Ok(x) parse as bare variant identifier/call.
     * This keeps docs-style match/return syntax working without forcing
     * the enum name at each use site. */
    if (parser_check(parser, TOKEN_DOT)
        && parser->current_token.length == 1
        && strcmp(parser->current_token.text, ".") == 0) {
        parser_advance(parser);
        Token variant = consume_member_name_token(parser,
            "Expected variant name after '.'");
        if (pgy_match_variant_lookup(variant.text)
                == PGY_MATCH_VARIANT_NONE_CTOR) {
            ASTNode *callee = ast_create_identifier(variant.text);
            return ast_create_call(callee);
        }
        return ast_create_identifier(variant.text);
    }

    if (parser_match(parser, TOKEN_AWAIT)) {
        return parser_parse_await_expression(parser);
    }

    if (parser_match(parser, TOKEN_SPAWN)) {
        return parser_parse_spawn_expression(parser);
    }

    /* `async { body }` block expression: an inline asynchronous task. The
     * schedule metadata is erased; the body is held as a concurrency task so
     * the node is usable in expression position (e.g. `let t = async { ... }`).
     * Function-form `async func` is dispatched earlier at declaration level. */
    if (parser_check(parser, TOKEN_ASYNC)
        && parser_peek_next(parser).type == TOKEN_LBRACE) {
        bool saved_async = parser->in_async_context;
        ASTNode* task = ast_create_parallel_block();
        parser_advance(parser);
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' after 'async'");
        parser->in_async_context = true;
        ASTNode* body = parser_parse_block(parser);
        parser->in_async_context = saved_async;
        if (body != NULL)
            ast_add_parallel_task(task, body);
        return task;
    }

    if (parser_match(parser, TOKEN_PARALLEL)) {
        return parser_parse_parallel_block(parser);
    }

    if (parser_check(parser, TOKEN_CHANNEL_OP)) {
        return parser_parse_channel_expression(parser);
    }

    if (parser_match(parser, TOKEN_TRUE)) {
        return ast_create_boolean(true);
    }

    if (parser_match(parser, TOKEN_FALSE)) {
        return ast_create_boolean(false);
    }

    if (parser_match(parser, TOKEN_LBRACKET)) {
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = parser->previous_token.line;
        arr->data.array_literal.elements = NULL;
        arr->data.array_literal.count = 0;
        size_t capacity = 0;

        if (!parser_check(parser, TOKEN_RBRACKET)) {
            do {
                ASTNode *elem = parser_parse_expression(parser);
                if (!parser_append_expr_node_with_capacity(parser,
                        &arr->data.array_literal.elements,
                        &arr->data.array_literal.count,
                        &capacity,
                        elem)) {
                    ast_destroy(elem);
                    break;
                }
            } while (parser_match(parser, TOKEN_COMMA));
        }
        parser_consume(parser, TOKEN_RBRACKET, "Expected ']' after array literal");
        return arr;
    }

    if (parser_check(parser, TOKEN_LBRACE)) {
        return parse_map_literal_expression(parser);
    }

    if (parser_match(parser, TOKEN_ELLIPSIS)) {
        parser_error(parser,
            "Spread/rest syntax '...' is reserved but not implemented.\n"
            "Reason: spread/rest needs call ABI, ownership, and collection lowering policy.\n"
            "Fix: pass values explicitly.");
        return NULL;
    }

    if (parser_match(parser, TOKEN_NUMBER)) {
        Token num = parser->previous_token;
        ASTNode* number = ast_create_number(num.text);
        /* Adjacent time-unit suffix (`10000ms`, `5s`): the unit is erased at the
         * parse level and the numeric value carries through. Adjacency (no
         * space) distinguishes a duration from `5 s` with a separate name. */
        if (parser_check(parser, TOKEN_IDENTIFIER)
            && parser->current_token.text != NULL
            && parser->current_token.column == num.column + (uint32_t)num.length
            && (strcmp(parser->current_token.text, "ms") == 0
                || strcmp(parser->current_token.text, "s") == 0
                || strcmp(parser->current_token.text, "us") == 0
                || strcmp(parser->current_token.text, "ns") == 0
                || strcmp(parser->current_token.text, "min") == 0))
            parser_advance(parser);
        return number;
    }

    if (parser_match(parser, TOKEN_STRING) || parser_match(parser, TOKEN_MULTILINE_STRING)) {
        const char *raw = parser->previous_token.text;
        if (raw != NULL && !is_multiline_string_token(raw)
            && strstr(raw, "${") != NULL) {
            return parse_interpolation_body(raw, false);
        }
        return ast_create_string(raw);
    }

    if (parser_match(parser, TOKEN_INTERPOLATED_STRING)) {
        const char *raw = parser->previous_token.text;
        return parse_interpolation_body(raw, true);
    }

    if (parser_match_expr_name_token(parser)) {
        Token name_token = parser->previous_token;
        char* name = pergyra_strdup(parser->previous_token.text);

        if (parser_check(parser, TOKEN_LESS)
            && (parser_name_accepts_call_type_arguments(name)
                || parser_generic_call_args_ahead(parser))) {
            /* Parse and stash the `<T>` args so finish_call can attach
             * them to the AST_CALL (needed for destructuring patterns
             * where the LHS has no type annotation to recover T from). */
            parser->pending_call_generic_args = parse_type_arguments(parser);
        }

        if (parser_name_is_builtin_like_identifier(name)) {
            ASTNode* ident = ast_create_identifier(name);
            if (ident != NULL) {
                ident->line = name_token.line;
                ident->column = name_token.column;
            }
            free(name);
            return ident;
        }

        ASTNode* ident = ast_create_identifier(name);
        if (ident != NULL) {
            ident->line = name_token.line;
            ident->column = name_token.column;
        }
        free(name);
        if (parser_check(parser, TOKEN_CHANNEL_OP)) {
            Token op = parser_advance(parser);
            ASTNode* value = parser_parse_expression(parser);
            ASTNode *send = ast_create_channel_send(ident, value);
            if (send != NULL) {
                send->line = op.line;
                send->column = op.column;
            }
            return send;
        }

        return ident;
    }

    if (parser_is_lambda_start(parser)) {
        return parse_lambda_expression(parser);
    }

    if (parser_check(parser, TOKEN_PATTERN_OR)) {
        return parse_pipe_lambda_expression(parser);
    }

    if (parser_match(parser, TOKEN_LPAREN)) {
        if (parser_check(parser, TOKEN_RPAREN)) {
            /* Unit value `()` -- a zero-element tuple literal. */
            ASTNode *unit = calloc(1, sizeof(ASTNode));
            if (unit != NULL) {
                unit->type = AST_TUPLE_LITERAL;
                unit->line = parser->previous_token.line;
                unit->data.tuple_literal.elements = NULL;
                unit->data.tuple_literal.count = 0;
            }
            parser_consume(parser, TOKEN_RPAREN, "Expected ')' for unit value");
            return unit;
        }
        ASTNode* first = parser_parse_expression(parser);
        if (parser_check(parser, TOKEN_COMMA)) {
            ASTNode *tuple = calloc(1, sizeof(ASTNode));
            tuple->type = AST_TUPLE_LITERAL;
            tuple->line = parser->previous_token.line;
            size_t cap = 4;
            tuple->data.tuple_literal.elements = calloc(cap, sizeof(ASTNode *));
            tuple->data.tuple_literal.count = 0;
            if (!parser_append_expr_node_with_capacity(parser,
                    &tuple->data.tuple_literal.elements,
                    &tuple->data.tuple_literal.count,
                    &cap,
                    first)) {
                ast_destroy(first);
                return tuple;
            }
            while (parser_match(parser, TOKEN_COMMA)) {
                if (parser_check(parser, TOKEN_RPAREN))
                    break;
                ASTNode *elem = parser_parse_expression(parser);
                if (!parser_append_expr_node_with_capacity(parser,
                        &tuple->data.tuple_literal.elements,
                        &tuple->data.tuple_literal.count,
                        &cap,
                        elem)) {
                    ast_destroy(elem);
                    break;
                }
            }
            parser_consume(parser, TOKEN_RPAREN, "Expected ')' after tuple literal");
            return tuple;
        }
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
        return first;
    }

    parser_error(parser, "Unexpected token in expression");
    return NULL;
}
