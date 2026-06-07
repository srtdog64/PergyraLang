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

    TEST("Cancel with active ReadView uses pin boundary diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_view("view", type_create_read_view(TYPE_INT),
                               "scores", 1, 1));

        ASTNode *call_args[1] = { make_identifier("pending", 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "cannot cross a cancel cleanup boundary"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Cancel rejects movable resource Future payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("pending", 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "Cancel does not support slot handle (movable) future payloads yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Cancel rejects anchored slot-handle Future payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *slot_type = type_create_slot(TYPE_INT, false);
        Type *args[1] = { slot_type };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("pending", 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "Cancel does not support slot handle (anchored) future payloads yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Cancel rejects boundary-value Future payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->program_root = program;

        Type *array_args[1] = { TYPE_INT };
        Type *array_type = type_create_constructed(TYPE_ARRAY, array_args, 1);
        Type *args[1] = { array_type };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("pending", 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "Cancel does not support boundary value future payloads yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
        ast_destroy(program);
    }

    TEST("Cancel rejects authority Token Future payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *token_args[1] = { TYPE_INT };
        Type *token_type = type_create_constructed(TYPE_TOKEN, token_args, 1);
        Type *args[1] = { token_type };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("pending", 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "Cancel does not support authority-bearing Token future payloads yet"));

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

    TEST("async block with active ReadView uses pin boundary diagnostic");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot();\n"
            "    let view: ReadView<Int> = ViewRead(scores);\n"
            "    async { Log(1); }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(result,
            "cannot cross an async block boundary"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("detached async block rejects local capture");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let value: Int = 7;\n"
            "    async { Log(value); }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(result,
            "Detached async block cannot capture local 'value' by pointer"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
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

    TEST("channel send with active ReadView uses pin boundary diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_view("view", type_create_read_view(TYPE_INT),
                               "scores", 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_number(42, 1));
        send->line = 1; send->column = 1;

        Type *t = type_check_expression(send, ctx);
        EXPECT(ctx->has_error);
        EXPECT(type_equals(t, TYPE_VOID));
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "cannot cross a channel handoff boundary"));

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

    TEST("TryRecv with active ReadView uses pin boundary diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_view("view", type_create_read_view(TYPE_INT),
                               "scores", 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("TryRecv", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "cannot cross a channel handoff boundary"));

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

    TEST("TryRecv rejects movable resource channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("TryRecv", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "TryRecv does not support slot handle (movable) channels yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("RecvTimeout rejects movable resource channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(1000, 1) };
        ASTNode *call = make_call("RecvTimeout", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "RecvTimeout does not support slot handle (movable) channels yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TryRecv rejects anchored slot-handle channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *slot_type = type_create_slot(TYPE_INT, false);
        Type *args[1] = { slot_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("TryRecv", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "TryRecv does not support slot handle (anchored) channels yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("RecvTimeout rejects boundary-value channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->program_root = program;

        Type *array_args[1] = { TYPE_INT };
        Type *array_type = type_create_constructed(TYPE_ARRAY, array_args, 1);
        Type *channel_args[1] = { array_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, channel_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(1000, 1) };
        ASTNode *call = make_call("RecvTimeout", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "RecvTimeout cannot yield Array storage yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
        ast_destroy(program);
    }

    TEST("channel send rejects borrowed Slice payload");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->program_root = program;

        Type *slice_args[1] = { TYPE_INT };
        Type *slice_type = type_create_constructed(TYPE_SLICE, slice_args, 1);
        Type *channel_args[1] = { slice_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, channel_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_variable("view", slice_type, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_identifier("view", 1));
        send->line = 1; send->column = 1;

        Type *t = type_check_expression(send, ctx);
        EXPECT(ctx->has_error);
        EXPECT(type_equals(t, TYPE_VOID));
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "Channel send does not support borrowed Slice transport yet"));

        semantic_context_destroy(ctx);
        ast_destroy(send);
        ast_destroy(program);
    }

    TEST("channel send rejects Array storage payload");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *program = ast_create_program();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->program_root = program;

        Type *array_args[1] = { TYPE_INT };
        Type *array_type = type_create_constructed(TYPE_ARRAY, array_args, 1);
        Type *channel_args[1] = { array_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, channel_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_variable("items", array_type, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_identifier("items", 1));
        send->line = 1; send->column = 1;

        Type *t = type_check_expression(send, ctx);
        EXPECT(ctx->has_error);
        EXPECT(type_equals(t, TYPE_VOID));
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "Channel send cannot transport Array"));

        semantic_context_destroy(ctx);
        ast_destroy(send);
        ast_destroy(program);
    }

    TEST("TryRecv rejects authority Token channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *token_args[1] = { TYPE_INT };
        Type *token_type = type_create_constructed(TYPE_TOKEN, token_args, 1);
        Type *args[1] = { token_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("TryRecv", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "TryRecv cannot yield Token values yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TryRecv rejects borrowed Slice channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *slice_args[1] = { TYPE_INT };
        Type *slice_type = type_create_constructed(TYPE_SLICE, slice_args, 1);
        Type *channel_args[1] = { slice_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, channel_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("TryRecv", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "TryRecv cannot yield borrowed Slice values yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("RecvTimeout rejects borrowed Slice channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *slice_args[1] = { TYPE_INT };
        Type *slice_type = type_create_constructed(TYPE_SLICE, slice_args, 1);
        Type *channel_args[1] = { slice_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, channel_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = {
            make_identifier("ch", 1),
            make_number(1000, 1),
        };
        ASTNode *call = make_call("RecvTimeout", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "RecvTimeout cannot yield borrowed Slice values yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }
