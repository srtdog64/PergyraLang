static void
test_parallel_family_semantics(void)
{
    printf("\n[parallel_family_semantics]\n");

    TEST("async-suspension: await outside async context triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = false;

        ASTNode *num = make_number(42, 1);
        ASTNode *await = ast_create_await_expression(num);
        await->line = 1; await->column = 1;

        type_check_expression(await, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(await);
    }

    TEST("async-suspension: await inside async context passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));

        ASTNode *await = ast_create_await_expression(make_identifier("pending", 1));
        await->line = 1; await->column = 1;

        type_check_expression(await, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(await);
    }

    TEST("parallel-family: spawn expression returns Future<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *num = make_number(42, 1);
        ASTNode *spawn = ast_create_spawn_expression(num);
        spawn->line = 1; spawn->column = 1;

        Type *t = type_check_spawn_expr(spawn, ctx);
        EXPECT(t != NULL);
        EXPECT(t->kind == TYPE_KIND_CONSTRUCTED);
        EXPECT(type_equals(t->data.constructed.constructor, TYPE_FUTURE));
        EXPECT(type_equals(t->data.constructed.args[0], TYPE_INT));
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(spawn);
    }

    TEST("select-readiness: empty select passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *sel = ast_create_select_statement();
        sel->line = 1; sel->column = 1;

        type_check_select_stmt(sel, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(sel);
    }

    TEST("select-readiness: non-channel case is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *sel = ast_create_select_statement();
        ASTNode *bad = ast_create_block();
        ast_add_statement(bad, make_call("Log", NULL, 0, 1));
        sel->data.select_stmt.case_count = 1;
        sel->data.select_stmt.cases = calloc(1, sizeof(ASTNode *));
        sel->data.select_stmt.cases[0] = bad;

        type_check_select_stmt(sel, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "select case must begin"));

        semantic_context_destroy(ctx);
        ast_destroy(sel);
    }
}
