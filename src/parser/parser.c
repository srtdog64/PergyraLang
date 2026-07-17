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
    parser->error_msg = NULL;
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
    parser->token_stream = parser->current_token.stream;
    if (parser->token_stream.source_fingerprint == 0)
        parser_error(parser, "parser received an unanchored token stream");

    return parser;
}

// 파서 소멸
void parser_destroy(Parser* parser) {
    if (parser) {
        free(parser->error_msg);
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
    if (!lexer_token_stream_handle_equal(parser->token_stream,
                                         parser->current_token.stream)) {
        parser_error(parser, "parser token stream anchor changed during parse");
    }
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
    va_list args;
    char *message;

    if (parser == NULL)
        return;
    if (parser->has_error)
        return;

    parser->has_error = true;

    /* Heap-exact throughout: the rendered text is what the user (and the
     * code-prefix router in driver_diag) actually consumes, so no stage of
     * it may be clipped without saying so. The byte layout below is the
     * same one the old fixed-buffer path produced. */
    va_start(args, format);
    message = pergyra_strdup_vprintf(format, args);
    va_end(args);

    free(parser->error_msg);
    if (parser->current_token.type == TOKEN_ERROR && parser->lexer != NULL) {
        const char *lex_error = lexer_get_error(parser->lexer);
        parser->error_msg = pergyra_strdup_printf(
            "%s at line %d, column %d",
            lex_error != NULL ? lex_error : "Lexer error",
            parser->current_token.line, parser->current_token.column);
    } else {
        parser->error_msg = pergyra_strdup_printf(
            "%s\nCode: %s\nReason: %s\nFix: %s at line %d, column %d",
            message != NULL ? message : "parse error",
            PGY_CODE_PARSE_SYNTAX,
            PGY_CAUSE_PARSE_UNEXPECTED_TOKEN,
            PGY_FIX_CHECK_SYNTAX,
            parser->current_token.line, parser->current_token.column);
    }
    free(message);
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
    if (parser == NULL)
        return "";
    if (parser->error_msg != NULL)
        return parser->error_msg;
    /* has_error with no text can only mean the diagnostic allocation itself
     * failed. Say that, rather than handing back NULL to callers that %s it
     * -- an OOM must stay a distinguishable message, not a crash. */
    return parser->has_error
        ? "parse error (diagnostic allocation failed)"
        : "";
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

    if (parser_check(parser, TOKEN_LBRACE)) {
        parser_error(parser,
            "Object initializer syntax 'Type { ... }' is reserved but not implemented.\n"
            "Reason: initializer sugar must not bypass constructor, projection, or ownership contracts.\n"
            "Fix: use a constructor or factory function.");
    }
}

// ============= 문장 파싱 =============

/* Script register: a top-level statement is "executable" (belongs in the
 * implicit Main body) when it is a statement or a bare expression, never a
 * declaration. The list is a conservative allowlist of clearly-executable node
 * kinds; anything else (every AST_*_DECL, namespace, import, extern, ...) stays
 * a top-level declaration. Missing an executable kind only leaves it at top
 * level (the pre-existing no-run behavior), never pulls a declaration into Main.
 */
static bool parser_is_top_level_executable(const ASTNode* stmt) {
    if (stmt == NULL)
        return false;
    switch (stmt->type) {
    case AST_IF_STMT:
    case AST_WHILE_LOOP:
    case AST_FOR_LOOP:
    case AST_RETURN:
    case AST_ASSIGNMENT:
    case AST_LET_DECL:
    case AST_LET_DESTRUCTURE:
    case AST_BLOCK:
    case AST_MATCH_STMT:
    case AST_BREAK:
    case AST_CONTINUE:
    case AST_DEFER_STMT:
    case AST_BIND_STMT:
    case AST_WITH_STMT:
    case AST_PARALLEL_BLOCK:
    case AST_ASYNC_BLOCK:
    case AST_UNSAFE_BLOCK:
    case AST_TRANSACTION_BLOCK:
    case AST_SELECT_STMT:
    case AST_CALL:
    case AST_BINARY:
    case AST_UNARY:
    case AST_IDENTIFIER:
    case AST_MEMBER_ACCESS:
    case AST_AWAIT_EXPR:
    case AST_SPAWN_EXPR:
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
        return true;
    default:
        return false;
    }
}

/* Collect top-level executable statements (in source order) into a synthesized
 * `func Main() { ... }` so the rest of the compiler reuses the existing entry
 * machinery. Declarations stay at top level. If the source ALSO declares an
 * explicit `func Main`, two Mains result and the existing duplicate-definition
 * check rejects it -- top-level statements and an explicit Main are mutually
 * exclusive, with no silent dropped code. Top-level `let` is script-local
 * because it lives inside this implicit Main. No-op when there are no top-level
 * executable statements (a normal structured program is unchanged).
 */
static bool program_has_func_named(ASTNode* program, const char* name) {
    size_t count = ast_program_statement_count(program);
    for (size_t i = 0; i < count; i++) {
        ASTNode* s = program->data.program.statements[i];
        if (s != NULL && s->type == AST_FUNC_DECL
            && s->data.func_decl.name != NULL
            && strcmp(s->data.func_decl.name, name) == 0)
            return true;
    }
    return false;
}

static void parser_synthesize_implicit_main(Parser* parser, ASTNode* program) {
    if (program == NULL || program->type != AST_PROGRAM)
        return;

    ASTNode* implicit_body = NULL;
    size_t count = ast_program_statement_count(program);
    size_t write = 0;
    for (size_t i = 0; i < count; i++) {
        ASTNode* stmt = program->data.program.statements[i];
        if (parser_is_top_level_executable(stmt)) {
            if (implicit_body == NULL)
                implicit_body = ast_create_block();
            ast_add_statement(implicit_body, stmt);          /* ownership -> Main body */
        } else if (stmt != NULL) {
            program->data.program.statements[write++] = stmt; /* keep, compacted */
        }
    }
    /* Drop moved/NULL slots. ast_destroy frees only [0, count), so the moved
     * executables are owned solely by the Main body (no double free, no holes). */
    program->data.program.count = write;

    if (implicit_body == NULL)
        return;

    /* Top-level statements ARE the implicit Main body, so an explicit `func Main`
     * alongside them is a duplicate Main. Reject loudly -- never silently drop
     * the top-level code. */
    if (program_has_func_named(program, "Main")) {
        parser_error(parser,
            "Top-level statements and an explicit 'func Main' are mutually "
            "exclusive: the top-level statements already form the program's "
            "Main.\n"
            "Fix: move the statements into 'func Main', or remove 'func Main' "
            "and keep them at top level.");
        ast_destroy(implicit_body);
        return;
    }

    ASTNode* implicit_main = ast_create_function("Main");
    ASTNode* void_type = implicit_main != NULL ? ast_create_type("Void") : NULL;
    if (implicit_main == NULL || void_type == NULL)
        goto implicit_main_oom;
    if (!ast_func_set_return_type(implicit_main, void_type))
        goto implicit_main_oom;
    void_type = NULL;
    if (!ast_func_attach_body(implicit_main, implicit_body))
        goto implicit_main_oom;
    implicit_body = NULL;
    if (!ast_program_append_statement(program, implicit_main))
        goto implicit_main_oom;
    implicit_main = NULL;
    return;

implicit_main_oom:
    {
        parser_error(parser, "Out of memory while synthesizing implicit Main.");
        ast_destroy(void_type);
        ast_destroy(implicit_main);
        ast_destroy(implicit_body);
        return;
    }
}

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

    parser_synthesize_implicit_main(parser, program);

    if (!ast_assign_stable_ids(program)) {
        parser_error(parser, "Syntax node identity space exhausted.");
        ast_destroy(program);
        return NULL;
    }
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
        node->data.let_destructure.field_bindings = NULL;
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

/* parallel 블록 파싱은 parser_parallel.c 소유 (docs/181; 550-line
 * responsibility rule). */

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
