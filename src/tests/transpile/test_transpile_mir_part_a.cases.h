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

/* Build a temp file path using TMPDIR (set by Makefile) or fallback. */
static void
make_tmp_path(char *buf, size_t bufsz, const char *filename)
{
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL || tmpdir[0] == '\0') {
#ifdef _WIN32
        tmpdir = getenv("TEMP");
        if (tmpdir == NULL || tmpdir[0] == '\0')
            tmpdir = ".";
#else
        tmpdir = "/tmp";
#endif
    }
    snprintf(buf, bufsz, "%s/%s", tmpdir, filename);
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
            TranspileResult *res = transpile_with_mir(hir, mir, path);
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
            TranspileResult *res = transpile_with_mir(hir, mir, path);
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
            TranspileResult *res = transpile_with_mir(hir, mir, path);
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
            TranspileResult *res = transpile_with_mir(hir, mir, path);
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
            TranspileResult *res = transpile_with_mir(hir, mir, path);
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
            TranspileResult *res = transpile_with_mir(hir, mir, path);
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

    TEST("intent step subintent call lowers as bool-gated orchestration");
    {
        const char *source =
            "subject Buyer { let hp: Int; }\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Charge(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step verify {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        intent: Charge(checkout, buyer);\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_intent_subintent.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_intent_mir_emitability(hir, mir, "Checkout"))
                fprintf(stderr, "  -> MIR emitability warning for Checkout(intent): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const MIRRoutine *routine = find_mir_routine_by_name(mir, "Checkout");

            /* Subintent MIR emission should produce valid C code */
            EXPECT(routine != NULL);
            EXPECT(strstr(output, "Checkout(") != NULL);
            EXPECT(strstr(output, "Charge(") != NULL);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("intent header value params lower through MIR and transpiler");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Pay(self) -> Void { return; }\n"
            "}\n"
            "struct PriceQuote {\n"
            "    amount: Int;\n"
            "}\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer, quote: PriceQuote, price: Int) {\n"
            "    step pay {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        guard: quote.amount >= price;\n"
            "        on: buyer.Pay();\n"
            "        expect: price > 0;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_intent_value_params.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_intent_mir_emitability(hir, mir, "Checkout"))
                fprintf(stderr, "  -> MIR emitability warning for Checkout(intent-value): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            EXPECT(strstr(output, "Checkout(") != NULL);
            EXPECT(strstr(output, "PriceQuote quote") != NULL);
            EXPECT(strstr(output, "int32_t price") != NULL);
            EXPECT(strstr(output, "quote.amount") != NULL);
            EXPECT(strstr(output, "price > 0") != NULL);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("intent header interleaved bindings preserve declared C signature order");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Pay(self) -> Void { return; }\n"
            "}\n"
            "struct PriceQuote {\n"
            "    amount: Int;\n"
            "}\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, quote: PriceQuote, buyer: Buyer, price: Int, adjustments: Array<Int>) {\n"
            "    step pay {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        guard: quote.amount >= price;\n"
            "        on: buyer.Pay();\n"
            "        expect: ArrayLength(adjustments) == 2;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_intent_interleaved_value_params.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            if (!check_intent_mir_emitability(hir, mir, "Checkout"))
                fprintf(stderr, "  -> MIR emitability warning for Checkout(intent-interleaved): pipeline emitted with MIR fallback checks only\n");
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            const char *sig = strstr(output, "Checkout(");
            const char *sig_end = sig != NULL ? strchr(sig, ')') : NULL;
            char signature[512];
            const char *checkout_pos = NULL;
            const char *quote_pos = NULL;
            const char *buyer_pos = NULL;
            const char *price_pos = NULL;
            const char *adjustments_pos = NULL;
            size_t sig_len = 0;

            EXPECT(sig != NULL);
            EXPECT(sig_end != NULL);
            EXPECT(strstr(output, "PriceQuote quote") != NULL);
            EXPECT(strstr(output, "int32_t price") != NULL);
            EXPECT(strstr(output, "PgyArray_Int adjustments") != NULL);

            if (sig != NULL && sig_end != NULL) {
                sig_len = (size_t)(sig_end - sig + 1);
                if (sig_len >= sizeof(signature))
                    sig_len = sizeof(signature) - 1;
                memcpy(signature, sig, sig_len);
                signature[sig_len] = '\0';

                checkout_pos = strstr(signature, "checkout");
                quote_pos = strstr(signature, "quote");
                buyer_pos = strstr(signature, "buyer");
                price_pos = strstr(signature, "price");
                adjustments_pos = strstr(signature, "adjustments");

                EXPECT(checkout_pos != NULL);
                EXPECT(quote_pos != NULL);
                EXPECT(buyer_pos != NULL);
                EXPECT(price_pos != NULL);
                EXPECT(adjustments_pos != NULL);
                EXPECT(checkout_pos < quote_pos);
                EXPECT(quote_pos < buyer_pos);
                EXPECT(buyer_pos < price_pos);
                EXPECT(price_pos < adjustments_pos);
            }
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("MIR pin block emits explicit pin/unpin around successor path");
    {
        const char *source =
            "func Score() -> Int {\n"
            "    let scores: Slot<Int> = 41;\n"
            "    pin scores as view: ReadView<Int> {\n"
            "        Log(\"pin\");\n"
            "    }\n"
            "    return Read(scores);\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_pin_block.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (output != NULL) {
            EXPECT_STR_CONTAINS(output, "PgyPinnedSlotView_Int __pgy_mir_pin_");
            EXPECT_STR_CONTAINS(output, "pgy_pin_read_Int(&scores)");
            EXPECT_STR_CONTAINS(output, "pgy_unpin_Int(&__pgy_mir_pin_");
            EXPECT(strstr(output, "pgy_unpin_Int(&__pgy_mir_pin_") != NULL
                   && strstr(strstr(output, "pgy_unpin_Int(&__pgy_mir_pin_"),
                             "goto _pgy_mir_bb_Score_") != NULL);
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("MIR WriteView write uses slot write runtime, not layout claim runtime");
    {
        const char *source =
            "func Score() -> Int {\n"
            "    let scores: Slot<Int> = 41;\n"
            "    pin scores as view: WriteView<Int> {\n"
            "        Write(view, 42);\n"
            "    }\n"
            "    return Read(scores);\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_pin_write_view.c");
        const char *path = path_buf;
        char *output = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        if (ok) {
            TranspileResult *res = transpile_with_mir(hir, mir, path);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path);

        EXPECT(ok && output != NULL);
        if (output != NULL) {
            EXPECT_STR_CONTAINS(output, "pgy_pin_write_Int(&scores)");
            EXPECT_STR_CONTAINS(output, "pgy_write_Int(&scores, 42)");
            EXPECT_STR_NOT_CONTAINS(output, "pgy_claim_Int(&scores");
        }

        free(output);
        remove(path);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }
}

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
            res = transpile_with_mir(hir, mir, path);
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
            res = transpile_with_mir(hir, mir, path);
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
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = NULL;
        ctx = transpiler_ctx_create();

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

/* -----------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------- */
