    TEST("zone sync binds object-target relation/effect layers before projection reads");
    {
        const char *source =
            "object Door { hp: Int; }\n"
            "object Key { id: Int; }\n"
            "object DoorView { hp: Int; }\n"
            "effect Highlighted for object target: Door {\n"
            "    object slot view: DoorView\n"
            "    refresh view from target\n"
            "}\n"
            "relation KeyBinding for object door: Door, object key: Key {\n"
            "}\n"
            "zone LockZone {\n"
            "    object slot door: Door\n"
            "    object slot key: Key\n"
            "    effect slot glow: Highlighted\n"
            "    relation slot binding: KeyBinding\n"
            "    apply glow to door\n"
            "    link binding between door, key\n"
            "    func Show(self) -> Void {\n"
            "        Log(self.glow.view.hp);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Highlighted glow;");
        EXPECT_STR_CONTAINS(ctx->out->data, "KeyBinding binding;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->glow.target = self->door;");
        EXPECT_STR_CONTAINS(ctx->out->data, "Highlighted_sync(&self->glow);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->binding.door = self->door;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->binding.key = self->key;");
        EXPECT_STR_CONTAINS(ctx->out->data, "KeyBinding_sync(&self->binding);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->glow.view.hp);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

/* -----------------------------------------------------------------
 * Parallel execution emitter tests
 * ----------------------------------------------------------------- */

static void
test_parallel_execution_emit(void)
{
    printf("\n[parallel_execution_emit]\n");

    TEST("subject emits struct typedef");
    {
        ASTNode subject_node; memset(&subject_node, 0, sizeof(subject_node));
        subject_node.type = AST_CLASS_DECL;
        subject_node.data.class_decl.name = "Counter";
        subject_node.data.class_decl.nominal_kind = NOMINAL_DECL_SUBJECT;

        ClassField field; memset(&field, 0, sizeof(field));
        field.name = "count";
        ASTNode field_type; memset(&field_type, 0, sizeof(field_type));
        field_type.type = AST_TYPE;
        field_type.data.type.name = "Int";
        field.type = &field_type;

        ClassField *fields[1] = { &field };
        subject_node.data.class_decl.fields = fields;
        subject_node.data.class_decl.field_count = 1;
        subject_node.data.class_decl.methods = NULL;
        subject_node.data.class_decl.method_count = 0;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_class_decl(&subject_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct Counter");
        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t count");
        EXPECT_STR_CONTAINS(ctx->out->data, "} Counter;");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SLOT_DEFINE(Counter, Counter)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SECURE_SLOT_DEFINE(Counter, Counter)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_BOX_DEFINE(Counter, Counter)");

        transpiler_ctx_destroy(ctx);
    }

    TEST("subject syntax lowers through subject codegen");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let c: Counter = Counter();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct Counter\n{");
        EXPECT_STR_CONTAINS(ctx->out->data, "} Counter;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("channel send emits pgy_channel_send");
    {
        ASTNode ch_node; memset(&ch_node, 0, sizeof(ch_node));
        ch_node.type = AST_IDENTIFIER;
        ch_node.data.identifier.name = "myChan";

        ASTNode val_node; memset(&val_node, 0, sizeof(val_node));
        val_node.type = AST_NUMBER;
        val_node.data.number.value = 42;

        ASTNode send_node; memset(&send_node, 0, sizeof(send_node));
        send_node.type = AST_CHANNEL_SEND;
        send_node.data.channel_send.channel = &ch_node;
        send_node.data.channel_send.value = &val_node;

        TranspilerCtx *ctx = transpiler_ctx_create();
        register_typed_var(ctx, "myChan", "Channel<Int>");
        char *result = emit_expression(&send_node, ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_channel_send") != NULL);
        EXPECT(strstr(result, "myChan") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("channel recv emits pgy_channel_recv");
    {
        ASTNode ch_node; memset(&ch_node, 0, sizeof(ch_node));
        ch_node.type = AST_IDENTIFIER;
        ch_node.data.identifier.name = "myChan";

        ASTNode recv_node; memset(&recv_node, 0, sizeof(recv_node));
        recv_node.type = AST_CHANNEL_RECV;
        recv_node.data.channel_recv.channel = &ch_node;

        TranspilerCtx *ctx = transpiler_ctx_create();
        register_typed_var(ctx, "myChan", "Channel<Int>");
        char *result = emit_expression(&recv_node, ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_channel_recv") != NULL);
        EXPECT(strstr(result, "myChan") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("channel send rejects non-lvalue channel expression");
    {
        ASTNode *channel_call = make_call("MakeChannel", NULL, 0, 1);
        ASTNode *value = make_number(42, 1);
        ASTNode *send = ast_create_channel_send(channel_call, value);

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(send, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeChannel");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "requires a named Channel<T> binding");

        free(result);
        ast_destroy(send);
        transpiler_ctx_destroy(ctx);
    }

    TEST("channel recv rejects non-lvalue channel expression");
    {
        ASTNode *channel_call = make_call("MakeChannel", NULL, 0, 1);
        ASTNode *recv = ast_create_channel_recv(channel_call);

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(recv, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeChannel");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "requires a named Channel<T> binding");

        free(result);
        ast_destroy(recv);
        transpiler_ctx_destroy(ctx);
    }

    TEST("channel send preserves unknown payload diagnostic");
    {
        ASTNode *channel = make_identifier("ch", 1);
        ASTNode *value = make_number(42, 1);
        ASTNode *send = ast_create_channel_send(channel, value);

        TranspilerCtx *ctx = transpiler_ctx_create();
        register_typed_var(ctx, "ch", "Channel<Unknown>");
        char *result = emit_expression(send, ctx);

        EXPECT(result != NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "requires concrete Channel<T> payload metadata");

        free(result);
        ast_destroy(send);
        transpiler_ctx_destroy(ctx);
    }

    TEST("TryRecv rejects non-lvalue channel expression");
    {
        ASTNode *channel_call = make_call("MakeChannel", NULL, 0, 1);
        ASTNode *args[1] = { channel_call };

        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *call = make_call("TryRecv", args, 1, 1);
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeChannel");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "requires a named Channel<T> binding");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelLength rejects non-lvalue channel expression");
    {
        ASTNode *channel_call = make_call("MakeChannel", NULL, 0, 1);
        ASTNode *args[1] = { channel_call };

        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *call = make_call("ChannelLength", args, 1, 1);
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeChannel");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "requires a named Channel<T> binding");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("TryRecv emits Option-based non-blocking receive");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);

        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("TryRecv", args, 1, 2), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "PgyRuntimeChannelIntResult _pgy_recv_result") != NULL);
        EXPECT(strstr(result, "pgy_channel_try_recv_result_Int(&ch)") != NULL);
        EXPECT(strstr(result, "PGY_RUNTIME_CHANNEL_RESULT_OK") != NULL);
        EXPECT(strstr(result, "Some_Int(_pgy_recv_result.ok)") != NULL);
        EXPECT(strstr(result, "None_Int()") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("RecvTimeout emits Option-based timed receive");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);

        ASTNode *args[2] = { make_identifier("ch", 2), make_number(1000, 2) };
        char *result = emit_expression(make_call("RecvTimeout", args, 2, 2), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "PgyRuntimeChannelIntResult _pgy_recv_result") != NULL);
        EXPECT(strstr(result, "pgy_channel_recv_timeout_result_Int(&ch, (uint64_t)(1000))") != NULL);
        EXPECT(strstr(result, "PGY_RUNTIME_CHANNEL_RESULT_OK") != NULL);
        EXPECT(strstr(result, "Some_Int(_pgy_recv_result.ok)") != NULL);
        EXPECT(strstr(result, "None_Int()") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("TrySend emits pgy_channel_try_send");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);

        ASTNode *args[2] = { make_identifier("ch", 2), make_number(42, 2) };
        char *result = emit_expression(make_call("TrySend", args, 2, 2), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_channel_try_send_Int(&ch, 42)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("TrySendStatus emits Option<Bool> channel status helper");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);

        ASTNode *args[2] = { make_identifier("ch", 2), make_number(42, 2) };
        char *result = emit_expression(make_call("TrySendStatus", args, 2, 2), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_channel_try_send_status_Int(&ch, 42)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Cancel emits pgy_task_cancel");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *spawn = calloc(1, sizeof(ASTNode));
        spawn->type = AST_SPAWN_EXPR;
        spawn->data.spawn_expr.function = make_identifier("Work", 1);
        spawn->data.spawn_expr.arguments = NULL;
        spawn->data.spawn_expr.arg_count = 0;
        emit_statement(make_let("pending", NULL, spawn, 1), ctx);

        ASTNode *args[1] = { make_identifier("pending", 2) };
        char *result = emit_expression(make_call("Cancel", args, 1, 2), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_task_cancel(pending)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("IsCancelled emits pgy_task_is_cancelled");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(make_call("IsCancelled", NULL, 0, 1), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_task_is_cancelled()") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("event invoke emits generated invoke helper call");
    {
        ASTNode event_node; memset(&event_node, 0, sizeof(event_node));
        event_node.type = AST_IDENTIFIER;
        event_node.data.identifier.name = "OnHit";

        ASTNode arg_node; memset(&arg_node, 0, sizeof(arg_node));
        arg_node.type = AST_NUMBER;
        arg_node.data.number.value = 7;

        ASTNode *args[1] = { &arg_node };
        ASTNode invoke_node; memset(&invoke_node, 0, sizeof(invoke_node));
        invoke_node.type = AST_EVENT_INVOKE;
        invoke_node.data.event_invoke.event = &event_node;
        invoke_node.data.event_invoke.arguments = args;
        invoke_node.data.event_invoke.arg_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&invoke_node, ctx);

        EXPECT_STR_CONTAINS(result, "OnHit_INVOKE(&OnHit, 7)");

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("context access emits self role slot access");
    {
        ASTNode context; memset(&context, 0, sizeof(context));
        context.type = AST_CONTEXT_ACCESS;
        context.data.context_access.method_name = "GetRole";
        context.data.context_access.role_slot_name = "tank";

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&context, ctx);

        EXPECT_STR_CONTAINS(result, "self->tank");

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelLength emits pgy_channel_length");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("ChannelLength", args, 1, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_length_Int(&ch)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelCapacity emits pgy_channel_capacity");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("ChannelCapacity", args, 1, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_capacity_Int(&ch)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelSpace emits pgy_channel_space");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("ChannelSpace", args, 1, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_space_Int(&ch)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelFull emits pgy_channel_full");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("ChannelFull", args, 1, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_full_Int(&ch)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelClosed emits pgy_channel_closed");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("ChannelClosed", args, 1, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_closed_Int(&ch)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("SendTimeoutStatus emits Option<Bool> timed channel status helper");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[3] = {
            make_identifier("ch", 2),
            make_number(42, 2),
            make_number(1000, 2)
        };
        char *result = emit_expression(make_call("SendTimeoutStatus", args, 3, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_send_timeout_status_Int(&ch, 42, (uint64_t)(1000))") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("lambda expression emits helper prototype and definition");
    {
        ASTNode param; memset(&param, 0, sizeof(param));
        param.type = AST_IDENTIFIER;
        param.data.identifier.name = "x";

        ASTNode *body_expr = make_identifier("x", 1);
        ASTNode *body_stmts[1] = { make_return(body_expr, 1) };
        ASTNode *body = make_block(body_stmts, 1);

        ASTNode *params[1] = { &param };
        ASTNode lambda; memset(&lambda, 0, sizeof(lambda));
        lambda.type = AST_LAMBDA_EXPR;
        lambda.data.lambda_expr.params = params;
        lambda.data.lambda_expr.param_count = 1;
        lambda.data.lambda_expr.body = body;
        lambda.data.lambda_expr.return_type = make_type_node("Int");

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&lambda, ctx);

        EXPECT_STR_CONTAINS(result, "pgy_lambda_");
        EXPECT_STR_CONTAINS(ctx->decls->data, "static int32_t pgy_lambda_");
        EXPECT_STR_CONTAINS(ctx->helpers->data, "return x;");

        free(result);
        ast_destroy(body);
        ast_destroy(lambda.data.lambda_expr.return_type);
        transpiler_ctx_destroy(ctx);
    }

}

/* -----------------------------------------------------------------
 * Slot sugar tests
 * ----------------------------------------------------------------- */
