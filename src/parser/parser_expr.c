#include "parser_internal.h"

/* Forward declarations */
ASTNode* parse_pipe(Parser* parser);
ASTNode* parse_logical_or(Parser* parser);

static void
free_lookahead_token(Token *token, bool owned)
{
    if (owned && token != NULL) {
        free(token->text);
        token->text = NULL;
    }
}

static bool
parser_is_lambda_start(Parser *parser)
{
    Lexer lookahead;
    Token token;
    bool token_owned = false;
    int paren_depth = 0;

    if (!parser_check(parser, TOKEN_LPAREN))
        return false;

    lookahead = *parser->lexer;
    token = parser->current_token;

    while (true) {
        if (token.type == TOKEN_LPAREN) {
            paren_depth++;
        } else if (token.type == TOKEN_RPAREN) {
            paren_depth--;
            if (paren_depth == 0) {
                Token next = lexer_next_token(&lookahead);
                bool is_lambda = next.type == TOKEN_LAMBDA;
                free(next.text);
                free_lookahead_token(&token, token_owned);
                return is_lambda;
            }
        } else if (token.type == TOKEN_EOF) {
            free_lookahead_token(&token, token_owned);
            return false;
        }

        free_lookahead_token(&token, token_owned);
        token = lexer_next_token(&lookahead);
        token_owned = true;
    }
}

/* Forward declaration */
static ASTNode *parse_interpolated_expression_fragment(const char *expr_src);

/* Parse interpolation body: handles both f"{x}" and "${x}" syntax */
static ASTNode *
parse_interpolation_body(const char *raw, bool is_fstring)
{
    size_t len = strlen(raw);
    const char *s = raw + 1;              /* skip opening " */
    const char *end = raw + len - 1;      /* before closing " */
    ASTNode *result = NULL;

    while (s < end) {
        const char *interp = NULL;
        int delim_len = 0;

        if (is_fstring) {
            /* f"..." uses {expr} */
            interp = strchr(s, '{');
            /* Skip escaped { */
            while (interp != NULL && interp > s && *(interp - 1) == '\\') {
                interp = strchr(interp + 1, '{');
            }
            delim_len = 1;
        } else {
            /* legacy "${expr}" */
            interp = strstr(s, "${");
            delim_len = 2;
        }

        if (interp == NULL || interp >= end) {
            /* Remaining literal part */
            if (s < end) {
                size_t part_len = end - s;
                char *part = malloc(part_len + 3);
                part[0] = '"';
                memcpy(part + 1, s, part_len);
                part[part_len + 1] = '"';
                part[part_len + 2] = '\0';
                ASTNode *str_node = ast_create_string(part);
                free(part);
                if (result == NULL) result = str_node;
                else {
                    Token plus_tok = { .type = TOKEN_PLUS, .text = "+", .length = 1 };
                    result = ast_create_binary(result, plus_tok, str_node);
                }
            }
            break;
        }

        /* Literal part before interpolation */
        if (interp > s) {
            size_t part_len = interp - s;
            char *part = malloc(part_len + 3);
            part[0] = '"';
            memcpy(part + 1, s, part_len);
            part[part_len + 1] = '"';
            part[part_len + 2] = '\0';
            ASTNode *str_node = ast_create_string(part);
            free(part);
            if (result == NULL) result = str_node;
            else {
                Token plus_tok = { .type = TOKEN_PLUS, .text = "+", .length = 1 };
                result = ast_create_binary(result, plus_tok, str_node);
            }
        }

        /* Extract expression between { and } or ${ and } */
        const char *expr_start = interp + delim_len;
        const char *expr_end = strchr(expr_start, '}');
        if (expr_end == NULL || expr_end >= end) break;

        size_t expr_len = expr_end - expr_start;
        char *expr_str = malloc(expr_len + 1);
        memcpy(expr_str, expr_start, expr_len);
        expr_str[expr_len] = '\0';

        ASTNode *inner_expr =
            parse_interpolated_expression_fragment(expr_str);
        ASTNode *to_string = ast_create_call(
            ast_create_identifier("ToString"));
        ast_add_argument(to_string, inner_expr);
        free(expr_str);

        if (result == NULL) result = to_string;
        else {
            Token plus_tok = { .type = TOKEN_PLUS, .text = "+", .length = 1 };
            result = ast_create_binary(result, plus_tok, to_string);
        }

        s = expr_end + 1;
    }

    return result != NULL ? result : ast_create_string(raw);
}

static ASTNode *
parse_interpolated_expression_fragment(const char *expr_src)
{
    Lexer *lexer;
    Parser *parser;
    ASTNode *expr;

    if (expr_src == NULL)
        return ast_create_string("\"\"");

    lexer = lexer_create(expr_src);
    if (lexer == NULL)
        return ast_create_identifier(expr_src);

    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return ast_create_identifier(expr_src);
    }

    expr = parser_parse_expression(parser);
    parser_destroy(parser);
    lexer_destroy(lexer);

    if (expr == NULL)
        return ast_create_identifier(expr_src);
    return expr;
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
            /* Insert expr as first argument */
            size_t old_count = right->data.call.arg_count;
            ASTNode **new_args = realloc(right->data.call.arguments,
                (old_count + 1) * sizeof(ASTNode *));
            if (new_args == NULL) {
                /* realloc 실패: expr와 right를 해제하고 NULL 반환 */
                ast_destroy(expr);
                ast_destroy(right);
                return NULL;
            }
            memmove(new_args + 1, new_args, old_count * sizeof(ASTNode *));
            new_args[0] = expr;
            right->data.call.arguments = new_args;
            right->data.call.arg_count = old_count + 1;
            expr = right;
        } else if (right->type == AST_IDENTIFIER) {
            /* a |> f → f(a) */
            ASTNode *call = ast_create_call(right);
            call->data.call.arguments = calloc(1, sizeof(ASTNode *));
            call->data.call.arguments[0] = expr;
            call->data.call.arg_count = 1;
            expr = call;
        } else {
            /* Fallback: treat as binary op */
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
            Token name = consume_name_token(parser, "Expected property name after '.'");
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
        Token variant = consume_name_token(parser,
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
                if (arr->data.array_literal.count >= capacity) {
                    capacity = capacity == 0 ? 4 : capacity * 2;
                    arr->data.array_literal.elements = realloc(
                        arr->data.array_literal.elements,
                        capacity * sizeof(ASTNode*));
                }
                arr->data.array_literal.elements[arr->data.array_literal.count++] = elem;
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
    if (parser_match(parser, TOKEN_STRING)) {
        const char *raw = parser->previous_token.text;
        /* Check for string interpolation: "...${expr}..." */
        if (raw != NULL && strstr(raw, "${") != NULL) {
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
    if (parser_match_name_token(parser)) {
        char* name = pergyra_strdup(parser->previous_token.text);

        if ((strcmp(name, "ClaimSlot") == 0 ||
             strcmp(name, "ClaimSecureSlot") == 0) &&
            parser_check(parser, TOKEN_LESS)) {
            skip_generic_arguments(parser);
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
            free(name);
            return ident;
        }

        // 채널 송신 체크: channel <- value
        ASTNode* ident = ast_create_identifier(name);
        free(name);
        if (parser_check(parser, TOKEN_CHANNEL_OP)) {
            parser_advance(parser);
            ASTNode* value = parser_parse_expression(parser);
            return ast_create_channel_send(ident, value);
        }

        return ident;
    }

    // 람다식: (x: Int) => { ... }
    if (parser_is_lambda_start(parser)) {
        return parse_lambda_expression(parser);
    }

    // 괄호 표현식
    if (parser_match(parser, TOKEN_LPAREN)) {
        ASTNode* expr = parser_parse_expression(parser);
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    parser_error(parser, "Unexpected token in expression");
    return NULL;
}

// 람다식 파싱: (x, y) => x + y
ASTNode* parse_lambda_expression(Parser* parser) {
    ASTNode* lambda = ast_create_lambda_expression();

    // 파라미터 파싱
    parser_consume(parser, TOKEN_LPAREN, "Expected '(' before lambda parameters");

    while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
        Token param_name = consume_name_token(parser, "Expected parameter name");

        ASTNode* param = ast_create_identifier(param_name.text);

        // 타입 어노테이션 (선택적)
        if (parser_match(parser, TOKEN_COLON)) {
            ASTNode* param_type = parse_type(parser);
            ASTNode* typed_param = ast_create_let_declaration(param_name.text);
            typed_param->data.let_decl.type = param_type;
            ast_destroy(param);
            param = typed_param;
        }

        lambda->data.lambda_expr.param_count++;
        lambda->data.lambda_expr.params = realloc(
            lambda->data.lambda_expr.params,
            lambda->data.lambda_expr.param_count * sizeof(ASTNode*)
        );
        lambda->data.lambda_expr.params[lambda->data.lambda_expr.param_count - 1] = param;

        if (!parser_match(parser, TOKEN_COMMA)) break;
    }

    parser_consume(parser, TOKEN_RPAREN, "Expected ')' after lambda parameters");

    // 람다 연산자 =>
    parser_consume(parser, TOKEN_LAMBDA, "Expected '=>' in lambda expression");

    // 바디 파싱 (표현식 또는 블록)
    if (parser_check(parser, TOKEN_LBRACE)) {
        // 블록 바디
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' for lambda body");
        lambda->data.lambda_expr.body = parser_parse_block(parser);
    } else {
        // 표현식 바디
        lambda->data.lambda_expr.body = parser_parse_expression(parser);
    }

    return lambda;
}
