static void
test_projection_contract_diagnostics(void)
{
    printf("\n[projection_contract_diagnostics]\n");

    TEST("zone refresh reports missing source field with structured diagnostic");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "object PlayerView {\n"
            "    hp: Int;\n"
            "    mana: Int;\n"
            "}\n"
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
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "target field 'mana' is missing from source slot 'player'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "projection target 'playerView' expects field 'mana'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "add field 'mana' to source declaration 'Player'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone refresh reports typed source path mismatch with structured diagnostic");
    {
        const char *source =
            "vessel Stats {\n"
            "    let hp: String;\n"
            "}\n"
            "subject Player {\n"
            "    vessel stats: Stats;\n"
            "}\n"
            "object PlayerView {\n"
            "    hp: Int;\n"
            "}\n"
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
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "target field 'hp' cannot accept source path 'stats.hp' from slot 'player'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "projection target slot 'playerView' expects field 'hp' to have type 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "resolved source path 'stats.hp' from slot 'player' has type 'String'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone refresh reports ambiguous nested source field with structured diagnostic");
    {
        const char *source =
            "vessel Stats {\n"
            "    let hp: Int;\n"
            "}\n"
            "vessel Aura {\n"
            "    let hp: Int;\n"
            "}\n"
            "subject Player {\n"
            "    vessel stats: Stats;\n"
            "    vessel aura: Aura;\n"
            "}\n"
            "object PlayerView {\n"
            "    hp: Int;\n"
            "}\n"
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
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "target field 'hp' is ambiguous in source slot 'player'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "multiple projection source paths match field 'hp'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "automatic projection cannot choose one path safely"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone refresh reports wrong projection kind with structured diagnostic");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "tobject PlayerPacket {\n"
            "    hp: Int;\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    tobject slot playerPacket: PlayerPacket\n"
            "    refresh playerPacket from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "target slot 'playerPacket' uses the wrong projection kind"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "source slot 'player' is driving this refresh path"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "refresh requires target slot 'playerPacket' to use object declaration"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "actual target type 'PlayerPacket' uses a different projection kind"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone refresh reports duplicate field map with structured diagnostic");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    let mana: Int;\n"
            "}\n"
            "object PlayerView {\n"
            "    hp: Int;\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    refresh playerView from player map {\n"
            "        hp <- hp;\n"
            "        hp <- mana;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "projection map duplicates target field 'hp'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "each projection target field may be filled from exactly one source field"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "keep a single mapping for 'hp'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone state reports unknown effect slot with structured diagnostic");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    shared damage: Int;\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    state poisoned: effect poison on player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "Zone state 'poisoned' references unknown effect slot 'poison'."));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "state 'poisoned' is declared as an effect-backed lifecycle alias"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "declare effect slot 'poison' before this state"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone apply reports unknown effect slot with structured diagnostic");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    apply poison to player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "Zone apply references unknown effect slot 'poison'."));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "apply mutates an effect lifecycle and must target a declared zone effect slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "declare effect slot 'poison' in zone 'BattleZone'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone maintain reports unknown state with structured diagnostic");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    maintain ghostState\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "Zone maintain references unknown state 'ghostState'."));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "maintain state aliases must reference a declared zone state"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "declare state 'ghostState' before this maintain clause"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
