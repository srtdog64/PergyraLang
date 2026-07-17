static void
test_intent_observability_emit(void)
{
    printf("\n[intent_observability_emit]\n");

    TEST("intent trace calls are omitted when observability builtins are unused");
    {
        const char *source =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    authority buyer requires Payable\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        authorized by: buyer;\n"
            "        requires: Payable;\n"
            "        on: buyer.Pay();\n"
            "    }\n"
            "    success: true;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        const char *path = NULL;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        TranspileResult *res = NULL;

        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_intent_no_trace_fast_path.c");
        path = path_buf;
        if (ok) {
            res = transpile_mir_with_test_evidence(mir, path);
            ok = (res != NULL && res->success);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && res != NULL && output != NULL);
        if (ok && res != NULL && output != NULL) {
            EXPECT(!res->uses_intent_observability);
            EXPECT_STR_CONTAINS(output, "#define PGY_INTENT_OBSERVABILITY_ENABLED 0");
            EXPECT_STR_NOT_CONTAINS(output, "pgy_intent_trace_step_export(");
            EXPECT_STR_NOT_CONTAINS(output, "pgy_intent_trace_bind_export(");
            EXPECT_STR_NOT_CONTAINS(output, "pgy_intent_trace_step_ok_export(");
            EXPECT_STR_NOT_CONTAINS(output, "pgy_intent_trace_fail_export(");
        }

        transpile_result_destroy(res);
        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("intent trace calls stay enabled when observability builtin appears later");
    {
        const char *source =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    authority buyer requires Payable\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        authorized by: buyer;\n"
            "        requires: Payable;\n"
            "        on: buyer.Pay();\n"
            "    }\n"
            "    success: true;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    Log(IntentLastTrace());\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        const char *path = NULL;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        TranspileResult *res = NULL;

        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_intent_trace_prescan.c");
        path = path_buf;
        if (ok) {
            res = transpile_mir_with_test_evidence(mir, path);
            ok = (res != NULL && res->success);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && res != NULL && output != NULL);
        if (ok && res != NULL && output != NULL) {
            EXPECT(res->uses_intent_observability);
            EXPECT_STR_CONTAINS(output, "#define PGY_INTENT_OBSERVABILITY_ENABLED 1");
            EXPECT_STR_CONTAINS(output, "pgy_intent_trace_step_export(");
            EXPECT_STR_CONTAINS(output, "pgy_intent_trace_bind_export(");
            EXPECT_STR_CONTAINS(output, "pgy_intent_trace_step_ok_export(");
            EXPECT_STR_CONTAINS(output, "pgy_intent_last_trace_export(");
        }

        transpile_result_destroy(res);
        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("intent trace calls stay enabled for initializer observability builtin");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let current: Int = IntentCurrentHandle();\n"
            "    Log(ToString(current));\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        const char *path = NULL;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        TranspileResult *res = NULL;

        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_intent_trace_init_fact.c");
        path = path_buf;
        if (ok) {
            res = transpile_mir_with_test_evidence(mir, path);
            ok = (res != NULL && res->success);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && res != NULL && output != NULL);
        if (ok && res != NULL && output != NULL) {
            EXPECT(res->uses_intent_observability);
            EXPECT_STR_CONTAINS(output, "#define PGY_INTENT_OBSERVABILITY_ENABLED 1");
            EXPECT_STR_CONTAINS(output, "pgy_intent_current_handle_export(");
        }

        transpile_result_destroy(res);
        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("intent observability builtins lower to runtime exports");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let current: Int = IntentCurrentHandle();\n"
            "    let recent: Int = IntentRecentHandle(0);\n"
            "    let trace: Int = IntentRecentTraceId(0);\n"
            "    let steps: Int = IntentActiveStepCount(current);\n"
            "    let name: String = IntentActiveStepName(recent, 0);\n"
            "    let zone: String = IntentActiveStepZone(recent, 0);\n"
            "    let phase: String = IntentActiveStepPhase(recent, 0);\n"
            "    let participant: String = IntentActiveStepParticipant(recent, 0);\n"
            "    let slot: String = IntentActiveStepSlot(recent, 0);\n"
            "    let from_zone: String = IntentActiveStepFromZone(recent, 0);\n"
            "    let from_slot: String = IntentActiveStepFromSlot(recent, 0);\n"
            "    let to_zone: String = IntentActiveStepToZone(recent, 0);\n"
            "    let to_slot: String = IntentActiveStepToSlot(recent, 0);\n"
            "    let ok: Bool = IntentActiveStepOk(recent, 0);\n"
            "    let failure: String = IntentActiveStepFailure(recent, 0);\n"
            "    Log(ToString(current + recent + trace + steps));\n"
            "    Log(name + zone + phase + participant + slot + from_zone + from_slot + to_zone + to_slot + failure);\n"
            "    Log(ok);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = NULL;
        ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_current_handle_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_recent_handle_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_recent_trace_id_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_count_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_name_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_zone_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_phase_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_participant_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_slot_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_from_zone_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_from_slot_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_to_zone_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_to_slot_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_ok_export(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_intent_active_step_failure_export(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
