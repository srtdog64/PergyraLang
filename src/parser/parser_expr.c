#include "parser_internal.h"

/* Forward declarations */
ASTNode* parse_pipe(Parser* parser);

static int
parser_name_table_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const char *const *candidate = (const char *const *)entry;

    return strcmp(name, *candidate);
}

static bool
parser_name_in_sorted_table(const char *name,
                            const char *const *names,
                            size_t count)
{
    if (name == NULL || names == NULL || count == 0)
        return false;

    return bsearch(name, names, count, sizeof(names[0]),
                   parser_name_table_compare) != NULL;
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

static bool
parser_prepend_call_argument(Parser *parser, ASTNode *call, ASTNode *argument)
{
    ASTNode **new_args;
    size_t old_count;

    if (call == NULL || call->type != AST_CALL)
        return false;

    old_count = call->data.call.arg_count;
    if (old_count == call->data.call.arg_capacity) {
        size_t next_capacity = call->data.call.arg_capacity == 0 ? 4 : call->data.call.arg_capacity * 2;
        new_args = realloc(call->data.call.arguments,
            next_capacity * sizeof(ASTNode *));
        if (new_args == NULL) {
            parser_error(parser, "Out of memory while prepending pipe argument");
            return false;
        }
        call->data.call.arguments = new_args;
        call->data.call.arg_capacity = next_capacity;
    }

    memmove(call->data.call.arguments + 1, call->data.call.arguments, old_count * sizeof(ASTNode *));
    call->data.call.arguments[0] = argument;
    call->data.call.arg_count = old_count + 1;
    return true;
}

/* Expression parsing entry point. */
ASTNode* parser_parse_expression(Parser* parser) {
    return parser_parse_assignment(parser);
}

/* Assignment and event-subscription expressions. */
ASTNode* parser_parse_assignment(Parser* parser) {
    ASTNode* expr = parse_pipe(parser);

    if (parser_match(parser, TOKEN_ASSIGN)) {
        ASTNode* value = parser_parse_assignment(parser);
        ASTNode* assign = ast_create_assignment(expr, value);
        return assign;
    }

    /* Event subscription (+=) and unsubscription (-=) */
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

/* Pipe operator: a |> f becomes f(a); a |> f(b) becomes f(a, b). */
ASTNode* parse_pipe(Parser* parser) {
    ASTNode* expr = parse_logical_or(parser);

    while (parser_match(parser, TOKEN_PIPE_ARROW)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_logical_or(parser);
        /* If right is already a call f(b), transform to f(a, b). */
        if (right->type == AST_CALL) {
            if (!parser_prepend_call_argument(parser, right, expr)) {
                ast_destroy(expr);
                ast_destroy(right);
                return NULL;
            }
            expr = right;
        } else if (right->type == AST_IDENTIFIER) {
            /* a |> f becomes f(a). */
            ASTNode *call = ast_create_call(right);
            call->data.call.arguments = calloc(1, sizeof(ASTNode *));
            call->data.call.arguments[0] = expr;
            call->data.call.arg_count = 1;
            expr = call;
        } else {
            expr = ast_create_binary(expr, op, right);
        }
    }

    return expr;
}

/* Logical OR. */
ASTNode* parse_logical_or(Parser* parser) {
    ASTNode* expr = parse_logical_and(parser);

    while (parser_match(parser, TOKEN_OR)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_logical_and(parser);
        expr = ast_create_binary(expr, op, right);
    }

    return expr;
}

/* Logical AND. */
ASTNode* parse_logical_and(Parser* parser) {
    ASTNode* expr = parse_equality(parser);

    while (parser_match(parser, TOKEN_AND)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_equality(parser);
        expr = ast_create_binary(expr, op, right);
    }

    return expr;
}

/* Equality comparison. */
ASTNode* parse_equality(Parser* parser) {
    ASTNode* expr = parse_comparison(parser);

    while (parser_match(parser, TOKEN_EQUAL) ||
           parser_match(parser, TOKEN_NOT_EQUAL)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_comparison(parser);
        expr = ast_create_binary(expr, op, right);
    }

    return expr;
}

/* Relational comparison. */
ASTNode* parse_comparison(Parser* parser) {
    ASTNode* expr = parse_addition(parser);

    while (parser_match(parser, TOKEN_LESS) ||
           parser_match(parser, TOKEN_LESS_EQUAL) ||
           parser_match(parser, TOKEN_GREATER) ||
           parser_match(parser, TOKEN_GREATER_EQUAL)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_addition(parser);
        expr = ast_create_binary(expr, op, right);
    }

    return expr;
}

/* Addition and subtraction. */
ASTNode* parse_addition(Parser* parser) {
    ASTNode* expr = parse_multiplication(parser);

    while (parser_match(parser, TOKEN_PLUS) ||
           parser_match(parser, TOKEN_MINUS)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_multiplication(parser);
        expr = ast_create_binary(expr, op, right);
    }

    return expr;
}

/* Multiplication, division, and remainder. */
ASTNode* parse_multiplication(Parser* parser) {
    ASTNode* expr = parse_unary(parser);

    while (parser_match(parser, TOKEN_STAR) ||
           parser_match(parser, TOKEN_SLASH) ||
           parser_match(parser, TOKEN_PERCENT)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_unary(parser);
        expr = ast_create_binary(expr, op, right);
    }

    return expr;
}

/* Unary prefix operators. */
ASTNode* parse_unary(Parser* parser) {
    if (parser_match(parser, TOKEN_NOT) ||
        parser_match(parser, TOKEN_MINUS)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_unary(parser);
        return ast_create_unary(op, right);
    }

    return parser_parse_call(parser);
}

/* Function calls, member access, indexing, and postfix try. */
ASTNode* parser_parse_call(Parser* parser) {
    ASTNode* expr = parser_parse_primary(parser);

    while (true) {
        if (parser_match(parser, TOKEN_LPAREN)) {
            expr = finish_call(parser, expr);
        } else if (parser_check(parser, TOKEN_DOT) &&
                   parser->current_token.length == 1 &&
                   strcmp(parser->current_token.text, ".") == 0) {
            parser_advance(parser);
            Token name = consume_member_name_token(parser, "Expected property name after '.'");
            expr = ast_create_member_access(expr, name.text);
        } else if (parser_match(parser, TOKEN_LBRACKET)) {
            ASTNode* index = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_RBRACKET, "Expected ']' after array index");
            expr = ast_create_array_access(expr, index);
        } else if (parser_match(parser, TOKEN_QUESTION)) {
            /* Postfix ? is try/propagate: early-return on error. */
            ASTNode *try_node = calloc(1, sizeof(ASTNode));
            if (try_node != NULL) {
                try_node->type = AST_UNARY;
                try_node->line = parser->previous_token.line;
                try_node->data.unary.op = parser->previous_token;
                try_node->data.unary.operand = expr;
            }
            expr = try_node;
        } else {
            break;
        }
    }

    return expr;
}

/* Finish parsing a function call after `(` has been consumed. */
ASTNode* finish_call(Parser* parser, ASTNode* callee) {
    ASTNode* call = ast_create_call(callee);
    if (call != NULL && callee != NULL) {
        call->line = callee->line;
        call->column = callee->column;
    }

    /* Adopt generic args parsed by parser_parse_primary (e.g.
     * ClaimSecureSlot<Int> left them on the parser). Consumed here so
     * subsequent calls don't inherit them. */
    if (parser->pending_call_generic_args != NULL) {
        call->data.call.generic_args = parser->pending_call_generic_args;
        parser->pending_call_generic_args = NULL;
    }

    /* Parse arguments. */
    if (!parser_check(parser, TOKEN_RPAREN)) {
        do {
            ASTNode* arg = parser_parse_expression(parser);
            ast_add_argument(call, arg);
        } while (parser_match(parser, TOKEN_COMMA));
    }

    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after arguments");

    return call;
}

/* Primary expressions. */
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
        if (strcmp(variant.text, "None") == 0) {
            ASTNode *callee = ast_create_identifier(variant.text);
            return ast_create_call(callee);
        }
        return ast_create_identifier(variant.text);
    }

    /* await expression. */
    if (parser_match(parser, TOKEN_AWAIT)) {
        return parser_parse_await_expression(parser);
    }

    /* spawn expression. */
    if (parser_match(parser, TOKEN_SPAWN)) {
        return parser_parse_spawn_expression(parser);
    }

    /* Parallel block can also appear in expression position. */
    if (parser_match(parser, TOKEN_PARALLEL)) {
        return parser_parse_parallel_block(parser);
    }

    /* Channel receive: <-channel. */
    if (parser_check(parser, TOKEN_CHANNEL_OP)) {
        return parser_parse_channel_expression(parser);
    }

    /* true. */
    if (parser_match(parser, TOKEN_TRUE)) {
        return ast_create_boolean(true);
    }

    /* false. */
    if (parser_match(parser, TOKEN_FALSE)) {
        return ast_create_boolean(false);
    }

    /* Array literal: [1, 2, 3]. */
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

    /* Number literal. */
    if (parser_match(parser, TOKEN_NUMBER)) {
        return ast_create_number(parser->previous_token.text);
    }

    /* String literal. */
    if (parser_match(parser, TOKEN_STRING) || parser_match(parser, TOKEN_MULTILINE_STRING)) {
        const char *raw = parser->previous_token.text;
        /* Check for string interpolation: "...${expr}..."; skip for multiline string
         * to keep it as raw banner/text payload. */
        if (raw != NULL && !is_multiline_string_token(raw)
            && strstr(raw, "${") != NULL) {
            return parse_interpolation_body(raw, false);
        }
        return ast_create_string(raw);
    }

    /* Interpolated string literal: f"Hello {name}". */
    if (parser_match(parser, TOKEN_INTERPOLATED_STRING)) {
        const char *raw = parser->previous_token.text;
        return parse_interpolation_body(raw, true);
    }

    /* Identifier-like expression names and builtin-like calls. */
    if (parser_match_expr_name_token(parser)) {
        Token name_token = parser->previous_token;
        char* name = pergyra_strdup(parser->previous_token.text);

        if (parser_name_accepts_call_type_arguments(name)
            && parser_check(parser, TOKEN_LESS)) {
            /* Parse and stash the `<T>` args so finish_call can attach
             * them to the AST_CALL (needed for destructuring patterns
             * where the LHS has no type annotation to recover T from). */
            parser->pending_call_generic_args = parse_type_arguments(parser);
        }

        /* Builtin-like names are represented as identifiers first. */
        if (parser_name_is_builtin_like_identifier(name)) {
            ASTNode* ident = ast_create_identifier(name);
            if (ident != NULL) {
                ident->line = name_token.line;
                ident->column = name_token.column;
            }
            free(name);
            return ident;
        }

        /* Channel send: channel <- value. */
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

    /* Lambda expression: (x: Int) => { ... }. */
    if (parser_is_lambda_start(parser)) {
        return parse_lambda_expression(parser);
    }

    /* Parenthesized expression or tuple literal. */
    if (parser_match(parser, TOKEN_LPAREN)) {
        ASTNode* first = parser_parse_expression(parser);
        if (parser_check(parser, TOKEN_COMMA)) {
            /* Tuple literal: (a, b, ...) */
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
                    break; /* allow trailing comma */
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
