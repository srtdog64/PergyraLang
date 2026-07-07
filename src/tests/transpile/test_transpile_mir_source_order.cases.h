static void
test_source_order_mir_emit(void)
{
    printf("\n[source_order_mir_emit]\n");

    TEST("non-CFG MIR block preserves source statement order for async scheduler probe");
    {
        const char *example_path = "examples/resource_scheduler_async_probe/main.pgy";
        char *source = read_file_text(example_path);
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        TranspilerCtx *ctx = NULL;
        const char *log_pos = NULL;
        const char *await_pos = NULL;
        const char *recv_pos = NULL;
        bool ok = false;

        if (source != NULL)
            ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            ctx = transpiler_ctx_create();
            ctx->mir = mir;
            emit_program(ctx);
        }

        if (ok && ctx != NULL && ctx->out != NULL && ctx->out->data != NULL) {
            log_pos = strstr(ctx->out->data,
                "=== ASYNC RESOURCE SCHEDULER PROBE ===");
            await_pos = strstr(ctx->out->data, "pgy_lane_await(");
            recv_pos = strstr(ctx->out->data,
                "pgy_lane_channel_recv_val_Int(PGY_LANE_PINNED_ZONE, &laneA)");
        }

        EXPECT(ok && ctx != NULL && ctx->out != NULL && ctx->out->data != NULL);
        EXPECT(log_pos != NULL);
        EXPECT(await_pos != NULL);
        EXPECT(recv_pos != NULL);
        if (log_pos != NULL && recv_pos != NULL)
            EXPECT(log_pos < recv_pos);
        if (await_pos != NULL && recv_pos != NULL)
            EXPECT(await_pos < recv_pos);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        free(source);
    }

    TEST("with-slot MIR resource ops materialize before residual Read statements");
    {
        const char *source =
            "func Cost() -> Int {\n"
            "    return 4;\n"
            "}\n"
            "\n"
            "func Main() -> Void {\n"
            "    with slot<Int> as s {\n"
            "        Write(s, Cost());\n"
            "        Print(ToString(Read(s)));\n"
            "    }\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        TranspilerCtx *ctx = NULL;
        const char *claim_pos = NULL;
        const char *write_pos = NULL;
        const char *read_pos = NULL;
        const char *release_pos = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);

        if (ok) {
            ctx = transpiler_ctx_create();
            ctx->mir = mir;
            emit_program(ctx);
        }

        if (ok && ctx != NULL && ctx->out != NULL && ctx->out->data != NULL) {
            claim_pos = strstr(ctx->out->data,
                "PgySlot_Int s = pgy_claim_Int();");
            write_pos = strstr(ctx->out->data, "pgy_write_Int(&s, Cost())");
            read_pos = strstr(ctx->out->data, "pgy_read_Int(&s)");
            release_pos = strstr(ctx->out->data, "pgy_release_Int(&s)");
        }

        EXPECT(ok && ctx != NULL && ctx->out != NULL && ctx->out->data != NULL);
        EXPECT(claim_pos != NULL);
        EXPECT(write_pos != NULL);
        EXPECT(read_pos != NULL);
        EXPECT(release_pos != NULL);
        if (claim_pos != NULL && write_pos != NULL)
            EXPECT(claim_pos < write_pos);
        if (claim_pos != NULL && read_pos != NULL)
            EXPECT(claim_pos < read_pos);
        if (read_pos != NULL && release_pos != NULL)
            EXPECT(read_pos < release_pos);
        if (claim_pos != NULL)
            EXPECT(strstr(claim_pos + 1, "PgySlot_Int s = pgy_claim_Int();")
                   == NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("with-slot MIR resource ops do not outrun preceding locals");
    {
        const char *source =
            "func Seed() -> Int {\n"
            "    return 7;\n"
            "}\n"
            "\n"
            "func Main() -> Void {\n"
            "    let base: Int = Seed();\n"
            "    with slot<Int> as first {\n"
            "        Write(first, base + 1);\n"
            "        Print(ToString(Read(first)));\n"
            "    }\n"
            "    with slot<Int> as second {\n"
            "        Write(second, base + 2);\n"
            "        Print(ToString(Read(second)));\n"
            "    }\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        TranspilerCtx *ctx = NULL;
        const char *base_pos = NULL;
        const char *first_claim_pos = NULL;
        const char *first_write_pos = NULL;
        const char *second_claim_pos = NULL;
        const char *second_write_pos = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);

        if (ok) {
            ctx = transpiler_ctx_create();
            ctx->mir = mir;
            emit_program(ctx);
        }

        if (ok && ctx != NULL && ctx->out != NULL && ctx->out->data != NULL) {
            base_pos = strstr(ctx->out->data, "Seed()");
            first_claim_pos = strstr(ctx->out->data,
                "PgySlot_Int first = pgy_claim_Int();");
            first_write_pos = strstr(ctx->out->data, "pgy_write_Int(&first,");
            second_claim_pos = strstr(ctx->out->data,
                "PgySlot_Int second = pgy_claim_Int();");
            second_write_pos = strstr(ctx->out->data, "pgy_write_Int(&second,");
        }

        EXPECT(ok && ctx != NULL && ctx->out != NULL && ctx->out->data != NULL);
        EXPECT(base_pos != NULL);
        EXPECT(first_claim_pos != NULL);
        EXPECT(first_write_pos != NULL);
        EXPECT(second_claim_pos != NULL);
        EXPECT(second_write_pos != NULL);
        if (base_pos != NULL && first_write_pos != NULL)
            EXPECT(base_pos < first_write_pos);
        if (first_claim_pos != NULL && first_write_pos != NULL)
            EXPECT(first_claim_pos < first_write_pos);
        if (second_claim_pos != NULL && second_write_pos != NULL)
            EXPECT(second_claim_pos < second_write_pos);
        if (first_write_pos != NULL && second_claim_pos != NULL)
            EXPECT(first_write_pos < second_claim_pos);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }
}
