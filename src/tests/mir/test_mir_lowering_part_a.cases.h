static void
test_mir_lowering_part_a(void)
{
    printf("\n[mir]\n");

    TEST("MIR lifts slot flow into routine instructions");
    {
        const char *src =
            "func Flow() -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot<Int>();\n"
            "    Write(s, 1);\n"
            "    let v = Read(s);\n"
            "    Release(s);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *flow = NULL;
        const MIRValueSummary *value_summary = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            flow = find_mir_routine(mir, "Flow", MIR_SCOPE_FUNCTION);
        if (flow != NULL)
            value_summary = find_value_summary_with_slot(flow, "s.", "s");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && flow != NULL
               && flow->block_count >= 1
               && flow->instruction_count >= 4
               && block_has_inst_named_with_slot(&flow->blocks[flow->entry_block], "Claim", "s")
               && block_has_inst_named_with_slot(&flow->blocks[flow->entry_block], "Write", "s")
               && block_has_inst_named_with_slot(&flow->blocks[flow->entry_block], "Read", "s")
               && block_has_inst_named_with_slot(&flow->blocks[flow->entry_block], "Release", "s")
               && value_summary != NULL);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR builds cleanup block for intent compensation");
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
        const MIRRoutine *purchase = NULL;
        bool cleanup_has_compensate = false;
        bool cleanup_has_policy = false;
        bool cleanup_has_invalidation = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            purchase = find_mir_routine(mir, "Purchase", MIR_SCOPE_INTENT);
        if (purchase != NULL && purchase->has_rollback_block) {
            const MIRBasicBlock *rollback = &purchase->blocks[purchase->rollback_block];
            cleanup_has_compensate = block_has_inst_kind(rollback, MIR_INST_CLEANUP_EDGE)
                                     && block_has_inst_named_with_slot(rollback, "CompensateIntentStep", "pay");
            cleanup_has_policy = block_has_inst_named(rollback, "RollbackPolicy");
        }
        if (purchase != NULL && purchase->has_invalidation_block) {
            const MIRBasicBlock *invalidation = &purchase->blocks[purchase->invalidation_block];
            cleanup_has_invalidation = block_has_inst_named_with_slot(invalidation, "DetachInvalidation", "payment");
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && purchase != NULL
               && block_has_inst_named_with_slot(&purchase->blocks[purchase->entry_block],
                   "IntentStep", "pay")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentParticipant", "payment", "PaymentZone")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentParticipant", "buyer", "Buyer")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentZoneWhere", "PaymentZone", "pay")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentZoneAlias", "payment", "pay")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentInvalidationTarget", "payment", "pay")
               && block_has_inst_named_args(&purchase->blocks[purchase->entry_block],
                   "IntentWho", "buyer", "pay")
               && block_has_inst_named_with_slot(&purchase->blocks[purchase->entry_block],
                   "IntentAuthorizedBy", "pay")
               && purchase->has_cleanup_block
               && purchase->has_rollback_block
               && purchase->has_invalidation_block
               && purchase->cleanup_instruction_count >= 2
               && purchase->cleanup_edge_count >= 1
               && purchase->blocks[purchase->entry_block].has_cleanup_succ
               && purchase->blocks[purchase->entry_block].cleanup_succ == purchase->cleanup_block
               && purchase->blocks[purchase->cleanup_block].has_rollback_succ
               && purchase->blocks[purchase->cleanup_block].rollback_succ == purchase->rollback_block
               && purchase->blocks[purchase->cleanup_block].has_invalidation_succ
               && purchase->blocks[purchase->cleanup_block].invalidation_succ == purchase->invalidation_block
               && purchase->blocks[purchase->rollback_block].has_cleanup_succ
               && purchase->blocks[purchase->rollback_block].cleanup_succ == purchase->invalidation_block
               && block_has_inst_named_with_slot(&purchase->blocks[purchase->entry_block], "cleanup-edge", "cleanup")
               && cleanup_has_compensate
               && cleanup_has_policy
               && cleanup_has_invalidation
               && block_has_inst_named_with_slot(&purchase->blocks[purchase->rollback_block], "AbortIntent", "Purchase"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects cleanup block with normal CFG successor");
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
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            purchase = find_mir_routine_mut(mir, "Purchase", MIR_SCOPE_INTENT);
        if (purchase != NULL && purchase->has_cleanup_block) {
            MIRBasicBlock *cleanup = &purchase->blocks[purchase->cleanup_block];
            cleanup->has_succ_true = true;
            cleanup->succ_true = purchase->entry_block;
        }
        rejected = ok
                   && purchase != NULL
                   && purchase->has_cleanup_block
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "cleanup block") != NULL
                   && strstr(mir_error, "normal CFG successors") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects predecessor without forward edge");
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
        bool rejected = false;
        bool corrupted = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            purchase = find_mir_routine_mut(mir, "Purchase", MIR_SCOPE_INTENT);
        if (purchase != NULL) {
            for (size_t target = 0; target < purchase->block_count && !corrupted; target++) {
                MIRBasicBlock *block = &purchase->blocks[target];
                if (block->predecessor_count == 0)
                    continue;
                for (size_t pred = 0; pred < purchase->block_count; pred++) {
                    size_t next_count;
                    size_t *grown;
                    if (pred == target)
                        continue;
                    if (!test_block_has_forward_edge_to(&purchase->blocks[pred], target)) {
                        next_count = block->predecessor_count + 1;
                        grown = realloc(block->predecessors,
                                        next_count * sizeof(size_t));
                        if (grown == NULL)
                            break;
                        block->predecessors = grown;
                        block->predecessors[block->predecessor_count] = pred;
                        block->predecessor_count = next_count;
                        block->predecessor_capacity = next_count;
                        corrupted = true;
                        break;
                    }
                }
            }
        }
        rejected = ok
                   && purchase != NULL
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "predecessor") != NULL
                   && strstr(mir_error, "no matching forward edge") != NULL;
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

    TEST("MIR validator rejects pin-region without source anchor");
    {
        const char *src =
            "func PinFlow(flag: Bool) -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    pin scores as view: WriteView<Int> {\n"
            "        Write(view, 1);\n"
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
            for (size_t i = 0; i < routine->block_count; i++) {
                MIRBasicBlock *block = &routine->blocks[i];
                if (!block->is_pin_region)
                    continue;
                block->pin_source_name = NULL;
                corrupted = true;
                break;
            }
        }
        rejected = ok
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "pin-region") != NULL
                   && strstr(mir_error, "pin source name") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects pin-region without view name");
    {
        const char *src =
            "func PinFlow(flag: Bool) -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    pin scores as view: WriteView<Int> {\n"
            "        Write(view, 1);\n"
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
            for (size_t i = 0; i < routine->block_count; i++) {
                MIRBasicBlock *block = &routine->blocks[i];
                if (!block->is_pin_region)
                    continue;
                block->pin_view_name = NULL;
                corrupted = true;
                break;
            }
        }
        rejected = ok
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "pin-region") != NULL
                   && strstr(mir_error, "pin view name") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects pin-region without cleanup root");
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
        if (routine != NULL && routine->has_cleanup_block) {
            routine->has_cleanup_block = false;
            for (size_t i = 0; i < routine->block_count; i++) {
                MIRBasicBlock *block = &routine->blocks[i];
                if (!block->is_pin_region)
                    continue;
                block->has_cleanup_succ = false;
                corrupted = true;
                break;
            }
        }
        rejected = ok
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "cleanup root") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR keeps pin cleanup fact across early return");
    {
        const char *src =
            "func PinEarlyReturn(flag: Bool) -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    pin scores as view: WriteView<Int> {\n"
            "        if flag {\n"
            "            Write(view, 1);\n"
            "            return;\n"
            "        }\n"
            "        Write(view, 2);\n"
            "    }\n"
            "    Release(scores);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool found_pin_block = false;
        bool found_pin_cleanup = false;
        bool found_pin_return = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "PinEarlyReturn", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t i = 0; i < routine->block_count; i++) {
                const MIRBasicBlock *block = &routine->blocks[i];
                if (!block->is_pin_region)
                    continue;
                found_pin_block = true;
                if (block->has_cleanup_succ
                    && block->cleanup_succ == routine->cleanup_block
                    && block_has_inst_named_with_slot(block,
                                                      "pin-unpin-cleanup-edge",
                                                      "scores")) {
                    found_pin_cleanup = true;
                }
                if (block->source_terminator_kind == HIR_BLOCK_RETURN
                    || block_has_inst_kind(block, MIR_INST_RETURN)) {
                    found_pin_return = true;
                }
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine->has_cleanup_block
               && found_pin_block
               && found_pin_cleanup
               && found_pin_return);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR keeps pin cleanup fact across branch returns");
    {
        const char *src =
            "func PinBranchReturns(flag: Bool) -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    pin scores as view: WriteView<Int> {\n"
            "        if flag {\n"
            "            Write(view, 1);\n"
            "            return;\n"
            "        } else {\n"
            "            Write(view, 2);\n"
            "            return;\n"
            "        }\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool found_pin_block = false;
        bool found_pin_cleanup = false;
        bool found_branch = false;
        bool found_pin_return = false;
        size_t pin_return_count = 0;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "PinBranchReturns", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t i = 0; i < routine->block_count; i++) {
                const MIRBasicBlock *block = &routine->blocks[i];
                if (!block->is_pin_region)
                    continue;
                found_pin_block = true;
                if (block->has_succ_true && block->has_succ_false)
                    found_branch = true;
                if (block->has_cleanup_succ
                    && block->cleanup_succ == routine->cleanup_block
                    && block_has_inst_named_with_slot(block,
                                                      "pin-unpin-cleanup-edge",
                                                      "scores")) {
                    found_pin_cleanup = true;
                }
                if (block->source_terminator_kind == HIR_BLOCK_RETURN
                    || block_has_inst_kind(block, MIR_INST_RETURN)) {
                    found_pin_return = true;
                    pin_return_count++;
                }
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine->has_cleanup_block
               && found_pin_block
               && found_pin_cleanup
               && found_branch
               && found_pin_return
               && pin_return_count >= 2);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR keeps pin cleanup fact across loop break and continue");
    {
        const char *src =
            "func PinLoopControl(flag: Bool) -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    while flag {\n"
            "        pin scores as view: WriteView<Int> {\n"
            "            if flag {\n"
            "                Write(view, 1);\n"
            "                break;\n"
            "            }\n"
            "            Write(view, 2);\n"
            "            continue;\n"
            "        }\n"
            "    }\n"
            "    Release(scores);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool found_pin_block = false;
        bool found_break_cleanup = false;
        bool found_continue_cleanup = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "PinLoopControl", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t i = 0; i < routine->block_count; i++) {
                const MIRBasicBlock *block = &routine->blocks[i];
                if (!block->is_pin_region)
                    continue;
                found_pin_block = true;
                if (block->source_terminator_kind != HIR_BLOCK_GOTO)
                    continue;
                if (!block->has_cleanup_succ
                    || block->cleanup_succ != routine->cleanup_block
                    || !block_has_inst_named_with_slot(block,
                                                       "pin-unpin-cleanup-edge",
                                                       "scores")) {
                    continue;
                }
                if (block_source_has_stmt_type(block, AST_BREAK))
                    found_break_cleanup = true;
                if (block_source_has_stmt_type(block, AST_CONTINUE))
                    found_continue_cleanup = true;
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine->has_cleanup_block
               && found_pin_block
               && found_break_cleanup
               && found_continue_cleanup);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
