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

    TEST("ReadView return escape uses pin escape diagnostic");
    {
        const char *source =
            "func Leak() -> ReadView<Int> {\n"
            "    let s: Slot<Int> = ClaimSlot();\n"
            "    let r: ReadView<Int> = ViewRead(s);\n"
            "    return r;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Pinned view cannot escape through return"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("await with active ReadView uses pin await diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *future_args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE,
                                                    future_args, 1);
        Symbol *view = symbol_create_view("r",
            type_create_read_view(TYPE_INT), "s", 1, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));
        scope_declare(ctx->scope, view);

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("pending", 1));
        await_expr->line = 1; await_expr->column = 1;

        type_check_expression(await_expr, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "cannot cross an await suspension boundary"));

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("parallel with active ReadView uses pin conflict diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Symbol *view = symbol_create_view("r",
            type_create_read_view(TYPE_INT), "s", 1, 1);
        scope_declare(ctx->scope, view);

        ASTNode *parallel = ast_create_parallel_block();
        parallel->line = 1; parallel->column = 1;

        type_check_parallel_block(parallel, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "cannot cross a parallel boundary"));

        semantic_context_destroy(ctx);
        ast_destroy(parallel);
    }

    TEST("ViewRead inside parallel task is rejected by pin conflict diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_parallel = true;

        Type *slot_type = type_create_slot(TYPE_INT, false);
        Symbol *slot = symbol_create_slot("s", slot_type, false, NULL, 1, 1);
        scope_declare(ctx->scope, slot);

        ASTNode *decl = ast_create_let_declaration("r");
        ASTNode *args[1] = { make_identifier("s", 1) };
        decl->data.let_decl.type = make_generic_type("ReadView", "Int");
        decl->data.let_decl.initializer = make_call("ViewRead", args, 1, 1);
        decl->line = 1; decl->column = 1;

        type_check_let_decl(decl, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "cannot be acquired inside a parallel task"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("ViewRead rejects QubitSlot with pin qubit diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        scope_declare(ctx->scope,
            symbol_create_variable("q", TYPE_QUBIT, 1, 1));

        ASTNode *args[1] = { make_identifier("q", 1) };
        ASTNode *call = make_call("ViewRead", args, 1, 1);

        type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "ViewRead cannot pin QubitSlot resources"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
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
