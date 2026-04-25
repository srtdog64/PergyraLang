static void
test_misc_grammar_edges(void)
{
    printf("\n[misc_grammar]\n");

    TEST("unsafe block type-checks its body");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *unsafe_block = ast_create_unsafe_block(body);
        unsafe_block->line = 1; unsafe_block->column = 1;

        type_check_statement(unsafe_block, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(unsafe_block);
    }

    TEST("defer statement type-checks its body");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *defer_stmt = ast_create_defer_statement(body);
        defer_stmt->line = 1; defer_stmt->column = 1;

        type_check_statement(defer_stmt, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(defer_stmt);
    }

    TEST("defer statement fallback restores QubitSlot resource state");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Symbol *q = symbol_create_variable("q", TYPE_QUBIT, 1, 1);
        scope_declare(ctx->scope, q);

        ASTNode *body = ast_create_block();
        ASTNode *release_args[1] = { make_identifier("q", 2) };
        ast_add_statement(body, make_call("ReleaseQubit", release_args, 1, 2));
        ASTNode *defer_stmt = ast_create_defer_statement(body);
        defer_stmt->line = 1; defer_stmt->column = 1;

        type_check_statement(defer_stmt, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(!q->is_consumed);

        semantic_context_destroy(ctx);
        ast_destroy(defer_stmt);
    }

    TEST("bind statement is accepted by semantic pass");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *bind = ast_create_bind_statement("team", "fighter", "Warrior");
        bind->line = 1; bind->column = 1;

        type_check_statement(bind, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(bind);
    }

    TEST("else if chain type-checks nested branch structure");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *then_block = ast_create_block();
        ASTNode *then_args[1] = { make_number(1, 1) };
        ast_add_statement(then_block, make_call("Log", then_args, 1, 1));

        ASTNode *else_then = ast_create_block();
        ASTNode *else_then_args[1] = { make_number(2, 1) };
        ast_add_statement(else_then, make_call("Log", else_then_args, 1, 1));

        ASTNode *else_final = ast_create_block();
        ASTNode *else_final_args[1] = { make_number(3, 1) };
        ast_add_statement(else_final, make_call("Log", else_final_args, 1, 1));

        ASTNode *nested_if = ast_create_if_statement();
        nested_if->data.if_stmt.condition = ast_create_boolean(false);
        nested_if->data.if_stmt.then_branch = else_then;
        nested_if->data.if_stmt.else_branch = else_final;

        ASTNode *outer_if = ast_create_if_statement();
        outer_if->data.if_stmt.condition = ast_create_boolean(true);
        outer_if->data.if_stmt.then_branch = then_block;
        outer_if->data.if_stmt.else_branch = nested_if;
        outer_if->line = 1; outer_if->column = 1;

        type_check_statement(outer_if, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(outer_if);
    }

    TEST("CFG body flow rejects missing return on one branch");
    {
        const char *source =
            "func Pick(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "may fall through without returning a value"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

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

    TEST("CFG parallel tasks allow shared ref subject boundary reads");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Inspect(ref hero: Hero) -> Void {\n"
            "    Log(hero.hp);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let hero: Hero = Hero(10);\n"
            "    parallel {\n"
            "        Inspect(hero);\n"
            "        Inspect(hero);\n"
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

    TEST("CFG spawn rejects borrowed subject boundary crossing");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Inspect(ref hero: Hero) -> Void {\n"
            "    Log(hero.hp);\n"
            "}\n"
            "func Main() -> Void with effects remote {\n"
            "    let hero: Hero = Hero(10);\n"
            "    let pending: Future<Void> = spawn Inspect(hero);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot cross spawn boundary"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG spawn allows copy ref boundary crossing");
    {
        const char *source =
            "func Inspect(ref value: Int) -> Int {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void with effects remote {\n"
            "    let value: Int = 10;\n"
            "    let pending: Future<Int> = spawn Inspect(value);\n"
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

    TEST("generic own/ref parameter requires ownership classifier fact");
    {
        const char *source =
            "func BorrowGeneric<T>(ref value: T) -> Void {\n"
            "    return;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "parameter mode requires a boundary-visible type"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "type 'T' is not a copy-visible value"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG spawn rejects anonymous async body until capture lifetime is closed");
    {
        const char *source =
            "func Main() -> Void with effects remote {\n"
            "    let pending = spawn async () {\n"
            "        Log(1);\n"
            "    };\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Anonymous async spawn bodies are beta-out-of-scope"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel channel send consumes resource after join");
    {
        const char *source =
            "func Main(ch: Channel<QubitSlot>) -> Void {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    parallel {\n"
            "        ch <- q;\n"
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

    TEST("CFG parallel channel sends reject double resource consume");
    {
        const char *source =
            "func Main(ch: Channel<QubitSlot>) -> Void {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    parallel {\n"
            "        ch <- q;\n"
            "        ch <- q;\n"
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

    /* ---- RemoteFuture design: await returns Result, Claim/Read/Write/Release rejected ---- */

    TEST("await on local Future<Int> returns Int (not Result)");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("local_future", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("local_future", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("await on RemoteFuture<Int> returns Result<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("remote_future", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("remote_future", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_RESULT));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("Read on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *read_args[1] = { make_identifier("rf", 1) };
        ASTNode *read_call = make_call("Read", read_args, 1, 1);
        type_check_expression(read_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Read"));

        semantic_context_destroy(ctx);
        ast_destroy(read_call);
    }

    TEST("Write on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *write_args[2] = { make_identifier("rf", 1), make_number(42, 1) };
        ASTNode *write_call = make_call("Write", write_args, 2, 1);
        type_check_expression(write_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Write"));

        semantic_context_destroy(ctx);
        ast_destroy(write_call);
    }

    TEST("Release on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *release_args[1] = { make_identifier("rf", 1) };
        ASTNode *release_call = make_call("Release", release_args, 1, 1);
        type_check_expression(release_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Release"));

        semantic_context_destroy(ctx);
        ast_destroy(release_call);
    }

    TEST("Option<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *opt = make_generic_type("Option", "Int");
        Type *t = resolve_type_node(opt, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);
        semantic_context_destroy(ctx);
        ast_destroy(opt);
    }

    TEST("type alias annotations resolve in program declarations");
    {
        const char *source =
            "type UserId = Int;\n"
            "type NameList = List<String>;\n"
            "func Main() -> Void {\n"
            "    let id: UserId = 7;\n"
            "    let names: NameList = ListNew();\n"
            "    ListPush(names, \"gyri\");\n"
            "    Log(id);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("function-body let with annotation and no initializer is parser-rejected");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let x: Int;\n"
            "    Log(x);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(parser_has_error(parser));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("function-body let with aggregate annotation and no initializer is parser-rejected");
    {
        const char *source =
            "class Box<T> {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let b: Box<Int>;\n"
            "    Log(0);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(parser_has_error(parser));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject field let with no initializer does not trigger the uninit-local guard");
    {
        /* Class/subject fields use a distinct parser path (ClassField), so
         * the function-body uninit-local guard must not fire on them.  The
         * substring check stays negative to pin the behavior in place. */
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    Log(1);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(!ctx_has_diagnostic_substring_from_result(result,
            "type annotation but no initializer"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

