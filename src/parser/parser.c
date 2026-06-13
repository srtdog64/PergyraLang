/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser implementation with generic-first design
 */

#include "parser_internal.h"
#include "../semantic/diag_codes.h"
#include "../common/string_compat.h"
#include <stdint.h>

/*
 * Recursive-descent depth guard. The grammar recurses on nested types
 * (Wrap<Wrap<...>>), parenthesized expressions, and nested blocks; without a
 * bound a deeply nested input overflows the native call stack (SIGSEGV).
 * The chokepoint functions (parse_type, parser_parse_expression,
 * parser_parse_block) call parser_enter_recursion on entry and
 * parser_leave_recursion on exit through thin wrappers.
 */
#define PARSER_MAX_RECURSION_DEPTH 400

bool
parser_enter_recursion(Parser *parser)
{
    if (parser == NULL)
        return false;
    if (parser->recursion_depth >= PARSER_MAX_RECURSION_DEPTH) {
        parser_error(parser,
            "Expression, type, or block nesting is too deep (limit is "
            "400); refactor the deeply nested construct");
        return false;
    }
    parser->recursion_depth++;
    return true;
}

void
parser_leave_recursion(Parser *parser)
{
    if (parser != NULL && parser->recursion_depth > 0)
        parser->recursion_depth--;
}

// 파서 생성
Parser* parser_create(Lexer* lexer) {
    Parser* parser = calloc(1, sizeof(Parser));
    if (!parser) return NULL;

    ast_reset_node_budget();

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
    /* Save the full lexer state: lookahead must not publish errors. */
    Lexer saved_lexer = *parser->lexer;

    Token next = lexer_next_token(parser->lexer);

    /* Restore lexer state */
    *parser->lexer = saved_lexer;

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

/* Consume a statement terminator. A ';' always terminates; otherwise, in the
 * newline-terminated style a statement may end where the next token begins on a
 * later line or closes the block / ends input. Only when none of those hold is
 * the missing ';' a real error. This avoids same-line ambiguity (no automatic
 * insertion mid-line) while accepting the common newline-separated form. */
void parser_consume_statement_terminator(Parser* parser, const char* message) {
    if (parser_match(parser, TOKEN_SEMICOLON))
        return;
    if (parser_is_at_end(parser)
        || parser_check(parser, TOKEN_RBRACE)
        || parser->current_token.line > parser->previous_token.line)
        return;
    parser_consume(parser, TOKEN_SEMICOLON, message);
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
    pergyra_str_append(parser->error_msg, sizeof(parser->error_msg), "\nCode: ");
    pergyra_str_append(parser->error_msg, sizeof(parser->error_msg), PGY_CODE_PARSE_SYNTAX);
    pergyra_str_append(parser->error_msg, sizeof(parser->error_msg), "\nReason: ");
    pergyra_str_append(parser->error_msg, sizeof(parser->error_msg), PGY_CAUSE_PARSE_UNEXPECTED_TOKEN);
    pergyra_str_append(parser->error_msg, sizeof(parser->error_msg), "\nFix: ");
    pergyra_str_append(parser->error_msg, sizeof(parser->error_msg), PGY_FIX_CHECK_SYNTAX);
    pergyra_str_append(parser->error_msg, sizeof(parser->error_msg), location);
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

static bool
parser_check_contextual_is(Parser *parser)
{
    return parser != NULL
        && parser_check(parser, TOKEN_IDENTIFIER)
        && parser->current_token.text != NULL
        && strcmp(parser->current_token.text, "is") == 0;
}

void
parser_reject_reserved_cast_after_expression(Parser *parser)
{
    if (parser == NULL)
        return;

    if (parser_match(parser, TOKEN_AS)) {
        parser_error(parser,
            "Cast syntax 'expr as Type' is reserved but not implemented.\n"
            "Reason: implicit cast/type-test lowering is not frozen across semantic, ABI, and backend diagnostics.\n"
            "Fix: use an explicit conversion helper.");
        (void)parse_type(parser);
        return;
    }

    if (parser_check_contextual_is(parser)) {
        parser_advance(parser);
        parser_error(parser,
            "Type-test syntax 'expr is Type' is reserved but not implemented.\n"
            "Reason: runtime type-test semantics are not frozen for the beta subset.\n"
            "Fix: use an explicit predicate helper.");
        (void)parse_type(parser);
        return;
    }

    if (parser_check(parser, TOKEN_LBRACE)) {
        parser_error(parser,
            "Object initializer syntax 'Type { ... }' is reserved but not implemented.\n"
            "Reason: initializer sugar must not bypass constructor, projection, or ownership contracts.\n"
            "Fix: use a constructor or factory function.");
    }
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

    ast_assign_stable_ids(program);
    return program;
}

// 문장 파싱
// let 선언 파싱
ASTNode* parser_parse_let_declaration(Parser* parser) {
    if (parser_check(parser, TOKEN_LBRACE)) {
        parser_error(parser,
            "Named field destructuring is reserved but not implemented.\n"
            "Reason: named destructuring must preserve field provenance and CFG ownership facts.\n"
            "Fix: use positional destructuring or explicit field reads.");
        parser_synchronize(parser);
        return NULL;
    }

    /* Destructuring: let (a, b, c) = expr; */
    if (parser_check(parser, TOKEN_LPAREN)) {
        parser_advance(parser);  /* consume '(' */
        ASTNode *node = calloc(1, sizeof(ASTNode));
        node->type = AST_LET_DESTRUCTURE;
        node->line = parser->previous_token.line;
        node->column = parser->previous_token.column;
        node->data.let_destructure.names = NULL;
        node->data.let_destructure.name_count = 0;
        node->data.let_destructure.name_capacity = 0;
        node->data.let_destructure.initializer = NULL;

        while (!parser_check(parser, TOKEN_RPAREN) && !parser_is_at_end(parser)) {
            Token var = consume_binding_name_token(parser, "Expected variable name in destructuring");
            if (!parser_append_destructure_name(parser, node, var.text))
                break;
            if (!parser_match(parser, TOKEN_COMMA))
                break;
        }
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after destructuring names");
        parser_consume(parser, TOKEN_ASSIGN, "Expected '=' in let destructuring");
        node->data.let_destructure.initializer = parser_parse_expression(parser);
        parser_reject_reserved_cast_after_expression(parser);
        parser_consume_statement_terminator(parser, "Expected ';' after let destructuring");
        return node;
    }

    // 선택적 가변 수정자: let mut name = ...
    bool let_is_mutable = false;
    if (parser->current_token.text != NULL
        && strcmp(parser->current_token.text, "mut") == 0
        && parser_peek_next(parser).type == TOKEN_IDENTIFIER) {
        parser_advance(parser);  /* consume 'mut' */
        let_is_mutable = true;
    }

    // 변수 이름
    Token name = consume_binding_name_token(parser, "Expected variable name");

    ASTNode* let_decl = ast_create_let_declaration(name.text);
    let_decl->data.let_decl.is_mutable = let_is_mutable;

    // 타입 어노테이션 (선택적)
    if (parser_match(parser, TOKEN_COLON)) {
        let_decl->data.let_decl.type = parse_type(parser);
    }

    // 초기화 표현식
    parser_consume(parser, TOKEN_ASSIGN, "Expected '=' in let declaration");
    let_decl->data.let_decl.initializer = parser_parse_expression(parser);
    parser_reject_reserved_cast_after_expression(parser);

    parser_consume_statement_terminator(parser, "Expected ';' after let declaration");

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

        if (slot_kind.text == NULL) {
            parser_error(parser, "Expected 'slot' or 'SecureSlot' after 'with'");
        } else if (strcmp(slot_kind.text, "SecureSlot") == 0) {
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

    /* `parallel (collection) [join with <mode>]`: a parallel-join directive
     * over a collection with no following block body. */
    if (parser_check(parser, TOKEN_LPAREN)) {
        parser_advance(parser);  /* '(' */
        ast_destroy(parser_parse_expression(parser));
        parser_consume(parser, TOKEN_RPAREN,
            "Expected ')' after parallel target");
        if (parser->current_token.text != NULL
            && strcmp(parser->current_token.text, "join") == 0) {
            parser_advance(parser);  /* join */
            if (parser->current_token.text != NULL
                && strcmp(parser->current_token.text, "with") == 0)
                parser_advance(parser);  /* with */
            if (parser_check(parser, TOKEN_IDENTIFIER))
                parser_advance(parser);  /* mode: all / any / ... */
        }
        if (parser_match(parser, TOKEN_LBRACE)) {
            parser->in_parallel_block = true;
            ASTNode* body = parser_parse_block(parser);
            parser->in_parallel_block = false;
            if (body != NULL)
                ast_add_parallel_task(parallel, body);
        }
        return parallel;
    }

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

    if (!parser_enter_recursion(parser)) {
        return block;
    }

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

    parser_leave_recursion(parser);
    return block;
}

// 표현식 문장
ASTNode* parser_parse_expression_statement(Parser* parser) {
    ASTNode* expr = parser_parse_expression(parser);
    parser_reject_reserved_cast_after_expression(parser);
    parser_consume_statement_terminator(parser, "Expected ';' after expression");
    return expr;
}
