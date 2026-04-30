    TEST("Match subject outside beta-stable scalar or algebraic surface is rejected");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let value: String = \"a\";\n"
            "    match value {\n"
            "        case \"a\":\n"
            "            Log(1);\n"
            "        default:\n"
            "            Log(0);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Match subject type 'String' is not beta-stable"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("None without contextual Option annotation is rejected");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let maybe = None();\n"
            "    Log(0);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Cannot infer Option<T> from None without an explicit annotation"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG dynamic loop defer is explicitly rejected");
    {
        const char *source =
            "func Main(flag: Bool) -> Void {\n"
            "    while flag {\n"
            "        defer {\n"
            "            Log(1);\n"
            "        };\n"
            "        break;\n"
            "    }\n"
            "    Log(2);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "defer inside dynamic while control is not beta-stable"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG slot release in terminating branch does not poison fallthrough path");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let s = ClaimSlot<Int>();\n"
            "    if true {\n"
            "        Release(s);\n"
            "        return;\n"
            "    }\n"
            "    Read(s);\n"
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

    TEST("CFG slot release in fallthrough branch poisons joined path");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let s = ClaimSlot<Int>();\n"
            "    if true {\n"
            "        Release(s);\n"
            "    }\n"
            "    Read(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Cannot read from released slot"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG own subject move in terminating branch does not poison fallthrough path");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Take(own hero: Hero) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let hero: Hero = Hero(10);\n"
            "    if true {\n"
            "        Take(hero);\n"
            "        return;\n"
            "    }\n"
            "    Log(hero.hp);\n"
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

    TEST("CFG own subject move in fallthrough branch poisons joined path");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Take(own hero: Hero) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let hero: Hero = Hero(10);\n"
            "    if true {\n"
            "        Take(hero);\n"
            "    }\n"
            "    Log(hero.hp);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "was moved or released and cannot be used again"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel task return does not terminate outer path");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    parallel {\n"
            "        return;\n"
            "    }\n"
            "    Log(1);\n"
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

    TEST("CFG parallel task move consumes resource after join");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    parallel {\n"
            "        let a: QubitSlot = q;\n"
            "    }\n"
            "    QubitState(q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "was moved or released and cannot be used again"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel task own subject move consumes boundary after join");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Take(own hero: Hero) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let hero: Hero = Hero(10);\n"
            "    parallel {\n"
            "        Take(hero);\n"
            "    }\n"
            "    Log(hero.hp);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "was moved or released and cannot be used again"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel tasks reject double resource consume");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    parallel {\n"
            "        let a: QubitSlot = q;\n"
            "        let b: QubitSlot = q;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Parallel tasks cannot consume the same resource"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel tasks reject double own subject consume");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Take(own hero: Hero) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let hero: Hero = Hero(10);\n"
            "    parallel {\n"
            "        Take(hero);\n"
            "        Take(hero);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Parallel tasks cannot consume the same resource/boundary"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel tasks reject ref and own subject boundary conflict");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Inspect(ref hero: Hero) -> Void {\n"
            "    Log(hero.hp);\n"
            "}\n"
            "func Take(own hero: Hero) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let hero: Hero = Hero(10);\n"
            "    parallel {\n"
            "        Inspect(hero);\n"
            "        Take(hero);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Parallel tasks cannot consume the same resource/boundary"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
