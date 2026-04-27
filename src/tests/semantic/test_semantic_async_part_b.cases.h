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

    TEST("ChannelClose(Channel<Int>) returns Void");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelClose", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_VOID));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelClose with active ReadView uses pin boundary diagnostic");
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
        ASTNode *call = make_call("ChannelClose", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "cannot cross a channel close boundary"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelClose rejects movable resource channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelClose", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "ChannelClose does not support slot handle (movable) channels yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelClose rejects authority Token channel payloads");
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
        ASTNode *call = make_call("ChannelClose", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(t == TYPE_UNKNOWN);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "ChannelClose cannot close Token channels yet"));

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

    TEST("TrySend with active ReadView uses pin boundary diagnostic");
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

        ASTNode *call_args[2] = {
            make_identifier("ch", 1),
            make_number(1, 1),
        };
        ASTNode *call = make_call("TrySend", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(ctx->has_error);
        EXPECT(type_equals(t, TYPE_BOOL));
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "cannot cross a channel handoff boundary"));

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

    TEST("SendTimeout rejects movable resource channel payloads");
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

        ASTNode *call_args[3] = {
            make_identifier("ch", 2),
            make_identifier("q", 2),
            make_number(1000, 2)
        };
        ASTNode *call = make_call("SendTimeout", call_args, 3, 2);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx,
                "SendTimeout does not support slot handle (movable) sends yet"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(call);
    }

    TEST("TrySendStatus rejects authority Token channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *token_args[1] = { TYPE_INT };
        Type *token_type = type_create_constructed(TYPE_TOKEN, token_args, 1);
        Type *channel_args[1] = { token_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, channel_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_variable("tok", token_type, 1, 1));

        ASTNode *call_args[2] = {
            make_identifier("ch", 2),
            make_identifier("tok", 2)
        };
        ASTNode *call = make_call("TrySendStatus", call_args, 2, 2);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx,
                "TrySendStatus cannot transport Token values yet"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("SendTimeoutStatus rejects authority Token channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *token_args[1] = { TYPE_INT };
        Type *token_type = type_create_constructed(TYPE_TOKEN, token_args, 1);
        Type *channel_args[1] = { token_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, channel_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_variable("tok", token_type, 1, 1));

        ASTNode *call_args[3] = {
            make_identifier("ch", 2),
            make_identifier("tok", 2),
            make_number(1000, 2)
        };
        ASTNode *call = make_call("SendTimeoutStatus", call_args, 3, 2);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx,
                "SendTimeoutStatus cannot transport Token values yet"));

        semantic_context_destroy(ctx);
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

    TEST("channel send rejects authority Token payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *token_args[1] = { TYPE_INT };
        Type *token_type = type_create_constructed(TYPE_TOKEN, token_args, 1);
        Type *channel_args[1] = { token_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, channel_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_variable("tok", token_type, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_identifier("tok", 1));
        send->line = 1; send->column = 1;

        type_check_expression(send, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Token values yet"));

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
