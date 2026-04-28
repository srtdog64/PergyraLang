/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser implementation with generic-first design
 */

#include "parser_internal.h"
#include "../semantic/diag_codes.h"

static const char *
parser_decl_hint_name(ASTNode *node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_CLASS_DECL:
        return node->data.class_decl.name;
    case AST_INTENT_DECL:
        return node->data.intent_decl.name;
    case AST_ZONE_DECL:
        return node->data.zone_decl.name;
    case AST_WORLD_DECL:
        return node->data.world_decl.name;
    case AST_ROSTER_DECL:
        return node->data.roster_decl.name;
    case AST_PARTY_DECL:
        return node->data.party_decl.name;
    case AST_RELATION_DECL:
        return node->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return node->data.effect_decl.name;
    case AST_EVENT_DECL:
        return node->data.event_decl.name;
    default:
        return NULL;
    }
}

static void
parser_register_decl_hint(Parser *parser, ASTNode *node)
{
    const char *name;
    ASTNodeType node_type;
    NominalDeclKind nominal_kind = NOMINAL_DECL_CLASS;

    if (parser == NULL || node == NULL || parser->scope_depth != 0)
        return;

    name = parser_decl_hint_name(node);
    if (name == NULL)
        return;

    for (size_t i = 0; i < parser->decl_hint_count; i++) {
        if (parser->decl_hint_names[i] != NULL
            && strcmp(parser->decl_hint_names[i], name) == 0) {
            parser->decl_hint_types[i] = node->type;
            if (node->type == AST_CLASS_DECL)
                parser->decl_hint_nominal_kinds[i] = node->data.class_decl.nominal_kind;
            return;
        }
    }

    if (parser->decl_hint_count >= parser->decl_hint_capacity) {
        size_t new_capacity = parser->decl_hint_capacity == 0
            ? 16 : parser->decl_hint_capacity * 2;
        char **new_names = realloc(parser->decl_hint_names,
            new_capacity * sizeof(char *));
        ASTNodeType *new_types = realloc(parser->decl_hint_types,
            new_capacity * sizeof(ASTNodeType));
        NominalDeclKind *new_nominal_kinds = realloc(parser->decl_hint_nominal_kinds,
            new_capacity * sizeof(NominalDeclKind));
        if (new_names == NULL || new_types == NULL || new_nominal_kinds == NULL) {
            free(new_names);
            free(new_types);
            free(new_nominal_kinds);
            return;
        }
        parser->decl_hint_names = new_names;
        parser->decl_hint_types = new_types;
        parser->decl_hint_nominal_kinds = new_nominal_kinds;
        parser->decl_hint_capacity = new_capacity;
    }

    node_type = node->type;
    if (node_type == AST_CLASS_DECL)
        nominal_kind = node->data.class_decl.nominal_kind;
    parser->decl_hint_names[parser->decl_hint_count] = pergyra_strdup(name);
    parser->decl_hint_types[parser->decl_hint_count] = node_type;
    parser->decl_hint_nominal_kinds[parser->decl_hint_count] = nominal_kind;
    parser->decl_hint_count++;
}

bool
parser_lookup_decl_hint(Parser *parser, const char *name,
                        ASTNodeType *node_type_out,
                        NominalDeclKind *nominal_kind_out)
{
    if (node_type_out != NULL)
        *node_type_out = AST_PROGRAM;
    if (nominal_kind_out != NULL)
        *nominal_kind_out = NOMINAL_DECL_CLASS;
    if (parser == NULL || name == NULL)
        return false;

    for (size_t i = 0; i < parser->decl_hint_count; i++) {
        if (parser->decl_hint_names[i] != NULL
            && strcmp(parser->decl_hint_names[i], name) == 0) {
            if (node_type_out != NULL)
                *node_type_out = parser->decl_hint_types[i];
            if (nominal_kind_out != NULL)
                *nominal_kind_out = parser->decl_hint_nominal_kinds[i];
            return true;
        }
    }

    return false;
}

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
ASTNode* parser_parse_statement(Parser* parser) {
    parser_collect_doc_comments(parser);

    if (parser_check_within_context_block_start(parser)) {
        return parser_finalize_statement(parser,
            parser_parse_within_context_block(parser));
    }

    // async 함수 선언
    if (parser_match(parser, TOKEN_ASYNC)) {
        if (parser_check(parser, TOKEN_FUNC))
            return parser_finalize_statement(parser, parser_parse_async_function(parser));
        if (parser_check(parser, TOKEN_LBRACE))
            return parser_finalize_statement(parser, parser_parse_async_block(parser));
        parser_error(parser, "Expected 'func' or '{' after 'async'");
        return NULL;
    }

    // select 문
    if (parser_match(parser, TOKEN_SELECT)) {
        return parser_finalize_statement(parser, parser_parse_select_statement(parser));
    }

    // export 수식어 — declaration only
    if (parser_match(parser, TOKEN_EXPORT)) {
        return parser_parse_export_declaration(parser);
    }

    // top-level access modifier for ability and nominal declarations
    if (parser_check(parser, TOKEN_PUBLIC)
        || parser_check(parser, TOKEN_PRIVATE)) {
        AccessModifier access = ACCESS_PUBLIC;
        bool explicit_access = true;
        PgyTokenType access_tok = parser->current_token.type;
        ASTNode *node = NULL;

        parser_advance(parser);
        if (access_tok == TOKEN_PRIVATE)
            access = ACCESS_PRIVATE;

        if (parser_match(parser, TOKEN_INNATE)) {
            parser_consume(parser, TOKEN_ABILITY,
                "Expected 'ability' after 'innate'");
            node = parse_ability_declaration(parser, true);
        } else if (parser_match(parser, TOKEN_ABILITY)) {
            node = parse_ability_declaration(parser, false);
        } else if (parser_match(parser, TOKEN_SUBJECT)) {
            node = parse_subject_declaration(parser);
        } else if (parser_match(parser, TOKEN_CLASS)) {
            node = parse_class_declaration(parser);
        } else if (parser_match(parser, TOKEN_STRUCT)) {
            node = parse_struct_declaration(parser);
        } else if (parser_match(parser, TOKEN_OBJECT)) {
            node = parse_object_declaration(parser);
        } else if (parser_match(parser, TOKEN_TOBJECT)) {
            node = parse_tobject_declaration(parser);
        } else if (parser_match(parser, TOKEN_VESSEL)) {
            node = parse_vessel_declaration(parser);
        } else if (parser_match(parser, TOKEN_PARTY)) {
            node = parse_party_declaration(parser);
        } else if (parser_match(parser, TOKEN_ROSTER)) {
            node = parse_roster_declaration(parser);
        } else if (parser_match(parser, TOKEN_WORLD)) {
            node = parse_world_declaration(parser);
        } else if (parser_match(parser, TOKEN_RELATION)) {
            node = parse_relation_declaration(parser);
        } else if (parser_match(parser, TOKEN_EFFECT)) {
            node = parse_effect_declaration(parser);
        } else if (parser_match(parser, TOKEN_ZONE)) {
            node = parse_zone_declaration(parser);
        } else if (parser_match(parser, TOKEN_INTENT)) {
            node = parse_intent_declaration(parser);
        } else if (parser_match(parser, TOKEN_EVENT)) {
            node = parse_event_declaration(parser);
        } else {
            parser_error(parser,
                "Top-level access modifiers currently apply only to ability, nominal, or domain declarations");
            return NULL;
        }

        if (node != NULL && node->type == AST_ABILITY_DECL) {
            node->access = access;
            node->has_explicit_access = explicit_access;
            node->data.ability_decl.access = access;
            node->data.ability_decl.has_explicit_access = explicit_access;
            if (access == ACCESS_PRIVATE || access == ACCESS_PROTECTED)
                node->is_exported = false;
            else
                node->is_exported = true;
        } else if (node != NULL && node->type == AST_CLASS_DECL) {
            node->access = access;
            node->has_explicit_access = explicit_access;
            if (access == ACCESS_PRIVATE || access == ACCESS_PROTECTED)
                node->is_exported = false;
            else
                node->is_exported = true;
        } else if (node != NULL
                   && (node->type == AST_PARTY_DECL
                       || node->type == AST_ROSTER_DECL
                       || node->type == AST_WORLD_DECL
                       || node->type == AST_RELATION_DECL
                       || node->type == AST_EFFECT_DECL
                       || node->type == AST_ZONE_DECL
                       || node->type == AST_INTENT_DECL
                       || node->type == AST_EVENT_DECL)) {
            node->access = access;
            node->has_explicit_access = explicit_access;
            if (access == ACCESS_PRIVATE || access == ACCESS_PROTECTED)
                node->is_exported = false;
            else
                node->is_exported = true;
        }
        return parser_finalize_statement(parser, node);
    }

    // 함수 선언
    if (parser_match(parser, TOKEN_FUNC)) {
        return parser_finalize_statement(parser, parse_function_declaration(parser));
    }

    // use 선언 (standard library)
    if (parser_match(parser, TOKEN_USE)) {
        Token mod_name = parser_consume(parser, TOKEN_IDENTIFIER,
            "Expected module name after 'use'");
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after use");
        ASTNode *use_node = calloc(1, sizeof(ASTNode));
        use_node->type = AST_USE_DECL;
        use_node->line = mod_name.line;
        use_node->column = mod_name.column;
        use_node->data.use_decl.module_name = pergyra_strndup(mod_name.text, mod_name.length);
        return parser_finalize_statement(parser, use_node);
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
        return parser_finalize_statement(parser, imp);
    }

    // namespace 선언
    if (parser_match(parser, TOKEN_NAMESPACE)) {
        Token name_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected namespace name");
        ASTNode *ns = ast_create_namespace_declaration(name_tok.text);
        parser_consume(parser, TOKEN_LBRACE, "Expected '{' after namespace name");
        while (!parser_check(parser, TOKEN_RBRACE) && !parser_is_at_end(parser)) {
            ASTNode *stmt = parser_parse_statement(parser);
            if (stmt != NULL)
                ast_add_statement(ns, stmt);
            if (parser->has_error)
                parser_synchronize(parser);
        }
        parser_consume(parser, TOKEN_RBRACE, "Expected '}' after namespace body");
        return parser_finalize_statement(parser, ns);
    }

    // extern 블록
    if (parser_match(parser, TOKEN_EXTERN)) {
        return parser_finalize_statement(parser, parse_extern_block(parser));
    }

    if (parser_match(parser, PGY_TOKEN_TYPE)) {
        return parser_finalize_statement(parser, parse_type_alias_declaration(parser));
    }

    // 클래스 / subject 선언
    if (parser_starts_named_declaration(parser, TOKEN_SUBJECT)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_subject_declaration(parser));
    }

    if (parser_starts_named_declaration(parser, TOKEN_CLASS)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_class_declaration(parser));
    }

    // object declaration: declaration-context keyword for local/internal projection
    if (parser_starts_named_declaration(parser, TOKEN_OBJECT)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_object_declaration(parser));
    }

    if (parser_starts_named_declaration(parser, TOKEN_VESSEL)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_vessel_declaration(parser));
    }

    if (parser_starts_named_declaration(parser, TOKEN_INTENT)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_intent_declaration(parser));
    }

    // tobject / struct declarations share a token family today, but not a language contract
    if (parser_starts_named_declaration(parser, TOKEN_TOBJECT)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_tobject_declaration(parser));
    }

    if (parser_starts_named_declaration(parser, TOKEN_STRUCT)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_struct_declaration(parser));
    }

    // let 선언
    if (parser_match(parser, TOKEN_LET)) {
        return parser_finalize_statement(parser, parser_parse_let_declaration(parser));
    }

    // := 단축 선언: name := expr
    // Consume identifier speculatively, check for :=, rewind if not
    if (parser_check(parser, TOKEN_IDENTIFIER)) {
        Token saved = parser->current_token;
        Token saved_prev = parser->previous_token;
        const char *lx_saved = parser->lexer->current;
        size_t lx_pos = parser->lexer->position;
        uint32_t lx_line = parser->lexer->line;
        uint32_t lx_col = parser->lexer->column;

        parser_advance(parser);  // consume identifier
        if (parser_check(parser, TOKEN_COLON_ASSIGN)) {
            parser_advance(parser);  // consume :=
            ASTNode *init = parser_parse_expression(parser);
            parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after := declaration");
            ASTNode *let_node = ast_create_let_declaration(saved.text);
            let_node->data.let_decl.initializer = init;
            return let_node;
        }
        // Rewind — not a := declaration
        parser->current_token = saved;
        parser->previous_token = saved_prev;
        parser->lexer->current = lx_saved;
        parser->lexer->position = lx_pos;
        parser->lexer->line = lx_line;
        parser->lexer->column = lx_col;
    }

    /* labeled loop: label: while ... / label: for ... */
    if (parser_check(parser, TOKEN_IDENTIFIER)) {
        Token saved = parser->current_token;
        Token saved_prev = parser->previous_token;
        const char *lx_saved = parser->lexer->current;
        size_t lx_pos = parser->lexer->position;
        uint32_t lx_line = parser->lexer->line;
        uint32_t lx_col = parser->lexer->column;
        Token next = parser_peek_next(parser);

        if (next.type == TOKEN_COLON) {
            parser_advance(parser);
            parser_consume(parser, TOKEN_COLON, "Expected ':' after loop label");
            if (parser_match(parser, TOKEN_FOR)) {
                ASTNode *loop = parse_for_loop(parser);
                loop->data.for_loop.label = pergyra_strdup(saved.text);
                return parser_finalize_statement(parser, loop);
            }
            if (parser_match(parser, TOKEN_WHILE)) {
                ASTNode *loop = parse_while_statement(parser);
                loop->data.while_loop.label = pergyra_strdup(saved.text);
                return parser_finalize_statement(parser, loop);
            }

            parser->current_token = saved;
            parser->previous_token = saved_prev;
            parser->lexer->current = lx_saved;
            parser->lexer->position = lx_pos;
            parser->lexer->line = lx_line;
            parser->lexer->column = lx_col;
        }
    }

    // using <expr> as <alias>;
    // Surface-compression alias statement. Lower directly to immutable let.
    if (parser_check(parser, TOKEN_IDENTIFIER)
        && parser->current_token.text != NULL
        && strcmp(parser->current_token.text, "using") == 0) {
        parser_advance(parser);  // consume contextual 'using'
        ASTNode *aliased_expr = parser_parse_expression(parser);
        parser_consume(parser, TOKEN_AS, "Expected 'as' after aliased expression");
        Token alias_name = consume_binding_name_token(parser,
            "Expected alias name after 'as'");
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after using alias");

        ASTNode *alias_decl = ast_create_let_declaration(alias_name.text);
        alias_decl->line = alias_name.line;
        alias_decl->column = alias_name.column;
        alias_decl->data.let_decl.initializer = aliased_expr;
        alias_decl->data.let_decl.is_alias = true;
        return parser_finalize_statement(parser, alias_decl);
    }

    if (parser_check_pin_block_start(parser))
        return parser_finalize_statement(parser, parser_parse_pin_block(parser));

    // with 문
    if (parser_match(parser, TOKEN_WITH)) {
        return parser_finalize_statement(parser, parser_parse_with_statement(parser));
    }

    // parallel 블록
    if (parser_match(parser, TOKEN_PARALLEL)) {
        return parser_finalize_statement(parser, parser_parse_parallel_block(parser));
    }

    // for 루프
    if (parser_match(parser, TOKEN_FOR)) {
        return parser_finalize_statement(parser, parse_for_loop(parser));
    }

    // while 루프
    if (parser_match(parser, TOKEN_WHILE)) {
        return parser_finalize_statement(parser, parse_while_statement(parser));
    }

    // match 문
    if (parser_match(parser, TOKEN_MATCH)) {
        return parser_finalize_statement(parser, parse_match_statement(parser));
    }

    // if 문
    if (parser_match(parser, TOKEN_IF)) {
        return parser_finalize_statement(parser, parse_if_statement(parser));
    }

    // return 문
    if (parser_match(parser, TOKEN_RETURN)) {
        return parser_finalize_statement(parser, parse_return_statement(parser));
    }

    // break
    if (parser_match(parser, TOKEN_BREAK)) {
        char *label = NULL;
        if (parser_check(parser, TOKEN_IDENTIFIER)) {
            Token label_tok = parser_advance(parser);
            label = pergyra_strndup(label_tok.text, label_tok.length);
        }
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after break");
        ASTNode *node = calloc(1, sizeof(ASTNode));
        node->type = AST_BREAK;
        node->line = parser->previous_token.line;
        node->data.break_stmt.label = label;
        return parser_finalize_statement(parser, node);
    }

    // continue
    if (parser_match(parser, TOKEN_CONTINUE)) {
        char *label = NULL;
        if (parser_check(parser, TOKEN_IDENTIFIER)) {
            Token label_tok = parser_advance(parser);
            label = pergyra_strndup(label_tok.text, label_tok.length);
        }
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after continue");
        ASTNode *node = calloc(1, sizeof(ASTNode));
        node->type = AST_CONTINUE;
        node->line = parser->previous_token.line;
        node->data.continue_stmt.label = label;
        return parser_finalize_statement(parser, node);
    }

    // enum 선언
    if (parser_match(parser, TOKEN_ENUM)) {
        return parser_finalize_statement(parser,
            parser_parse_enum_declaration_after_keyword(parser));
    }

    // unsafe 블록
    if (parser_match(parser, TOKEN_UNSAFE)) {
        return parser_finalize_statement(parser, parse_unsafe_block(parser));
    }

    // defer 문
    if (parser_match(parser, TOKEN_DEFER)) {
        return parser_finalize_statement(parser, parse_defer_statement(parser));
    }

    // bind party.slot = Role;
    if (parser_match(parser, TOKEN_BIND)) {
        Token party_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected party variable after 'bind'");
        parser_consume(parser, TOKEN_DOT, "Expected '.' after party variable");
        Token slot_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected slot name after '.'");
        parser_consume(parser, TOKEN_ASSIGN, "Expected '=' after slot name");
        Token role_tok = parser_consume(parser, TOKEN_IDENTIFIER, "Expected role name after '='");
        parser_consume(parser, TOKEN_SEMICOLON, "Expected ';' after bind statement");
        return parser_finalize_statement(parser,
            ast_create_bind_statement(party_tok.text, slot_tok.text, role_tok.text));
    }

    // roster declaration
    if (parser_starts_named_declaration(parser, TOKEN_ROSTER)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_roster_declaration(parser));
    }

    // world 선언
    if (parser_starts_named_declaration(parser, TOKEN_WORLD)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_world_declaration(parser));
    }

    // relation 선언
    if (parser_starts_named_declaration(parser, TOKEN_RELATION)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_relation_declaration(parser));
    }

    // effect 선언
    if (parser_starts_named_declaration(parser, TOKEN_EFFECT)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_effect_declaration(parser));
    }

    // zone 선언
    if (parser_starts_named_declaration(parser, TOKEN_ZONE)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_zone_declaration(parser));
    }

    // party 선언
    if (parser_match(parser, TOKEN_PARTY)) {
        return parser_finalize_statement(parser, parse_party_declaration(parser));
    }

    if (parser_match(parser, TOKEN_INNATE)) {
        parser_consume(parser, TOKEN_ABILITY,
            "Expected 'ability' after 'innate'");
        return parser_finalize_statement(parser,
            parse_ability_declaration(parser, true));
    }

    // ability 선언
    if (parser_match(parser, TOKEN_ABILITY)) {
        return parser_finalize_statement(parser,
            parse_ability_declaration(parser, false));
    }

    // role 선언
    if (parser_match(parser, TOKEN_ROLE)) {
        return parser_finalize_statement(parser, parse_role_declaration(parser));
    }

    // event 선언
    if (parser_starts_named_declaration(parser, TOKEN_EVENT)) {
        parser_advance(parser);
        return parser_finalize_statement(parser, parse_event_declaration(parser));
    }

    // 표현식 문장
    return parser_finalize_statement(parser, parser_parse_expression_statement(parser));
}

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
