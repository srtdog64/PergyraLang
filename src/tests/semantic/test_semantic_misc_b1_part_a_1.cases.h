    TEST("lexical zone context injects within zone for enclosed subject actions");
    {
        const char *source =
            "within BattleZone {\n"
            "    subject Hero {\n"
            "        let hp: Int;\n"
            "        action Guard(self) authorized by self {\n"
            "            return;\n"
            "        }\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *subject_decl = program != NULL && program->type == AST_PROGRAM
            && program->data.program.count > 0 ? program->data.program.statements[0] : NULL;
        ASTNode *method = subject_decl != NULL && subject_decl->type == AST_CLASS_DECL
            && subject_decl->data.class_decl.method_count > 0
            ? subject_decl->data.class_decl.methods[0] : NULL;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(method != NULL
            && method->data.func_decl.within_zone != NULL
            && strcmp(method->data.func_decl.within_zone, "BattleZone") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("explicit cross-module private field access is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        ASTNode *vault = ast_create_subject("Vault");
        ASTNode *access = ast_create_member_access(make_identifier("v", 2), "code");
        ClassField *field = calloc(1, sizeof(ClassField));
        Symbol *vsym;

        vault->origin_path = pergyra_strdup("vault_module.pgy");
        field->name = pergyra_strdup("code");
        field->type = ast_create_type("Int");
        field->access = ACCESS_PRIVATE;
        field->has_explicit_access = true;
        vault->data.class_decl.fields = calloc(1, sizeof(ClassField *));
        vault->data.class_decl.fields[0] = field;
        vault->data.class_decl.field_count = 1;
        ast_add_statement(program, vault);
        ctx->program_root = program;

        type_check_class_decl(vault, ctx);
        ctx->current_module_path = "consumer_module.pgy";
        vsym = symbol_create_variable("v",
            scope_lookup(ctx->scope, "Vault")->type, 2, 1);
        scope_declare(ctx->scope, vsym);

        type_check_expression(access, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "not accessible across the current visibility boundary"));

        semantic_context_destroy(ctx);
        ast_destroy(access);
        ast_destroy(program);
    }

    TEST("explicit cross-module public field access remains allowed");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        ASTNode *vault = ast_create_subject("Vault");
        ASTNode *access = ast_create_member_access(make_identifier("v", 2), "code");
        ClassField *field = calloc(1, sizeof(ClassField));
        Symbol *vsym;

        vault->origin_path = pergyra_strdup("vault_module.pgy");
        field->name = pergyra_strdup("code");
        field->type = ast_create_type("Int");
        field->access = ACCESS_PUBLIC;
        field->has_explicit_access = true;
        vault->data.class_decl.fields = calloc(1, sizeof(ClassField *));
        vault->data.class_decl.fields[0] = field;
        vault->data.class_decl.field_count = 1;
        ast_add_statement(program, vault);
        ctx->program_root = program;

        type_check_class_decl(vault, ctx);
        ctx->current_module_path = "consumer_module.pgy";
        vsym = symbol_create_variable("v",
            scope_lookup(ctx->scope, "Vault")->type, 2, 1);
        scope_declare(ctx->scope, vsym);

        EXPECT(type_check_expression(access, ctx) == TYPE_INT);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(access);
        ast_destroy(program);
    }

    TEST("private top-level subject stays hidden across modules");
    {
        const char *module_path = "test_private_subject_module_a.pgy";
        const char *main_path = "test_private_subject_module_main_a.pgy";
        const char *module_source =
            "private subject Vault {\n"
            "    let code: Int;\n"
            "}\n";
        const char *main_source =
            "import \"test_private_subject_module_a.pgy\";\n"
            "func Main() -> Int {\n"
            "    let v = Vault(7);\n"
            "    return 0;\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Constructor 'Vault' is not accessible across the current visibility boundary"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("public top-level subject remains visible across modules");
    {
        const char *module_path = "test_public_subject_module_a.pgy";
        const char *main_path = "test_public_subject_module_main_a.pgy";
        const char *module_source =
            "public subject Vault {\n"
            "    let code: Int;\n"
            "}\n";
        const char *main_source =
            "import \"test_public_subject_module_a.pgy\";\n"
            "func Main() -> Int {\n"
            "    let v = Vault(7);\n"
            "    return v.code;\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("private top-level zone stays hidden across modules");
    {
        const char *module_path = "test_private_zone_module_a.pgy";
        const char *main_path = "test_private_zone_module_main_a.pgy";
        const char *module_source =
            "private zone VaultZone {\n"
            "    subject slot owner: Keeper;\n"
            "}\n"
            "subject Keeper { let id: Int; }\n";
        const char *main_source =
            "import \"test_private_zone_module_a.pgy\";\n"
            "func Main() -> Void {\n"
            "    let z = VaultZone(Keeper(1));\n"
            "    return;\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Constructor 'VaultZone' is not accessible across the current visibility boundary"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("private zone cannot leak through imported action contract");
    {
        const char *module_path = "test_private_zone_action_module_a.pgy";
        const char *main_path = "test_private_zone_action_module_main_a.pgy";
        const char *module_source =
            "private zone HiddenZone {\n"
            "    subject slot hero: Hero;\n"
            "}\n"
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Patrol(self) -> Void within HiddenZone {\n"
            "        return;\n"
            "    }\n"
            "}\n";
        const char *main_source =
            "import \"test_private_zone_action_module_a.pgy\";\n"
            "func Main() -> Void { return; }\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot reference non-exported zone 'HiddenZone'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("private effect cannot leak through imported action contract");
    {
        const char *module_path = "test_private_effect_action_module_a.pgy";
        const char *main_path = "test_private_effect_action_module_main_a.pgy";
        const char *module_source =
            "private effect HiddenEffect {\n"
            "    for target: Hero\n"
            "}\n"
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Trigger(self) -> Void causes HiddenEffect {\n"
            "        return;\n"
            "    }\n"
            "}\n";
        const char *main_source =
            "import \"test_private_effect_action_module_a.pgy\";\n"
            "func Main() -> Void { return; }\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot reference non-exported effect 'HiddenEffect'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("private intent stays hidden across modules");
    {
        const char *module_path = "test_private_intent_module_a.pgy";
        const char *main_path = "test_private_intent_module_main_a.pgy";
        const char *module_source =
            "private intent HiddenIntent(value: Int) {\n"
            "    step Check {\n"
            "        on: value > 0;\n"
            "    }\n"
            "}\n";
        const char *main_source =
            "import \"test_private_intent_module_a.pgy\";\n"
            "func Main() -> Bool {\n"
            "    return HiddenIntent(1);\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Callable 'HiddenIntent' is not accessible across the current visibility boundary"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("private event stays hidden across modules");
    {
        const char *module_path = "test_private_event_module_a.pgy";
        const char *main_path = "test_private_event_module_main_a.pgy";
        const char *module_source =
            "private event HiddenPing(value: Int);\n";
        const char *main_source =
            "import \"test_private_event_module_a.pgy\";\n"
            "func Main() -> Void {\n"
            "    HiddenPing(1);\n"
            "    return;\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Callable 'HiddenPing' is not accessible across the current visibility boundary"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("own Slot<subject-host> parameter is allowed in closed subset");
    {
        const char *source =
            "subject Session { let id: Int; }\n"
            "func Consume(own s: Slot<Session>) -> Void {\n"
            "    Release(s);\n"
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

    TEST("ref SecureSlot<subject-host> parameter is accepted as explicit anchored-handle borrow boundary");
    {
        const char *source =
            "subject Session { let id: Int; }\n"
            "func Inspect(ref s: SecureSlot<Session>) -> Void {\n"
            "    return;\n"
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
