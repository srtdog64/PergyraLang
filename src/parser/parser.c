/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser implementation with generic-first design
 */

#include "parser_internal.h"
#include "../semantic/diag_codes.h"

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
    parser->next_decl_exported = false;
    parser->last_func_decl_async = false;
    parser->pending_doc_comment = NULL;
    parser->source_path = NULL;
    parser->decl_hint_names = NULL;
    parser->decl_hint_types = NULL;
    parser->decl_hint_nominal_kinds = NULL;
    parser->decl_hint_count = 0;
    parser->decl_hint_capacity = 0;

    // 첫 번째 토큰 읽기
    parser->current_token = lexer_next_token(lexer);

    return parser;
}

// 파서 소멸
void parser_destroy(Parser* parser) {
    if (parser) {
        ast_destroy_structured_comment(parser->pending_doc_comment);
        if (parser->decl_hint_names != NULL) {
            for (size_t i = 0; i < parser->decl_hint_count; i++)
                free(parser->decl_hint_names[i]);
        }
        free(parser->decl_hint_names);
        free(parser->decl_hint_types);
        free(parser->decl_hint_nominal_kinds);
        free(parser);
    }
}

// 토큰 진행
Token parser_advance(Parser* parser) {
    parser->previous_token = parser->current_token;
    parser->current_token = lexer_next_token(parser->lexer);
    return parser->previous_token;
}

// 다음 토큰 미리보기 (non-destructive lookahead)
Token parser_peek_next(Parser* parser) {
    /* Save lexer state */
    const char *saved_current = parser->lexer->current;
    size_t saved_pos = parser->lexer->position;
    uint32_t saved_line = parser->lexer->line;
    uint32_t saved_col = parser->lexer->column;

    Token next = lexer_next_token(parser->lexer);

    /* Restore lexer state */
    parser->lexer->current = saved_current;
    parser->lexer->position = saved_pos;
    parser->lexer->line = saved_line;
    parser->lexer->column = saved_col;

    return next;
}

// 토큰 타입 확인
bool parser_check(Parser* parser, PgyTokenType type) {
    return parser->current_token.type == type;
}

// 토큰 매칭 및 진행
bool parser_match(Parser* parser, PgyTokenType type) {
    if (!parser_check(parser, type)) return false;
    parser_advance(parser);
    return true;
}

// 토큰 소비 (필수)
Token parser_consume(Parser* parser, PgyTokenType type, const char* message) {
    if (parser_check(parser, type)) {
        return parser_advance(parser);
    }

    parser_error(parser, message);
    return parser->current_token;
}

// 에러 처리
void parser_error(Parser* parser, const char* format, ...) {
    if (parser == NULL)
        return;
    if (parser->has_error)
        return;

    parser->has_error = true;

    char message[384];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // 에러 위치 정보 추가
    char location[256];
    snprintf(location, sizeof(location), " at line %d, column %d",
             parser->current_token.line, parser->current_token.column);
    if (parser->current_token.type == TOKEN_ERROR && parser->lexer != NULL) {
        const char *lex_error = lexer_get_error(parser->lexer);
        snprintf(parser->error_msg, sizeof(parser->error_msg), "%s%s",
                 lex_error != NULL ? lex_error : "Lexer error",
                 location);
        return;
    }
    snprintf(parser->error_msg, sizeof(parser->error_msg), "%s", message);
    strncat(parser->error_msg, "\nCode: ",
            sizeof(parser->error_msg) - strlen(parser->error_msg) - 1);
    strncat(parser->error_msg, PGY_CODE_PARSE_SYNTAX,
            sizeof(parser->error_msg) - strlen(parser->error_msg) - 1);
    strncat(parser->error_msg, "\nReason: ",
            sizeof(parser->error_msg) - strlen(parser->error_msg) - 1);
    strncat(parser->error_msg, PGY_CAUSE_PARSE_UNEXPECTED_TOKEN,
            sizeof(parser->error_msg) - strlen(parser->error_msg) - 1);
    strncat(parser->error_msg, "\nFix: ",
            sizeof(parser->error_msg) - strlen(parser->error_msg) - 1);
    strncat(parser->error_msg, PGY_FIX_CHECK_SYNTAX,
            sizeof(parser->error_msg) - strlen(parser->error_msg) - 1);
    strncat(parser->error_msg, location,
            sizeof(parser->error_msg) - strlen(parser->error_msg) - 1);
}

// 에러 복구 - 다음 문장까지 건너뛰기
void parser_synchronize(Parser* parser) {
    parser_advance(parser);

    while (!parser_is_at_end(parser)) {
        if (parser->previous_token.type == TOKEN_SEMICOLON) return;
        if (parser_check(parser, TOKEN_OBJECT)
            || parser_check(parser, TOKEN_VESSEL)
            || parser_check(parser, TOKEN_INTENT)
            || parser_check(parser, TOKEN_TOBJECT)
            || parser_check(parser, TOKEN_WORLD)
            || parser_check(parser, TOKEN_ROSTER)
            || parser_check(parser, TOKEN_RELATION)
            || parser_check(parser, TOKEN_EFFECT)
            || parser_check(parser, TOKEN_ZONE)
            || parser_check(parser, TOKEN_EVENT)) {
            return;
        }

        switch (parser->current_token.type) {
            case TOKEN_SUBJECT:
            case TOKEN_CLASS:
            case TOKEN_STRUCT:
            case TOKEN_OBJECT:
            case TOKEN_TOBJECT:
            case TOKEN_VESSEL:
            case TOKEN_INTENT:
            case TOKEN_WORLD:
            case TOKEN_ROSTER:
            case TOKEN_RELATION:
            case TOKEN_EFFECT:
            case TOKEN_ZONE:
            case TOKEN_EVENT:
            case TOKEN_EXTERN:
            case TOKEN_FUNC:
            case PGY_TOKEN_TYPE:
            case TOKEN_LET:
            case TOKEN_INNATE:
            case TOKEN_ABILITY:
            case TOKEN_ROLE:
            case TOKEN_PARTY:
            case TOKEN_NAMESPACE:
            case TOKEN_EXPORT:
            case TOKEN_WITH:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_RETURN:
            case TOKEN_BREAK:
            case TOKEN_CONTINUE:
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

ASTNode *
parser_finalize_statement(Parser *parser, ASTNode *node)
{
    if (node != NULL && parser->next_decl_exported) {
        if (parser_is_exportable_decl(node)) {
            node->is_exported = true;
            node->has_explicit_export = true;
        } else {
            parser_error(parser, "'export' can only apply to declarations");
        }
    }
    if (node != NULL && node->origin_path == NULL && parser->source_path != NULL)
        node->origin_path = pergyra_strdup(parser->source_path);
    parser_register_decl_hint(parser, node);
    if (!parser_attach_pending_doc_comment(parser, node))
        parser_discard_pending_doc_comment(parser);
    parser->next_decl_exported = false;
    parser->last_func_decl_async = false;
    return node;
}

// ============= 문장 파싱 =============

// 프로그램 파싱
ASTNode* parser_parse_program(Parser* parser) {
    ASTNode* program = ast_create_program();

    while (!parser_is_at_end(parser)) {
        ASTNode* stmt = parser_parse_statement(parser);
        if (stmt) {
            if (stmt->type == AST_BLOCK) {
                for (size_t i = 0; i < stmt->data.block.count; i++) {
                    ASTNode *child = stmt->data.block.statements[i];
                    if (child != NULL)
                        ast_add_statement(program, child);
                    stmt->data.block.statements[i] = NULL;
                }
                ast_destroy(stmt);
            } else {
                ast_add_statement(program, stmt);
            }
        }

        if (parser->has_error) {
            parser_synchronize(parser);
        }
    }

    return program;
}

// 문장 파싱
// let 선언 파싱
ASTNode* parser_parse_let_declaration(Parser* parser) {
    /* Destructuring: let (a, b, c) = expr; */
    if (parser_check(parser, TOKEN_LPAREN)) {
        parser_advance(parser);  /* consume '(' */
        ASTNode *node = calloc(1, sizeof(ASTNode));
        node->type = AST_LET_DESTRUCTURE;
        node->line = parser->previous_token.line;
        node->column = parser->previous_token.column;
        node->data.let_destructure.names = NULL;
        node->data.let_destructure.name_count = 0;
        node->data.let_destructure.initializer = NULL;

        while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
            Token var = consume_binding_name_token(parser, "Expected variable name in destructuring");
            size_t n = ++node->data.let_destructure.name_count;
            node->data.let_destructure.names = realloc(
                node->data.let_destructure.names, n * sizeof(char *));
            node->data.let_destructure.names[n - 1] = pergyra_strdup(var.text);
            if (!parser_match(parser, TOKEN_COMMA))
                break;
        }
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after destructuring names");
        parser_consume(parser, TOKEN_ASSIGN, "Expected '=' in let destructuring");
        node->data.let_destructure.initializer = parser_parse_expression(parser);
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after let destructuring");
        return node;
    }

    // 변수 이름
    Token name = consume_binding_name_token(parser, "Expected variable name");

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
    Token alias = consume_binding_name_token(parser, "Expected variable name after 'as'");
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
        if (parser->has_error) {
            parser_synchronize(parser);
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
        if (parser->has_error) {
            parser_synchronize(parser);
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
