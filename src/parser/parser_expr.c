#include "parser_internal.h"

/* Forward declarations */
ASTNode* parse_pipe(Parser* parser);

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

// 표현식 파싱 (우선순위 기반)
ASTNode* parser_parse_expression(Parser* parser) {
    return parser_parse_assignment(parser);
}

// 할당 표현식
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

// 파이프 연산자 |> (left-to-right data flow)
ASTNode* parse_pipe(Parser* parser) {
    ASTNode* expr = parse_logical_or(parser);

    while (parser_match(parser, TOKEN_PIPE_ARROW)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_logical_or(parser);
        /* a |> f → f(a).  If right is a call f(b), transform to f(a, b). */
        if (right->type == AST_CALL) {
            if (!parser_prepend_call_argument(parser, right, expr)) {
                /* realloc 실패: expr와 right를 해제하고 NULL 반환 */
                ast_destroy(expr);
                ast_destroy(right);
                return NULL;
            }
            expr = right;
        } else if (right->type == AST_IDENTIFIER) {
            /* a |> f → f(a) */
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

// 논리 OR
ASTNode* parse_logical_or(Parser* parser) {
    ASTNode* expr = parse_logical_and(parser);

    while (parser_match(parser, TOKEN_OR)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_logical_and(parser);
        expr = ast_create_binary(expr, op, right);
    }

    return expr;
}

// 논리 AND
ASTNode* parse_logical_and(Parser* parser) {
    ASTNode* expr = parse_equality(parser);

    while (parser_match(parser, TOKEN_AND)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_equality(parser);
        expr = ast_create_binary(expr, op, right);
    }

    return expr;
}

// 동등성 비교
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

// 비교 연산
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

// 덧셈/뺄셈
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

// 곱셈/나눗셈
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

// 단항 연산
ASTNode* parse_unary(Parser* parser) {
    if (parser_match(parser, TOKEN_NOT) ||
        parser_match(parser, TOKEN_MINUS)) {
        Token op = parser->previous_token;
        ASTNode* right = parse_unary(parser);
        return ast_create_unary(op, right);
    }

    return parser_parse_call(parser);
}

// 함수 호출 / 멤버 접근
ASTNode* parser_parse_call(Parser* parser) {
    ASTNode* expr = parser_parse_primary(parser);

    while (true) {
        if (parser_match(parser, TOKEN_LPAREN)) {
            // 함수 호출
            expr = finish_call(parser, expr);
        } else if (parser_check(parser, TOKEN_DOT) &&
                   parser->current_token.length == 1 &&
                   strcmp(parser->current_token.text, ".") == 0) {
            parser_advance(parser);
            // 멤버 접근
            Token name = consume_member_name_token(parser, "Expected property name after '.'");
            expr = ast_create_member_access(expr, name.text);
        } else if (parser_match(parser, TOKEN_LBRACKET)) {
            // 배열 인덱싱
            ASTNode* index = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_RBRACKET, "Expected ']' after array index");
            expr = ast_create_array_access(expr, index);
        } else if (parser_match(parser, TOKEN_QUESTION)) {
            /* Postfix ? — try/propagate: expr? → early return on error */
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

// 함수 호출 완성
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

    // 인자 파싱
    if (!parser_check(parser, TOKEN_RPAREN)) {
        do {
            ASTNode* arg = parser_parse_expression(parser);
            ast_add_argument(call, arg);
        } while (parser_match(parser, TOKEN_COMMA));
    }

    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after arguments");

    return call;
}

// 기본 표현식
ASTNode* parser_parse_primary(Parser* parser) {
    /* Leading-dot enum/union variant shorthand:
     * .Some(v), .None, .Ok(x) → parse as bare variant identifier/call.
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

    // await 표현식
    if (parser_match(parser, TOKEN_AWAIT)) {
        return parser_parse_await_expression(parser);
    }

    // spawn 표현식
    if (parser_match(parser, TOKEN_SPAWN)) {
        return parser_parse_spawn_expression(parser);
    }

    // parallel 블록은 초기화 식에서도 쓰인다.
    if (parser_match(parser, TOKEN_PARALLEL)) {
        return parser_parse_parallel_block(parser);
    }

    // 채널 수신: <-channel
    if (parser_check(parser, TOKEN_CHANNEL_OP)) {
        return parser_parse_channel_expression(parser);
    }

    // true
    if (parser_match(parser, TOKEN_TRUE)) {
        return ast_create_boolean(true);
    }

    // false
    if (parser_match(parser, TOKEN_FALSE)) {
        return ast_create_boolean(false);
    }

    // 배열 리터럴 [1, 2, 3]
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

    // 숫자
    if (parser_match(parser, TOKEN_NUMBER)) {
        return ast_create_number(parser->previous_token.text);
    }

    // 문자열
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

    // 보간 문자열: f"Hello {name}"
    if (parser_match(parser, TOKEN_INTERPOLATED_STRING)) {
        const char *raw = parser->previous_token.text;
        return parse_interpolation_body(raw, true);
    }

    // 식별자 또는 슬롯 연산
    if (parser_match_expr_name_token(parser)) {
        Token name_token = parser->previous_token;
        char* name = pergyra_strdup(parser->previous_token.text);

        if ((strcmp(name, "ClaimSlot") == 0 ||
             strcmp(name, "ClaimSecureSlot") == 0) &&
            parser_check(parser, TOKEN_LESS)) {
            /* Parse and stash the `<T>` args so finish_call can attach
             * them to the AST_CALL (needed for destructuring patterns
             * where the LHS has no type annotation to recover T from). */
            parser->pending_call_generic_args = parse_type_arguments(parser);
        }

        // 내장 함수 처리
        if (strcmp(name, "ClaimSlot") == 0 ||
            strcmp(name, "ClaimSecureSlot") == 0 ||
            strcmp(name, "Write") == 0 ||
            strcmp(name, "Read") == 0 ||
            strcmp(name, "Release") == 0 ||
            strcmp(name, "Log") == 0 ||
            strcmp(name, "Channel") == 0) {
            ASTNode* ident = ast_create_identifier(name);
            if (ident != NULL) {
                ident->line = name_token.line;
                ident->column = name_token.column;
            }
            free(name);
            return ident;
        }

        // 채널 송신 체크: channel <- value
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

    // 람다식: (x: Int) => { ... }
    if (parser_is_lambda_start(parser)) {
        return parse_lambda_expression(parser);
    }

    // 괄호 표현식 / 튜플 리터럴
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
