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

    TEST("MIR validator rejects intent instruction metadata drift");
    {
        const char *src =
            "subject Hero {\n"
            "    action Guard(self) -> Void { return; }\n"
            "}\n"
            "zone Arena {\n"
            "    subject slot hero: Hero\n"
            "}\n"
            "intent Patrol(arena: Arena, hero: Hero) {\n"
            "    step Guard {\n"
            "        where: Arena;\n"
            "        using: arena;\n"
            "        who: hero;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *patrol = NULL;
        MIRInstruction *intent_step = NULL;
        MIRInstruction *intent_who = NULL;
        MIRInstruction *intent_check = NULL;
        const char *saved_step_arg0 = NULL;
        const char *saved_who_arg0 = NULL;
        const char *saved_who_arg1 = NULL;
        const char *saved_check_arg0 = NULL;
        ASTNode *saved_check_expr0 = NULL;
        char *mir_error = NULL;
        bool rejected_step_name = false;
        bool rejected_payload = false;
        bool rejected_step_link = false;
        bool rejected_phase = false;
        bool rejected_expr_payload = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            patrol = find_mir_routine_mut(mir, "Patrol", MIR_SCOPE_INTENT);
        if (patrol != NULL) {
            MIRBasicBlock *block = &patrol->blocks[patrol->entry_block];
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (inst->name == NULL)
                    continue;
                if (strcmp(inst->name, "IntentStep") == 0)
                    intent_step = inst;
                else if (strcmp(inst->name, "IntentWho") == 0)
                    intent_who = inst;
                else if (strcmp(inst->name, "IntentCheck") == 0
                         && inst->arg0 != NULL
                         && strcmp(inst->arg0, "expect") == 0) {
                    intent_check = inst;
                }
            }
        }
        if (intent_step != NULL) {
            saved_step_arg0 = intent_step->arg0;
            intent_step->arg0 = NULL;
            rejected_step_name =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "IntentStep is missing MIR step name fact") != NULL;
            intent_step->arg0 = saved_step_arg0;
            free(mir_error);
            mir_error = NULL;
        }
        if (intent_who != NULL) {
            saved_who_arg0 = intent_who->arg0;
            intent_who->arg0 = NULL;
            rejected_payload =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "is missing MIR payload fact") != NULL;
            intent_who->arg0 = saved_who_arg0;
            free(mir_error);
            mir_error = NULL;

            saved_who_arg1 = intent_who->arg1;
            intent_who->arg1 = NULL;
            rejected_step_link =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "is missing step link fact") != NULL;
            intent_who->arg1 = saved_who_arg1;
            free(mir_error);
            mir_error = NULL;
        }
        if (intent_check != NULL) {
            saved_check_arg0 = intent_check->arg0;
            intent_check->arg0 = NULL;
            rejected_phase =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "is missing phase fact") != NULL;
            intent_check->arg0 = saved_check_arg0;
            free(mir_error);
            mir_error = NULL;

            saved_check_expr0 = intent_check->expr0;
            intent_check->expr0 = NULL;
            rejected_expr_payload =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "is missing expression payload fact") != NULL;
            intent_check->expr0 = saved_check_expr0;
            free(mir_error);
            mir_error = NULL;
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && intent_step != NULL
               && intent_who != NULL
               && intent_check != NULL
               && rejected_step_name
               && rejected_payload
               && rejected_step_link
               && rejected_phase
               && rejected_expr_payload);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects invalid statement inventory shape");
    {
        const char *src =
            "func InventoryShape() -> Void {\n"
            "    let x = 1;\n"
            "    return;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRBasicBlock *block = NULL;
        ASTNode **saved_items = NULL;
        size_t saved_count = 0;
        bool saved_has_index = false;
        size_t saved_index = 0;
        bool saved_has_surface_usage = false;
        char *mir_error = NULL;
        bool rejected_storage = false;
        bool rejected_index = false;
        bool rejected_surface = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "InventoryShape", MIR_SCOPE_FUNCTION);
        if (routine != NULL && routine->block_count > 0) {
            block = &routine->blocks[routine->entry_block];
            saved_items = block->source_statement_inventory.items;
            saved_count = block->source_statement_inventory.count;
            block->source_statement_inventory.items = NULL;
            block->source_statement_inventory.count = 1;
            rejected_storage = !mir_validate(mir, &mir_error)
                               && mir_error != NULL
                               && strstr(mir_error, "statement inventory") != NULL;
            free(mir_error);
            mir_error = NULL;
            block->source_statement_inventory.items = saved_items;
            block->source_statement_inventory.count = saved_count;

            if (block->instruction_count > 0) {
                saved_has_index = block->instructions[0].has_source_statement_index;
                saved_index = block->instructions[0].source_statement_index;
                block->instructions[0].has_source_statement_index = true;
                block->instructions[0].source_statement_index = saved_count + 1;
                rejected_index = !mir_validate(mir, &mir_error)
                                 && mir_error != NULL
                                 && strstr(mir_error, "source statement index") != NULL;
                block->instructions[0].has_source_statement_index = saved_has_index;
                block->instructions[0].source_statement_index = saved_index;
            }
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (inst->ast == NULL
                    && inst->expr0 == NULL
                    && inst->expr1 == NULL
                    && !inst->has_source_location) {
                    continue;
                }
                saved_has_surface_usage = inst->has_surface_usage_facts;
                inst->has_surface_usage_facts = false;
                rejected_surface = !mir_validate(mir, &mir_error)
                                   && mir_error != NULL
                                   && strstr(mir_error, "surface usage facts") != NULL;
                inst->has_surface_usage_facts = saved_has_surface_usage;
                break;
            }
        }
        EXPECT(ok
               && routine != NULL
               && block != NULL
               && rejected_storage
               && rejected_index
               && rejected_surface);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects Write resource op without value expression fact");
    {
        const char *src =
            "func WritePayload() -> Void {\n"
            "    let slot: Slot<Int> = ClaimSlot<Int>();\n"
            "    Write(slot, 42);\n"
            "    return;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *write_inst = NULL;
        ASTNode *saved_expr0 = NULL;
        char *mir_error = NULL;
        bool rejected_missing_value_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "WritePayload", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && write_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_RESOURCE_OP
                        && inst->name != NULL
                        && strcmp(inst->name, "Write") == 0) {
                        write_inst = inst;
                        break;
                    }
                }
            }
        }
        if (write_inst != NULL) {
            saved_expr0 = write_inst->expr0;
            ASTNode *saved_ast = write_inst->ast;
            write_inst->expr0 = NULL;
            write_inst->ast = NULL;
            rejected_missing_value_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "missing MIR value expression fact") != NULL;
            write_inst->ast = saved_ast;
            write_inst->expr0 = saved_expr0;
        }
        EXPECT(ok
               && routine != NULL
               && write_inst != NULL
               && saved_expr0 != NULL
               && rejected_missing_value_fact
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR records intent observability surface usage fact");
    {
        const char *src =
            "func IntentObs() -> Void {\n"
            "    Log(IntentLastTrace());\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        bool has_intent_observability_fact = false;
        bool has_inventory_fact = false;
        bool rejected_stale_fact = false;
        bool rejected_inventory_stale = false;
        char *mir_error = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "IntentObs", MIR_SCOPE_FUNCTION);
        if (mir != NULL) {
            has_inventory_fact =
                mir->has_inventory_surface_usage_facts
                && mir->inventory_uses_intent_observability_surface;
        }
        if (routine != NULL) {
            for (size_t b = 0; b < routine->block_count; b++) {
                MIRBasicBlock *block = &routine->blocks[b];
                for (size_t i = 0; i < block->instruction_count; i++) {
                    MIRInstruction *inst = &block->instructions[i];
                    if (inst->has_surface_usage_facts
                        && inst->uses_intent_observability_surface) {
                        bool saved_fact = inst->uses_intent_observability_surface;
                        has_intent_observability_fact = true;
                        inst->uses_intent_observability_surface = false;
                        rejected_stale_fact =
                            !mir_validate(mir, &mir_error)
                            && mir_error != NULL
                            && strstr(mir_error, "intent observability surface usage fact") != NULL;
                        inst->uses_intent_observability_surface = saved_fact;
                        free(mir_error);
                        mir_error = NULL;
                        break;
                    }
                }
                if (has_intent_observability_fact)
                    break;
            }
        }
        if (mir != NULL && has_inventory_fact) {
            mir->inventory_uses_intent_observability_surface = false;
            rejected_inventory_stale =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "intent observability inventory surface usage fact") != NULL;
        }
        EXPECT(ok
               && routine != NULL
               && has_inventory_fact
               && has_intent_observability_fact
               && rejected_stale_fact
               && rejected_inventory_stale);
        free(mir_error);
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
               && routine_has_return_source_terminator(routine)
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
                ASTNode *stmt = block->source_statement_inventory.count > 0
                    && block->source_statement_inventory.items != NULL
                    ? block->source_statement_inventory.items[0]
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
        MIRRoutine *routine = NULL;
        MIRInstruction *defer_inst = NULL;
        ASTNode *saved_expr0 = NULL;
        char *mir_error = NULL;
        bool rejected_missing_body_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "BranchDefer", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && defer_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_STMT
                        && inst->source_ast_type == AST_DEFER_STMT) {
                        defer_inst = inst;
                        break;
                    }
                }
            }
        }
        if (defer_inst != NULL) {
            saved_expr0 = defer_inst->expr0;
            ASTNode *saved_ast = defer_inst->ast;
            defer_inst->expr0 = NULL;
            defer_inst->ast = NULL;
            rejected_missing_body_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "missing MIR body expression fact") != NULL;
            defer_inst->ast = saved_ast;
            defer_inst->expr0 = saved_expr0;
        }
        EXPECT(ok
                && mir_validate(mir, NULL)
                && routine != NULL
                && defer_inst != NULL
                && saved_expr0 != NULL
                && rejected_missing_body_fact
                && routine_has_stmt_ast_type(routine, AST_DEFER_STMT)
                && routine_has_stmt_call_named(routine, "Log"));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR carries direct statement call facts");
    {
        const char *src =
            "func DirectCallFact() -> Void {\n"
            "    Log(1);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "DirectCallFact", MIR_SCOPE_FUNCTION);
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine_has_stmt_call_named(routine, "Log")
               && routine_has_stmt_call_fact_named(routine, "Log"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR carries direct initializer call facts");
    {
        const char *src =
            "func SourceValue() -> Int {\n"
            "    return 1;\n"
            "}\n"
            "func InitializerCallFact() -> Int {\n"
            "    let value: Int = SourceValue();\n"
            "    return value;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "InitializerCallFact", MIR_SCOPE_FUNCTION);
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine_has_def_call_fact_named(routine, "SourceValue"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects DEF without initializer expression fact");
    {
        const char *src =
            "func DefExpr() -> Int {\n"
            "    let value: Int = 7;\n"
            "    return value;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *def_inst = NULL;
        ASTNode *saved_expr0 = NULL;
        char *mir_error = NULL;
        bool rejected_missing_init_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "DefExpr", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && def_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_DEF
                        && inst->ast != NULL
                        && inst->ast->type == AST_LET_DECL
                        && inst->ast->data.let_decl.initializer != NULL) {
                        def_inst = inst;
                        break;
                    }
                }
            }
        }
        if (def_inst != NULL) {
            saved_expr0 = def_inst->expr0;
            ASTNode *saved_ast = def_inst->ast;
            def_inst->expr0 = NULL;
            def_inst->ast = NULL;
            rejected_missing_init_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "missing MIR initializer expression fact") != NULL;
            def_inst->ast = saved_ast;
            def_inst->expr0 = saved_expr0;
        }
        EXPECT(ok
               && routine != NULL
               && def_inst != NULL
               && saved_expr0 != NULL
               && rejected_missing_init_fact
               && mir_validate(mir, NULL));
        free(mir_error);
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

    TEST("MIR validator rejects terminator without HIR source provenance");
    {
        const char *src =
            "func TerminatorSource(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    }\n"
            "    return 2;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "TerminatorSource", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && !mutated; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_RETURN) {
                        inst->has_source_terminator_kind = false;
                        mutated = true;
                        break;
                    }
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "HIR source terminator kind") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects terminator without expression fact");
    {
        const char *src =
            "func TerminatorExpr(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    }\n"
            "    return 2;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *branch_inst = NULL;
        MIRInstruction *return_inst = NULL;
        ASTNode *saved_branch_expr = NULL;
        ASTNode *saved_return_expr = NULL;
        char *mir_error = NULL;
        bool rejected_branch = false;
        bool rejected_return = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "TerminatorExpr", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (branch_inst == NULL
                        && inst->kind == MIR_INST_BRANCH
                        && inst->branch_shape == MIR_BRANCH_EXPR)
                        branch_inst = inst;
                    if (return_inst == NULL
                        && inst->kind == MIR_INST_RETURN
                        && inst->ast != NULL)
                        return_inst = inst;
                }
            }
        }
        if (branch_inst != NULL) {
            saved_branch_expr = branch_inst->expr0;
            ASTNode *saved_ast = branch_inst->ast;
            branch_inst->expr0 = NULL;
            branch_inst->ast = NULL;
            rejected_branch =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "branch is missing MIR terminator expression fact") != NULL;
            branch_inst->ast = saved_ast;
            branch_inst->expr0 = saved_branch_expr;
            free(mir_error);
            mir_error = NULL;
        }
        if (return_inst != NULL) {
            saved_return_expr = return_inst->expr0;
            ASTNode *saved_ast = return_inst->ast;
            return_inst->expr0 = NULL;
            return_inst->ast = NULL;
            rejected_return =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "return is missing MIR terminator expression fact") != NULL;
            return_inst->ast = saved_ast;
            return_inst->expr0 = saved_return_expr;
        }
        EXPECT(ok
               && branch_inst != NULL
               && return_inst != NULL
               && saved_branch_expr != NULL
               && saved_return_expr != NULL
               && rejected_branch
               && rejected_return
               && mir_validate(mir, NULL));
        free(mir_error);
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
               && !probe->used_non_cfg_body_fallback
               && probe->non_cfg_body_fallback_count == 0
               && probe->has_dce
               && probe->dce_removed_count > 0
               && !routine_has_stmt_call_named(probe, "ChannelLength"));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

}
