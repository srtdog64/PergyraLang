    TEST("role impl rejects generic ability reference that violates ability where bound");
    {
        const char *source =
            "ability Bufferable<T> where T: Int {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Bufferable<String> {\n"
            "        func Put(value: String) -> Void { return; }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "role 'HeroRole' uses ability 'Bufferable' with generic argument 'String'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy bound 'Int'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("party role slot rejects generic ability reference that violates ability where bound");
    {
        const char *source =
            "ability Bufferable<T> where T: Int {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n"
            "subject Hero { let hp: Int; }\n"
            "party RaidTeam {\n"
            "    role slot fighter: Bufferable<String>\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "party role slot 'fighter' uses ability 'Bufferable' with generic argument 'String'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy bound 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual type args are 'String'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority rejects generic ability reference that violates ability where bound");
    {
        const char *source =
            "ability Bufferable<T> where T: Int {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Bufferable<String> {\n"
            "        func Put(value: String) -> Void { return; }\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero requires Bufferable<String>\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Zone authority 'hero' uses ability 'Bufferable' with generic argument 'String'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy bound 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual type args are 'String'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step requires reports actual implementation provenance");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { action Act(self) -> Void { return; } }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<String> { func Ping() -> Void { return; } }\n"
            "}\n"
            "intent Battle(hero: Hero) {\n"
            "    step Attack {\n"
            "        who: hero;\n"
            "        requires: Combatable<Int>;\n"
            "        on: hero.Act();\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Intent step 'Attack' requires ability 'Combatable<Int>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "implements 'Combatable<String>' instead"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual implementation is 'Combatable<String>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("foreign ability is visible by default without explicit export");
    {
        const char *module_path = "test_default_export_ability_module_a.pgy";
        const char *main_path = "test_default_export_ability_module_main_a.pgy";
        const char *module_source =
            "ability SharedAbility {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_default_export_ability_module_a.pgy\";\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability SharedAbility {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
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

    TEST("foreign generic ability is visible by default without explicit export");
    {
        const char *module_path = "test_default_export_generic_ability_module_a.pgy";
        const char *main_path = "test_default_export_generic_ability_module_main_a.pgy";
        const char *module_source =
            "ability SharedAbility<T> {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_default_export_generic_ability_module_a.pgy\";\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability SharedAbility<Int> {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "subject Boss {\n"
            "    action Do(self) -> Void requires SharedAbility<Int> { return; }\n"
            "}\n"
            "role BossRole for Boss {\n"
            "    impl ability SharedAbility<Int> {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
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

    TEST("private generic ability stays hidden across modules");
    {
        const char *module_path = "test_private_generic_ability_module_a.pgy";
        const char *main_path = "test_private_generic_ability_module_main_a.pgy";
        const char *module_source =
            "private ability HiddenAbility<T> {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_private_generic_ability_module_a.pgy\";\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability HiddenAbility<Int> {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "subject Boss {\n"
            "    action Do(self) -> Void requires HiddenAbility<Int> { return; }\n"
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
            "non-exported ability 'HiddenAbility'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("zone authority generic ability stays hidden across modules");
    {
        const char *module_path = "test_private_generic_zone_authority_module_a.pgy";
        const char *main_path = "test_private_generic_zone_authority_module_main_a.pgy";
        const char *module_source =
            "private ability HiddenAbility<T> {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_private_generic_zone_authority_module_a.pgy\";\n"
            "subject Hero { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero requires HiddenAbility<Int>\n"
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
            "non-exported ability 'HiddenAbility'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("party role slot generic ability stays hidden across modules");
    {
        const char *module_path = "test_private_generic_party_role_slot_module_a.pgy";
        const char *main_path = "test_private_generic_party_role_slot_module_main_a.pgy";
        const char *module_source =
            "private ability HiddenAbility<T> {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_private_generic_party_role_slot_module_a.pgy\";\n"
            "party RaidTeam {\n"
            "    role slot fighter: HiddenAbility<Int>\n"
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
            "non-exported ability 'HiddenAbility'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("zone bind group expands into multiple derived projection sync entries");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind [playerView, snapshot] from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *zone = NULL;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        if (program != NULL) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_ZONE_DECL
                    && stmt->data.zone_decl.name != NULL
                    && strcmp(stmt->data.zone_decl.name, "BattleZone") == 0) {
                    zone = stmt;
                    break;
                }
            }
        }
        EXPECT(zone != NULL);
        EXPECT(zone != NULL && zone->data.zone_decl.refresh_count == 2);
        if (zone != NULL && zone->data.zone_decl.refresh_count == 2) {
            ASTNode *first = zone->data.zone_decl.refreshes[0];
            ASTNode *second = zone->data.zone_decl.refreshes[1];
            EXPECT(first != NULL
                && first->data.zone_refresh.derive_target_kind
                && strcmp(first->data.zone_refresh.object_slot_name, "playerView") == 0
                && strcmp(first->data.zone_refresh.source_slot_name, "player") == 0);
            EXPECT(second != NULL
                && second->data.zone_refresh.derive_target_kind
                && strcmp(second->data.zone_refresh.object_slot_name, "snapshot") == 0
                && strcmp(second->data.zone_refresh.source_slot_name, "player") == 0);
        }

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
