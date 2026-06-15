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
