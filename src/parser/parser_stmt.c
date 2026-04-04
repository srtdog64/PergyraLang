#include "parser_internal.h"

// for 루프 파싱
// Supports two forms:
//   for x in start..end { }   — range loop
//   for item in collection { } — for-in collection loop
ASTNode* parse_for_loop(Parser* parser) {
    ASTNode* for_loop = ast_create_for_loop();

    // 루프 변수
    Token var = parser_consume(parser, TOKEN_IDENTIFIER, "Expected loop variable");
    for_loop->data.for_loop.variable = pergyra_strdup(var.text);

    parser_consume(parser, TOKEN_IN, "Expected 'in' in for loop");

    // Parse the first expression
    ASTNode* first = parser_parse_expression(parser);

    // Check if this is a range (expr..expr) or collection (expr)
    if (parser_check(parser, TOKEN_DOT)
        && parser->current_token.length == 2
        && parser->current_token.text != NULL
        && strncmp(parser->current_token.text, "..", 2) == 0) {
        // Range form: start..end
        parser_advance(parser);  // consume '..'
        ASTNode* end = parser_parse_expression(parser);
        for_loop->data.for_loop.range_start = first;
        for_loop->data.for_loop.range_end = end;
    } else {
        // Collection form: for item in collection { }
        for_loop->data.for_loop.iterable = first;
    }

    // 루프 본문
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after for loop header");
    for_loop->data.for_loop.body = parser_parse_block(parser);

    return for_loop;
}

// while 문 파싱
ASTNode* parse_while_statement(Parser* parser) {
    ASTNode* while_loop = ast_create_while_loop();
    while_loop->line = parser->previous_token.line;
    while_loop->column = parser->previous_token.column;

    // 조건식
    while_loop->data.while_loop.condition = parser_parse_expression(parser);

    // 본문
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after while condition");
    while_loop->data.while_loop.body = parser_parse_block(parser);

    return while_loop;
}

// match 문 파싱
ASTNode* parse_match_statement(Parser* parser) {
    ASTNode* match = ast_create_match_statement();
    match->line = parser->previous_token.line;
    match->column = parser->previous_token.column;

    // subject 표현식
    match->data.match_stmt.subject = parser_parse_expression(parser);

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after match expression");

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        if (parser_match(parser, TOKEN_CASE)) {
            ASTNode* mc = ast_create_match_case();
            mc->line = parser->previous_token.line;
            mc->column = parser->previous_token.column;

            // 패턴: 리터럴 또는 식별자
            mc->data.match_case.pattern = parser_parse_expression(parser);

            // guard (선택적)
            if (parser_match(parser, TOKEN_IF)) {
                mc->data.match_case.guard = parser_parse_expression(parser);
            }

            parser_consume(parser, TOKEN_COLON, "Expected ':' after case pattern");

            // 본문: 다음 case/default/} 전까지
            ASTNode* body = ast_create_block();
            while (!parser_check(parser, TOKEN_CASE) &&
                   !parser_check(parser, TOKEN_DEFAULT) &&
                   !parser_check(parser, TOKEN_RBRACE) &&
                   !parser_is_at_end(parser)) {
                ASTNode* stmt = parser_parse_statement(parser);
                if (stmt) ast_add_statement(body, stmt);
                if (parser->has_error) {
                    parser_synchronize(parser);
                }
            }
            mc->data.match_case.body = body;

            // case를 match에 추가
            match->data.match_stmt.case_count++;
            match->data.match_stmt.cases = realloc(
                match->data.match_stmt.cases,
                sizeof(ASTNode*) * match->data.match_stmt.case_count);
            match->data.match_stmt.cases[match->data.match_stmt.case_count - 1] = mc;

        } else if (parser_match(parser, TOKEN_DEFAULT)) {
            parser_consume(parser, TOKEN_COLON, "Expected ':' after default");

            ASTNode* body = ast_create_block();
            while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
                ASTNode* stmt = parser_parse_statement(parser);
                if (stmt) ast_add_statement(body, stmt);
                if (parser->has_error) {
                    parser_synchronize(parser);
                }
            }
            match->data.match_stmt.default_body = body;
        } else {
            parser_error(parser, "Expected 'case' or 'default' in match");
            parser_advance(parser);
        }
    }

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after match body");
    return match;
}

// if 문 파싱
ASTNode* parse_if_statement(Parser* parser) {
    ASTNode* if_stmt = ast_create_if_statement();

    // 조건
    if_stmt->data.if_stmt.condition = parser_parse_expression(parser);

    // then 블록
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after if condition");
    if_stmt->data.if_stmt.then_branch = parser_parse_block(parser);

    // else 블록 (선택적)
    if (parser_match(parser, TOKEN_ELSE)) {
        if (parser_match(parser, TOKEN_IF)) {
            if_stmt->data.if_stmt.else_branch = parse_if_statement(parser);
        } else {
            parser_consume(parser, TOKEN_LBRACE, "Expected '{' after else");
            if_stmt->data.if_stmt.else_branch = parser_parse_block(parser);
        }
    }

    return if_stmt;
}

// unsafe 블록 파싱
ASTNode* parse_unsafe_block(Parser* parser) {
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after unsafe");
    ASTNode* body = parser_parse_block(parser);
    return ast_create_unsafe_block(body);
}

// defer 문 파싱
ASTNode* parse_defer_statement(Parser* parser) {
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after defer");
    ASTNode* body = parser_parse_block(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after defer block");
    return ast_create_defer_statement(body);
}

// return 문 파싱
ASTNode* parse_return_statement(Parser* parser) {
    ASTNode* return_stmt = ast_create_return_statement();

    if (!parser_check(parser, TOKEN_SEMICOLON)) {
        return_stmt->data.return_stmt.value = parser_parse_expression(parser);
    }

    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after return statement");

    return return_stmt;
}
