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
            "    priority: 1;\n"
            "    success: true;\n"
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
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentEval", "priority", "Patrol"));
            EXPECT(block_has_inst_named_args(&patrol->blocks[patrol->entry_block],
                "IntentCheck", "success", "Patrol"));
        }
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR carries exact intent outcome binding and on DEF");
    {
        const char *src =
            "subject Worker {\n"
            "    action Measure(self) -> Int { return 1; }\n"
            "}\n"
            "zone Lab { subject slot worker: Worker }\n"
            "intent Inspect(lab: Lab, worker: Worker) {\n"
            "    step Measure {\n"
            "        where: Lab;\n"
            "        using: lab;\n"
            "        who: worker;\n"
            "        on outcome: worker.Measure();\n"
            "        expect: outcome > 0;\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *inspect = NULL;
        MIRInstruction *carrier = NULL;
        MIRInstruction *on_eval = NULL;
        const char *saved_carrier_type = NULL;
        const char *saved_eval_result = NULL;
        char expected_action_id[16];
        char *mir_error = NULL;
        bool rejected_type_drift = false;
        bool rejected_missing_def = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);

        if (ok)
            inspect = find_mir_routine_mut(mir, "Inspect", MIR_SCOPE_INTENT);
        if (inspect != NULL) {
            MIRBasicBlock *block = &inspect->blocks[inspect->entry_block];
            for (size_t i = 0; i < block->instruction_count; i++) {
                MIRInstruction *inst = &block->instructions[i];
                if (mir_instruction_is_intent_stmt(
                        inst, "IntentOutcomeBinding")) {
                    carrier = inst;
                } else if (mir_instruction_is_intent_stmt(inst, "IntentEval")
                           && mir_instruction_intent_phase_matches(inst, "on")) {
                    on_eval = inst;
                }
            }
        }
        expected_action_id[0] = '\0';
        if (carrier != NULL && carrier->ast != NULL) {
            (void)snprintf(expected_action_id, sizeof(expected_action_id),
                "%u", (unsigned)
                    ast_intent_step_outcome_action_decl_syntax_id(
                        carrier->ast));
        }
        if (carrier != NULL) {
            saved_carrier_type = carrier->abi_type_name;
            carrier->abi_type_name = "Bool";
            rejected_type_drift = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "missing or mismatched") != NULL;
            carrier->abi_type_name = saved_carrier_type;
            free(mir_error);
            mir_error = NULL;
        }
        if (on_eval != NULL) {
            saved_eval_result = on_eval->result_name;
            on_eval->result_name = NULL;
            rejected_missing_def = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "one matching on DEF") != NULL;
            on_eval->result_name = saved_eval_result;
            free(mir_error);
            mir_error = NULL;
        }
        EXPECT(ok && inspect != NULL && carrier != NULL && on_eval != NULL
               && carrier->slot_anchor != NULL
               && strcmp(carrier->slot_anchor, "outcome") == 0
               && carrier->result_name != NULL
               && strcmp(carrier->result_name, "outcome") == 0
               && carrier->abi_type_name != NULL
               && strcmp(carrier->abi_type_name, "Int") == 0
               && carrier->arg0 != NULL
               && strcmp(carrier->arg0, expected_action_id) == 0
               && carrier->arg1 != NULL
               && strcmp(carrier->arg1, "Measure") == 0
               && on_eval->result_name != NULL
               && strcmp(on_eval->result_name, "outcome") == 0
               && on_eval->abi_type_name != NULL
               && strcmp(on_eval->abi_type_name, "Int") == 0
               && rejected_type_drift
               && rejected_missing_def
               && mir_validate(mir, NULL));
        free(mir_error);
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
        bool saved_has_source_location = false;
        char *mir_error = NULL;
        bool rejected_storage = false;
        bool rejected_index = false;
        bool rejected_surface = false;
        bool rejected_source_location = false;
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
                if (inst->ast != NULL) {
                    saved_has_source_location = inst->has_source_location;
                    inst->has_source_location = false;
                    rejected_source_location =
                        !mir_validate(mir, &mir_error)
                        && mir_error != NULL
                        && strstr(mir_error, "source payload without source-location fact") != NULL;
                    inst->has_source_location = saved_has_source_location;
                    free(mir_error);
                    mir_error = NULL;
                }
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
               && rejected_source_location
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
