/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser implementation with generic-first design
 */

#include "parser_internal.h"

// 파서 생성
Parser* parser_create(Lexer* lexer) {
    Parser* parser = calloc(1, sizeof(Parser));
    if (!parser) return NULL;

    parser->lexer = lexer;
    parser->has_error = false;
    parser->scope_depth = 0;
    parser->in_parallel_block = false;
    parser->in_with_statement = false;
    parser->in_extern_block = false;

    // 첫 번째 토큰 읽기
    parser->current_token = lexer_next_token(lexer);

    return parser;
}

// 파서 소멸
void parser_destroy(Parser* parser) {
    if (parser) {
        free(parser);
    }
}

// 토큰 진행
Token parser_advance(Parser* parser) {
    parser->previous_token = parser->current_token;
    parser->current_token = lexer_next_token(parser->lexer);
    return parser->previous_token;
}

// 토큰 타입 확인
bool parser_check(Parser* parser, TokenType type) {
    return parser->current_token.type == type;
}

// 토큰 매칭 및 진행
bool parser_match(Parser* parser, TokenType type) {
    if (!parser_check(parser, type)) return false;
    parser_advance(parser);
    return true;
}

// 토큰 소비 (필수)
Token parser_consume(Parser* parser, TokenType type, const char* message) {
    if (parser_check(parser, type)) {
        return parser_advance(parser);
    }

    parser_error(parser, message);
    return parser->current_token;
}

// 에러 처리
void parser_error(Parser* parser, const char* format, ...) {
    parser->has_error = true;

    va_list args;
    va_start(args, format);
    vsnprintf(parser->error_msg, sizeof(parser->error_msg), format, args);
    va_end(args);

    // 에러 위치 정보 추가
    char location[256];
    snprintf(location, sizeof(location), " at line %d, column %d",
             parser->current_token.line, parser->current_token.column);
    strncat(parser->error_msg, location,
            sizeof(parser->error_msg) - strlen(parser->error_msg) - 1);
}

// 에러 복구 - 다음 문장까지 건너뛰기
void parser_synchronize(Parser* parser) {
    parser_advance(parser);

    while (!parser_is_at_end(parser)) {
        if (parser->previous_token.type == TOKEN_SEMICOLON) return;

        switch (parser->current_token.type) {
            case TOKEN_CLASS:
            case TOKEN_STRUCT:
            case TOKEN_EXTERN:
            case TOKEN_FUNC:
            case TOKEN_LET:
            case TOKEN_WITH:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_RETURN:
                return;
            default:
                break;
        }

        parser_advance(parser);
    }
}

// 파싱 종료 확인
bool parser_is_at_end(const Parser* parser) {
    return parser->current_token.type == TOKEN_EOF;
}

// 에러 확인
bool parser_has_error(const Parser* parser) {
    return parser->has_error;
}

// 에러 메시지 가져오기
const char* parser_get_error(const Parser* parser) {
    return parser->error_msg;
}

// ============= 문장 파싱 =============

// 프로그램 파싱
ASTNode* parser_parse_program(Parser* parser) {
    ASTNode* program = ast_create_program();

    while (!parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_add_statement(program, stmt);
        }

        if (parser->has_error) {
            parser_synchronize(parser);
        }
    }

    return program;
}

// 문장 파싱
ASTNode* parser_parse_statement(Parser* parser) {
    // async 함수 선언
    if (parser_match(parser, TOKEN_ASYNC)) {
        return parser_parse_async_function(parser);
    }

    // actor 선언
    if (parser_match(parser, TOKEN_ACTOR)) {
        return parser_parse_actor_declaration(parser);
    }

    // select 문
    if (parser_match(parser, TOKEN_SELECT)) {
        return parser_parse_select_statement(parser);
    }

    // 함수 선언
    if (parser_match(parser, TOKEN_FUNC)) {
        return parse_function_declaration(parser);
    }

    // import 선언
    if (parser_match(parser, TOKEN_IMPORT)) {
        Token path = parser_consume(parser, TOKEN_STRING,
            "Expected string path after 'import'");
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after import");
        /* Strip quotes from string literal */
        char *raw = pergyra_strndup(path.text + 1, path.length - 2);
        ASTNode *imp = ast_create_import_declaration(raw);
        free(raw);
        return imp;
    }

    // extern 블록
    if (parser_match(parser, TOKEN_EXTERN)) {
        return parse_extern_block(parser);
    }

    // 클래스 선언
    if (parser_match(parser, TOKEN_CLASS)) {
        return parse_class_declaration(parser);
    }

    // 구조체 선언
    if (parser_match(parser, TOKEN_STRUCT)) {
        return parse_struct_declaration(parser);
    }

    // let 선언
    if (parser_match(parser, TOKEN_LET)) {
        return parser_parse_let_declaration(parser);
    }

    // with 문
    if (parser_match(parser, TOKEN_WITH)) {
        return parser_parse_with_statement(parser);
    }

    // parallel 블록
    if (parser_match(parser, TOKEN_PARALLEL)) {
        return parser_parse_parallel_block(parser);
    }

    // for 루프
    if (parser_match(parser, TOKEN_FOR)) {
        return parse_for_loop(parser);
    }

    // while 루프
    if (parser_match(parser, TOKEN_WHILE)) {
        return parse_while_statement(parser);
    }

    // match 문
    if (parser_match(parser, TOKEN_MATCH)) {
        return parse_match_statement(parser);
    }

    // if 문
    if (parser_match(parser, TOKEN_IF)) {
        return parse_if_statement(parser);
    }

    // return 문
    if (parser_match(parser, TOKEN_RETURN)) {
        return parse_return_statement(parser);
    }

    // unsafe 블록
    if (parser_match(parser, TOKEN_UNSAFE)) {
        return parse_unsafe_block(parser);
    }

    // defer 문
    if (parser_match(parser, TOKEN_DEFER)) {
        return parse_defer_statement(parser);
    }

    // systemic 선언
    if (parser_match(parser, TOKEN_SYSTEMIC)) {
        return parse_systemic_declaration(parser);
    }

    // world 선언
    if (parser_match(parser, TOKEN_WORLD)) {
        return parse_world_declaration(parser);
    }

    // party 선언
    if (parser_match(parser, TOKEN_PARTY)) {
        return parse_party_declaration(parser);
    }

    // ability 선언
    if (parser_match(parser, TOKEN_ABILITY)) {
        return parse_ability_declaration(parser);
    }

    // role 선언
    if (parser_match(parser, TOKEN_ROLE)) {
        return parse_role_declaration(parser);
    }

    // event 선언
    if (parser_match(parser, TOKEN_EVENT)) {
        return parse_event_declaration(parser);
    }

    // 표현식 문장
    return parser_parse_expression_statement(parser);
}

// let 선언 파싱
ASTNode* parser_parse_let_declaration(Parser* parser) {
    // 변수 이름
    Token name = consume_name_token(parser, "Expected variable name");

    ASTNode* let_decl = ast_create_let_declaration(name.text);

    // 타입 어노테이션 (선택적)
    if (parser_match(parser, TOKEN_COLON)) {
        let_decl->data.let_decl.type = parse_type(parser);
    }

    // 초기화 표현식
    parser_consume(parser, TOKEN_ASSIGN, "Expected '=' in let declaration");
    let_decl->data.let_decl.initializer = parser_parse_expression(parser);

    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after let declaration");

    return let_decl;
}

// with 문 파싱
ASTNode* parser_parse_with_statement(Parser* parser) {
    ASTNode* with_stmt = ast_create_with_statement();

    // 슬롯 타입
    if (parser_match(parser, TOKEN_SLOT)) {
        with_stmt->data.with_stmt.is_secure = false;
    } else {
        Token slot_kind = parser_consume(
            parser,
            TOKEN_IDENTIFIER,
            "Expected 'slot' or 'SecureSlot' after 'with'"
        );

        if (strcmp(slot_kind.text, "SecureSlot") == 0) {
            with_stmt->data.with_stmt.is_secure = true;
        } else if (strcmp(slot_kind.text, "slot") != 0) {
            parser_error(parser, "Expected 'slot' or 'SecureSlot' after 'with'");
        }
    }

    // 제네릭 타입
    parser_consume(parser, TOKEN_LESS, "Expected '<' after slot type");
    with_stmt->data.with_stmt.slot_type = parse_type(parser);
    parser_consume(parser, TOKEN_GREATER, "Expected '>' after slot type");

    // 보안 레벨 (선택적)
    if (parser_match(parser, TOKEN_LPAREN)) {
        Token level = parser_consume(parser, TOKEN_IDENTIFIER, "Expected security level");
        with_stmt->data.with_stmt.security_level = pergyra_strdup(level.text);
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after security level");
    }

    // as 변수명
    parser_consume(parser, TOKEN_AS, "Expected 'as' in with statement");
    Token alias = parser_consume(parser, TOKEN_IDENTIFIER, "Expected variable name after 'as'");
    with_stmt->data.with_stmt.alias = pergyra_strdup(alias.text);

    // 블록
    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after with statement");
    parser->in_with_statement = true;
    with_stmt->data.with_stmt.body = parser_parse_block(parser);
    parser->in_with_statement = false;

    return with_stmt;
}

// parallel 블록 파싱
ASTNode* parser_parse_parallel_block(Parser* parser) {
    ASTNode* parallel = ast_create_parallel_block();

    parser_consume(parser, TOKEN_LBRACE, "Expected '{' after 'parallel'");

    parser->in_parallel_block = true;
    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_add_parallel_task(parallel, stmt);
        }
    }
    parser->in_parallel_block = false;

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after parallel block");

    return parallel;
}

// 블록 파싱
ASTNode* parser_parse_block(Parser* parser) {
    ASTNode* block = ast_create_block();

    parser->scope_depth++;

    while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            ast_add_statement(block, stmt);
        }
    }

    parser->scope_depth--;

    parser_consume(parser, TOKEN_RBRACE, "Expected '}' after block");

    return block;
}

// 표현식 문장
ASTNode* parser_parse_expression_statement(Parser* parser) {
    ASTNode* expr = parser_parse_expression(parser);
    parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression");
    return expr;
}
