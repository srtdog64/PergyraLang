static void
test_mir_lowering_part_b(void)
{
    TEST("MIR captures intent dispatch and causes metadata");
    {
        const char *src =
            "subject Hero {\n"
            "    action Guard(self) -> Void { return; }\n"
            "}\n"
            "effect Guarded {\n"
            "    subject slot hero: Hero\n"
            "}\n"
            "zone Arena {\n"
            "    subject slot hero: Hero\n"
            "    effect slot guarded: Guarded\n"
            "}\n"
            "intent Patrol(arena: Arena, hero: Hero) {\n"
            "    step Guard {\n"
            "        where: Arena;\n"
            "        using: arena;\n"
            "        who: hero;\n"
            "        causes: Guarded;\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *patrol = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            patrol = find_mir_routine(mir, "Patrol", MIR_SCOPE_INTENT);
        EXPECT(ok && mir_validate(mir, NULL) && patrol != NULL);
        if (patrol != NULL) {
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentParticipant", "hero", "Hero"));
            EXPECT(block_has_inst_named_with_slot(&patrol->blocks[patrol->entry_block],
                "IntentStep", "Guard"));
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentZoneWhere", "Arena", "Guard"));
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentWho", "hero", "Guard"));
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentDispatch", "hero", "Guard"));
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentCauses", "Guarded", "Guard"));
        }
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR inserts phi nodes and SSA-renamed locals on merge");
    {
        const char *src =
            "func Merge(flag: Bool) -> Int {\n"
            "    let score = 0;\n"
            "    if flag {\n"
            "        score = 1;\n"
            "    } else {\n"
            "        score = 2;\n"
            "    }\n"
            "    return score;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *merge = NULL;
        bool has_phi = false;
        bool has_phi_inputs = false;
        bool has_renamed_local = false;
        bool has_branch = false;
        bool has_return = false;
        bool has_uses = false;
        bool has_def = false;
        bool has_entry = false;
        bool has_exit = false;
        bool has_live_out = false;
        const MIRValueSummary *score_summary = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            merge = find_mir_routine(mir, "Merge", MIR_SCOPE_FUNCTION);
        if (merge != NULL)
            score_summary = find_value_summary_prefix_with_use(merge, "score.");
        if (merge != NULL) {
            for (size_t i = 0; i < merge->block_count; i++) {
                const MIRBasicBlock *block = &merge->blocks[i];
                if (block_has_phi_result_prefix(block, "score."))
                    has_phi = true;
                if (block_has_renamed_local_prefix(block, "score."))
                    has_renamed_local = true;
                if (block_has_use_prefix(block, "score."))
                    has_uses = true;
                if (block_has_inst_kind(block, MIR_INST_DEF))
                    has_def = true;
                if (block_has_entry_prefix(block, "score."))
                    has_entry = true;
                if (block_has_exit_prefix(block, "score."))
                    has_exit = true;
                if (block_has_live_out_prefix(block, "score."))
                    has_live_out = true;
                for (size_t j = 0; j < block->instruction_count; j++) {
                    const MIRInstruction *inst = &block->instructions[j];
                    if (inst->kind == MIR_INST_PHI && inst->phi_incoming_count >= 2)
                        has_phi_inputs = true;
                    if (inst->kind == MIR_INST_BRANCH)
                        has_branch = true;
                    if (inst->kind == MIR_INST_RETURN)
                        has_return = true;
                }
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && merge != NULL
               && merge->phi_inserted_count > 0
               && merge->renamed_value_count > 0
               && has_phi
               && has_phi_inputs
               && has_renamed_local
               && has_def
               && has_entry
               && has_exit
               && has_live_out
               && has_uses
               && has_branch
               && has_return
               && merge->has_liveness
               && merge->has_dce
               && merge->live_value_count > 0
               && merge->has_use_def_summary
               && score_summary != NULL
               && score_summary->slot_anchor != NULL
               && strcmp(score_summary->slot_anchor, "score") == 0
               && score_summary->use_count > 0
               && score_summary->ast_write_count > 1
               && score_summary->has_ast_reassignment
               && score_summary->used_outside_def_block
               && score_summary->crosses_block_boundary
               && score_summary->live_out_block_count > 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR DCE removes dead SSA defs and dead phi merges");
    {
        const char *src =
            "func DeadMerge(flag: Bool) -> Int {\n"
            "    let live = 1;\n"
            "    let dead = 0;\n"
            "    if flag {\n"
            "        dead = 2;\n"
            "    } else {\n"
            "        dead = 3;\n"
            "    }\n"
            "    return live;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *dead_merge = NULL;
        const MIRValueSummary *live_summary = NULL;
        bool has_dead_phi = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            dead_merge = find_mir_routine(mir, "DeadMerge", MIR_SCOPE_FUNCTION);
        if (dead_merge != NULL)
            live_summary = find_value_summary_prefix_with_use(dead_merge, "live.");
        if (dead_merge != NULL) {
            for (size_t bi = 0; bi < dead_merge->block_count; bi++) {
                if (block_has_phi_result_prefix(&dead_merge->blocks[bi], "dead.")) {
                    has_dead_phi = true;
                    break;
                }
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && dead_merge != NULL
               && dead_merge->has_dce
               && dead_merge->dce_removed_count > 0
               && live_summary != NULL
               && !has_dead_phi);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR lowers for-loop init as loop-init instead of fallback statement");
    {
        const char *src =
            "func CfgOwnedControl(flag: Bool, value: Int) -> Int {\n"
            "    for i in 0..3 {\n"
            "        if flag {\n"
            "            continue;\n"
            "        }\n"
            "        break;\n"
            "    }\n"
            "    match value {\n"
            "        case 0:\n"
            "            return 1;\n"
            "        default:\n"
            "            value = value + 1;\n"
            "    }\n"
            "    return value;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "CfgOwnedControl", MIR_SCOPE_FUNCTION);
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine_has_inst_kind(routine, MIR_INST_LOOP_INIT)
               && routine_has_complete_loop_init_for(routine, "i")
               && routine_has_complete_loop_branch_for(routine, "i")
               && !routine_has_stmt_ast_type(routine, AST_FOR_LOOP)
               && !routine_has_stmt_ast_type(routine, AST_MATCH_STMT)
               && !routine_has_stmt_ast_type(routine, AST_BREAK)
               && !routine_has_stmt_ast_type(routine, AST_CONTINUE)
               && !routine_has_stmt_ast_type(routine, AST_IF_STMT));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR lowers for-in init as loop-init instead of fallback statement");
    {
        const char *src =
            "func ForInList(values: List<Int>) -> Int {\n"
            "    let total: Int = 0;\n"
            "    for value in values {\n"
            "        total = total + value;\n"
            "    }\n"
            "    return total;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "ForInList", MIR_SCOPE_FUNCTION);
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine_has_inst_kind(routine, MIR_INST_LOOP_INIT)
               && routine_has_complete_loop_init_for(routine, "value")
               && routine_has_complete_loop_branch_for(routine, "value")
               && !routine_has_stmt_ast_type(routine, AST_FOR_LOOP));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects CFG-owned control fallback statements");
    {
        const char *src =
            "func CfgOwnedControl(flag: Bool, value: Int) -> Int {\n"
            "    for i in 0..3 {\n"
            "        if flag {\n"
            "            continue;\n"
            "        }\n"
            "        break;\n"
            "    }\n"
            "    return value;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        char *mir_error = NULL;
        bool injected = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "CfgOwnedControl", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && !injected; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                ASTNode *stmt = block->source_statement_count > 0
                    ? block->source_statements[0]
                    : NULL;
                if (stmt == NULL || stmt->type != AST_FOR_LOOP
                    || (!block->has_succ_true && !block->has_succ_false)) {
                    continue;
                }
                MIRInstruction *grown = realloc(block->instructions,
                    (block->instruction_count + 1) * sizeof(MIRInstruction));
                if (grown == NULL)
                    break;
                block->instructions = grown;
                memset(&block->instructions[block->instruction_count], 0,
                    sizeof(MIRInstruction));
                block->instructions[block->instruction_count].id =
                    routine->instruction_count++;
                block->instructions[block->instruction_count].kind = MIR_INST_STMT;
                block->instructions[block->instruction_count].name = "stmt";
                block->instructions[block->instruction_count].ast = stmt;
                block->instruction_count++;
                injected = true;
            }
        }
        rejected = ok
                   && injected
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "CFG-owned control statement") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR preserves defer statements inside CFG branch blocks");
    {
        const char *src =
            "func BranchDefer() -> Void {\n"
            "    if true {\n"
            "        defer { Log(1); };\n"
            "    }\n"
            "    Log(2);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "BranchDefer", MIR_SCOPE_FUNCTION);
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine_has_stmt_ast_type(routine, AST_DEFER_STMT)
               && routine_has_stmt_call_named(routine, "Log"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR preserves defer statements inside CFG loop blocks");
    {
        const char *src =
            "func LoopDefer() -> Void {\n"
            "    while true {\n"
            "        defer {\n"
            "            Log(1);\n"
            "        };\n"
            "        break;\n"
            "    }\n"
            "    Log(2);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        EXPECT(lower_mir_from_source(src, &hir, &rir, &mir)
               && mir != NULL
               && mir->routine_count == 1
               && mir_validate(mir, NULL)
               && routine_has_stmt_ast_type(&mir->routines[0], AST_DEFER_STMT)
               && routine_has_stmt_call_named(&mir->routines[0], "Log"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR DCE removes dead pure-query statements while preserving routine validity");
    {
        const char *src =
            "func Probe(ch: Channel<Int>) -> Int {\n"
            "    ChannelLength(ch);\n"
            "    return 1;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *probe = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            probe = find_mir_routine(mir, "Probe", MIR_SCOPE_FUNCTION);
        EXPECT(ok
               && mir_validate(mir, NULL)
               && probe != NULL
               && probe->has_dce
               && probe->dce_removed_count > 0
               && !routine_has_stmt_call_named(probe, "ChannelLength"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
