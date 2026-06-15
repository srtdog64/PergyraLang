    TEST("intent step derives using and where from transfer target");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Promote(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CartZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
            "    step Promote {\n"
            "        transfer: cart -> payment;\n"
            "        who: buyer;\n"
            "        on: buyer.Promote();\n"
            "        expect: true;\n"
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

    TEST("intent step move shorthand derives using and where from transfer target");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Promote(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CartZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
            "    step Promote {\n"
            "        move cart to payment;\n"
            "        who: buyer;\n"
            "        on: buyer.Promote();\n"
            "        expect: true;\n"
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

    TEST("intent step warns when matching action contract is restated locally");
    {
        const char *source =
            "ability DriverCap { func Ping() -> Void; }\n"
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void\n"
            "        requires DriverCap\n"
            "        within CockpitZone\n"
            "        causes Started\n"
            "        authorized by self {\n"
            "        self.started = true;\n"
            "    }\n"
            "}\n"
            "role DriverCapRole for Driver {\n"
            "    impl ability DriverCap {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "effect Started for bearer: Driver {\n"
            "    subject slot bearer: Driver\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    effect slot started: Started\n"
            "    authority driver requires DriverCap\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        who: driver;\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        requires: DriverCap;\n"
            "        causes: Started;\n"
            "        authorized by: driver;\n"
            "        on: true;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "restates contract clauses already provided by the matching action contract"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Matching action contract: Driver.Ignite"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Redundant clauses:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "- where"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "- requires"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "- causes"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "- authorized by"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step reuses action where requires causes and authorized by");
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
            "    step Guard {\n"
            "        who: hero;\n"
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
        EXPECT(step != NULL);
        EXPECT(step != NULL && step->data.intent_step.inherited_where_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_requires_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_causes_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_authorized_by_from_action);
        EXPECT(step != NULL
            && step->data.intent_step.where_type != NULL
            && step->data.intent_step.where_type->type == AST_TYPE
            && strcmp(step->data.intent_step.where_type->data.type.name, "BattleZone") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.required_ability_count == 1
            && step->data.intent_step.required_abilities[0] != NULL
            && step->data.intent_step.required_abilities[0]->type == AST_TYPE
            && strcmp(step->data.intent_step.required_abilities[0]->data.type.name, "Prepared") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.causes_effect != NULL
            && strcmp(step->data.intent_step.causes_effect, "Guarded") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.authorized_by_count == 1
            && strcmp(step->data.intent_step.authorized_by[0], "hero") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.using_expr != NULL
            && step->data.intent_step.using_expr->type == AST_IDENTIFIER
            && strcmp(step->data.intent_step.using_expr->data.identifier.name, "battle") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step derives who from unique matching subject action");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Guard(self) -> Void\n"
            "        within BattleZone\n"
            "        authorized by self {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero\n"
            "}\n"
            "intent Patrol(battle: BattleZone, hero: Hero) {\n"
            "    step Guard {\n"
            "        using: battle;\n"
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
        EXPECT(step != NULL && step->data.intent_step.inherited_who_from_action);
        EXPECT(step != NULL && step->data.intent_step.who_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.who_names[0], "hero") == 0);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.authorized_by[0], "hero") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.using_expr != NULL
            && step->data.intent_step.using_expr->type == AST_IDENTIFIER
            && strcmp(step->data.intent_step.using_expr->data.identifier.name, "battle") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step derives where from explicit using zone binding");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Guard(self) -> Void {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero\n"
            "}\n"
            "intent Patrol(battle: BattleZone, hero: Hero) {\n"
            "    step Guard {\n"
            "        using: battle;\n"
            "        who: hero;\n"
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
        EXPECT(step != NULL
            && step->data.intent_step.where_type != NULL
            && step->data.intent_step.where_type->type == AST_TYPE
            && strcmp(step->data.intent_step.where_type->data.type.name, "BattleZone") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step reuses intent-level who and where defaults");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Guard(self) -> Void {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero\n"
            "}\n"
            "intent Patrol(battle: BattleZone, hero: Hero) {\n"
            "    who: hero;\n"
            "    where: BattleZone;\n"
            "    step Guard {\n"
            "        using: battle;\n"
            "        authorized by: hero;\n"
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
        EXPECT(step != NULL && step->data.intent_step.inherited_who_from_intent);
        EXPECT(step != NULL && step->data.intent_step.inherited_where_from_intent);
        EXPECT(step != NULL && !step->data.intent_step.inherited_who_from_action);
        EXPECT(step != NULL && !step->data.intent_step.inherited_where_from_action);
        EXPECT(step != NULL && step->data.intent_step.who_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.who_names[0], "hero") == 0);
        EXPECT(step != NULL && !step->data.intent_step.derived_authorized_by_from_zone);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.authorized_by[0], "hero") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.where_type != NULL
            && step->data.intent_step.where_type->type == AST_TYPE
            && strcmp(step->data.intent_step.where_type->data.type.name, "BattleZone") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent-level who default failure reports provenance");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "}\n"
            "intent Patrol(battle: BattleZone, hero: Hero) {\n"
            "    who: ghost;\n"
            "    where: BattleZone;\n"
            "    step Guard {\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "inherited from the intent-level who default"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "declare the participant with 'who ghost: <Subject>;'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step move shorthand accepts unique target zone type");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Promote(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CartZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
            "    step Promote {\n"
            "        move cart to PaymentZone;\n"
            "        who: buyer;\n"
            "        on: buyer.Promote();\n"
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
        EXPECT(step != NULL
            && step->data.intent_step.transfer_to_alias != NULL
            && strcmp(step->data.intent_step.transfer_to_alias, "payment") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.using_expr != NULL
            && step->data.intent_step.using_expr->type == AST_IDENTIFIER
            && strcmp(step->data.intent_step.using_expr->data.identifier.name, "payment") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.where_type != NULL
            && step->data.intent_step.where_type->type == AST_TYPE
            && strcmp(step->data.intent_step.where_type->data.type.name, "PaymentZone") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
