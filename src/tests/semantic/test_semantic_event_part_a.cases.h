static void
test_event_semantics(void)
{
    printf("\n[event_semantics]\n");

    TEST("event subscribe/unsubscribe accepts matching handler");
    {
        const char *source =
            "event OnDamage(amount: Int);\n"
            "func HandleDamage(amount: Int) -> Void {\n"
            "    Log(amount);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    OnDamage += HandleDamage;\n"
            "    OnDamage -= HandleDamage;\n"
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

    TEST("event declaration allows forward-declared parameter types");
    {
        const char *source =
            "event OnSpawn(hero: Hero);\n"
            "class Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func HandleSpawn(hero: Hero) -> Void {\n"
            "    Log(hero.hp);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    OnSpawn += HandleSpawn;\n"
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

    TEST("event subscribe rejects handler arity mismatch");
    {
        const char *source =
            "event OnDamage(amount: Int);\n"
            "func BadHandler() -> Void {\n"
            "}\n"
            "func Main() -> Void {\n"
            "    OnDamage += BadHandler;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(
                result, "parameter count mismatch"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("event subscribe rejects handler return type mismatch");
    {
        const char *source =
            "event OnDamage(amount: Int);\n"
            "func BadHandler(amount: Int) -> Int {\n"
            "    return amount;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    OnDamage += BadHandler;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(
                result, "must return Void"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("event subscribe rejects lambda parameter type mismatch");
    {
        const char *source =
            "event OnDamage(amount: Int);\n"
            "func Main() -> Void {\n"
            "    OnDamage += (amount: String) => { Log(amount); };\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(
                result, "parameter 1 mismatch"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("event invoke validates argument types");
    {
        const char *source =
            "event OnScore(points: Int);\n"
            "func Main() -> Void {\n"
            "    OnScore(\"oops\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "Type mismatch"));
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "cannot assign 'String' to 'Int'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("event invoke with correct args passes");
    {
        const char *source =
            "event OnScore(points: Int);\n"
            "func Main() -> Void {\n"
            "    OnScore(42);\n"
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

    TEST("event unsubscribe rejects handler arity mismatch");
    {
        const char *source =
            "event OnDamage(amount: Int);\n"
            "func BadHandler() -> Void {\n"
            "}\n"
            "func Main() -> Void {\n"
            "    OnDamage -= BadHandler;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(
                result, "parameter count mismatch"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("event invoke rejects argument count mismatch");
    {
        const char *source =
            "event OnScore(points: Int);\n"
            "func Main() -> Void {\n"
            "    OnScore();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(
                result, "'OnScore' expects 1 argument(s), got 0"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("event declaration rejects non-void return type");
    {
        const char *source =
            "event OnScore(points: Int) -> Int;\n"
            "func Main() -> Void {\n"
            "    return;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(
                result, "must return Void"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
