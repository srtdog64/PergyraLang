    TEST("lambda call propagates lambda body summary");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *func = ast_create_function("CallLambda");
        ASTNode *lambda = ast_create_lambda_expression();
        ASTNode *decl = ast_create_let_declaration("f");
        ASTNode *pending;
        ASTNode *ret;

        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();

        lambda->data.lambda_expr.return_type = ast_create_type("Void");
        lambda->data.lambda_expr.body = ast_create_block();
        pending = ast_create_let_declaration("pending");
        pending->data.let_decl.initializer =
            ast_create_spawn_expression(make_number(9, 2));
        ast_add_statement(lambda->data.lambda_expr.body, pending);
        ret = ast_create_return_statement();
        ast_add_statement(lambda->data.lambda_expr.body, ret);

        decl->data.let_decl.initializer = lambda;
        ast_add_statement(func->data.func_decl.body, decl);
        ast_add_statement(func->data.func_decl.body,
            make_call("f", NULL, 0, 4));

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "CallLambda");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        uint32_t body_summary = sym != NULL && sym->type != NULL
            ? type_function_body_summary(sym->type) : BODY_SUMMARY_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && (body_summary & BODY_SUMMARY_EFFECTS) != 0
            && (body_summary & BODY_SUMMARY_SPAWNS_TASK) != 0
            && (body_summary & BODY_SUMMARY_MAY_RETURN) == 0);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("structured comment @effects merges declared effect into function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("PlanRemote");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.doc_comment =
            make_effect_doc_comment("remote secure");

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 2);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("source-level /// @effects flows from parser into semantic effects");
    {
        const char *source =
            "/// @effects remote secure\n"
            "func PlanRemote() -> Int {\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        EXPECT(!parser_has_error(parser));
        type_check_program(program, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("source-level with effects clause flows from parser into semantic effects");
    {
        const char *source =
            "func PlanRemote() -> Int with effects remote, secure {\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        EXPECT(!parser_has_error(parser));
        type_check_program(program, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("declared effects must cover derived body effects");
    {
        const char *source =
            "/// @effects local\n"
            "func Dispatch() -> Int {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "missing declared effects: remote"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("signature effects must cover derived body effects");
    {
        const char *source =
            "func Dispatch() -> Int with effects local {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "missing declared effects: remote"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("signature effects may exactly match derived body effects");
    {
        const char *source =
            "func Dispatch() -> Int with effects remote {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("declared effects may exactly match derived body effects");
    {
        const char *source =
            "/// @effects remote\n"
            "func Dispatch() -> Int {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect-lattice: collapse contract subsumes nondeterministic body effect");
    {
        const char *source =
            "func Observe() -> Int with effects collapse {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    return Measure(q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect-flow: if branch effect joins into function contract");
    {
        const char *source =
            "func Dispatch(flag: Bool) -> Int with effects remote {\n"
            "    if flag {\n"
            "        let pending = spawn 42;\n"
            "    }\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect-flow: match case effect joins into function contract");
    {
        const char *source =
            "func Dispatch(flag: Bool) -> Int with effects remote {\n"
            "    match flag {\n"
            "        case true:\n"
            "            let pending = spawn 42;\n"
            "            return 1;\n"
            "        default:\n"
            "            return 2;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect-flow: while body effect joins into function contract");
    {
        const char *source =
            "func Dispatch(flag: Bool) -> Int with effects remote {\n"
            "    while flag {\n"
            "        let pending = spawn 42;\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect-flow: for body effect joins into function contract");
    {
        const char *source =
            "func Dispatch() -> Int with effects remote {\n"
            "    for i in 0..3 {\n"
            "        let pending = spawn i;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect-partial-order: disjoint branch effects join into combined contract");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Dispatch(flag: Bool) -> Int with effects remote, secure {\n"
            "    if flag {\n"
            "        let s: SecureSlot<Bot> = Bot(1);\n"
            "    } else {\n"
            "        let pending = spawn 42;\n"
            "    }\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect-conflict: secure and remote combination emits warning");
    {
        const char *source =
            "/// @effects secure, remote\n"
            "func Mixed() -> Void {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "combines effect classes that are currently treated as conflicting"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect-conflict: secure and collapse combination emits warning");
    {
        const char *source =
            "/// @effects secure, collapse\n"
            "func MixedCollapse() -> Void {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "combines effect classes that are currently treated as conflicting"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect-conflict: non-adjacent branch combination still emits warning");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Mixed(flag: Int) -> Void with effects secure, remote {\n"
            "    match flag {\n"
            "        case 0:\n"
            "            let s: SecureSlot<Bot> = Bot(1);\n"
            "            return;\n"
            "        case 1:\n"
            "            return;\n"
            "        default:\n"
            "            let pending = spawn 42;\n"
            "            return;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "combines effect classes that are currently treated as conflicting"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
