    TEST("CFG body flow accepts return on both if branches");
    {
        const char *source =
            "func Pick(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    } else {\n"
            "        return 2;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let x: Int = Pick(true);\n"
            "    Log(x);\n"
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

    TEST("CFG body flow accepts exhaustive match returns");
    {
        const char *source =
            "func Pick(opt: Option<Int>) -> Int {\n"
            "    match opt {\n"
            "        case .Some(v):\n"
            "            return v;\n"
            "        case .None:\n"
            "            return 0;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let x: Int = Pick(Some(1));\n"
            "    Log(x);\n"
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

    TEST("CFG body flow warns on unreachable statement after return");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    return;\n"
            "    Log(1);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Statement is unreachable after a control-flow terminator"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG body flow warns after all if branches terminate");
    {
        const char *source =
            "func Pick(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    } else {\n"
            "        return 2;\n"
            "    }\n"
            "    return 3;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Statement is unreachable after a control-flow terminator"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG body flow warns after exhaustive match terminates");
    {
        const char *source =
            "func Pick(opt: Option<Int>) -> Int {\n"
            "    match opt {\n"
            "        case .Some(v):\n"
            "            return v;\n"
            "        case .None:\n"
            "            return 0;\n"
            "    }\n"
            "    return 3;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Statement is unreachable after a control-flow terminator"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG body flow warns after loop break terminates path");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    while true {\n"
            "        break;\n"
            "        Log(1);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Statement is unreachable after a control-flow terminator"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG body flow warns after loop continue terminates path");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    while true {\n"
            "        continue;\n"
            "        Log(1);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Statement is unreachable after a control-flow terminator"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG loop move join consumes QubitSlot on break path");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    while true {\n"
            "        let a: QubitSlot = q;\n"
            "        ReleaseQubit(a);\n"
            "        break;\n"
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

    TEST("CFG loop move join rejects consumed QubitSlot on continue backedge");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    while true {\n"
            "        let a: QubitSlot = q;\n"
            "        continue;\n"
            "    }\n"
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

    TEST("CFG static false while does not merge unreachable resource state");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let slot: Slot<Int> = ClaimSlot<Int>();\n"
            "    while false {\n"
            "        Release(slot);\n"
            "    }\n"
            "    Write(slot, 1);\n"
            "    Release(slot);\n"
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

    TEST("CFG defer return does not make following statement unreachable");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    defer {\n"
            "        return;\n"
            "    };\n"
            "    Log(1);\n"
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

    TEST("CFG defer return does not satisfy non-Void all-path return");
    {
        const char *source =
            "func Pick() -> Int {\n"
            "    defer {\n"
            "        return 1;\n"
            "    };\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "may fall through without returning a value"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG defer QubitSlot release does not consume current path");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    defer {\n"
            "        ReleaseQubit(q);\n"
            "    };\n"
            "    QubitState(q);\n"
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

    TEST("CFG defer loop break does not consume current path resource state");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    while true {\n"
            "        defer {\n"
            "            let a: QubitSlot = q;\n"
            "            break;\n"
            "        };\n"
            "        break;\n"
            "    }\n"
            "    QubitState(q);\n"
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

    TEST("CFG dynamic branch defer is explicitly rejected");
    {
        const char *source =
            "func Main(flag: Bool) -> Void {\n"
            "    if flag {\n"
            "        defer {\n"
            "            Log(1);\n"
            "        };\n"
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
            "defer inside dynamic if control is not beta-stable"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG static match defer remains accepted");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    match 1 {\n"
            "        case 1:\n"
            "            defer {\n"
            "                Log(1);\n"
            "            };\n"
            "        default:\n"
            "            Log(0);\n"
            "    }\n"
            "    Log(2);\n"
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

    TEST("CFG dynamic match defer is explicitly rejected");
    {
        const char *source =
            "func Main(value: Int) -> Void {\n"
            "    match value {\n"
            "        case 1:\n"
            "            defer {\n"
            "                Log(1);\n"
            "            };\n"
            "        default:\n"
            "            Log(0);\n"
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
            "defer inside dynamic match control is not beta-stable"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Return type inference rejects incompatible return paths");
    {
        const char *source =
            "func Pick(flag: Bool) {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    }\n"
            "    return \"one\";\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Return types disagree across paths and cannot be inferred"));
        EXPECT(ctx_has_diagnostic_code_from_result(result,
            PGY_CODE_SEM_INFER_REQUIRED));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Return type inference rejects unresolved recursion");
    {
        const char *source =
            "func Loop(n: Int) {\n"
            "    return Loop(n);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Cannot infer the return type of function 'Loop'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "return-type inference is local"));
        EXPECT(ctx_has_diagnostic_code_from_result(result,
            PGY_CODE_SEM_INFER_REQUIRED));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
