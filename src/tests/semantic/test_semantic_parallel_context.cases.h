static void
test_parallel_context_semantics(void)
{
    printf("\n[parallel_context_semantics]\n");

    TEST("boundary-witness oracle matches op_guard");
    {
        EXPECT(pgy_boundary_witness_guard_accepts(
            0, false, PGY_BOUNDARY_WITNESS_OP_ACQ_READ));
        EXPECT(pgy_boundary_witness_guard_accepts(
            1, false, PGY_BOUNDARY_WITNESS_OP_ACQ_READ));
        EXPECT(!pgy_boundary_witness_guard_accepts(
            0, true, PGY_BOUNDARY_WITNESS_OP_ACQ_READ));
        EXPECT(pgy_boundary_witness_guard_accepts(
            0, false, PGY_BOUNDARY_WITNESS_OP_ACQ_WRITE));
        EXPECT(!pgy_boundary_witness_guard_accepts(
            1, false, PGY_BOUNDARY_WITNESS_OP_ACQ_WRITE));
        EXPECT(!pgy_boundary_witness_guard_accepts(
            0, true, PGY_BOUNDARY_WITNESS_OP_ACQ_WRITE));
        EXPECT(pgy_boundary_witness_guard_accepts(
            1, true, PGY_BOUNDARY_WITNESS_OP_RELEASE));
    }

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
        EXPECT(result != NULL
            && result->boundary_witness_summary.acq_write_count >= 2);
        EXPECT(result != NULL
            && result->boundary_witness_summary.rejected_count >= 1);
        EXPECT(result != NULL
            && pgy_boundary_witness_summary_is_guard_consistent(
                &result->boundary_witness_summary));

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

    TEST("parallel-safe: shared slot reads satisfy boundary witness");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    with slot<Int> as s {\n"
            "        parallel {\n"
            "            let a = Read(s);\n"
            "            let b = Read(s);\n"
            "        }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL
            && result->boundary_witness_summary.acq_read_count >= 2);
        EXPECT(result != NULL
            && result->boundary_witness_summary.rejected_count == 0);
        EXPECT(result != NULL
            && pgy_boundary_witness_summary_is_guard_consistent(
                &result->boundary_witness_summary));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("parallel-rejected: read-write slot race fails op_guard");
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
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(result != NULL
            && result->boundary_witness_summary.acq_read_count >= 1);
        EXPECT(result != NULL
            && result->boundary_witness_summary.acq_write_count >= 1);
        EXPECT(result != NULL
            && result->boundary_witness_summary.rejected_count >= 1);
        EXPECT(result != NULL
            && pgy_boundary_witness_summary_is_guard_consistent(
                &result->boundary_witness_summary));

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

    TEST("parallel-rejected: write-read slot race fails op_guard");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    with slot<Int> as s {\n"
            "        parallel {\n"
            "            Write(s, 2);\n"
            "            let a = Read(s);\n"
            "        }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(result != NULL
            && result->boundary_witness_summary.acq_read_count >= 1);
        EXPECT(result != NULL
            && result->boundary_witness_summary.acq_write_count >= 1);
        EXPECT(result != NULL
            && result->boundary_witness_summary.rejected_count >= 1);
        EXPECT(result != NULL
            && pgy_boundary_witness_summary_is_guard_consistent(
                &result->boundary_witness_summary));

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

    TEST("parallel-rejected: collection mutators stay serialized");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *array_args[1] = { TYPE_INT };
        Type *array_type = type_create_constructed(TYPE_ARRAY, array_args, 1);
        ASTNode *call_args[2] = {
            make_identifier("items", 1),
            make_number(1, 1)
        };
        ASTNode *call = make_call("ArrayPush", call_args, 2, 1);

        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_parallel = true;
        scope_declare(ctx->scope,
            symbol_create_variable("items", array_type, 1, 1));

        type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "does not permit collection mutator 'ArrayPush'"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("parallel-rejected: map mutators stay serialized");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *map_args[2] = { TYPE_INT, TYPE_INT };
        Type *map_type = type_create_constructed(TYPE_HASHMAP, map_args, 2);
        ASTNode *call_args[3] = {
            make_identifier("table", 1),
            make_number(1, 1),
            make_number(2, 1)
        };
        ASTNode *call = make_call("MapSet", call_args, 3, 1);

        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_parallel = true;
        scope_declare(ctx->scope,
            symbol_create_variable("table", map_type, 1, 1));

        type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "does not permit map mutator 'MapSet'"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }
}
