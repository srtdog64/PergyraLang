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

