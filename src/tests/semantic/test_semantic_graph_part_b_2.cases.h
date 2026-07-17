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
        /* The nested constructed type is deliberately NOT Channel<...>:
         * Channel cannot cross param/return boundaries (docs/189 C12), and
         * the alias correctly does not evade that rule. Option<Array<Int>>
         * keeps what this test is about -- a forward alias materializing a
         * nested constructed type. */
        const char *source =
            "func Echo(value: Later) -> Later {\n"
            "    return value;\n"
            "}\n"
            "type Later = Option<Array<Int>>;\n";
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
