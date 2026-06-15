    TEST("graph-backed reverse schedule places providers before generic and authority consumers");
    {
        const char *source =
            "ability Commandable { func Command() -> Void; }\n"
            "subject Player {\n"
            "    action Command(self) -> Void { return; }\n"
            "}\n"
            "role PlayerCommandable for Player {\n"
            "    impl ability Commandable {\n"
            "        func Command() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "func Wrap<T = Int>(value: T) -> T where T: Int {\n"
            "    return value;\n"
            "}\n"
            "party StrikeTeam {\n"
            "    role slot commander: Commandable\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TypeResolutionGraph *graph = NULL;

        EXPECT(!parser_has_error(parser));

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        graph = &ctx->type_resolution_graph;
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "func Wrap.T",
            "Int",
            "default-type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "func Wrap.T",
            "Int",
            "where-bound lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.player",
            "Commandable",
            "zone authority ability consumer lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "party StrikeTeam.commander",
            "Commandable",
            "party role slot ability consumer lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "func Wrap.T",
            "Int"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "zone BattleZone.player",
            "Commandable"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "party StrikeTeam.commander",
            "Commandable"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed declaration order handles forward generic and authority consumers");
    {
        const char *source =
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable\n"
            "}\n"
            "party StrikeTeam {\n"
            "    role slot commander: Commandable\n"
            "}\n"
            "func Wrap<T = UserId>(value: T) -> T where T: UserId {\n"
            "    return value;\n"
            "}\n"
            "type UserId = Int;\n"
            "subject Player {\n"
            "    action Command(self) -> Void { return; }\n"
            "}\n"
            "ability Commandable { func Command() -> Void; }\n"
            "role PlayerCommandable for Player {\n"
            "    impl ability Commandable {\n"
            "        func Command() -> Void { return; }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TypeResolutionGraph *graph = NULL;

        EXPECT(!parser_has_error(parser));

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        graph = &ctx->type_resolution_graph;
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "func Wrap.T",
            "UserId",
            "default-type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "func Wrap.T",
            "UserId",
            "where-bound lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "UserId",
            "Int",
            "type-alias target lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.player",
            "Commandable",
            "zone authority ability consumer lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "party StrikeTeam.commander",
            "Commandable",
            "party role slot ability consumer lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "func Wrap.T",
            "UserId"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "UserId",
            "Int"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "zone BattleZone.player",
            "Commandable"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "party StrikeTeam.commander",
            "Commandable"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers action and intent effect/zone consumers");
    {
        const char *source =
            "effect Alerted for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot alert: Alerted\n"
            "    authority player\n"
            "}\n"
            "subject Player {\n"
            "    action Guard(self) -> Void\n"
            "        causes Alerted\n"
            "        within BattleZone\n"
            "        authorized by self\n"
            "    {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "intent BattlePlan(zone: BattleZone, player: Player) {\n"
            "    step act {\n"
            "        where: BattleZone;\n"
            "        using: zone;\n"
            "        who: player;\n"
            "        causes: Alerted;\n"
            "        authorized by: player;\n"
            "        on: true;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TypeResolutionGraph *graph = NULL;

        EXPECT(!parser_has_error(parser));

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        graph = &ctx->type_resolution_graph;
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "Guard",
            "BattleZone",
            "action within-zone lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "Guard",
            "Alerted",
            "action causes-effect lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "intent BattlePlan.act",
            "BattleZone",
            "intent step where-type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "intent BattlePlan.act",
            "Alerted",
            "intent step causes-effect lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "Guard",
            "BattleZone"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "Guard",
            "Alerted"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "intent BattlePlan.act",
            "BattleZone"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "intent BattlePlan.act",
            "Alerted"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers party extends and world systemic consumers");
    {
        const char *source =
            "party BaseTeam { }\n"
            "party EliteTeam extends BaseTeam { }\n"
            "roster CombatRoster {\n"
            "    party slot squad: EliteTeam\n"
            "}\n"
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    roster combat: CombatRoster\n"
            "    zone battle: BattleZone\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TypeResolutionGraph *graph = NULL;

        EXPECT(!parser_has_error(parser));

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        graph = &ctx->type_resolution_graph;
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "EliteTeam",
            "BaseTeam",
            "party extends lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "squad",
            "EliteTeam",
            "roster party lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "combat",
            "CombatRoster",
            "world roster lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "battle",
            "BattleZone",
            "world zone lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "EliteTeam",
            "BaseTeam"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "squad",
            "EliteTeam"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "combat",
            "CombatRoster"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "battle",
            "BattleZone"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers role include generic argument derivation");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "role BaseRole<T = Int> for Player { }\n"
            "role DefaultRole for Player {\n"
            "    include role BaseRole;\n"
            "}\n"
            "role LongRole for Player {\n"
            "    include role BaseRole<Long>;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TypeResolutionGraph *graph = NULL;

        EXPECT(!parser_has_error(parser));

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        graph = &ctx->type_resolution_graph;
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "role DefaultRole.include",
            "BaseRole",
            "role include lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "role LongRole.include",
            "BaseRole",
            "role include lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "BaseRole",
            "Int",
            "omitted default generic argument lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "BaseRole",
            "Long",
            "provided generic argument lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "role DefaultRole.include",
            "BaseRole"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "role LongRole.include",
            "BaseRole"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "BaseRole",
            "Int"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "BaseRole",
            "Long"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers inline generic constraint consumers");
    {
        const char *source =
            "ability Damageable { }\n"
            "class Box<T: Damageable> { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TypeResolutionGraph *graph = NULL;

        EXPECT(!parser_has_error(parser));

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        graph = &ctx->type_resolution_graph;
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "class Box.T",
            "Damageable",
            "generic constraint lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "class Box.T",
            "Damageable"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers type alias target consumers");
    {
        const char *source =
            "type UserId = Int;\n"
            "type NameList = List<String>;\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        TypeResolutionGraph *graph = NULL;

        EXPECT(!parser_has_error(parser));

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        graph = &ctx->type_resolution_graph;
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "UserId",
            "Int",
            "type-alias target lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "NameList",
            "List",
            "type-alias target lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "NameList",
            "String",
            "type-alias target lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "UserId",
            "Int"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "NameList",
            "List"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "NameList",
            "String"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
