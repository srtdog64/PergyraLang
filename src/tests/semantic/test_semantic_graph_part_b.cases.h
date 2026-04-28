    TEST("graph-backed reverse schedule covers zone shared field consumers");
    {
        const char *source =
            "zone BattleZone {\n"
            "    shared label: String = \"idle\"\n"
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
            "label",
            "String",
            "zone shared field type lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "label",
            "String"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed reverse schedule covers relation/effect declaration consumers");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "relation LeftAnchored between Player, subject for leftPlayer: Player, rightPlayer: Player {\n"
            "    shared leftNote: String\n"
            "}\n"
            "relation RightAnchored between subject, Player for actor: Player, peer: Player {\n"
            "    shared rightNote: String\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    shared reason: String\n"
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
            "LeftAnchored",
            "Player",
            "relation between-left type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "RightAnchored",
            "Player",
            "relation between-right type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "leftPlayer",
            "Player",
            "relation slot type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "peer",
            "Player",
            "relation slot type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "leftNote",
            "String",
            "relation shared field type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "rightNote",
            "String",
            "relation shared field type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "bearer",
            "Player",
            "effect slot type lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "reason",
            "String",
            "effect shared field type lookup"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "LeftAnchored",
            "Player"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "RightAnchored",
            "Player"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "leftNote",
            "String"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "rightNote",
            "String"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "bearer",
            "Player"));
        EXPECT(type_resolution_graph_provider_before_consumer_schedule(
            graph,
            "reason",
            "String"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph records zone refresh projection field-path edges");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { totalHp: Int; label: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    refresh playerView from player map {\n"
            "        totalHp <- hp;\n"
            "        label <- name;\n"
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
            "zone BattleZone.refresh.playerView",
            "zone BattleZone.projection.playerView.totalHp<-player.hp",
            "zone refresh projection-path lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.refresh.playerView",
            "zone BattleZone.slot.playerView",
            "zone refresh target-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.refresh.playerView",
            "zone BattleZone.slot.player",
            "zone refresh source-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.projection.playerView.totalHp<-player.hp",
            "zone BattleZone.slot.playerView",
            "projection target-slot carrier"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.projection.playerView.totalHp<-player.hp",
            "zone BattleZone.slot.player",
            "projection source-slot carrier"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.projection.playerView.totalHp<-player.hp",
            "zone BattleZone.slot.playerView.field.totalHp",
            "projection target field-path lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.projection.playerView.totalHp<-player.hp",
            "zone BattleZone.slot.player.field.hp",
            "projection source field-path lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.projection.playerView.label<-player.name",
            "zone BattleZone.slot.playerView.field.label",
            "projection target field-path lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.projection.playerView.label<-player.name",
            "zone BattleZone.slot.player.field.name",
            "projection source field-path lookup"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph records relation/effect propagation edges for zone lifecycle contracts");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    apply poison to player by player\n"
            "    link trust between player, enemy by player\n"
            "    maintain poison on player by player\n"
            "    maintain trust between player, enemy by player\n"
            "    maintain poisoned by player\n"
            "    maintain allied by player\n"
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
            "zone BattleZone.apply.poison",
            "zone BattleZone.layer.poison",
            "zone apply effect-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.apply.poison",
            "zone BattleZone.slot.player",
            "zone apply target-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.link.trust",
            "zone BattleZone.layer.trust",
            "zone link relation-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.link.trust",
            "zone BattleZone.slot.player",
            "zone link left-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.link.trust",
            "zone BattleZone.slot.enemy",
            "zone link right-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.maintain-effect.poison",
            "zone BattleZone.layer.poison",
            "zone maintain-effect slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.maintain-effect.poison",
            "zone BattleZone.slot.player",
            "zone maintain-effect target-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.maintain-relation.trust",
            "zone BattleZone.layer.trust",
            "zone maintain-relation slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.maintain-relation.trust",
            "zone BattleZone.slot.player",
            "zone maintain-relation left-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.maintain-relation.trust",
            "zone BattleZone.slot.enemy",
            "zone maintain-relation right-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.state.allied",
            "zone BattleZone.layer.trust",
            "zone state layer lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.state.allied",
            "zone BattleZone.slot.player",
            "zone state target-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.state.allied",
            "zone BattleZone.slot.enemy",
            "zone state right-slot lookup"));
        EXPECT(type_resolution_graph_has_edge_between(
            graph,
            "zone BattleZone.maintain-state.allied",
            "zone BattleZone.state.allied",
            "zone maintain-state lookup"));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph reports alias dependency cycles with semantic provenance");
    {
        const char *source =
            "type Left = Right;\n"
            "type Right = Left;\n"
            "func Main() -> Void {\n"
            "    let value: Left = 1;\n"
            "    Log(value);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Type resolution dependency cycle detected"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "cycle path:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Left"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Right"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed generic default and where-bound preserve cycle provenance");
    {
        const char *source =
            "type Left = Right;\n"
            "type Right = Left;\n"
            "func Wrap<T = Left>(value: T) -> T where T: Left {\n"
            "    return value;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Type resolution dependency cycle detected"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "cycle path:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Left"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Right"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed forward alias materializes nested constructed type");
    {
        const char *source =
            "func Echo(value: Later) -> Later {\n"
            "    return value;\n"
            "}\n"
            "type Later = Channel<Slot<Int>>;\n";
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

    TEST("graph-backed action ability consumer preserves generic cycle provenance");
    {
        const char *source =
            "type Left = Right;\n"
            "type Right = Left;\n"
            "ability Comparable<T = Left> where T: Left {\n"
            "    func Compare(self, other: T) -> Int;\n"
            "}\n"
            "subject Player {\n"
            "    action Command(self) -> Void requires Comparable { return; }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Type resolution dependency cycle detected"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "cycle path:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Left"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Right"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed role impl consumer preserves generic cycle provenance");
    {
        const char *source =
            "type Left = Right;\n"
            "type Right = Left;\n"
            "ability Comparable<T = Left> where T: Left {\n"
            "    func Compare(self, other: T) -> Int;\n"
            "}\n"
            "subject Player {\n"
            "    action Compare(self, other: Int) -> Int { return 0; }\n"
            "}\n"
            "role PlayerComparable for Player {\n"
            "    impl ability Comparable {\n"
            "        func Compare(self, other: Int) -> Int { return 0; }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Type resolution dependency cycle detected"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "cycle path:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Left"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Right"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed zone authority consumer preserves generic cycle provenance");
    {
        const char *source =
            "type Left = Right;\n"
            "type Right = Left;\n"
            "ability Commandable<T = Left> where T: Left {\n"
            "    func Command(self, value: T) -> Void;\n"
            "}\n"
            "subject Player {\n"
            "    action Command(self, value: Int) -> Void { return; }\n"
            "}\n"
            "role PlayerCommandable for Player {\n"
            "    impl ability Commandable {\n"
            "        func Command(self, value: Int) -> Void { return; }\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Type resolution dependency cycle detected"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "cycle path:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Left"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Right"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed party role slot consumer preserves generic cycle provenance");
    {
        const char *source =
            "type Left = Right;\n"
            "type Right = Left;\n"
            "ability Bufferable<T = Left> where T: Left {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n"
            "subject Bag {\n"
            "    let item: Int;\n"
            "}\n"
            "role IntBuffer for Bag {\n"
            "    impl ability Bufferable {\n"
            "        func Put(value: Int) -> Void { return; }\n"
            "    }\n"
            "}\n"
            "party StorageParty {\n"
            "    role slot buffer: Bufferable\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Type resolution dependency cycle detected"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "cycle path:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Left"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Right"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed intent step consumer preserves generic cycle provenance");
    {
        const char *source =
            "type Left = Right;\n"
            "type Right = Left;\n"
            "ability Commandable<T = Left> where T: Left {\n"
            "    func Command(self, value: T) -> Void;\n"
            "}\n"
            "subject Player {\n"
            "    action Command(self, value: Int) -> Void { return; }\n"
            "}\n"
            "role PlayerCommandable for Player {\n"
            "    impl ability Commandable {\n"
            "        func Command(self, value: Int) -> Void { return; }\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "}\n"
            "intent BattlePlan(zone: BattleZone, player: Player) {\n"
            "    step act {\n"
            "        where: BattleZone;\n"
            "        using: zone;\n"
            "        who: player;\n"
            "        requires: Commandable;\n"
            "        on: player.Command(1);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Type resolution dependency cycle detected"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "cycle path:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Left"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Right"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
               "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("graph-backed quiet resolution failure path stays stable across repeated semantic runs");
    {
        const char *source =
            "type Left = Right;\n"
            "type Right = Left;\n"
            "ability Bufferable<T = Left> where T: Left {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n"
            "subject Bag {\n"
            "    let item: Int;\n"
            "}\n"
            "role BagBuffer for Bag {\n"
            "    impl ability Bufferable {\n"
            "        func Put(value: Int) -> Void { return; }\n"
            "    }\n"
            "}\n";
        bool stable = true;

        for (int iteration = 0; iteration < 32; iteration++) {
            Lexer *lexer = lexer_create(source);
            Parser *parser = parser_create(lexer);
            ASTNode *program = parser_parse_program(parser);
            SemanticResult *result = semantic_analyze(program);

            if (parser_has_error(parser)
                || result == NULL
                || result->error_count == 0
                || !ctx_has_diagnostic_substring_from_result(
                    result, "Type resolution dependency cycle detected")
                || !ctx_has_diagnostic_substring_from_result(
                    result, "cycle path:")
                || !ctx_has_diagnostic_substring_from_result(
                    result, "Contract source:")
                || !ctx_has_diagnostic_substring_from_result(
                    result, "Fix:")) {
                stable = false;
            }

            semantic_result_destroy(result);
            ast_destroy(program);
            parser_destroy(parser);
            lexer_destroy(lexer);

            if (!stable)
                break;
        }

        EXPECT(stable);
    }

    TEST("parser failure cleanup stays stable across repeated same-process runs");
    {
        const char *source =
            "subject Broken {\n"
            "    let hp: Int\n"
            "    func Main( -> Void {\n"
            "        return;\n"
            "    }\n"
            "}\n";
        bool stable = true;

        for (int iteration = 0; iteration < 64; iteration++) {
            Lexer *lexer = lexer_create(source);
            Parser *parser = parser_create(lexer);
            ASTNode *program = parser_parse_program(parser);

            if (!parser_has_error(parser)) {
                stable = false;
            }

            ast_destroy(program);
            parser_destroy(parser);
            lexer_destroy(lexer);

            if (!stable)
                break;
        }

        EXPECT(stable);
    }
}
