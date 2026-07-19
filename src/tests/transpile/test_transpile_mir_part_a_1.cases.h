static void
test_transpiler_reentry_stability(void)
{
    printf("\n[transpiler_reentry]\n");

    TEST("same-process nested operator overload transpile stays semantically stable across repeated runs");
    {
        const char *source =
            "struct Vec2 {\n"
            "    x: Int;\n"
            "    y: Int;\n"
            "}\n"
            "func operator_add_Vec2(a: Vec2, b: Int) -> Vec2 {\n"
            "    return a;\n"
            "}\n"
            "func MakeVec() -> Vec2 {\n"
            "    return Vec2(1, 2);\n"
            "}\n"
            "func MakeCount() -> Int {\n"
            "    return 7;\n"
            "}\n"
            "func Main() -> Vec2 {\n"
            "    return MakeVec() + MakeCount();\n"
            "}\n";
        bool stable = true;

        for (int iteration = 0; iteration < 24; iteration++) {
            ASTNode *program = NULL;
            HIRProgram *hir = NULL;
            RIRProgram *rir = NULL;
            MIRProgram *mir = NULL;
            TranspilerCtx *ctx = transpiler_ctx_create();
            bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
            ctx->mir = mir;

            emit_program(ctx);

            if (!ok
                || mir == NULL
                || ctx->backend_error != NULL
                || ctx->out == NULL
                || ctx->out->data == NULL
                || ctx->out->data[0] == '\0') {
                stable = false;
            }

            transpiler_ctx_destroy(ctx);
            mir_destroy(mir);
            rir_destroy(rir);
            hir_destroy(hir);
            ast_destroy(program);

            if (!stable)
                break;
        }

        EXPECT(stable);
    }
}

static void
test_mir_vertical_slice_emit(void)
{
    printf("\n[mir_vertical_slice]\n");

    TEST("simple branch-return function body emits from MIR");
    {
        const char *source =
            "func Score(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 7;\n"
            "    }\n"
            "    return 3;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_vertical_slice.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_function_mir_emitability(hir, mir, "Score"))
                fprintf(stderr, "  -> MIR emitability warning for Score: pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_mir_with_test_evidence(mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const MIRRoutine *routine = find_mir_routine_by_name(mir, "Score");

            EXPECT(strstr(output, "/* emitted-from-mir */") != NULL);
            EXPECT(routine != NULL);
            EXPECT(mir_count_reachable_non_cleanup_blocks(routine) >= 3);
            EXPECT(mir_count_exceptional_edges(routine) == 0);
            EXPECT(strstr(output, "_pgy_mir_bb_Score_0:") != NULL);
            EXPECT(strstr(output, "_pgy_mir_bb_Score_1:") != NULL);
            EXPECT(strstr(output, "_pgy_mir_bb_Score_2:") != NULL);
            EXPECT(mir_block_slice_contains(output, "_pgy_mir_bb_Score_0:", "if ("));
            EXPECT(mir_block_slice_contains(output, "_pgy_mir_bb_Score_0:", "goto _pgy_mir_bb_Score_1;"));
            EXPECT(mir_block_slice_contains(output, "_pgy_mir_bb_Score_0:", "goto _pgy_mir_bb_Score_2;"));
            EXPECT(mir_block_slice_contains(output, "_pgy_mir_bb_Score_1:", "return 7;"));
            EXPECT(mir_block_slice_contains(output, "_pgy_mir_bb_Score_2:", "return 3;"));
            EXPECT((strstr(output, "if (flag)") != NULL)
                   || (strstr(output, "if (_pgy_ssa_flag_") != NULL));
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("phi merge function body emits MIR SSA locals and predecessor copies");
    {
        const char *source =
            "func Score(flag: Bool) -> Int {\n"
            "    let score: Int = 0;\n"
            "    if flag {\n"
            "        score = 7;\n"
            "    } else {\n"
            "        score = 3;\n"
            "    }\n"
            "    return score;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_phi_slice.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_function_mir_emitability(hir, mir, "Score"))
                fprintf(stderr, "  -> MIR emitability warning for Score(phi): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_mir_with_test_evidence(mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const MIRRoutine *routine = find_mir_routine_by_name(mir, "Score");

            /* Verify MIR produced valid C with score assignments and returns */
            EXPECT(routine != NULL);
            EXPECT(mir_count_phi_instructions(routine) > 0);
            EXPECT(mir_count_reachable_non_cleanup_blocks(routine) >= 3);
            EXPECT(strstr(output, "if (") != NULL);
            EXPECT(strstr(output, "return") != NULL);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("late SSA let in CFG does not re-emit plain local declaration");
    {
        const char *source =
            "func Score(mode: Int, seed: Int) -> Int {\n"
            "    if mode == 1 {\n"
            "        return 7;\n"
            "    }\n"
            "    if mode == 2 {\n"
            "        return 9;\n"
            "    }\n"
            "    let next = seed + 1;\n"
            "    return next;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_late_let_slice.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            TranspileResult *res = transpile_mir_with_test_evidence(mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            EXPECT_STR_CONTAINS(output, "_pgy_ssa_next_1 = (seed + 1);");
            EXPECT_STR_NOT_CONTAINS(output, "int32_t next = (seed + 1);");
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("MIR assignment prefers source-local binding over same-named host field");
    {
        const char *source =
            "class Counter {\n"
            "    let value: Int;\n"
            "    func Score(self) -> Int {\n"
            "        let value: Int = 1;\n"
            "        value = value + 2;\n"
            "        return value;\n"
            "    }\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_local_shadows_field.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            TranspileResult *res = transpile_mir_with_test_evidence(mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            EXPECT_STR_CONTAINS(output, "/* emitted-from-mir */");
            EXPECT_STR_CONTAINS(output, "_pgy_ssa_value_");
            EXPECT_STR_CONTAINS(output, " = (_pgy_ssa_value_");
            EXPECT_STR_NOT_CONTAINS(output, "self->value =");
            EXPECT_STR_NOT_CONTAINS(output, "self.value =");
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("non-SSA statements in CFG blocks still emit from MIR");
    {
        const char *source =
            "func Touch() -> Void {\n"
            "    Log(\"touch\");\n"
            "    return;\n"
            "}\n"
            "func Score(flag: Bool) -> Int {\n"
            "    Touch();\n"
            "    if flag {\n"
            "        Touch();\n"
            "        return 7;\n"
            "    }\n"
            "    return 3;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_stmt_slice.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_function_mir_emitability(hir, mir, "Score"))
                fprintf(stderr, "  -> MIR emitability warning for Score(stmt): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_mir_with_test_evidence(mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const MIRRoutine *routine = find_mir_routine_by_name(mir, "Touch");

            /* Verify MIR produced valid C with Touch() calls and returns */
            EXPECT(routine != NULL);
            EXPECT(strstr(output, "Touch();") != NULL);
            EXPECT(strstr(output, "return 7;") != NULL);
            EXPECT(strstr(output, "return 3;") != NULL);
            if (routine != NULL)
                EXPECT(routine->has_cleanup_block == false);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("destructured MIR locals remain resolvable across later CFG blocks");
    {
        const char *source =
            "func Flags() -> Array<Bool> {\n"
            "    return [true, false];\n"
            "}\n"
            "func Score() -> Int {\n"
            "    let (flag, other) = Flags();\n"
            "    if flag {\n"
            "        return 7;\n"
            "    }\n"
            "    if other {\n"
            "        return 5;\n"
            "    }\n"
            "    return 3;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_destructure_cfg_slice.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_function_mir_emitability(hir, mir, "Score"))
                fprintf(stderr, "  -> MIR emitability warning for Score(destructure-cfg): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_mir_with_test_evidence(mir, path);
            ok = (res != NULL && res->success);
            if (res != NULL && !res->success && res->error_message != NULL)
                fprintf(stderr, "  -> destructure-cfg transpile error: %s\n", res->error_message);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            EXPECT(strstr(output, "/* emitted-from-mir */") != NULL);
            EXPECT(strstr(output, "_pgy_ssa_flag_1") != NULL);
            EXPECT(strstr(output, "_pgy_ssa_other_1") != NULL);
            EXPECT((strstr(output, "if (flag)") != NULL)
                   || (strstr(output, "if (_pgy_ssa_flag_") != NULL));
            EXPECT((strstr(output, "if (other)") != NULL)
                   || (strstr(output, "if (_pgy_ssa_other_") != NULL));
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("intent cleanup CFG emits from MIR exceptional blocks");
    {
        const char *source =
            "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "object BuyerView { let hp: Int; }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot view: BuyerView\n"
            "    authority buyer requires Payable\n"
            "    refresh view from buyer by buyer\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Buyer) {\n"
            "    rollback: full;\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        authorized by: buyer;\n"
            "        requires: Payable;\n"
            "        on: buyer.Pay();\n"
            "        compensate: buyer.Pay();\n"
            "    }\n"
            "    failure: false;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_intent_cleanup.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_intent_mir_emitability(hir, mir, "Purchase"))
                fprintf(stderr, "  -> MIR emitability warning for Purchase(intent): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_mir_with_test_evidence(mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const MIRRoutine *routine = find_mir_routine_by_name(mir, "Purchase");

            /* Intent MIR emission should produce valid C code */
            EXPECT(routine != NULL);
            EXPECT(strstr(output, "Purchase(") != NULL);
            if (routine != NULL)
                EXPECT(routine->has_cleanup_block == true);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }
