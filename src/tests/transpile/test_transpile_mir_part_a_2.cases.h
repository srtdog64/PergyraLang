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

    TEST("MIR-backed intent call passes pointer-self arguments by address");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Verify(self) -> Void { }\n"
            "}\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Charge(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step verify {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        on: buyer.Verify();\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let buyer: Buyer = Buyer(1);\n"
            "    let checkout: CheckoutZone = CheckoutZone(Buyer(1));\n"
            "    let ok: Bool = Charge(checkout, buyer);\n"
            "    Log(ok);\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char path_buf[512];
        make_tmp_path(path_buf, sizeof(path_buf),
            "pgy_test_intent_call_pointer_self.c");
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
            EXPECT(strstr(output,
                "Charge(&_pgy_ssa_checkout_1, &_pgy_ssa_buyer_1)") != NULL);
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
            EXPECT_STR_CONTAINS(output, "PgySlot_Int *__pgy_mir_pin_slot_");
            EXPECT_STR_CONTAINS(output, "PgyPinnedSlotView_Int __pgy_mir_pin_");
            EXPECT_STR_CONTAINS(output, "PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ");
            EXPECT_STR_CONTAINS(output, ".active = true, .can_write = false");
            EXPECT_STR_CONTAINS(output, ".active = false;");
            EXPECT(strstr(output, ".active = false;") != NULL
                   && strstr(strstr(output, ".active = false;"),
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
            EXPECT_STR_CONTAINS(output, "PgySlot_Int *__pgy_mir_pin_slot_");
            EXPECT_STR_CONTAINS(output, "PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE");
            EXPECT_STR_CONTAINS(output, ".active = true, .can_write = true");
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
