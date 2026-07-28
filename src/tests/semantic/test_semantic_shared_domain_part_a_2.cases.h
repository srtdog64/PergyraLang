    TEST("zone refresh rejects wrong slot kinds");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; }\n"
            "zone BrokenZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot packet: PlayerDto\n"
            "    refresh player from player\n"
            "    refresh playerView from packet\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone bind rejects non object/tobject targets and tobject sources");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; }\n"
            "zone BrokenZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot packet: PlayerDto\n"
            "    bind player from player\n"
            "    bind playerView from packet\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone projection slot initializer is rejected before type fallback");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BrokenZone {\n"
            "    subject slot player: Player\n"
            "    object slot bad: Int = ToObject(PlayerView, player)\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(
            result, "cannot declare an initializer"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone maintain validates relation/effect contracts");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    maintain poison on player by player\n"
            "    maintain trust between player, enemy by player\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
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

    TEST("zone lifecycle statements can reference declared state aliases");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    apply poisoned by player\n"
            "    link allied by player\n"
            "    detach poisoned by player\n"
            "    unlink allied by player\n"
            "    maintain poisoned by player\n"
            "    maintain allied by player\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
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

    TEST("zone lifecycle state aliases reject wrong verb kind");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    apply allied by player\n"
            "    link poisoned by player\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority requires declared subject slots and explicit by subject alias");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    authority playerView\n"
            "    refresh playerView from player by playerView\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority can require abilities implemented by subject roles");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "ability Commandable { func Command() -> Void; }\n"
            "ability Damageable { func Hit() -> Void; }\n"
            "role PlayerCommander for Player {\n"
            "    impl ability Commandable { func Command() -> Void { Log(1); } }\n"
            "    impl ability Damageable { func Hit() -> Void { Log(1); } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable, Damageable\n"
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

    TEST("zone authority ability requirements reject missing role impls");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "ability Commandable { func Command() -> Void; }\n"
            "ability Healable { func Heal() -> Void; }\n"
            "role PlayerCommander for Player {\n"
            "    impl ability Commandable { func Command() -> Void { Log(1); } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable, Healable\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone rejects mutable rules that omit by subject alias when authority exists");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    apply poison to player\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count >= 1);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone maintain warns on duplicate and conflicting lifecycle rules");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    maintain poison on player by player\n"
            "    maintain poison on player by player\n"
            "    detach poison from player by player\n"
            "    maintain trust between player, enemy by player\n"
            "    maintain trust between player, enemy by player\n"
            "    unlink trust between player, enemy by player\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 4);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone apply rejects unknown effect or target slot");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    apply missing to player\n"
            "    apply poison to missingTarget\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone link rejects unknown relation or endpoint slot");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    link missing between player, enemy\n"
            "    link trust between player, missingEnemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone detach and unlink reject unknown slots");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    detach missing from player\n"
            "    detach poison from missingEnemy\n"
            "    unlink missing between player, enemy\n"
            "    unlink trust between player, missingEnemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 4);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone link enforces relation endpoint types and arity");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "subject Monster { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Monster\n"
            "    relation slot trust: TrustedLink\n"
            "    relation slot crowd: CrowdLink\n"
            "    link trust between player, enemy\n"
            "    link crowd between player, enemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "relation CrowdLink for a: Player, b: Player, c: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone apply enforces effect target type and arity");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "subject Monster { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot curse: Cursed\n"
            "    effect slot split: SplitMind\n"
            "    apply curse to player\n"
            "    apply split to player\n"
            "}\n"
            "effect Cursed for bearer: Monster { }\n"
            "effect SplitMind for a: Player, b: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
