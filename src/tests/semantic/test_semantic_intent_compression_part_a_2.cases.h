    TEST("intent step keeps parameter authority explicit across multiple on calls");
    {
        const char *source =
            "subject Hero {\n"
            "    let mut hp: Int;\n"
            "    action Protect(self, healer: Healer) -> Void\n"
            "        within BattleZone\n"
            "        authorized by healer\n"
            "        causes Protected {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "subject Healer { let level: Int; }\n"
            "effect Protected for bearer: Hero {\n"
            "    subject slot bearer: Hero\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    subject slot healer: Healer\n"
            "    effect slot protected: Protected\n"
            "    authority healer\n"
            "}\n"
            "intent Rescue(battle: BattleZone, hero: Hero, healer: Healer) {\n"
            "    step Verify {\n"
            "        on: hero.Protect(healer);\n"
            "        on: hero.Protect(healer);\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(step != NULL && step->data.intent_step.derived_who_from_on_receiver);
        EXPECT(step != NULL && step->data.intent_step.inherited_where_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_causes_from_action);
        EXPECT(step != NULL && !step->data.intent_step.inherited_authorized_by_from_action);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step keeps parameter authority explicit for expression on argument");
    {
        const char *source =
            "subject Hero {\n"
            "    let mut hp: Int;\n"
            "    action Protect(self, healer: Healer) -> Void\n"
            "        within BattleZone\n"
            "        authorized by healer\n"
            "        causes Protected {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "subject Healer { let level: Int; }\n"
            "effect Protected for bearer: Hero {\n"
            "    subject slot bearer: Hero\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    subject slot healer: Healer\n"
            "    effect slot protected: Protected\n"
            "    authority healer\n"
            "}\n"
            "intent Rescue(battle: BattleZone, hero: Hero, healer: Healer) {\n"
            "    step Verify {\n"
            "        on: hero.Protect(42);\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(step != NULL && step->data.intent_step.derived_who_from_on_receiver);
        EXPECT(step != NULL && step->data.intent_step.inherited_where_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_causes_from_action);
        EXPECT(step != NULL && !step->data.intent_step.inherited_authorized_by_from_action);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step rejects conflicting on-call action zones without explicit where");
    {
        const char *source =
            "subject Player {\n"
            "    let mut hp: Int;\n"
            "    action Guard(self) -> Void within Arena authorized by self {\n"
            "        return;\n"
            "    }\n"
            "    action Rest(self) -> Void within Camp authorized by self {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "zone Arena {\n"
            "    subject slot hero: Player\n"
            "    authority hero\n"
            "}\n"
            "zone Camp {\n"
            "    subject slot hero: Player\n"
            "    authority hero\n"
            "}\n"
            "intent Patrol(arena: Arena, camp: Camp, hero: Player) {\n"
            "    step Verify {\n"
            "        on: hero.Guard();\n"
            "        on: hero.Rest();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(result,
            "cannot infer a where zone from on-call actions"));
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(result,
            "both 'Arena' and 'Camp'"));
        EXPECT(step != NULL && step->data.intent_step.derived_who_from_on_receiver);
        EXPECT(step != NULL && !step->data.intent_step.inherited_where_from_action);
        EXPECT(step != NULL && step->data.intent_step.where_type == NULL);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step rejects ambiguous on-call receivers without explicit who");
    {
        const char *source =
            "subject Player {\n"
            "    let mut hp: Int;\n"
            "    action Guard(self) -> Void within Arena authorized by self {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "zone Arena {\n"
            "    subject slot hero: Player\n"
            "    subject slot rival: Player\n"
            "    authority hero\n"
            "}\n"
            "intent Patrol(arena: Arena, hero: Player, rival: Player) {\n"
            "    step Guard {\n"
            "        where: Arena;\n"
            "        using: arena;\n"
            "        on: hero.Guard();\n"
            "        on: rival.Guard();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(step != NULL && !step->data.intent_step.derived_who_from_on_receiver);
        EXPECT(step != NULL && step->data.intent_step.who_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step who alone does not satisfy zone authorization");
    {
        const char *source =
            "subject Driver {\n"
            "    let mut hp: Int;\n"
            "    action Ignite(self) -> Void {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "object DriverView { let hp: Int; }\n"
            "effect Started for bearer: Driver { }\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "    object slot dashboard: DriverView = DriverView(0)\n"
            "    bind dashboard from driver by driver\n"
            "}\n"
            "intent SyncDrive(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        causes: Started;\n"
            "        on: driver.Ignite();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(step != NULL && step->data.intent_step.who_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.who_names[0],
                                      "driver") == 0);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 0);
        EXPECT(step != NULL && !step->data.intent_step.derived_authorized_by_from_zone);
        EXPECT(step != NULL && !step->data.intent_step.inherited_authorized_by_from_action);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step keeps explicit who separate from explicit authorization");
    {
        const char *source =
            "subject Driver {\n"
            "    let mut hp: Int;\n"
            "    action Ignite(self) -> Void {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "object DriverView { let hp: Int; }\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "    object slot dashboard: DriverView = DriverView(0)\n"
            "    bind dashboard from driver by driver\n"
            "}\n"
            "intent SyncDrive(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        authorized by: driver;\n"
            "        on: driver.Ignite();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 0);
        EXPECT(step != NULL && !step->data.intent_step.derived_who_from_on_receiver);
        EXPECT(step != NULL && step->data.intent_step.who_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.who_names[0],
                                      "driver") == 0);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.authorized_by[0],
                                      "driver") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
