    TEST("MIR validator rejects predecessor count without inventory");
    {
        const char *src =
            "func Branch(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *branch = NULL;
        char *mir_error = NULL;
        bool corrupted = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            branch = find_mir_routine_mut(mir, "Branch", MIR_SCOPE_FUNCTION);
        if (branch != NULL) {
            for (size_t i = 0; i < branch->block_count; i++) {
                MIRBasicBlock *block = &branch->blocks[i];
                if (block->predecessor_count == 0)
                    continue;
                free(block->predecessors);
                block->predecessors = NULL;
                corrupted = true;
                break;
            }
        }
        rejected = ok
                   && branch != NULL
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "predecessor count without predecessor inventory") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects predecessor count above capacity");
    {
        const char *src =
            "func Branch(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *branch = NULL;
        char *mir_error = NULL;
        bool corrupted = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            branch = find_mir_routine_mut(mir, "Branch", MIR_SCOPE_FUNCTION);
        if (branch != NULL) {
            for (size_t i = 0; i < branch->block_count; i++) {
                MIRBasicBlock *block = &branch->blocks[i];
                if (block->predecessor_count == 0)
                    continue;
                block->predecessor_capacity = block->predecessor_count - 1;
                corrupted = true;
                break;
            }
        }
        rejected = ok
                   && branch != NULL
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "predecessor count above predecessor capacity") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects missing rollback and invalidation cleanup facts");
    {
        const char *src =
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
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *purchase = NULL;
        char *mir_error = NULL;
        bool rollback_corrupted = false;
        bool invalidation_corrupted = false;
        bool rollback_rejected = false;
        bool invalidation_rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            purchase = find_mir_routine_mut(mir, "Purchase", MIR_SCOPE_INTENT);
        if (purchase != NULL && purchase->has_rollback_block) {
            rollback_corrupted = block_rename_inst_named(
                &purchase->blocks[purchase->rollback_block],
                "cleanup-edge-from-rollback",
                "corrupted-cleanup-edge-from-rollback");
        }
        rollback_rejected = ok
                            && rollback_corrupted
                            && !mir_validate(mir, &mir_error)
                            && mir_error != NULL
                            && strstr(mir_error, "rollback block missing cleanup-edge MIR fact") != NULL;
        free(mir_error);
        mir_error = NULL;
        if (purchase != NULL && rollback_corrupted) {
            (void)block_rename_inst_named(
                &purchase->blocks[purchase->rollback_block],
                "corrupted-cleanup-edge-from-rollback",
                "cleanup-edge-from-rollback");
        }
        if (purchase != NULL && purchase->has_invalidation_block) {
            invalidation_corrupted = block_rename_inst_named(
                &purchase->blocks[purchase->invalidation_block],
                "cleanup-edge-from-invalidation",
                "corrupted-cleanup-edge-from-invalidation");
        }
        invalidation_rejected = ok
                                && invalidation_corrupted
                                && !mir_validate(mir, &mir_error)
                                && mir_error != NULL
                                && strstr(mir_error, "invalidation block missing cleanup-edge MIR fact") != NULL;
        EXPECT(rollback_rejected && invalidation_rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects overlapping cleanup roots");
    {
        const char *src =
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
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *purchase = NULL;
        char *mir_error = NULL;
        bool cleanup_rollback_rejected = false;
        bool entry_cleanup_rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        size_t saved_entry = 0;
        size_t saved_cleanup = 0;

        if (ok)
            purchase = find_mir_routine_mut(mir, "Purchase", MIR_SCOPE_INTENT);
        if (purchase != NULL
            && purchase->has_cleanup_block
            && purchase->has_rollback_block) {
            saved_cleanup = purchase->cleanup_block;
            purchase->cleanup_block = purchase->rollback_block;
            cleanup_rollback_rejected =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "cleanup and rollback blocks must be distinct") != NULL;
            free(mir_error);
            mir_error = NULL;
            purchase->cleanup_block = saved_cleanup;
        }
        if (purchase != NULL && purchase->has_cleanup_block) {
            saved_entry = purchase->entry_block;
            purchase->entry_block = purchase->cleanup_block;
            entry_cleanup_rejected =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "entry and cleanup blocks must be distinct") != NULL;
            free(mir_error);
            mir_error = NULL;
            purchase->entry_block = saved_entry;
        }

        EXPECT(ok
               && purchase != NULL
               && cleanup_rollback_rejected
               && entry_cleanup_rejected);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects orphan cleanup-marked block");
    {
        const char *src =
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
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *purchase = NULL;
        char *mir_error = NULL;
        bool corrupted = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);

        if (ok)
            purchase = find_mir_routine_mut(mir, "Purchase", MIR_SCOPE_INTENT);
        if (purchase != NULL) {
            size_t next_count = purchase->block_count + 1;
            MIRBasicBlock *grown = realloc(purchase->blocks,
                                           next_count * sizeof(MIRBasicBlock));
            if (grown != NULL) {
                MIRBasicBlock orphan;
                memset(&orphan, 0, sizeof(orphan));
                purchase->blocks = grown;
                orphan.id = purchase->block_count;
                orphan.is_cleanup = true;
                orphan.is_reachable = true;
                orphan.source_hir_block_id = SIZE_MAX;
                purchase->blocks[purchase->block_count] = orphan;
                purchase->block_count = next_count;
                purchase->block_capacity = next_count;
                corrupted = true;
            }
        }

        rejected = ok
                   && purchase != NULL
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "cleanup block") != NULL
                   && strstr(mir_error, "not registered as a cleanup root") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR carries pin-region cleanup edge metadata");
    {
        const char *src =
            "func PinFlow(flag: Bool) -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    pin scores as view: WriteView<Int> {\n"
            "        if flag {\n"
            "            Write(view, 1);\n"
            "        } else {\n"
            "            Write(view, 2);\n"
            "        }\n"
            "    }\n"
            "    Release(scores);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool found_pin_block = false;
        bool found_pin_cleanup = false;
        bool found_after_pin_release = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "PinFlow", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t i = 0; i < routine->block_count; i++) {
                const MIRBasicBlock *block = &routine->blocks[i];
                if (block->is_pin_region) {
                    found_pin_block = true;
                    if (block->has_cleanup_succ
                        && block->cleanup_succ == routine->cleanup_block
                        && block_has_inst_named_with_slot(block, "pin-unpin-cleanup-edge", "scores")) {
                        found_pin_cleanup = true;
                    }
                } else if (block_has_inst_named_with_slot(block, "Release", "scores")) {
                    found_after_pin_release = true;
                }
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine->has_cleanup_block
               && found_pin_block
               && found_pin_cleanup
               && found_after_pin_release);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects pin-region without unpin cleanup fact");
    {
        const char *src =
            "func PinFlow(flag: Bool) -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    pin scores as view: WriteView<Int> {\n"
            "        if flag {\n"
            "            Write(view, 1);\n"
            "        } else {\n"
            "            Write(view, 2);\n"
            "        }\n"
            "    }\n"
            "    Release(scores);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        char *mir_error = NULL;
        bool corrupted = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "PinFlow", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t i = 0; i < routine->block_count && !corrupted; i++) {
                MIRBasicBlock *block = &routine->blocks[i];
                if (!block->is_pin_region)
                    continue;
                for (size_t j = 0; j < block->instruction_count; j++) {
                    MIRInstruction *inst = &block->instructions[j];
                    if (inst->name != NULL
                        && strcmp(inst->name, "pin-unpin-cleanup-edge") == 0) {
                        inst->name = "corrupted-pin-cleanup-edge";
                        corrupted = true;
                        break;
                    }
                }
            }
        }
        rejected = ok
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "pin-region") != NULL
                   && strstr(mir_error, "pin-unpin cleanup fact") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
