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
}
