static void
test_expression_emit(void)
{
    printf("\n[expression_emit]\n");

    TranspilerCtx *ctx;
    char *result;

    TEST("integer literal -> correct C literal");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_number(42, 1), ctx);
        EXPECT(strcmp(result, "42") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("string literal -> quoted C string");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_string_lit("hello", 1), ctx);
        EXPECT(strcmp(result, "\"hello\"") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Some(42) -> Some_Int(42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Some", args, 1, 1), ctx);
        EXPECT(strcmp(result, "Some_Int(42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Some(value) without concrete payload type fails closed");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("value", 1) };
        result = emit_expression(make_call("Some", args, 1, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "Some requires concrete payload type");
        transpiler_ctx_destroy(ctx);
    }

    TEST("None() without contextual Option<T> fails closed");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("None", NULL, 0, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("IsSome(None()) without concrete Option<T> fails closed");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_call("None", NULL, 0, 1) };
        result = emit_expression(make_call("IsSome", args, 1, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "IsSome requires concrete Option<T>");
        transpiler_ctx_destroy(ctx);
    }

    TEST("Ok(value) with unknown Result payload fails closed");
    {
        ctx = transpiler_ctx_create();
        ctx->expected_type = "Result<Unknown, NetError>";
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Ok", args, 1, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "cannot derive Result<T, E> specialization");
        transpiler_ctx_destroy(ctx);
    }

    TEST("Result suffix keeps user type names containing Unknown");
    {
        ctx = transpiler_ctx_create();
        ctx->expected_type = "Result<MyUnknownType, NetError>";
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Ok", args, 1, 1), ctx);
        EXPECT(strcmp(result, "Ok_MyUnknownType_NetError(42)") == 0);
        EXPECT(ctx->backend_error == NULL);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("identifier -> same name");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_identifier("myVar", 1), ctx);
        EXPECT(strcmp(result, "myVar") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("user function call -> funcName(arg0, arg1)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[2] = { make_number(1, 1), make_number(2, 1) };
        result = emit_expression(make_call("Add", args, 2, 1), ctx);
        EXPECT(strcmp(result, "Add(1, 2)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Log(42) -> pgy_log(42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Log", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log(42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Log(\"\\n + indentation\") normalizes multiline as banner");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second", 1),
        };
        result = emit_expression(make_call("Log", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log_banner(\"first\\nsecond\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("LogBlock(\"\\n + indentation\") normalizes multiline block text");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second\n", 1),
        };
        result = emit_expression(make_call("LogBlock", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log_banner(\"first\\nsecond\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("LogBanner(\"\\n + indentation\") normalizes multiline banner text");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second", 1),
        };
        result = emit_expression(make_call("LogBanner", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log_banner(\"first\\nsecond\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("LogRaw preserves raw newline and leading spaces");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second", 1),
        };
        result = emit_expression(make_call("LogRaw", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log(\"\\n  first\\n  second\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Read(s) -> pgy_read_Int(&s)");
    {
        ctx = transpiler_ctx_create();
        register_slot_var(ctx, "s", "Int", false, false);
        ASTNode *args[1] = { make_identifier("s", 1) };
        result = emit_expression(make_call("Read", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_read_Int(&s)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Release(s) -> pgy_release_Int(&s)");
    {
        ctx = transpiler_ctx_create();
        register_slot_var(ctx, "s", "Int", false, false);
        ASTNode *args[1] = { make_identifier("s", 1) };
        result = emit_expression(make_call("Release", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_release_Int(&s)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ViewRead(slot) -> slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("ViewRead", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ViewWrite(slot) -> slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("ViewWrite", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Move(slot) -> slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("Move", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("DeviceWrite missing value fails closed");
    {
        ctx = transpiler_ctx_create();
        register_typed_var(ctx, "gpu", "DeviceSlot<Int>");
        ASTNode *args[1] = { make_identifier("gpu", 1) };
        result = emit_expression(make_call("DeviceWrite", args, 1, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "DeviceWrite requires 2 arguments");
        transpiler_ctx_destroy(ctx);
    }

    TEST("SubmitDeviceRead missing slot fails closed");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("SubmitDeviceRead", NULL, 0, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "SubmitDeviceRead requires 1 argument");
        transpiler_ctx_destroy(ctx);
    }

    TEST("array access -> typed array ABI get");
    {
        ctx = transpiler_ctx_create();
        register_typed_var(ctx, "values", "Array<Int>");
        result = emit_expression(
            ast_create_array_access(make_identifier("values", 1),
                                    make_number(0, 1)),
            ctx);
        EXPECT(strcmp(result,
                      "({ PgyArray_Int _pgy_arr_get_1 = values; "
                      "pgy_array_get_Int(&_pgy_arr_get_1, 0); })") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("RcClone(shared) -> pgy_rc_clone_Int(shared)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("shared",
                     make_generic_type("Rc", "Int"),
                     make_call("RcNew", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("shared", 1) };
        result = emit_expression(make_call("RcClone", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_rc_clone_Int(shared)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("BoxGet(boxed) -> pgy_box_get_Int(boxed)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("boxed",
                     make_generic_type("Box", "Int"),
                     make_call("Box", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("boxed", 1) };
        result = emit_expression(make_call("BoxGet", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_box_get_Int(boxed)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("BoxSet(boxed, 42) -> pgy_box_set_Int(&boxed, 42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("boxed",
                     make_generic_type("Box", "Int"),
                     make_call("Box", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[2] = { make_identifier("boxed", 1), make_number(42, 1) };
        result = emit_expression(make_call("BoxSet", args, 2, 1), ctx);
        EXPECT(strcmp(result, "pgy_box_set_Int(&boxed, 42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("BoxSet missing value fails closed");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("boxed",
                     make_generic_type("Box", "Int"),
                     make_call("Box", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("boxed", 1) };
        result = emit_expression(make_call("BoxSet", args, 1, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "BoxSet requires exactly two arguments");
        transpiler_ctx_destroy(ctx);
    }

    TEST("RcGet rejects non-addressable Rc expression fail-closed");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        ASTNode *rc_new = make_call("RcNew", init_args, 1, 1);
        ASTNode *args[1] = { rc_new };
        result = emit_expression(make_call("RcGet", args, 1, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable Rc/Weak storage");
        transpiler_ctx_destroy(ctx);
    }

    TEST("BoxSet rejects non-addressable Box expression fail-closed");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        ASTNode *box_new = make_call("Box", init_args, 1, 1);
        ASTNode *args[2] = { box_new, make_number(42, 1) };
        result = emit_expression(make_call("BoxSet", args, 2, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable Box storage");
        transpiler_ctx_destroy(ctx);
    }

    TEST("IntentRecentName missing index fails closed");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("IntentRecentName", NULL, 0, 1),
                                 ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "requires exactly 1 argument");
        transpiler_ctx_destroy(ctx);
    }

    TEST("IntentActiveStepName missing step fails closed");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_number(0, 1) };
        result = emit_expression(make_call("IntentActiveStepName", args, 1, 1),
                                 ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "requires exactly 2 arguments");
        transpiler_ctx_destroy(ctx);
    }

    TEST("ToTObject(PlayerDto, player) -> tobject projection literal");
    {
        const char *source =
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let snapshot: PlayerDto = ToTObject(PlayerDto, player);\n"
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

        EXPECT_STR_CONTAINS(ctx->out->data, "PlayerDto");
        EXPECT_STR_CONTAINS(ctx->out->data, "= (PlayerDto){ .hp =");
        EXPECT_STR_CONTAINS(ctx->out->data, ".name =");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToObject(PlayerView, player) -> object projection literal");
    {
        const char *source =
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let view: PlayerView = ToObject(PlayerView, player);\n"
            "    Log(view.hp);\n"
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

        EXPECT_STR_CONTAINS(ctx->out->data, "PlayerView");
        EXPECT_STR_CONTAINS(ctx->out->data, "= (PlayerView){ .hp =");
        EXPECT_STR_CONTAINS(ctx->out->data, ".name =");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
