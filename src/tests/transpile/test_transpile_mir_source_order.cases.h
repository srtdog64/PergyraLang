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
            await_pos = strstr(ctx->out->data, "pgy_await(");
            recv_pos = strstr(ctx->out->data, "pgy_channel_recv_val_Int(&laneA)");
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
}
