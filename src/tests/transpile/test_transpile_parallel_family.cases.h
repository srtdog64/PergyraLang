static void
test_parallel_family_emit(void)
{
    printf("\n[parallel_family_emit]\n");

    TEST("parallel-family: spawn emits wrapper-based task launch");
    {
        ASTNode *worker = calloc(1, sizeof(ASTNode));
        worker->type = AST_FUNC_DECL;
        worker->data.func_decl.name = pergyra_strdup("DoWork");
        worker->data.func_decl.return_type = make_type_node("Void");
        worker->data.func_decl.body = ast_create_block();

        ASTNode *spawn_node = calloc(1, sizeof(ASTNode));
        spawn_node->type = AST_SPAWN_EXPR;
        spawn_node->data.spawn_expr.function = make_identifier("DoWork", 1);

        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, spawn_node);
        ASTNode *main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = pergyra_strdup("Main");
        main_fn->data.func_decl.return_type = make_type_node("Void");
        main_fn->data.func_decl.body = main_body;

        ASTNode *stmts[2] = { worker, main_fn };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT(ctx->out->data != NULL);
        EXPECT(strstr(ctx->out->data, "spawn") != NULL);
        EXPECT(strstr(ctx->out->data, "pgy_spawn_wrapper") != NULL);
        EXPECT(strstr(ctx->wrappers->data, "DoWork") != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(prog);
    }

    TEST("select-readiness: select emits round-robin switch");
    {
        ASTNode select_node; memset(&select_node, 0, sizeof(select_node));
        select_node.type = AST_SELECT_STMT;
        ASTNode *recv = ast_create_channel_recv(make_identifier("ch", 1));
        ASTNode *assign = ast_create_assignment(make_identifier("v", 1), recv);
        ASTNode *body = ast_create_block();
        ASTNode *case_block = ast_create_block();
        ast_add_statement(case_block, assign);
        ast_add_statement(case_block, body);
        ASTNode *cases[1] = { case_block };
        select_node.data.select_stmt.cases = cases;
        select_node.data.select_stmt.case_count = 1;

        ASTNode default_body; memset(&default_body, 0, sizeof(default_body));
        default_body.type = AST_BLOCK;
        default_body.data.block.statements = NULL;
        default_body.data.block.count = 0;
        select_node.data.select_stmt.default_case = &default_body;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", (ASTNode *[]){ make_number(4, 1) }, 1, 1), 1),
            ctx);
        emit_select_stmt(&select_node, ctx);

        EXPECT(ctx->out->data != NULL);
        EXPECT(strstr(ctx->out->data, "select") != NULL);
        EXPECT(strstr(ctx->out->data, "default") != NULL);
        EXPECT(strstr(ctx->out->data, "switch (_sel_start_") != NULL);
        EXPECT(strstr(ctx->out->data, "_sel_rr_") != NULL);
        EXPECT(strstr(ctx->out->data, "static _Atomic unsigned int _sel_rr_") != NULL);
        EXPECT(strstr(ctx->out->data, "atomic_fetch_add_explicit(&_sel_rr_") != NULL);

        transpiler_ctx_destroy(ctx);
    }

    TEST("async-suspension: async block emits detached coroutine wrapper");
    {
        ASTNode *ret_stmt = make_return(make_number(1, 1), 1);
        ASTNode *stmts[1] = { ret_stmt };

        ASTNode async_block; memset(&async_block, 0, sizeof(async_block));
        async_block.type = AST_ASYNC_BLOCK;
        async_block.data.async_block.statements = stmts;
        async_block.data.async_block.statement_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_statement(&async_block, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_lane_spawn_dispatch");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_lane_detach");
        EXPECT_STR_CONTAINS(ctx->wrappers->data, "return 1;");

        ast_destroy(ret_stmt);
        transpiler_ctx_destroy(ctx);
    }
}
