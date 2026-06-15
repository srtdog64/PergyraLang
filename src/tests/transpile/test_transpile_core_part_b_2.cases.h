    TEST("MIR secure slot identifier auto-read uses secure read after destructure claim");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let (slot, token) = ClaimSecureSlot<Int>(1);\n"
            "    Write(slot, 42, token);\n"
            "    Log(slot);\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        ctx = transpiler_ctx_create();
        ctx->mir = mir;

        EXPECT(ok);
        emit_program(ctx);

        EXPECT(ctx->backend_error == NULL);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Int token;");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_write_Int(&slot, 42, &token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_read_Int(&");
        EXPECT_STR_CONTAINS(ctx->out->data, "&token)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("MIR secure slot methods use paired token for non-destructure claim");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let ss: SecureSlot<Int> = ClaimSecureSlot<Int>(1);\n"
            "    ss.Write(42);\n"
            "    Log(ss.Read());\n"
            "    ss.Release();\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        ctx = transpiler_ctx_create();
        ctx->mir = mir;

        EXPECT(ok);
        emit_program(ctx);

        EXPECT(ctx->backend_error == NULL);
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_write_Int(&ss, 42, &ss_token);");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_read_Int(&ss, &ss_token)");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_release_Int(&ss, &ss_token);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("MIR secure slot methods use paired token after destructure claim");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let (slot, token) = ClaimSecureSlot<Int>(1);\n"
            "    slot.Write(42);\n"
            "    Log(slot.Read());\n"
            "    slot.Release();\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        ctx = transpiler_ctx_create();
        ctx->mir = mir;

        EXPECT(ok);
        emit_program(ctx);

        EXPECT(ctx->backend_error == NULL);
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_write_Int(&slot, 42, &token);");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_read_Int(&slot, &token)");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_release_Int(&slot, &token);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("Array<T>.Slice(start, len) preserves Slice<T> typing in transpiled MIR");
    {
        const char *source =
            "func Words() -> Array<String> {\n"
            "    return [\"hello\", \"world\", \"foo\"];\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let view = Words().Slice(1, 2);\n"
            "    let (second, third) = view;\n"
            "    Log(second);\n"
            "    Log(third);\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        ctx = transpiler_ctx_create();
        ctx->mir = mir;

        EXPECT(ok);
        emit_program(ctx);

        EXPECT(ctx->backend_error == NULL);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySlice_String");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_array_slice_String");
        EXPECT_STR_CONTAINS(ctx->out->data, "char* _pgy_ssa_second_1 = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data, "char* _pgy_ssa_third_1 = 0;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("SliceCopy materializes borrowed Slice<T> as owned Array<T>");
    {
        const char *source =
            "func Words() -> Array<Int> {\n"
            "    return [10, 20, 30];\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let view = Words().Slice(1, 2);\n"
            "    let owned = SliceCopy(view);\n"
            "    Log(ArrayLength(owned));\n"
            "    Log(owned[0]);\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        ctx = transpiler_ctx_create();
        ctx->mir = mir;

        EXPECT(ok);
        emit_program(ctx);

        EXPECT(ctx->backend_error == NULL);
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_slice_copy_Int");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyArray_Int");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("let dst: Slot<Int> = mt materializes moved slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *move_args[1] = { make_identifier("slot", 1) };
        ASTNode *mt = make_let("mt", make_type_node("MoveToken<Int>"),
                               make_call("Move", move_args, 1, 1), 1);
        emit_statement(mt, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySlot_Int mt = slot;");

        ASTNode *dst = make_let("dst", make_type_node("Slot<Int>"),
                                make_identifier("mt", 2), 2);
        emit_statement(dst, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySlot_Int dst = mt;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("return 0 -> return 0;");
    {
        ASTNode *node = make_return(make_number(0, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "return 0;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let vertices: Array<Vertex> = meshData -> PgyArray_Vertex vertices = meshData;");
    {
        ASTNode *node = make_let("vertices",
                                 make_generic_type("Array", "Vertex"),
                                 make_identifier("meshData", 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyArray_Vertex vertices = meshData;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let shared: Rc<Int> = RcNew(42) -> PgyRc_Int shared = pgy_rc_new_Int(42);");
    {
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *node = make_let("shared",
                                 make_generic_type("Rc", "Int"),
                                 make_call("RcNew", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyRc_Int shared = pgy_rc_new_Int(42);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let shared: Rc<Long> = RcNew(42L) lowers with Long runtime ABI");
    {
        ASTNode *literal = make_number(42, 1);
        ASTNode *args[1];
        ASTNode *node;
        const char *out;
        literal->data.number.is_long = true;
        args[0] = literal;
        node = make_let("shared",
                        make_generic_type("Rc", "Long"),
                        make_call("RcNew", args, 1, 1), 1);
        out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyRc_Long shared = pgy_rc_new_Long(42LL);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let shared: Rc<Float> = RcNew(3.5) lowers with Float runtime ABI");
    {
        ASTNode *args[1] = { make_number(3.5, 1) };
        ASTNode *node = make_let("shared",
                                 make_generic_type("Rc", "Float"),
                                 make_call("RcNew", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyRc_Float shared = pgy_rc_new_Float(3.5);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let shared: Rc<String> = RcNew(\"rc\") lowers with String runtime ABI");
    {
        ASTNode *args[1] = { make_string_lit("rc", 1) };
        ASTNode *node = make_let("shared",
                                 make_generic_type("Rc", "String"),
                                 make_call("RcNew", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyRc_String shared = pgy_rc_new_String(\"rc\");");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let alloc: Allocator = AllocatorPool(1024) -> PgyAllocator alloc = pgy_allocator_pool(1024);");
    {
        ASTNode *args[1] = { make_number(1024, 1) };
        ASTNode *node = make_let("alloc",
                                 make_type_node("Allocator"),
                                 make_call("AllocatorPool", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyAllocator alloc = pgy_allocator_pool(1024);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let scratch: Allocator = AllocatorScratch() -> PgyAllocator scratch = pgy_allocator_scratch();");
    {
        ASTNode *node = make_let("scratch",
                                 make_type_node("Allocator"),
                                 make_call("AllocatorScratch", NULL, 0, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyAllocator scratch = pgy_allocator_scratch();");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let storage: Box<Array<Int>> = BoxArray(128) -> fused BoxArray allocation");
    {
        ASTNode *array_type = make_generic_type("Array", "Int");
        ASTNode *boxed_array = make_generic_type_from_node("Box", array_type);
        ASTNode *args[1] = { make_number(128, 1) };
        ASTNode *node = make_let("storage", boxed_array,
                                 make_call("BoxArray", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyBoxArray_Int storage = pgy_box_array_new_Int(128, NULL);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("Write(slot, 42) -> pgy_write_Int(&slot, 42);");
    {
        ASTNode *args[2] = { make_identifier("slot", 1), make_number(42, 1) };
        ASTNode *call    = make_call("Write", args, 2, 1);
        ctx = transpiler_ctx_create();
        register_slot_var(ctx, "slot", "Int", false, false);
        emit_statement(call, ctx);
        const char *out  = ctx->out->data;
        EXPECT_STR_CONTAINS(out, "pgy_write_Int(&slot, 42)");
        transpiler_ctx_destroy(ctx);
    }

    TEST("unsafe block emits body directly");
    {
        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *unsafe_block = ast_create_unsafe_block(body);
        const char *out = emit_stmt_to_str(unsafe_block, &ctx);
        EXPECT_STR_CONTAINS(out, "/* unsafe */");
        EXPECT_STR_CONTAINS(out, "pgy_log");
        transpiler_ctx_destroy(ctx);
    }

    TEST("defer statement emits lexical inline cleanup");
    {
        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *defer_stmt = ast_create_defer_statement(body);
        ASTNode *block = ast_create_block();
        ast_add_statement(block, defer_stmt);
        ctx = transpiler_ctx_create();
        emit_block(block, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(1)");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data, "__attribute__((cleanup(_pgy_defer_");
        EXPECT_STR_NOT_CONTAINS(ctx->helpers->data, "static void _pgy_defer_");
        transpiler_ctx_destroy(ctx);
    }

    TEST("bind statement emits party-role rebinding call");
    {
        ASTNode *bind = ast_create_bind_statement("team", "fighter", "Warrior");
        const char *out = emit_stmt_to_str(bind, &ctx);
        EXPECT(strcmp(out, "") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "cannot resolve party type for bind statement 'team.fighter = Warrior'");
        transpiler_ctx_destroy(ctx);
    }
}

/* -----------------------------------------------------------------
 * Tests: full program output
 * ----------------------------------------------------------------- */
