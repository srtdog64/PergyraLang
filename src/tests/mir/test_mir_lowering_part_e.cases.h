static void
test_mir_lowering_part_e(void)
{
    TEST("MIR validator rejects pin-region without cleanup root");
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
        if (routine != NULL && routine->has_cleanup_block) {
            routine->has_cleanup_block = false;
            routine->cleanup_block = SIZE_MAX;
            routine->has_rollback_block = false;
            routine->rollback_block = SIZE_MAX;
            routine->has_invalidation_block = false;
            routine->invalidation_block = SIZE_MAX;
            corrupted = true;
        }
        rejected = ok
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "requires cleanup block for exceptional flow") != NULL;
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

    TEST("MIR validator rejects pin-region without cleanup root flag");
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

    TEST("MIR validator rejects unreachable cleanup root");
    {
        const char *src =
            "func PinCleanupReachable(flag: Bool) -> Void {\n"
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
            routine = find_mir_routine_mut(mir, "PinCleanupReachable", MIR_SCOPE_FUNCTION);
        if (routine != NULL
            && routine->has_cleanup_block
            && routine->cleanup_block < routine->block_count) {
            routine->blocks[routine->cleanup_block].is_reachable = false;
            corrupted = true;
        }
        rejected = ok
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "cleanup block") != NULL
                   && strstr(mir_error, "not reachable") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects unreachable exceptional source");
    {
        const char *src =
            "func PinCleanupReachable(flag: Bool) -> Void {\n"
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
            routine = find_mir_routine_mut(mir, "PinCleanupReachable", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t i = 0; i < routine->block_count; i++) {
                MIRBasicBlock *block = &routine->blocks[i];
                if (block->is_cleanup || !block->has_cleanup_succ)
                    continue;
                block->is_reachable = false;
                corrupted = true;
                break;
            }
        }
        rejected = ok
                   && corrupted
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "unreachable block") != NULL
                   && strstr(mir_error, "exceptional successor") != NULL;
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
                if (block_has_inst_kind(block, MIR_INST_RETURN)) {
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
                if (block_has_inst_kind(block, MIR_INST_RETURN)) {
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

    TEST("MIR keeps pin return value as SSA local definition");
    {
        const char *src =
            "func PinReturnValue() -> Int {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    Write(scores, 8);\n"
            "    pin scores as view: ReadView<Int> {\n"
            "        let value: Int = Read(view);\n"
            "        return value;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool found_value_def = false;
        bool found_return_value_use = false;
        bool found_value_stmt_fallback = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "PinReturnValue", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count; bi++) {
                const MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    const MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_DEF
                        && inst->arg0 != NULL
                        && strcmp(inst->arg0, "value") == 0
                        && inst->result_name != NULL
                        && strcmp(inst->result_name, "value.1") == 0
                        && inst->requires_source_local_decl_emit) {
                        found_value_def = true;
                    }
                    if (inst->kind == MIR_INST_RETURN) {
                        for (size_t ui = 0; ui < inst->use_count; ui++) {
                            if (inst->uses[ui] != NULL
                                && strcmp(inst->uses[ui], "value.1") == 0) {
                                found_return_value_use = true;
                            }
                        }
                    }
                    if (inst->kind == MIR_INST_STMT
                        && inst->arg0 != NULL
                        && strcmp(inst->arg0, "value") == 0) {
                        found_value_stmt_fallback = true;
                    }
                }
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && found_value_def
               && found_return_value_use
               && !found_value_stmt_fallback);
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
