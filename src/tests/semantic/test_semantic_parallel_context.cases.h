static void
test_parallel_context_semantics(void)
{
    printf("\n[parallel_context_semantics]\n");

    TEST("parallel-rejected: write-write slot conflict");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    with slot<Int> as s {\n"
            "        parallel {\n"
            "            Write(s, 1);\n"
            "            Write(s, 2);\n"
            "        }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);

        bool found = false;
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message,
                           "Parallel context slot conflict") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("parallel-safe: read-write slot race warns");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    with slot<Int> as s {\n"
            "        parallel {\n"
            "            let a = Read(s);\n"
            "            Write(s, 2);\n"
            "        }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        bool found = false;
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message,
                           "Parallel context race risk") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("parallel-rejected: SecureSlot access is capability-serialized");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Int> = 7;\n"
            "    parallel {\n"
            "        Write(s, 1, s_token);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        bool found = false;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message,
                           "Parallel context does not permit SecureSlot access yet") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("parallel-rejected: DeviceSlot operations stay serialized");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let dev: DeviceSlot<Int> = ClaimDeviceSlot();\n"
            "    parallel {\n"
            "        DeviceWrite(dev, 1);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        bool found = false;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message,
                           "Parallel context does not permit DeviceSlot operations yet") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("parallel-rejected: derived secure-effect helper calls stay serialized");
    {
        const char *source =
            "func Touch(s: SecureSlot<Int>, s_token: Token<Int>) -> Void {\n"
            "    Write(s, 1, s_token);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Int> = 7;\n"
            "    parallel {\n"
            "        Touch(s, s_token);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        bool found = false;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message,
                           "Parallel context does not permit calling secure-effect function 'Touch'") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("parallel-rejected: declared secure-effect helper calls stay serialized");
    {
        const char *source =
            "/// @effects secure\n"
            "func Touch() -> Void {\n"
            "    Log(\"secure\");\n"
            "}\n"
            "func Main() -> Void {\n"
            "    parallel {\n"
            "        Touch();\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        bool found = false;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message,
                           "Parallel context does not permit calling secure-effect function 'Touch'") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("parallel-rejected: token-capability helper calls stay serialized");
    {
        const char *source =
            "func UseToken(tok: Token<Int>) -> Void {\n"
            "    Log(\"secure-token\");\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Int> = 7;\n"
            "    parallel {\n"
            "        UseToken(s_token);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        bool found = false;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message,
                           "Parallel context does not permit calling secure-effect function 'UseToken'") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("parallel-rejected: declared secure-effect method calls stay serialized");
    {
        const char *source =
            "class Vault {\n"
            "    /// @effects secure\n"
            "    func Touch() -> Void {\n"
            "        Log(\"secure\");\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vault = Vault();\n"
            "    parallel {\n"
            "        v.Touch();\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        bool found = false;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message,
                           "Parallel context does not permit calling secure-effect method 'Vault.Touch'") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("parallel-rejected: derived secure-effect method calls stay serialized");
    {
        const char *source =
            "class Vault {\n"
            "    func Touch(s: SecureSlot<Int>, s_token: Token<Int>) -> Void {\n"
            "        Write(s, 1, s_token);\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vault = Vault();\n"
            "    let s: SecureSlot<Int> = 7;\n"
            "    parallel {\n"
            "        v.Touch(s, s_token);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        bool found = false;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        if (result != NULL) {
            for (size_t i = 0; i < result->diagnostic_count; i++) {
                if (strstr(result->diagnostics[i]->message,
                           "Parallel context does not permit calling secure-effect method 'Vault.Touch'") != NULL) {
                    found = true;
                    break;
                }
            }
        }
        EXPECT(found);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
