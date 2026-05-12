static void
test_qubit_slot_semantics_part_a(void)
{
    printf("\n[qubit_slot]\n");

    TEST("ClaimQubit infers QubitSlot");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *call = make_call("ClaimQubit", NULL, 0, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_QUBIT));
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("QubitSlot let declaration from ClaimQubit passes");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("QubitSlot move in let declaration consumes source");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *decl1 = ast_create_let_declaration("q");
        decl1->data.let_decl.type = ast_create_type("QubitSlot");
        decl1->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl1, ctx);

        ASTNode *decl2 = ast_create_let_declaration("q2");
        decl2->data.let_decl.type = ast_create_type("QubitSlot");
        decl2->data.let_decl.initializer = make_identifier("q", 2);
        type_check_let_decl(decl2, ctx);

        ASTNode *args[1] = { make_identifier("q", 3) };
        ASTNode *call = make_call("QubitState", args, 1, 3);
        type_check_expression(call, ctx);

        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(decl1);
        ast_destroy(decl2);
        ast_destroy(call);
    }

    TEST("Measure requires QubitSlot");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[1] = { make_number(1, 1) };
        ASTNode *call = make_call("Measure", args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("if branches move QubitSlot independently without cross-branch false positive");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *if_stmt = ast_create_if_statement();
        if_stmt->data.if_stmt.condition = make_boolean(true, 2);

        ASTNode *then_block = ast_create_block();
        ASTNode *then_decl = ast_create_let_declaration("a");
        then_decl->data.let_decl.type = ast_create_type("QubitSlot");
        then_decl->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(then_block, then_decl);
        ASTNode *then_release_args[1] = { make_identifier("a", 4) };
        ast_add_statement(then_block, make_call("ReleaseQubit", then_release_args, 1, 4));
        if_stmt->data.if_stmt.then_branch = then_block;

        ASTNode *else_block = ast_create_block();
        ASTNode *else_decl = ast_create_let_declaration("b");
        else_decl->data.let_decl.type = ast_create_type("QubitSlot");
        else_decl->data.let_decl.initializer = make_identifier("q", 5);
        ast_add_statement(else_block, else_decl);
        ASTNode *else_release_args[1] = { make_identifier("b", 6) };
        ast_add_statement(else_block, make_call("ReleaseQubit", else_release_args, 1, 6));
        if_stmt->data.if_stmt.else_branch = else_block;

        type_check_if_stmt(if_stmt, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 7) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 7);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(if_stmt);
        ast_destroy(state_call);
    }

    TEST("match cases move QubitSlot independently without cross-case false positive");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *match = ast_create_match_statement();
        match->data.match_stmt.subject = make_number(0, 2);

        ASTNode *case0 = ast_create_match_case();
        case0->data.match_case.pattern = make_number(0, 3);
        case0->data.match_case.body = ast_create_block();
        ASTNode *case0_decl = ast_create_let_declaration("a");
        case0_decl->data.let_decl.type = ast_create_type("QubitSlot");
        case0_decl->data.let_decl.initializer = make_identifier("q", 4);
        ast_add_statement(case0->data.match_case.body, case0_decl);
        ASTNode *case0_release_args[1] = { make_identifier("a", 5) };
        ast_add_statement(case0->data.match_case.body,
            make_call("ReleaseQubit", case0_release_args, 1, 5));

        ASTNode *case1 = ast_create_match_case();
        case1->data.match_case.pattern = make_number(1, 6);
        case1->data.match_case.body = ast_create_block();
        ASTNode *case1_decl = ast_create_let_declaration("b");
        case1_decl->data.let_decl.type = ast_create_type("QubitSlot");
        case1_decl->data.let_decl.initializer = make_identifier("q", 7);
        ast_add_statement(case1->data.match_case.body, case1_decl);
        ASTNode *case1_release_args[1] = { make_identifier("b", 8) };
        ast_add_statement(case1->data.match_case.body,
            make_call("ReleaseQubit", case1_release_args, 1, 8));

        match->data.match_stmt.cases = calloc(2, sizeof(ASTNode *));
        match->data.match_stmt.cases[0] = case0;
        match->data.match_stmt.cases[1] = case1;
        match->data.match_stmt.case_count = 2;

        type_check_match_stmt(match, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 9) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 9);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(match);
        ast_destroy(state_call);
    }

    TEST("while loop body move with break consumes QubitSlot after loop");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *loop = ast_create_while_loop();
        loop->data.while_loop.condition = make_boolean(true, 2);
        loop->data.while_loop.body = ast_create_block();

        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(loop->data.while_loop.body, moved);
        ASTNode *release_args[1] = { make_identifier("a", 4) };
        ast_add_statement(loop->data.while_loop.body,
            make_call("ReleaseQubit", release_args, 1, 4));
        ASTNode *br = calloc(1, sizeof(ASTNode));
        br->type = AST_BREAK;
        br->line = 5;
        ast_add_statement(loop->data.while_loop.body, br);

        type_check_while_loop(loop, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 6) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 6);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(loop);
        ast_destroy(state_call);
    }

    TEST("while loop continue path reuses moved QubitSlot on next iteration");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *loop = ast_create_while_loop();
        loop->data.while_loop.condition = make_boolean(true, 2);
        loop->data.while_loop.body = ast_create_block();

        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(loop->data.while_loop.body, moved);
        ASTNode *cont = calloc(1, sizeof(ASTNode));
        cont->type = AST_CONTINUE;
        cont->line = 4;
        ast_add_statement(loop->data.while_loop.body, cont);

        type_check_while_loop(loop, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(loop);
    }

    TEST("while loop fallthrough reuses moved QubitSlot on next iteration");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *loop = ast_create_while_loop();
        loop->data.while_loop.condition = make_boolean(true, 2);
        loop->data.while_loop.body = ast_create_block();

        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(loop->data.while_loop.body, moved);
        ASTNode *release_args[1] = { make_identifier("a", 4) };
        ast_add_statement(loop->data.while_loop.body,
            make_call("ReleaseQubit", release_args, 1, 4));

        type_check_while_loop(loop, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(loop);
    }

    TEST("for loop body move conservatively consumes QubitSlot after loop");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *loop = ast_create_for_loop();
        loop->data.for_loop.variable = pergyra_strdup("i");
        loop->data.for_loop.range_start = make_number(0, 2);
        loop->data.for_loop.range_end = make_number(1, 2);
        loop->data.for_loop.body = ast_create_block();

        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(loop->data.for_loop.body, moved);
        ASTNode *release_args[1] = { make_identifier("a", 4) };
        ast_add_statement(loop->data.for_loop.body,
            make_call("ReleaseQubit", release_args, 1, 4));

        type_check_for_loop(loop, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 5) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 5);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(loop);
        ast_destroy(state_call);
    }

    TEST("for loop repeated iterations reuse moved QubitSlot");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *loop = ast_create_for_loop();
        loop->data.for_loop.variable = pergyra_strdup("i");
        loop->data.for_loop.range_start = make_number(0, 2);
        loop->data.for_loop.range_end = make_number(2, 2);
        loop->data.for_loop.body = ast_create_block();

        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(loop->data.for_loop.body, moved);

        type_check_for_loop(loop, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(loop);
    }

    TEST("ReleaseQubit after move reports one diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl1 = ast_create_let_declaration("q");
        decl1->data.let_decl.type = ast_create_type("QubitSlot");
        decl1->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl1, ctx);

        ASTNode *decl2 = ast_create_let_declaration("q2");
        decl2->data.let_decl.type = ast_create_type("QubitSlot");
        decl2->data.let_decl.initializer = make_identifier("q", 2);
        type_check_let_decl(decl2, ctx);

        size_t before = ctx->diagnostic_count;
        ASTNode *release_args[1] = { make_identifier("q", 3) };
        ASTNode *release = make_call("ReleaseQubit", release_args, 1, 3);
        type_check_expression(release, ctx);

        EXPECT(ctx->has_error && ctx->diagnostic_count == before + 1);

        semantic_context_destroy(ctx);
        ast_destroy(decl1);
        ast_destroy(decl2);
        ast_destroy(release);
    }

    TEST("moving consumed QubitSlot into let reports one diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl1 = ast_create_let_declaration("q");
        decl1->data.let_decl.type = ast_create_type("QubitSlot");
        decl1->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl1, ctx);

        ASTNode *decl2 = ast_create_let_declaration("q2");
        decl2->data.let_decl.type = ast_create_type("QubitSlot");
        decl2->data.let_decl.initializer = make_identifier("q", 2);
        type_check_let_decl(decl2, ctx);

        size_t before = ctx->diagnostic_count;
        ASTNode *decl3 = ast_create_let_declaration("q3");
        decl3->data.let_decl.type = ast_create_type("QubitSlot");
        decl3->data.let_decl.initializer = make_identifier("q", 3);
        type_check_let_decl(decl3, ctx);

        EXPECT(ctx->has_error && ctx->diagnostic_count == before + 1);

        semantic_context_destroy(ctx);
        ast_destroy(decl1);
        ast_destroy(decl2);
        ast_destroy(decl3);
    }

    TEST("return skips unreachable QubitSlot move in function body");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("Pass");
        func->data.func_decl.return_type = ast_create_type("QubitSlot");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_identifier("q", 2);
        ast_add_statement(func->data.func_decl.body, ret);

        ASTNode *unreachable = ast_create_let_declaration("q2");
        unreachable->data.let_decl.type = ast_create_type("QubitSlot");
        unreachable->data.let_decl.initializer = make_identifier("q", 3);
        ast_add_statement(func->data.func_decl.body, unreachable);

        type_check_func_decl(func, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("if-return skips unreachable QubitSlot move in branch");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("BranchPass");
        func->data.func_decl.return_type = ast_create_type("QubitSlot");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *if_stmt = ast_create_if_statement();
        if_stmt->data.if_stmt.condition = make_boolean(true, 2);
        ASTNode *then_block = ast_create_block();
        ASTNode *then_ret = ast_create_return_statement();
        then_ret->data.return_stmt.value = make_identifier("q", 3);
        ast_add_statement(then_block, then_ret);
        ASTNode *unreachable = ast_create_let_declaration("q2");
        unreachable->data.let_decl.type = ast_create_type("QubitSlot");
        unreachable->data.let_decl.initializer = make_identifier("q", 4);
        ast_add_statement(then_block, unreachable);
        if_stmt->data.if_stmt.then_branch = then_block;
        ASTNode *else_block = ast_create_block();
        ASTNode *else_ret = ast_create_return_statement();
        else_ret->data.return_stmt.value = make_call("ClaimQubit", NULL, 0, 5);
        ast_add_statement(else_block, else_ret);
        if_stmt->data.if_stmt.else_branch = else_block;
        ast_add_statement(func->data.func_decl.body, if_stmt);

        type_check_func_decl(func, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("QubitSlot function argument moves from named variable");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseQubit");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        FuncParam *param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup("q");
        param->type = ast_create_type("QubitSlot");
        func->data.func_decl.params[0] = param;
        type_check_func_decl(func, ctx);

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        type_check_let_decl(decl, ctx);

        ASTNode *call_args[1] = { make_identifier("q", 3) };
        ASTNode *call = make_call("UseQubit", call_args, 1, 3);
        type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 4) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 4);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
        ast_destroy(decl);
        ast_destroy(call);
        ast_destroy(state_call);
    }

    TEST("QubitSlot function argument rejects anonymous temporary");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseQubit");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        FuncParam *param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup("q");
        param->type = ast_create_type("QubitSlot");
        func->data.func_decl.params[0] = param;
        type_check_func_decl(func, ctx);

        ASTNode *temp_args[1] = { make_call("ClaimQubit", NULL, 0, 2) };
        ASTNode *call = make_call("UseQubit", temp_args, 1, 2);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "bind the value first"));

        semantic_context_destroy(ctx);
        ast_destroy(func);
        ast_destroy(call);
    }

    TEST("own QubitSlot parameter is accepted as explicit transfer boundary");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseOwnedQubit");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        FuncParam *param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup("q");
        param->type = ast_create_type("QubitSlot");
        param->mode = PARAM_MODE_OWN;
        func->data.func_decl.params[0] = param;
        type_check_func_decl(func, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        type_check_let_decl(decl, ctx);

        ASTNode *call_args[1] = { make_identifier("q", 3) };
        ASTNode *call = make_call("UseOwnedQubit", call_args, 1, 3);
        type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 4) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 4);
        type_check_expression(state_call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
        ast_destroy(decl);
        ast_destroy(call);
        ast_destroy(state_call);
    }
    TEST("Slot<Int> parameter types remain rejected");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseSlot");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        FuncParam *param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup("s");
        param->type = make_generic_type("Slot", "Int");
        func->data.func_decl.params[0] = param;

        type_check_func_decl(func, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("Slot<subject> parameter types are accepted with ref qualifier");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Touch(ref s: Slot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2));\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(3, 7);\n"
            "    Touch(s);\n"
            "    Release(s);\n"
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

    TEST("SecureSlot<subject> parameter types are accepted with own qualifier");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Consume(own s: SecureSlot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Vec2> = Vec2(3, 7);\n"
            "    Consume(s);\n"
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

    TEST("ref Slot<subject> parameter allows safe ref helper forwarding");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Forward(ref inner: Slot<Vec2>) -> Void {\n"
            "    Write(inner, Vec2(9, 9));\n"
            "}\n"
            "func Touch(ref s: Slot<Vec2>) -> Void {\n"
            "    Forward(s);\n"
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

    TEST("ref Slot<subject> parameter rejects forwarding into own helper");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Consume(own inner: Slot<Vec2>) -> Void {\n"
            "    Release(inner);\n"
            "}\n"
            "func Touch(ref s: Slot<Vec2>) -> Void {\n"
            "    Consume(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through helper/function call"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

}
