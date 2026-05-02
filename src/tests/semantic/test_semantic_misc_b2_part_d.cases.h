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
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "- who"));
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

    TEST("intent step may orchestrate a subintent without local where/who");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Touch() -> Void { return; }\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Charge(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step verify {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        intent: Charge(checkout, buyer);\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
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

    TEST("using alias statement lowers to local binding");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    func ReadHP(self) -> Int {\n"
            "        using self.hp as hp;\n"
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

    TEST("intent declaration rejects spawn inside step clauses");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "}\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        on: spawn Touch();\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "on clause cannot contain 'spawn'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Now and Sleep builtins type-check");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let t0: Int = Now();\n"
            "    Sleep(1);\n"
            "    let t1: Int = Now();\n"
            "    Log(ToString(t1 - t0));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ReadLine builtin type-checks");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let line: String = ReadLine();\n"
            "    Print(line);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step intent clause must target another intent");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    func Pay(self) -> Bool { return true; }\n"
            "}\n"
            "func Charge(buyer: Buyer) -> Bool { return true; }\n"
            "intent Checkout(buyer: Buyer) {\n"
            "    step pay {\n"
            "        intent: Charge(buyer);\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "is not a declared intent"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "non-intent callees do not carry intent step provenance into AIR"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "declare 'Charge' as an intent that returns Bool"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ability fields cannot reference non-exported foreign type");
    {
        const char *source =
            "subject Hidden { let hp: Int; }\n"
            "ability NeedsHidden {\n"
            "    fields h: Hidden\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result;

        EXPECT(!parser_has_error(parser));
        program->data.program.statements[0]->origin_path = pergyra_strdup("hidden_module.pgy");
        program->data.program.statements[1]->origin_path = pergyra_strdup("ability_module.pgy");

        result = semantic_analyze(program);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot declare field 'h' in fields with non-exported type 'Hidden'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role impl cannot reference non-exported foreign ability");
    {
        const char *module_path = "test_role_impl_hidden_ability_module_a.pgy";
        const char *main_path = "test_role_impl_hidden_ability_module_main_a.pgy";
        const char *module_source =
            "private ability HiddenAbility {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_role_impl_hidden_ability_module_a.pgy\";\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability HiddenAbility {\n"
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
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot implement non-exported ability 'HiddenAbility'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("action requires cannot reference non-exported foreign ability");
    {
        const char *module_path = "test_action_requires_hidden_ability_module_a.pgy";
        const char *main_path = "test_action_requires_hidden_ability_module_main_a.pgy";
        const char *module_source =
            "private ability HiddenAbility {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_action_requires_hidden_ability_module_a.pgy\";\n"
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Do(self) -> Void requires HiddenAbility {\n"
            "        return;\n"
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
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot require non-exported ability 'HiddenAbility'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("action requires accepts generic ability type arguments when ability is generic");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Do(self) -> Void requires Combatable<Int> {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<Int> { func Ping() -> Void { return; } }\n"
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

    TEST("action requires rejects generic ability type arguments when ability is non-generic");
    {
        const char *source =
            "ability Combatable { func Ping() -> Void; }\n"
            "subject Hero {\n"
            "    action Do(self) -> Void requires Combatable<Int> {\n"
            "        return;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not accept generic type arguments"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority requires accepts generic ability type arguments when ability is generic");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<Int> { func Ping() -> Void { return; } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero requires Combatable<Int>\n"
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

    TEST("zone authority requires rejects wrong generic ability arity");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable { func Ping() -> Void { return; } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero requires Combatable<Int, String>\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires between 1 and 1 generic argument(s)"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("action requires rejects mismatched generic ability impl");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero {\n"
            "    action Do(self) -> Void requires Combatable<Int> {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<String> { func Ping() -> Void { return; } }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires ability 'Combatable<Int>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "implements 'Combatable<String>' instead"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority reports required and actual generic ability mismatch");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<String> { func Ping() -> Void { return; } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero requires Combatable<Int>\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires ability 'Combatable<Int>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "implements 'Combatable<String>' instead"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
