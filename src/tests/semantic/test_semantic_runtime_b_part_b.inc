    TEST("relation/effect/zone accept subject types in subject slot");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "relation ActiveLink {\n"
            "    subject slot source: Bot\n"
            "}\n"
            "effect ActiveEffect {\n"
            "    subject slot bearer: Bot\n"
            "}\n"
            "zone ActiveZone {\n"
            "    subject slot player: Bot\n"
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
}

/* -----------------------------------------------------------------
 * Parallel execution semantic tests
 * ----------------------------------------------------------------- */

static void
test_parallel_execution_semantics(void)
{
    printf("\n[parallel_execution_semantics]\n");

    TEST("subject declaration registers SYMBOL_CLASS");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *subject_decl = ast_create_subject("Counter");
        subject_decl->line = 1; subject_decl->column = 1;
        type_check_class_decl(subject_decl, ctx);
        EXPECT(!ctx->has_error);

        /* Verify symbol was registered */
        Symbol *sym = scope_lookup(ctx->scope, "Counter");
        EXPECT(sym != NULL && sym->kind == SYMBOL_CLASS);

        semantic_context_destroy(ctx);
        ast_destroy(subject_decl);
    }

    TEST("duplicate subject declaration triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *a1 = ast_create_subject("Counter");
        a1->line = 1; a1->column = 1;
        type_check_class_decl(a1, ctx);

        ASTNode *a2 = ast_create_subject("Counter");
        a2->line = 3; a2->column = 1;
        type_check_class_decl(a2, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(a1);
        ast_destroy(a2);
    }

    TEST("role can bind to subject host");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "}\n"
            "ability Tickable { func Tick() -> Void; }\n"
            "role CounterRole for Counter {\n"
            "    impl ability Tickable { func Tick() -> Void { Log(1); } }\n"
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

    TEST("await accepts Future<Slot<T>> anchored handle payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *slot_type = type_create_slot(TYPE_INT, false);
        Type *args[1] = { slot_type };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending_slot", future_type, 1, 1));

        ASTNode *await = ast_create_await_expression(make_identifier("pending_slot", 1));
        await->line = 1; await->column = 1;

        type_check_expression(await, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(await);
    }

    TEST("await on RemoteFuture<QubitSlot> yields Result<QubitSlot>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_QUBIT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("remote_qubit", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("remote_qubit", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_RESULT));
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_QUBIT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("await may initialize a movable resource binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_QUBIT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending_qubit", future_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer =
            ast_create_await_expression(make_identifier("pending_qubit", 1));
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 2) };
        ASTNode *state = make_call("QubitState", state_args, 1, 2);
        type_check_expression(state, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(state);
    }

    TEST("await movable resource rejects inline use without binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_QUBIT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending_qubit", future_type, 1, 1));

        ASTNode *await = ast_create_await_expression(make_identifier("pending_qubit", 1));
        await->line = 1; await->column = 1;
        ASTNode *state_args[1] = { await };
        ASTNode *state = make_call("QubitState", state_args, 1, 1);

        type_check_expression(state, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "bound to a named variable"));

        semantic_context_destroy(ctx);
        ast_destroy(state);
    }

    TEST("parallel-family: spawn returns Future<T>");
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

    TEST("Cancel(Future<Int>) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("pending", 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Cancel rejects non-future values");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *call_args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx,
                "Cancel requires Future<T> or RemoteFuture<T>"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("IsCancelled returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *call = make_call("IsCancelled", NULL, 0, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("channel send accepts plain value payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_number(42, 1));
        send->line = 1; send->column = 1;

        Type *t = type_check_expression(send, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_VOID));

        semantic_context_destroy(ctx);
        ast_destroy(send);
    }

    TEST("TryRecv(Channel<Int>) returns Option<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("TryRecv", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("RecvTimeout(Channel<Int>, Int) returns Option<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(1000, 1) };
        ASTNode *call = make_call("RecvTimeout", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelLength(Channel<Int>) returns Int");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelLength", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelCapacity(Channel<Int>) returns Int");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelCapacity", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelSpace(Channel<Int>) returns Int");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelSpace", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelFull(Channel<Int>) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelFull", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelClosed(Channel<Int>) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelClosed", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TrySendStatus(Channel<Int>, Int) returns Option<Bool>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(7, 1) };
        ASTNode *call = make_call("TrySendStatus", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && t != NULL
               && strcmp(t->name, "Option<Bool>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("SendTimeoutStatus(Channel<Int>, Int, Int) returns Option<Bool>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[3] = {
            make_identifier("ch", 1),
            make_number(7, 1),
            make_number(1000, 1)
        };
        ASTNode *call = make_call("SendTimeoutStatus", call_args, 3, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && t != NULL
               && strcmp(t->name, "Option<Bool>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelLength rejects non-channel values");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *call_args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("ChannelLength", call_args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TrySend(Channel<Int>, Int) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(42, 1) };
        ASTNode *call = make_call("TrySend", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TrySend rejects movable resource channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *call_args[2] = { make_identifier("ch", 2), make_identifier("q", 2) };
        ASTNode *call = make_call("TrySend", call_args, 2, 2);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx,
                "TrySend does not support slot handle (movable) sends yet"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(call);
    }

    TEST("channel send accepts anchored Slot handle payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *slot_type = type_create_slot(TYPE_INT, false);
        Type *args[1] = { slot_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_slot("slot", slot_type, false, NULL, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_identifier("slot", 1));
        send->line = 1; send->column = 1;

        type_check_expression(send, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(send);
    }

    TEST("channel send moves QubitSlot from named binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 2), make_identifier("q", 2));
        send->line = 2; send->column = 1;
        type_check_expression(send, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 3) };
        ASTNode *state = make_call("QubitState", state_args, 1, 3);
        type_check_expression(state, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(send);
        ast_destroy(state);
    }

    TEST("channel send rejects anonymous movable resource payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_call("ClaimQubit", NULL, 0, 1));
        send->line = 1; send->column = 1;

        type_check_expression(send, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "bind the value first"));

        semantic_context_destroy(ctx);
        ast_destroy(send);
    }

    TEST("channel recv may initialize a movable resource binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer =
            ast_create_channel_recv(make_identifier("ch", 1));
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 2) };
        ASTNode *state = make_call("QubitState", state_args, 1, 2);
        type_check_expression(state, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(state);
    }
}

static void
test_effect_inference(void)
{
    printf("\n[effect_inference]\n");

    TEST("Measure infers nondeterministic + collapse on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("Observe");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *ret = ast_create_return_statement();
        ASTNode *measure_args[1] = { make_identifier("q", 3) };
        ret->data.return_stmt.value = make_call("Measure", measure_args, 1, 3);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Observe");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_NONDETERMINISTIC)
            && type_effect_mask_has(effects, EFFECT_COLLAPSE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("effectful calls propagate function effects to callers");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *observe = ast_create_function("Observe");
        observe->data.func_decl.return_type = ast_create_type("Int");
        observe->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        ast_add_statement(observe->data.func_decl.body, decl);

        ASTNode *measure_ret = ast_create_return_statement();
        ASTNode *measure_args[1] = { make_identifier("q", 3) };
        measure_ret->data.return_stmt.value =
            make_call("Measure", measure_args, 1, 3);
        ast_add_statement(observe->data.func_decl.body, measure_ret);
        type_check_func_decl(observe, ctx);

        ASTNode *wrapper = ast_create_function("WrapObserve");
        wrapper->data.func_decl.return_type = ast_create_type("Int");
        wrapper->data.func_decl.body = ast_create_block();

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_call("Observe", NULL, 0, 6);
        ast_add_statement(wrapper->data.func_decl.body, ret);
        type_check_func_decl(wrapper, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "WrapObserve");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_NONDETERMINISTIC)
            && type_effect_mask_has(effects, EFFECT_COLLAPSE));

        semantic_context_destroy(ctx);
        ast_destroy(observe);
        ast_destroy(wrapper);
    }

    TEST("SecureSlot declarations infer secure effect on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("TouchSecure");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("secret");
        decl->data.let_decl.type = make_generic_type("SecureSlot", "Int");
        decl->data.let_decl.initializer = make_number(7, 2);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 3);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "TouchSecure");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("DeviceSlot parameter types require explicit own/ref qualifier");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseDevice");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        func->data.func_decl.params[0] =
            make_func_param("dev", make_generic_type("DeviceSlot", "Int"));

        type_check_func_decl(func, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Slot handle (anchored) parameters require explicit 'own' or 'ref'"));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("DeviceSlot copy into new binding is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *dev_args[1] = { TYPE_INT };
        Type *device_type = type_create_constructed(TYPE_DEVICE_SLOT, dev_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("dev", device_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("copy");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_identifier("dev", 2);

        type_check_let_decl(decl, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Slot handles (anchored) such as Slot/SecureSlot/DeviceSlot"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("DeviceSlot use-after-release is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *decl = ast_create_let_declaration("dev");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_call("ClaimDeviceSlot", NULL, 0, 1);
        EXPECT(type_check_let_decl(decl, ctx));

        ASTNode *release_args[1] = { make_identifier("dev", 2) };
        ASTNode *read_args[1] = { make_identifier("dev", 3) };
        ASTNode *release = make_call("ReleaseDeviceSlot", release_args, 1, 2);
        ASTNode *read = make_call("DeviceRead", read_args, 1, 3);

        type_check_expression(release, ctx);
        ctx->has_error = false;
        type_check_expression(read, ctx);

        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "released DeviceSlot"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(release);
        ast_destroy(read);
    }

    TEST("DeviceSlot double release is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *decl = ast_create_let_declaration("dev");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_call("ClaimDeviceSlot", NULL, 0, 1);
        EXPECT(type_check_let_decl(decl, ctx));

        ASTNode *release_a_args[1] = { make_identifier("dev", 2) };
        ASTNode *release_b_args[1] = { make_identifier("dev", 3) };
        ASTNode *release_a = make_call("ReleaseDeviceSlot", release_a_args, 1, 2);
        ASTNode *release_b = make_call("ReleaseDeviceSlot", release_b_args, 1, 3);

        type_check_expression(release_a, ctx);
        ctx->has_error = false;
        type_check_expression(release_b, ctx);

        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "already been released"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(release_a);
        ast_destroy(release_b);
    }

    TEST("subject copy into new binding is rejected");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Vec2 = Vec2();\n"
            "    let b: Vec2 = a;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subjects cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
