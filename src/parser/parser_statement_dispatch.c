/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser statement dispatch implementation.
 */

#include "parser_internal.h"

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

