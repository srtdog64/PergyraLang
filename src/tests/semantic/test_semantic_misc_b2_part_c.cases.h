    TEST("intent authorized participant reports ambiguous authority slot");
    {
        const char *source =
            "subject User {\n"
            "    let level: Int;\n"
            "}\n"
            "zone AccountZone {\n"
            "    subject slot owner: User\n"
            "    subject slot guest: User\n"
            "    authority owner\n"
            "}\n"
            "intent EditAccount(account: AccountZone, actor: User) {\n"
            "    step Edit {\n"
            "        where: AccountZone;\n"
            "        using: account;\n"
            "        who: actor;\n"
            "        authorized by: actor;\n"
            "        on: true;\n"
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
            "authorized participant 'actor' of type 'User' is ambiguous"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "must resolve to one concrete authority subject slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "rename the participant alias to the intended authority slot name"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject return diagnostic includes reason and fix");
    {
        const char *source =
            "subject Courier {\n"
            "    let level: Int;\n"
            "}\n"
            "func MakeCourier() -> Courier {\n"
            "    return Courier();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Returning subjects by value is not supported yet"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "subject values are zone/world slot handles (anchored)"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "return a struct/class/object/tobject projection instead"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent transfer mismatch diagnostic includes reason and fix");
    {
        const char *source =
            "subject Courier { let level: Int; }\n"
            "zone LoadingZone {\n"
            "    subject slot courier: Courier\n"
            "}\n"
            "zone DeliveryZone {\n"
            "    subject slot courier: Courier\n"
            "}\n"
            "intent MoveCargo(load: LoadingZone, deliver: DeliveryZone, courier: Courier) {\n"
            "    step Deliver {\n"
            "        who: courier;\n"
            "        where: LoadingZone;\n"
            "        using: load;\n"
            "        transfer: load -> deliver;\n"
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
            "does not match the current zone contract"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "using binding does not match the transfer target"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "transfer target derivation would choose using 'deliver'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "transfer target derivation would choose zone 'DeliveryZone'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "current zone contract provenance is"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "transfer handoff edge on step: load -> deliver"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "locally declared zone on step: LoadingZone"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "locally declared using on step: load"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authority diagnostic mentions locally declared step zone");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void {\n"
            "        self.started = true;\n"
            "    }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: true;\n"
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
            "locally declared zone on step: CockpitZone"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "locally declared using on step: cockpit"));
        EXPECT(!ctx_has_diagnostic_substring_from_result(result,
            "inherited zone from matching action"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("duplicate stdlib use emits warning but stays accepted");
    {
        const char *source =
            "use datetime;\n"
            "use datetime;\n"
            "func Main() -> Void {\n"
            "    Log(Now());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(
            result, "Duplicate stdlib use 'datetime'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent transfer derives zone using and authorized by from target authority");
    {
        const char *source =
            "subject Courier { let level: Int; }\n"
            "zone LoadingZone {\n"
            "    subject slot courier: Courier\n"
            "}\n"
            "zone DeliveryZone {\n"
            "    subject slot courier: Courier\n"
            "    authority courier\n"
            "}\n"
            "intent MoveCargo(load: LoadingZone, deliver: DeliveryZone, courier: Courier) {\n"
            "    step Deliver {\n"
            "        transfer: load -> deliver;\n"
            "        who: courier;\n"
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
        EXPECT(step != NULL && step->data.intent_step.derived_where_from_transfer);
        EXPECT(step != NULL && step->data.intent_step.derived_using_from_transfer);
        EXPECT(step != NULL && step->data.intent_step.derived_authorized_by_from_zone);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.authorized_by[0], "courier") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation without endpoint reports propagation warning with reason and fix");
    {
        const char *source =
            "relation Link { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Relation 'Link' should declare at least one endpoint slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "declaration propagation edge is relation 'Link' -> endpoint slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "relation and projection propagation stay underspecified"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot anchor a stable relation edge"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("effect without target reports propagation warning with reason and fix");
    {
        const char *source =
            "effect Guarded { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Effect 'Guarded' should declare at least one target slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "declaration propagation edge is effect 'Guarded' -> target slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "effect propagation and authority checks stay underspecified"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot preserve an effect target path"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("branch effect conflict warning reports branch join reason and fix");
    {
        const char *source =
            "/// @effects secure\n"
            "func SecureStep() -> Void { return; }\n"
            "/// @effects remote\n"
            "func RemoteStep() -> Void { return; }\n"
            "func Main(flag: Bool) -> Void {\n"
            "    if (flag) {\n"
            "        SecureStep();\n"
            "    } else {\n"
            "        RemoteStep();\n"
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
            "Control-flow branch/join combines conflicting effect classes"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "this control-flow join merges effect deltas from multiple paths"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "then branch contributes 'secure'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "else branch contributes 'remote'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone projection diagnostic includes reason and fix");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot wrong: Player\n"
            "    bind wrong from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "is not a projection slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "target slot 'wrong' on Zone 'BattleZone'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "source slot 'player' is driving this bind path"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "ownership anchors, not projection sinks"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "choose an object slot for local projection sync"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone publish projection diagnostic includes source provenance reason and fix");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    publish playerView from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "target slot 'playerView' must be a tobject slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "target slot 'playerView' on Zone 'BattleZone'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "source slot 'player' is driving this publish path"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "publish writes a boundary transfer snapshot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "change 'playerView' to a tobject slot"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone projection ambiguous source field includes reason and fix");
    {
        const char *source =
            "vessel CombatStats { hp: Int; }\n"
            "vessel ShieldState { hp: Int; }\n"
            "subject Player {\n"
            "    vessel combat: CombatStats;\n"
            "    vessel shield: ShieldState;\n"
            "}\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    refresh playerView from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "target field 'hp' is ambiguous in source slot 'player'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "target projection declaration is 'PlayerView'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "multiple projection source paths match field 'hp'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "automatic projection cannot choose one path safely"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToObject ambiguous source field includes reason and fix");
    {
        const char *source =
            "vessel CombatStats { hp: Int; }\n"
            "vessel ShieldState { hp: Int; }\n"
            "subject Player {\n"
            "    vessel combat: CombatStats;\n"
            "    vessel shield: ShieldState;\n"
            "}\n"
            "object PlayerView { hp: Int; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let view: PlayerView = ToObject(PlayerView, player);\n"
            "    Log(view.hp);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "ToObject target field 'hp' is ambiguous in source subject 'Player'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "multiple projection source paths match field 'hp'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "automatic projection cannot choose one path safely"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToTObject ambiguous source field includes reason and fix");
    {
        const char *source =
            "vessel CombatStats { hp: Int; }\n"
            "vessel ShieldState { hp: Int; }\n"
            "subject Player {\n"
            "    vessel combat: CombatStats;\n"
            "    vessel shield: ShieldState;\n"
            "}\n"
            "tobject PlayerPacket { hp: Int; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let packet: PlayerPacket = ToTObject(PlayerPacket, player);\n"
            "    Log(packet.hp);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "ToTObject target field 'hp' is ambiguous in source subject 'Player'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "multiple projection source paths match field 'hp'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "automatic projection cannot choose one path safely"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone effect contract mismatch includes provenance reason and fix");
    {
        const char *source =
            "subject Hero { let hp: Int; }\n"
            "subject Monster { let hp: Int; }\n"
            "effect Poison {\n"
            "    subject slot target: Hero\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot monster: Monster\n"
            "    effect slot poison: Poison\n"
            "    apply poison to monster\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Zone BattleZone target slot 'monster' has type 'Monster'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "target slot 'monster' has type 'Monster'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "effect 'Poison' expects target type 'Hero'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "effect slot 'poison' in zone contract 'BattleZone'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "zone slot 'monster' is bound to effect slot 'poison'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "propagation edge is effect slot 'poison' -> target slot 'monster'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "bind effect 'Poison' to a zone slot of type 'Hero'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone relation contract mismatch includes provenance reason and fix");
    {
        const char *source =
            "subject Hero { let hp: Int; }\n"
            "subject Villain { let hp: Int; }\n"
            "relation Rivalry {\n"
            "    subject slot left: Hero\n"
            "    subject slot right: Hero\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    subject slot villain: Villain\n"
            "    relation slot rivalry: Rivalry\n"
            "    link rivalry between hero, villain\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Zone BattleZone right slot 'villain' has type 'Villain'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "right slot 'villain' has type 'Villain'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "relation 'Rivalry' expects right endpoint type 'Hero'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "relation slot 'rivalry' in zone contract 'BattleZone'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "zone relation slot 'rivalry' binds declaration 'Rivalry'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "propagation edge is relation slot 'rivalry' -> right endpoint 'villain'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "bind relation 'Rivalry' to a right slot of type 'Hero'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authority-bearing zone derives authorized by for secure helper");
    {
        const char *source =
            "/// @effects secure\n"
            "func Gate() -> Bool { return true; }\n"
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void { self.started = true; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: Gate();\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
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
        EXPECT(step != NULL && step->data.intent_step.derived_authorized_by_from_zone);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.authorized_by[0], "driver") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authority-bearing zone-action helper records derived authorized by before action diagnostics");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void within CockpitZone causes Started {\n"
            "        self.started = true;\n"
            "    }\n"
            "}\n"
            "effect Started for bearer: Driver {\n"
            "    subject slot bearer: Driver\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    effect slot started: Started\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
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
        EXPECT(step != NULL && step->data.intent_step.derived_authorized_by_from_zone);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.authorized_by[0], "driver") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent declaration rejects non-bool guard and invariant");
    {
        const char *source =
            "subject Driver {\n"
            "    let hp: Int;\n"
            "    action Ignite(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "}\n"
            "intent DriveCar(driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        guard: driver.hp;\n"
            "        invariant: driver.hp;\n"
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
            "Intent guard expects a Bool value"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Intent invariant expects a Bool value"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent declaration supports compensate and trace builtins");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void { self.started = true; }\n"
            "    action RollbackIgnite(self) -> Void { self.started = false; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "}\n"
            "intent DriveCar(driver: Driver) {\n"
            "    rollback: current;\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        compensate: driver.RollbackIgnite();\n"
            "        pre: true;\n"
            "        guard: false;\n"
            "        post: driver.started;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    Log(IntentLastTrace());\n"
            "    Log(IntentLastFailure());\n"
            "    Log(IntentLastName());\n"
            "    Log(ToString(IntentLastHandle()));\n"
            "    Log(ToString(IntentLastTraceId()));\n"
            "    Log(ToString(IntentLastStepCount()));\n"
            "    Log(ToString(IntentLastFailed()));\n"
            "    Log(ToString(IntentHistoryCount()));\n"
            "    Log(IntentHistoryStepName(0));\n"
            "    Log(IntentHistoryStepZone(0));\n"
            "    Log(IntentHistoryStepPhase(0));\n"
            "    Log(IntentHistoryStepParticipant(0));\n"
            "    Log(IntentHistoryStepSlot(0));\n"
            "    Log(IntentHistoryStepFromZone(0));\n"
            "    Log(IntentHistoryStepFromSlot(0));\n"
            "    Log(IntentHistoryStepToZone(0));\n"
            "    Log(IntentHistoryStepToSlot(0));\n"
            "    Log(ToString(IntentHistoryStepOk(0)));\n"
            "    Log(IntentHistoryStepFailure(0));\n"
            "    Log(ToString(IntentActiveCount()));\n"
            "    Log(IntentActiveName(0));\n"
            "    Log(ToString(IntentActiveHandle(0)));\n"
            "    Log(ToString(IntentActiveParentHandle(0)));\n"
            "    Log(ToString(IntentActiveTraceId(0)));\n"
            "    Log(ToString(IntentActivePriority(0)));\n"
            "    Log(ToString(IntentActiveSubjectCount(0)));\n"
            "    Log(ToString(IntentActiveStepCount(0)));\n"
            "    Log(ToString(IntentActiveConcurrent(0)));\n"
            "    Log(ToString(IntentActiveFailed(0)));\n"
            "    Log(IntentActiveFailure(0));\n"
            "    Log(IntentActiveTrace(0));\n"
            "    Log(ToString(IntentRecentCount()));\n"
            "    Log(IntentRecentName(0));\n"
            "    Log(IntentRecentTrace(0));\n"
            "    Log(IntentRecentFailure(0));\n"
            "    Log(ToString(IntentRecentStepCount(0)));\n"
            "    Log(ToString(IntentRecentFailed(0)));\n"
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

    TEST("intent declaration validates cross-world transfer bindings");
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
            "        where: PaymentZone;\n"
            "        using: payment;\n"
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

    TEST("named call arguments are semantically rejected until dispatch ABI exists");
    {
        const char *source =
            "func Add(a: Int, b: Int) -> Int {\n"
            "    return a + b;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    Log(Add(a: 1, b: 2));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Named call arguments are reserved but not implemented yet"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
