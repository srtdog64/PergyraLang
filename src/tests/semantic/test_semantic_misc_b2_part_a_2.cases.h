    TEST("world composed states warn when raw zone slots are used as derived inputs");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    zone camp: BattleZone\n"
            "    state campLive: zone camp\n"
            "    state mixed: any battle, campLive\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Zones embedded into a world cannot be mutated through the old binding");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    shared hp: Int = 0\n"
            "    func Hurt() -> Void { hp = hp + 1; }\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let player = Player(10);\n"
            "    let battle = BattleZone(player);\n"
            "    let world = GameWorld(battle);\n"
            "    battle.hp = 3;\n"
            "    battle.Hurt();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 3);
        EXPECT(result != NULL && result->warning_count == 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "implicitly copies zone binding 'battle'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "embedding handoff edge is 'battle' -> world 'GameWorld' slot 'battle'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot be mutated via assignment after it was embedded into world 'GameWorld' slot 'battle'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot be mutated via hosted func/action call after it was embedded into world 'GameWorld' slot 'battle'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "derived embedding provenance points to world 'GameWorld' slot 'battle'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "mutate it through world 'GameWorld' slot 'battle' after embedding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Clone makes world zone copy explicit and keeps old binding mutable");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    shared hp: Int = 0\n"
            "    func Hurt() -> Void { hp = hp + 1; }\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let battle = BattleZone(Player(10));\n"
            "    let world = GameWorld(Clone(battle));\n"
            "    battle.hp = 3;\n"
            "    battle.Hurt();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    /* ---- Generic class semantic tests ---- */

    TEST("generic class declaration passes semantic check");
    {
        const char *source =
            "class Box<T> {\n"
            "    let value: T;\n"
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

    TEST("generic class with method passes semantic check");
    {
        const char *source =
            "class Wrapper<T> {\n"
            "    let data: T;\n"
            "    func GetData(self) -> T {\n"
            "        return self.data;\n"
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

    TEST("generic class with concrete usage passes semantic check");
    {
        const char *source =
            "class Pair<T> {\n"
            "    let first: T;\n"
            "    let second: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Pair<Int> = Pair(3, 7);\n"
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

    TEST("generic function where clause accepts matching exact bound");
    {
        const char *source =
            "func OnlyInt<T>(value: T) -> T where T: Int {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let x: Int = OnlyInt(3);\n"
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

    TEST("generic function where clause rejects non-matching exact bound");
    {
        const char *source =
            "func OnlyInt<T>(value: T) -> T where T: Int {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let y = OnlyInt(\"nope\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic class where clause rejects unknown bound type");
    {
        const char *source =
            "class Box<T> where T: MissingConstraint {\n"
            "    let value: T;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown constraint type 'MissingConstraint' in where clause"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role where clause rejects unknown bound type");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "role Support<T> for T where T: MissingAbility {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown constraint type 'MissingAbility' in where clause"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic function where clause accepts ability-style role constraint");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Player { let hp: Int; }\n"
            "role PlayerComparable for Player {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "func Keep<T>(value: T) -> T where T: Comparable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Player = Player(3);\n"
            "    let q: Player = Keep(p);\n"
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

    TEST("generic function where clause rejects missing ability-style role constraint");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Merchant { let trust: Int; }\n"
            "func Keep<T>(value: T) -> T where T: Comparable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let m: Merchant = Merchant(1);\n"
            "    let n: Merchant = Keep(m);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Comparable'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual type args are"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic function where clause accepts multiple ability-style bounds");
    {
        const char *source =
            "ability Comparable { }\n"
            "ability Cloneable { }\n"
            "subject Player { let hp: Int; }\n"
            "role PlayerCapabilities for Player {\n"
            "    impl ability Comparable { }\n"
            "    impl ability Cloneable { }\n"
            "}\n"
            "func Keep<T>(value: T) -> T where T: Comparable + Cloneable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Player = Player(3);\n"
            "    let q: Player = Keep(p);\n"
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

    TEST("generic function where clause rejects missing one of multiple ability-style bounds");
    {
        const char *source =
            "ability Comparable { }\n"
            "ability Cloneable { }\n"
            "subject Player { let hp: Int; }\n"
            "role PlayerComparable for Player {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "func Keep<T>(value: T) -> T where T: Comparable + Cloneable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Player = Player(3);\n"
            "    let q: Player = Keep(p);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Cloneable'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic class instantiation enforces exact where constraint");
    {
        const char *source =
            "class Box<T> where T: Int {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let ok: Box<Int> = Box(3);\n"
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

    TEST("generic class instantiation rejects non matching where constraint");
    {
        const char *source =
            "class Box<T> where T: Int {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let bad: Box<String> = Box(\"oops\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
