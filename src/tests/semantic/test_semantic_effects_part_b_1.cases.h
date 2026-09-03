    TEST("class constructor rejects Channel field storage in expression position");
    {
        const char *source =
            "class ChannelBox {\n"
            "    let ch: Channel<Int>;\n"
            "}\n"
            "func Take(box: ChannelBox) -> Void {}\n"
            "func Main() -> Void {\n"
            "    Take(ChannelBox());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "default-initialize Channel<T> field 'ch'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone constructor rejects default Channel shared field storage");
    {
        const char *source =
            "zone WorkZone {\n"
            "    shared ch: Channel<Int>\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let work: WorkZone = WorkZone();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "default-initialize Channel<T> field 'ch'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("spawn and channel send infer remote effect on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_async_function("Dispatch", true);
        func->data.async_func_decl.return_type = ast_create_type("Int");
        func->data.async_func_decl.body = ast_create_block();

        ASTNode *channel = ast_create_let_declaration("ch");
        ASTNode *capacity = make_number(1, 1);
        ASTNode *channel_args[] = { capacity };
        channel->data.let_decl.type = make_generic_type("Channel", "Int");
        channel->data.let_decl.initializer =
            make_call("Channel", channel_args, 1, 1);
        ast_add_statement(func->data.async_func_decl.body, channel);

        ASTNode *pending = ast_create_let_declaration("pending");
        pending->data.let_decl.initializer =
            ast_create_spawn_expression(make_number(42, 2));
        ast_add_statement(func->data.async_func_decl.body, pending);

        ASTNode *send = ast_create_channel_send(make_identifier("ch", 3),
                                                make_number(7, 3));
        ast_add_statement(func->data.async_func_decl.body, send);

        ast_add_statement(func->data.async_func_decl.body,
            ast_create_await_expression(make_identifier("pending", 4)));

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 4);
        ast_add_statement(func->data.async_func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Dispatch");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        uint32_t body_summary = sym != NULL && sym->type != NULL
            ? type_function_body_summary(sym->type) : BODY_SUMMARY_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && (body_summary & BODY_SUMMARY_EFFECTS) != 0
            && (body_summary & BODY_SUMMARY_SPAWNS_TASK) != 0
            && (body_summary & BODY_SUMMARY_SENDS_CHANNEL) != 0
            && (body_summary & BODY_SUMMARY_MAY_RETURN) != 0);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("function body summary records param boundary modes");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("Boundary");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 2;
        func->data.func_decl.params = calloc(2, sizeof(FuncParam *));
        func->data.func_decl.params[0] =
            make_func_param("owned", ast_create_type("Int"));
        func->data.func_decl.params[0]->mode = PARAM_MODE_OWN;
        func->data.func_decl.params[1] =
            make_func_param("borrowed", ast_create_type("Int"));
        func->data.func_decl.params[1]->mode = PARAM_MODE_REF;

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Boundary");
        uint32_t body_summary = sym != NULL && sym->type != NULL
            ? type_function_body_summary(sym->type) : BODY_SUMMARY_NONE;
        EXPECT(!ctx->has_error
            && (body_summary & BODY_SUMMARY_MOVES_PARAM) != 0
            && (body_summary & BODY_SUMMARY_BORROWS_PARAM) != 0);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("function call propagates callee body summary");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *worker_type = type_create_function(NULL, 0, TYPE_VOID);
        Symbol *worker_sym;

        worker_type->data.function.effect_mask = EFFECT_REMOTE;
        worker_type->data.function.body_summary_mask =
            BODY_SUMMARY_EFFECTS
            | BODY_SUMMARY_SPAWNS_TASK
            | BODY_SUMMARY_SENDS_CHANNEL
            | BODY_SUMMARY_DROPS_RESOURCE
            | BODY_SUMMARY_MAY_RETURN;
        worker_sym = symbol_create_function("Worker", worker_type, 1, 1);
        scope_declare(ctx->scope, worker_sym);

        ASTNode *func = ast_create_function("Caller");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        ast_add_statement(func->data.func_decl.body,
            make_call("Worker", NULL, 0, 2));

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Caller");
        uint32_t body_summary = sym != NULL && sym->type != NULL
            ? type_function_body_summary(sym->type) : BODY_SUMMARY_NONE;
        EXPECT(!ctx->has_error
            && (body_summary & BODY_SUMMARY_EFFECTS) != 0
            && (body_summary & BODY_SUMMARY_SPAWNS_TASK) != 0
            && (body_summary & BODY_SUMMARY_SENDS_CHANNEL) != 0
            && (body_summary & BODY_SUMMARY_DROPS_RESOURCE) != 0
            && (body_summary & BODY_SUMMARY_MAY_RETURN) == 0);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("callable summary prove helpers read body_summary bits directly");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        /* Quiet callee: no effects, no spawn, no channel send, no zone, no drop. */
        ASTNode *quiet = ast_create_function("Quiet");
        /* Active callee: spawns task, sends channel, drops resource, requires zone. */
        ASTNode *active = ast_create_function("Active");
        Symbol *qsym;
        Symbol *asym;

        ctx->program_root = program;

        quiet->data.func_decl.return_type = ast_create_type("Void");
        quiet->data.func_decl.body = ast_create_block();
        ast_add_statement(program, quiet);
        type_check_func_decl(quiet, ctx);

        active->data.func_decl.return_type = ast_create_type("Void");
        active->data.func_decl.body = ast_create_block();
        ast_add_statement(program, active);
        type_check_func_decl(active, ctx);

        /* Synthetically set body_summary_mask bits on the Active callee's
         * function type so the prove helpers exercise each named bit. */
        asym = scope_lookup(ctx->scope, "Active");
        if (asym != NULL && asym->type != NULL
            && asym->type->kind == TYPE_KIND_FUNCTION) {
            asym->type->data.function.has_body_summary_facts = true;
            asym->type->data.function.body_summary_mask =
                BODY_SUMMARY_DROPS_RESOURCE
                | BODY_SUMMARY_SPAWNS_TASK
                | BODY_SUMMARY_SENDS_CHANNEL
                | BODY_SUMMARY_REQUIRES_ZONE;
        }
        qsym = scope_lookup(ctx->scope, "Quiet");
        if (qsym != NULL && qsym->type != NULL
            && qsym->type->kind == TYPE_KIND_FUNCTION) {
            qsym->type->data.function.has_body_summary_facts = true;
            qsym->type->data.function.body_summary_mask = BODY_SUMMARY_NONE;
        }

        /* Quiet callee positively proves every bit absent. */
        EXPECT(semantic_callable_summary_proves_no_drop_resource(ctx, quiet));
        EXPECT(semantic_callable_summary_proves_no_spawn_task(ctx, quiet));
        EXPECT(semantic_callable_summary_proves_no_send_channel(ctx, quiet));
        EXPECT(semantic_callable_summary_proves_no_zone_requirement(ctx, quiet));

        /* Active callee proves none of the bits absent. */
        EXPECT(!semantic_callable_summary_proves_no_drop_resource(ctx, active));
        EXPECT(!semantic_callable_summary_proves_no_spawn_task(ctx, active));
        EXPECT(!semantic_callable_summary_proves_no_send_channel(ctx, active));
        EXPECT(!semantic_callable_summary_proves_no_zone_requirement(ctx, active));

        semantic_context_destroy(ctx);
        ast_destroy(program);
    }

    TEST("direct function call records callable declaration boundary summary");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        ASTNode *boundary = ast_create_function("Boundary");
        ASTNode *func = ast_create_function("Caller");
        ASTNode *args[2] = { make_number(1, 3), make_number(2, 3) };

        ctx->program_root = program;

        boundary->data.func_decl.return_type = ast_create_type("Void");
        boundary->data.func_decl.body = ast_create_block();
        boundary->data.func_decl.param_count = 2;
        boundary->data.func_decl.params = calloc(2, sizeof(FuncParam *));
        boundary->data.func_decl.params[0] =
            make_func_param("owned", ast_create_type("Int"));
        boundary->data.func_decl.params[0]->mode = PARAM_MODE_OWN;
        boundary->data.func_decl.params[1] =
            make_func_param("borrowed", ast_create_type("Int"));
        boundary->data.func_decl.params[1]->mode = PARAM_MODE_REF;
        ast_add_statement(program, boundary);
        type_check_func_decl(boundary, ctx);
        {
            Symbol *boundary_sym = scope_lookup(ctx->scope, "Boundary");
            Type *boundary_type = boundary_sym != NULL ? boundary_sym->type : NULL;
            EXPECT(type_function_has_param_escape_summary(boundary_type, 0));
            EXPECT(type_function_param_escape_summary(boundary_type, 0)
                   == SLOT_PARAM_SUMMARY_NONE);
            EXPECT(type_function_has_param_escape_summary(boundary_type, 1));
            EXPECT(type_function_param_escape_summary(boundary_type, 1)
                   == SLOT_PARAM_SUMMARY_NONE);
        }

        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        ast_add_statement(func->data.func_decl.body,
            make_call("Boundary", args, 2, 3));
        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Caller");
        uint32_t body_summary = sym != NULL && sym->type != NULL
            ? type_function_body_summary(sym->type) : BODY_SUMMARY_NONE;
        EXPECT(!ctx->has_error
            && (body_summary & BODY_SUMMARY_MOVES_PARAM) != 0
            && (body_summary & BODY_SUMMARY_BORROWS_PARAM) != 0);

        semantic_context_destroy(ctx);
        ast_destroy(func);
        ast_destroy(program);
    }

    TEST("method call records callable declaration body summary");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        ASTNode *vault = ast_create_subject("Vault");
        ASTNode *method = ast_create_function("Sync");
        ASTNode *call;
        ASTNode *func;
        Symbol *vsym;

        method->data.func_decl.return_type = ast_create_type("Void");
        method->data.func_decl.body = ast_create_block();
        method->data.func_decl.has_effects_clause = true;
        method->data.func_decl.declared_effects = EFFECT_REMOTE;
        method->data.func_decl.within_zone = pergyra_strdup("VaultZone");
        method->data.func_decl.causes_effect = pergyra_strdup("VaultEffect");
        method->data.func_decl.param_count = 1;
        method->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        method->data.func_decl.params[0] =
            make_func_param("token", ast_create_type("Int"));
        method->data.func_decl.params[0]->mode = PARAM_MODE_REF;
        ASTNode *pending = ast_create_let_declaration("pending");
        pending->data.let_decl.initializer =
            ast_create_spawn_expression(make_number(42, 4));
        ast_add_statement(method->data.func_decl.body, pending);
        ast_add_statement(method->data.func_decl.body,
            ast_create_await_expression(make_identifier("pending", 5)));

        vault->data.class_decl.methods = calloc(1, sizeof(ASTNode *));
        vault->data.class_decl.methods[0] = method;
        vault->data.class_decl.method_count = 1;
        ast_add_statement(program, vault);
        (void)ast_assign_stable_ids(program);
        ctx->program_root = program;
        /* This focused summary test constructs the method AST directly. Keep
         * the surrounding checker in an async-capable context so the joined
         * spawn remains valid without changing the method/action union shape. */
        ctx->in_async_func = true;
        type_check_class_decl(vault, ctx);
        ctx->in_async_func = false;

        vsym = symbol_create_variable("v",
            scope_lookup(ctx->scope, "Vault")->type, 2, 1);
        scope_declare(ctx->scope, vsym);

        func = ast_create_function("Caller");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        ASTNode *args[1] = { make_number(7, 3) };
        call = make_call_expr(
            make_member_access(make_identifier("v", 3), "Sync", 3),
            args, 1, 3);
        ast_add_statement(func->data.func_decl.body, call);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Caller");
        uint32_t body_summary = sym != NULL && sym->type != NULL
            ? type_function_body_summary(sym->type) : BODY_SUMMARY_NONE;
        EXPECT(!ctx->has_error
            && (body_summary & BODY_SUMMARY_EFFECTS) != 0
            && (body_summary & BODY_SUMMARY_REQUIRES_ZONE) != 0
            && (body_summary & BODY_SUMMARY_CAUSES_EFFECT) != 0
            && (body_summary & BODY_SUMMARY_BORROWS_PARAM) != 0
            && (body_summary & BODY_SUMMARY_SPAWNS_TASK) != 0);

        semantic_context_destroy(ctx);
        ast_destroy(func);
        ast_destroy(program);
    }

    TEST("lambda body summary stays on lambda type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *lambda = ast_create_lambda_expression();
        ASTNode *pending;
        ASTNode *ret;
        Type *lambda_type;

        ctx->current_function_effects = EFFECT_SECURE;
        ctx->current_function_body_summary = BODY_SUMMARY_SENDS_CHANNEL;

        lambda->data.lambda_expr.return_type = ast_create_type("Void");
        lambda->data.lambda_expr.body = ast_create_block();
        lambda->data.lambda_expr.is_async = true;
        pending = ast_create_let_declaration("pending");
        pending->data.let_decl.initializer =
            ast_create_spawn_expression(make_number(42, 2));
        ast_add_statement(lambda->data.lambda_expr.body, pending);
        ast_add_statement(lambda->data.lambda_expr.body,
            ast_create_await_expression(make_identifier("pending", 3)));
        ret = ast_create_return_statement();
        ast_add_statement(lambda->data.lambda_expr.body, ret);

        lambda_type = type_check_expression(lambda, ctx);

        EXPECT(!ctx->has_error);
        EXPECT(lambda_type != NULL
            && lambda_type->kind == TYPE_KIND_FUNCTION);
        EXPECT(type_effect_mask_has(type_function_effects(lambda_type),
            EFFECT_REMOTE));
        EXPECT((type_function_body_summary(lambda_type)
            & BODY_SUMMARY_SPAWNS_TASK) != 0);
        EXPECT((type_function_body_summary(lambda_type)
            & BODY_SUMMARY_MAY_RETURN) != 0);
        EXPECT(type_effect_mask_has(ctx->current_function_effects,
            EFFECT_SECURE));
        EXPECT(!type_effect_mask_has(ctx->current_function_effects,
            EFFECT_REMOTE));
        EXPECT(ctx->current_function_body_summary == BODY_SUMMARY_SENDS_CHANNEL);

        semantic_context_destroy(ctx);
        ast_destroy(lambda);
    }

    TEST("lambda body summary does not leak to enclosing function");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *func = ast_create_function("MakeLambda");
        ASTNode *lambda = ast_create_lambda_expression();
        ASTNode *decl = ast_create_let_declaration("f");
        ASTNode *pending;
        ASTNode *ret;

        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();

        lambda->data.lambda_expr.return_type = ast_create_type("Void");
        lambda->data.lambda_expr.body = ast_create_block();
        lambda->data.lambda_expr.is_async = true;
        pending = ast_create_let_declaration("pending");
        pending->data.let_decl.initializer =
            ast_create_spawn_expression(make_number(7, 2));
        ast_add_statement(lambda->data.lambda_expr.body, pending);
        ast_add_statement(lambda->data.lambda_expr.body,
            ast_create_await_expression(make_identifier("pending", 3)));
        ret = ast_create_return_statement();
        ast_add_statement(lambda->data.lambda_expr.body, ret);

        decl->data.let_decl.initializer = lambda;
        ast_add_statement(func->data.func_decl.body, decl);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "MakeLambda");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        uint32_t body_summary = sym != NULL && sym->type != NULL
            ? type_function_body_summary(sym->type) : BODY_SUMMARY_NONE;
        EXPECT(!ctx->has_error
            && !type_effect_mask_has(effects, EFFECT_REMOTE)
            && (body_summary & BODY_SUMMARY_SPAWNS_TASK) == 0
            && (body_summary & BODY_SUMMARY_MAY_RETURN) == 0);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("lambda value-type local is captured by copy (docs/135 Stage A)");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *func = ast_create_function("CaptureLocal");
        ASTNode *local = ast_create_let_declaration("x");
        ASTNode *lambda = ast_create_lambda_expression();
        ASTNode *decl = ast_create_let_declaration("f");

        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();

        local->data.let_decl.type = ast_create_type("Int");
        local->data.let_decl.initializer = make_number(1, 2);
        ast_add_statement(func->data.func_decl.body, local);

        lambda->data.lambda_expr.return_type = ast_create_type("Int");
        lambda->data.lambda_expr.body = make_identifier("x", 3);
        decl->data.let_decl.initializer = lambda;
        ast_add_statement(func->data.func_decl.body, decl);

        type_check_func_decl(func, ctx);

        /* Int is a copy-value type, so the capture is accepted and recorded on
         * the lambda node (the backends build the closure environment). */
        EXPECT(!ctx->has_error);
        EXPECT(ast_lambda_capture_count(lambda) == 1);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("lambda expr body is type-checked even with expected type (docs/135)");
    {
        /* Regression: when the return type comes from the annotated let type,
         * the expression body must still be type-checked. Skipping it let
         * `(y: Long) => y * 2` accept a Long*Int operand mismatch that a normal
         * body rejects, then miscompile (i64 * i32). */
        const char *source =
            "func Main() -> Void {\n"
            "    let h: func(Long) -> Long = (y: Long) => y * 2;\n"
            "    Log(h(21));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Type mismatch in binary operation"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("lambda expr body with matched operand types is accepted");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let h: func(Long) -> Long = (y: Long) => y * 2L;\n"
            "    Log(h(21));\n"
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

    TEST("lambda block local shadow is not treated as capture");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *func = ast_create_function("LambdaLocalShadow");
        ASTNode *outer = ast_create_let_declaration("value");
        ASTNode *lambda = ast_create_lambda_expression();
        ASTNode *inner = ast_create_let_declaration("value");
        ASTNode *ret = ast_create_return_statement();
        ASTNode *decl = ast_create_let_declaration("f");

        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();

        outer->data.let_decl.type = ast_create_type("Int");
        outer->data.let_decl.initializer = make_number(1, 2);
        ast_add_statement(func->data.func_decl.body, outer);

        lambda->data.lambda_expr.return_type = ast_create_type("Int");
        lambda->data.lambda_expr.body = ast_create_block();
        inner->data.let_decl.type = ast_create_type("Int");
        inner->data.let_decl.initializer = make_number(2, 3);
        ast_add_statement(lambda->data.lambda_expr.body, inner);
        ret->data.return_stmt.value = make_identifier("value", 4);
        ast_add_statement(lambda->data.lambda_expr.body, ret);

        decl->data.let_decl.initializer = lambda;
        ast_add_statement(func->data.func_decl.body, decl);

        type_check_func_decl(func, ctx);

        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }
