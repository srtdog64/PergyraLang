    TEST("party role slot reports required and actual generic ability mismatch");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<String> { func Ping() -> Void { return; } }\n"
            "}\n"
            "party RaidTeam {\n"
            "    role slot fighter: Combatable<Int>\n"
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
            "consumer path is party role slot 'fighter'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual implementation is 'Combatable<String>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role impl rejects generic type arguments when ability is non-generic");
    {
        const char *source =
            "ability Combatable { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<Int> { func Ping() -> Void { return; } }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not accept generic type arguments in impl clauses"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "ability declaration 'Combatable' is non-generic"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role impl rejects wrong generic ability arity");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<Int, String> { func Ping() -> Void { return; } }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires between 1 and 1 generic argument(s) in impl clauses, got 2"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "impl ability uses 'Combatable<Int, String>'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic ability declaration accepts generic fields entries and method signatures");
    {
        const char *source =
            "ability Bufferable<T> {\n"
            "    fields items: List<T>;\n"
            "    func Put(value: T) -> Void;\n"
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

    TEST("generic ability declaration accepts where clause bounds");
    {
        const char *source =
            "ability Bufferable<T> where T: String {\n"
            "    fields items: List<T>;\n"
            "    func Put(value: T) -> Void;\n"
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

    TEST("generic function declaration accepts default type argument");
    {
        const char *source =
            "func Identity<T = Int>(value: T) -> T {\n"
            "    return value;\n"
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

    TEST("generic class declaration accepts default type argument");
    {
        const char *source =
            "class Box<T = Int> {\n"
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

    TEST("generic ability declaration accepts default type argument");
    {
        const char *source =
            "ability Bufferable<T = Int> {\n"
            "    func Put(value: T) -> Void;\n"
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

    TEST("generic function declaration rejects default type argument that violates where bound");
    {
        const char *source =
            "func Identity<T = String>(value: T) -> T where T: Int {\n"
            "    return value;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Default generic type argument 'String' does not satisfy constraint 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "function 'Identity' declares 'T = String'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic class declaration rejects default type argument that violates where bound");
    {
        const char *source =
            "class Box<T = String> where T: Int {\n"
            "    let value: T;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Default generic type argument 'String' does not satisfy constraint 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "class 'Box' declares 'T = String'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic ability declaration rejects default type argument that violates where bound");
    {
        const char *source =
            "ability Bufferable<T = String> where T: Int {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Default generic type argument 'String' does not satisfy constraint 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "ability 'Bufferable' declares 'T = String'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic role declaration rejects default type argument that violates where bound");
    {
        const char *source =
            "subject Host { let hp: Int; }\n"
            "role BufferRole<T = String> for Host where T: Int {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Default generic type argument 'String' does not satisfy constraint 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "role 'BufferRole' declares 'T = String'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic function declaration rejects non trailing default type parameter");
    {
        const char *source =
            "func Pair<T = Int, U>(left: T, right: U) -> U {\n"
            "    return right;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Non-trailing default generic parameter 'U' in function declaration"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "generic defaults are only closed for trailing parameters"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic ability declaration rejects non trailing default type parameter");
    {
        const char *source =
            "ability Bufferable<T = Int, U> {\n"
            "    func Put(left: T, right: U) -> Void;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Non-trailing default generic parameter 'U' in ability declaration"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic party declaration accepts default type argument");
    {
        const char *source =
            "party RaidTeam<T = Int> {\n"
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

    TEST("generic roster declaration accepts default type argument");
    {
        const char *source =
            "roster RaidRoster<T = Int> {\n"
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

    TEST("generic function call uses trailing default type argument for where-bound validation");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Token { let id: Int; }\n"
            "role TokenComparable for Token {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "func Keep<T, U = Token>(value: T) -> T where U: Comparable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let value: Int = Keep(7);\n"
            "    Log(ToString(value));\n"
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

    TEST("generic class default type argument resolves when omitted");
    {
        const char *source =
            "class Box<T = Int> {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let b: Box = Box(1);\n"
            "    Log(ToString(b.value));\n"
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
