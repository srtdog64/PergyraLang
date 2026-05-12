static void
test_intent_compression_semantics(void)
{
    printf("\n[intent_compression]\n");

    TEST("intent step derives who from single subject participant");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Marked for bearer: Player {\n"
            "    subject slot bearer: Player\n"
            "}\n"
            "zone Arena {\n"
            "    subject slot player: Player\n"
            "    effect slot marked: Marked\n"
            "    authority player\n"
            "}\n"
            "intent Charge(arena: Arena, player: Player) {\n"
            "    step Verify {\n"
            "        where: Arena;\n"
            "        using: arena;\n"
            "        causes: Marked;\n"
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
        EXPECT(step != NULL && step->data.intent_step.derived_who_from_single_participant);
        EXPECT(step != NULL && step->data.intent_step.who_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.who_names[0], "player") == 0);
        EXPECT(step != NULL && step->data.intent_step.derived_authorized_by_from_zone);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.authorized_by[0], "player") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step keeps who explicit when subject participant is ambiguous");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Marked for bearer: Player {\n"
            "    subject slot bearer: Player\n"
            "}\n"
            "zone Arena {\n"
            "    subject slot player: Player\n"
            "    subject slot rival: Player\n"
            "    effect slot marked: Marked\n"
            "    authority player\n"
            "}\n"
            "intent Charge(arena: Arena, player: Player, rival: Player) {\n"
            "    step Verify {\n"
            "        where: Arena;\n"
            "        using: arena;\n"
            "        causes: Marked;\n"
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
        EXPECT(step != NULL && !step->data.intent_step.derived_who_from_single_participant);
        EXPECT(step != NULL && step->data.intent_step.who_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step derives who from on-call receiver");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
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
        EXPECT(step != NULL && step->data.intent_step.derived_who_from_on_receiver);
        EXPECT(step != NULL && !step->data.intent_step.derived_who_from_single_participant);
        EXPECT(step != NULL && step->data.intent_step.who_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.who_names[0], "hero") == 0);
        EXPECT(step != NULL && step->data.intent_step.inherited_authorized_by_from_action);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.authorized_by[0], "hero") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step derives where and using from on-call action contract");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action Guard(self) -> Void within Arena authorized by self {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "zone Arena {\n"
            "    subject slot hero: Player\n"
            "    authority hero\n"
            "}\n"
            "intent Patrol(arena: Arena, hero: Player) {\n"
            "    step Verify {\n"
            "        on: hero.Guard();\n"
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
        EXPECT(step != NULL && step->data.intent_step.derived_who_from_on_receiver);
        EXPECT(step != NULL && step->data.intent_step.inherited_where_from_action);
        EXPECT(step != NULL && step->data.intent_step.derived_using_from_where);
        EXPECT(step != NULL && step->data.intent_step.inherited_authorized_by_from_action);
        EXPECT(step != NULL && step->data.intent_step.where_type != NULL);
        EXPECT(step != NULL && step->data.intent_step.where_type->type == AST_TYPE);
        EXPECT(step != NULL && strcmp(step->data.intent_step.where_type->data.type.name,
                                      "Arena") == 0);
        EXPECT(step != NULL && step->data.intent_step.using_expr != NULL);
        EXPECT(step != NULL && step->data.intent_step.using_expr->type == AST_IDENTIFIER);
        EXPECT(step != NULL && strcmp(step->data.intent_step.using_expr->data.identifier.name,
                                      "arena") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step inherits requires causes and self authority from on-call action");
    {
        const char *source =
            "ability Prepared {\n"
            "    func Ready() -> Bool;\n"
            "}\n"
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Guard(self) -> Void\n"
            "        requires Prepared\n"
            "        within BattleZone\n"
            "        authorized by self\n"
            "        causes Guarded {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "role HeroPrepared for Hero {\n"
            "    impl ability Prepared {\n"
            "        func Ready() -> Bool { return true; }\n"
            "    }\n"
            "}\n"
            "effect Guarded for bearer: Hero { }\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    effect slot guarded: Guarded\n"
            "    authority hero requires Prepared\n"
            "}\n"
            "intent Patrol(battle: BattleZone, hero: Hero) {\n"
            "    step Verify {\n"
            "        on: hero.Guard();\n"
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
        EXPECT(step != NULL && step->data.intent_step.derived_who_from_on_receiver);
        EXPECT(step != NULL && step->data.intent_step.inherited_where_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_requires_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_causes_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_authorized_by_from_action);
        EXPECT(step != NULL
            && step->data.intent_step.required_ability_count == 1
            && step->data.intent_step.required_abilities[0] != NULL
            && step->data.intent_step.required_abilities[0]->type == AST_TYPE
            && strcmp(step->data.intent_step.required_abilities[0]->data.type.name,
                      "Prepared") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.causes_effect != NULL
            && strcmp(step->data.intent_step.causes_effect, "Guarded") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.authorized_by_count == 1
            && strcmp(step->data.intent_step.authorized_by[0], "hero") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step maps action parameter authority from on-call argument");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
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
        EXPECT(step != NULL && step->data.intent_step.derived_who_from_on_receiver);
        EXPECT(step != NULL && step->data.intent_step.inherited_where_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_causes_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_authorized_by_from_action);
        EXPECT(step != NULL
            && step->data.intent_step.authorized_by_count == 1
            && strcmp(step->data.intent_step.authorized_by[0], "healer") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step keeps parameter authority explicit across multiple on calls");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
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
            "    let hp: Int;\n"
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
            "    let hp: Int;\n"
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
            "    let hp: Int;\n"
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
}
