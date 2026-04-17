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

    TEST("ref SecureSlot<subject-host> parameter is rejected outside closed subset");
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
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "SecureSlot<subject-host> parameters currently support only 'own'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("explicit cross-module private method access is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        ASTNode *vault = ast_create_subject("Vault");
        ASTNode *call = make_call_expr(
            make_member_access(make_identifier("v", 2), "Reveal", 2),
            NULL, 0, 2);
        ASTNode *method = ast_create_function("Reveal");
        Symbol *vsym;

        vault->origin_path = pergyra_strdup("vault_module.pgy");
        method->data.func_decl.return_type = ast_create_type("Int");
        method->data.func_decl.body = ast_create_block();
        method->data.func_decl.access = ACCESS_PRIVATE;
        method->data.func_decl.has_explicit_access = true;
        ast_add_statement(method->data.func_decl.body,
            ast_create_return_statement());
        method->data.func_decl.body->data.block.statements[0]
            ->data.return_stmt.value = make_number(7, 1);
        vault->data.class_decl.methods = calloc(1, sizeof(ASTNode *));
        vault->data.class_decl.methods[0] = method;
        vault->data.class_decl.method_count = 1;
        ast_add_statement(program, vault);
        ctx->program_root = program;

        type_check_class_decl(vault, ctx);
        ctx->current_module_path = "consumer_module.pgy";
        vsym = symbol_create_variable("v",
            scope_lookup(ctx->scope, "Vault")->type, 2, 1);
        scope_declare(ctx->scope, vsym);

        type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "not accessible across the current visibility boundary"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
        ast_destroy(program);
    }

    TEST("explicit cross-module public method access remains allowed");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        ASTNode *vault = ast_create_subject("Vault");
        ASTNode *call = make_call_expr(
            make_member_access(make_identifier("v", 2), "Reveal", 2),
            NULL, 0, 2);
        ASTNode *method = ast_create_function("Reveal");
        Symbol *vsym;

        vault->origin_path = pergyra_strdup("vault_module.pgy");
        method->data.func_decl.return_type = ast_create_type("Int");
        method->data.func_decl.body = ast_create_block();
        method->data.func_decl.access = ACCESS_PUBLIC;
        method->data.func_decl.has_explicit_access = true;
        ast_add_statement(method->data.func_decl.body,
            ast_create_return_statement());
        method->data.func_decl.body->data.block.statements[0]
            ->data.return_stmt.value = make_number(7, 1);
        vault->data.class_decl.methods = calloc(1, sizeof(ASTNode *));
        vault->data.class_decl.methods[0] = method;
        vault->data.class_decl.method_count = 1;
        ast_add_statement(program, vault);
        ctx->program_root = program;

        type_check_class_decl(vault, ctx);
        ctx->current_module_path = "consumer_module.pgy";
        vsym = symbol_create_variable("v",
            scope_lookup(ctx->scope, "Vault")->type, 2, 1);
        scope_declare(ctx->scope, vsym);

        EXPECT(type_check_expression(call, ctx) == TYPE_INT);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
        ast_destroy(program);
    }

    TEST("for-in over List<subject> type-checks");
    {
        const char *source =
            "subject Event {\n"
            "    let title: String;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let events: List<Event> = ListNew();\n"
            "    ListPush(events, Event(\"Kickoff\"));\n"
            "    for event in events {\n"
            "        Print(event.title);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("string interpolation type-checks embedded ints and member access");
    {
        const char *source =
            "subject Event {\n"
            "    let title: String;\n"
            "    let day: Int;\n"
            "}\n"
            "func Render(event: Event) -> String {\n"
            "    return \"Day ${event.day}: ${event.title}\";\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Some(42) returns Option<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("Some", args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("IsSome requires Option<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("IsSome", args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "IsSome requires Option<T>"));
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ToTObject returns tobject projection type but warns as boundary projection outside domain context");
    {
        const char *source =
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let snapshot: PlayerDto = ToTObject(PlayerDto, player);\n"
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

    TEST("ToTObject accepts subject source");
    {
        const char *source =
            "tobject BotDto { hp: Int; }\n"
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let bot: Bot = Bot();\n"
            "    let snapshot: BotDto = ToTObject(BotDto, bot);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "direct boundary projection outside relation/effect/zone/world context"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToTObject rejects missing fields, non-tobject targets, and unnamed sources");
    {
        const char *source =
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; }\n"
            "class PassivePlayer { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let passive: PassivePlayer = PassivePlayer();\n"
            "    let missing: PlayerDto = ToTObject(PlayerDto, player);\n"
            "    let wrong = ToTObject(Player, player);\n"
            "    let wrongView = ToTObject(PlayerView, player);\n"
            "    let anon = ToTObject(PlayerDto, Player());\n"
            "    let legacy = ToTObject(PlayerDto, passive);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 5);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToObject returns object projection type but warns as internal projection outside domain context");
    {
        const char *source =
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let view: PlayerView = ToObject(PlayerView, player);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "direct internal projection outside relation/effect/zone/world context"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("object and tobject declarations may carry passive helper funcs");
    {
        const char *source =
            "object PlayerView {\n"
            "    hp: Int;\n"
            "    func Label(self) -> Int {\n"
            "        return hp;\n"
            "    }\n"
            "}\n"
            "tobject PlayerDto {\n"
            "    hp: Int;\n"
            "    func Export(self) -> Int {\n"
            "        return hp;\n"
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

    TEST("vessel declarations and subject vessel fields are accepted");
    {
        const char *source =
            "vessel HealthState {\n"
            "    current: Int;\n"
            "    func IsDead(self) -> Bool {\n"
            "        return current <= 0;\n"
            "    }\n"
            "}\n"
            "subject Player {\n"
            "    vessel health: HealthState;\n"
            "    action TakeDamage(self, amount: Int) -> Void {\n"
            "        Log(amount);\n"
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

    TEST("private nominal member access is allowed inside the same host");
    {
        const char *source =
            "class Vault {\n"
            "    private let secret: Int;\n"
            "    private func Hidden(self) -> Int {\n"
            "        return self.secret;\n"
            "    }\n"
            "    public func Reveal(self) -> Int {\n"
            "        return self.Hidden();\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vault = Vault(7);\n"
            "    Log(v.Reveal());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        fprintf(stderr, "MISC-LATE action requires test: parser=%d result=%p errors=%zu warnings=%zu\n",
            parser_has_error(parser),
            (void *)result,
            result != NULL ? result->error_count : 0u,
            result != NULL ? result->warning_count : 0u);
        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("private nominal member access is rejected outside the host");
    {
        const char *source =
            "class Vault {\n"
            "    private let secret: Int;\n"
            "    private func Hidden(self) -> Int {\n"
            "        return self.secret;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vault = Vault(7);\n"
            "    Log(v.secret);\n"
            "    Log(v.Hidden());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Member 'Vault.secret' is not accessible across the current visibility boundary"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Member 'Vault.Hidden' is not accessible across the current visibility boundary"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject vessel fields reject non-vessel target types");
    {
        const char *source =
            "object PlayerView { hp: Int; }\n"
            "subject Player {\n"
            "    vessel view: PlayerView;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "must reference a vessel type"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject action authorized by requires subject-host parameters and matching zone authority");
    {
        const char *source =
            "subject Player {\n"
            "    action Trade(self, amount: Int) -> Void\n"
            "        within TradeZone\n"
            "        authorized by amount {\n"
            "        Log(1);\n"
            "    }\n"
            "}\n"
            "zone TradeZone {\n"
            "    subject slot buyer: Player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "must be a subject host"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject action causes effect and within zone enforce target compatibility");
    {
        const char *source =
            "subject Player {\n"
            "    action Attack(self) -> Void\n"
            "        within BattleZone\n"
            "        causes DamageEffect\n"
            "        authorized by self {\n"
            "        Log(1);\n"
            "    }\n"
            "}\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count >= 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "has no matching effect slot"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("secure subject action within zone requires explicit authorized by");
    {
        const char *source =
            "subject Player {\n"
            "    action TouchVault(self) -> Void\n"
            "        within BattleZone {\n"
            "        let stash: SecureSlot<Player> = self;\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "must declare 'authorized by'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("secure subject action within zone passes with explicit authorized by");
    {
        const char *source =
            "subject Player {\n"
            "    action TouchVault(self) -> Void\n"
            "        within BattleZone\n"
            "        authorized by self {\n"
            "        let stash: SecureSlot<Player> = self;\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player\n"
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

    TEST("subject declarations allow both func and action");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    func CalculatePower(self) -> Int {\n"
            "        return hp * 2;\n"
            "    }\n"
            "    action Tick(self) -> Void {\n"
            "        Log(1);\n"
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

    TEST("ToObject rejects non-object targets and non-subject sources");
    {
        const char *source =
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; }\n"
            "subject Player { let hp: Int; }\n"
            "class PassivePlayer { let hp: Int; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let passive: PassivePlayer = PassivePlayer();\n"
            "    let wrongDto = ToObject(PlayerDto, player);\n"
            "    let wrongSubject = ToObject(Player, player);\n"
            "    let legacy = ToObject(PlayerView, passive);\n"
            "    let anon = ToObject(PlayerView, Player());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 4);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasState works inside zone methods for declared states");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    func Tick() -> Void {\n"
            "        if HasState(poisoned) || HasState(\"allied\") || HasState(poisoned, player) || HasState(allied, player, enemy) {\n"
            "            Log(1);\n"
            "        }\n"
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

    TEST("HasState validates state slot arity and endpoint matching");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    func Tick() -> Void {\n"
            "        let a = HasState(poisoned, enemy);\n"
            "        let b = HasState(allied, player);\n"
            "        let c = HasState(allied, enemy, player);\n"
            "        Log(a || b || c);\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasState rejects unknown states and use outside zone");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    state poisoned: effect poison on player\n"
            "    effect slot poison: Poisoned\n"
            "    func Tick() -> Void {\n"
            "        if HasState(missing) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n"
            "func Main() -> Void {\n"
            "    let active = HasState(\"poisoned\");\n"
            "    Log(active);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "zone state contracts are scoped to a zone body"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "declare a zone state named 'missing'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasLayer works inside zone methods for declared relation and effect slots");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    func Tick() -> Void {\n"
            "        if HasLayer(trust) || HasLayer(\"poison\") {\n"
            "            Log(1);\n"
            "        }\n"
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

    TEST("HasLayer rejects unknown names and use outside zone");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    func Tick() -> Void {\n"
            "        if HasLayer(missing) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let active = HasLayer(\"poison\");\n"
            "    Log(active);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "relation/effect layer slots are zone-local"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "declare a relation/effect slot named 'missing'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect pool syntax is accepted for positive capacities");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect pool damage: DamageEffect capacity 8\n"
            "    apply damage to player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "no explicit authority set"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect pool rejects non-positive capacities");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect pool damage: DamageEffect capacity 0\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

