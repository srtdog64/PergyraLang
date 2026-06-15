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
