static void
test_mir_lowering_part_h(void)
{
    TEST("MIR validator rejects missing channel receive emit fact");
    {
        const char *src =
            "func ChannelReceiveFact() -> Int {\n"
            "    let ch: Channel<Int> = Channel(1);\n"
            "    ch <- 7;\n"
            "    let other: Int = 1;\n"
            "    let value: Int = <- ch;\n"
            "    return value + other;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *def_inst = NULL;
        MIRInstruction *other_def_inst = NULL;
        char *mir_error = NULL;
        bool rejected_missing_fact = false;
        bool rejected_invalid_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "ChannelReceiveFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && def_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_DEF
                        && inst->requires_channel_receive_statement_emit) {
                        def_inst = inst;
                    } else if (inst->kind == MIR_INST_DEF
                        && inst->arg0 != NULL
                        && strcmp(inst->arg0, "other") == 0) {
                        other_def_inst = inst;
                    }
                    if (def_inst != NULL && other_def_inst != NULL)
                        break;
                }
            }
        }
        if (def_inst != NULL) {
            def_inst->requires_channel_receive_statement_emit = false;
            rejected_missing_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "channel receive DEF is missing source-statement receive emit fact") != NULL;
            def_inst->requires_channel_receive_statement_emit = true;
        }
        free(mir_error);
        mir_error = NULL;
        if (other_def_inst != NULL) {
            other_def_inst->requires_channel_receive_statement_emit = true;
            rejected_invalid_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "source-statement receive emit fact is invalid") != NULL;
            other_def_inst->requires_channel_receive_statement_emit = false;
        }
        EXPECT(ok
               && routine != NULL
               && def_inst != NULL
               && other_def_inst != NULL
               && rejected_missing_fact
               && rejected_invalid_fact
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects invalid select receive emit fact");
    {
        const char *src =
            "func SelectReceiveFact() -> Int {\n"
            "    let ch: Channel<Int> = Channel(1);\n"
            "    ch <- 7;\n"
            "    let regular: Int = <- ch;\n"
            "    ch <- 9;\n"
            "    select {\n"
            "        case v = <-ch:\n"
            "            return v + regular;\n"
            "        default:\n"
            "            return regular;\n"
            "    }\n"
            "    return regular;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *select_def_inst = NULL;
        MIRInstruction *regular_def_inst = NULL;
        char *mir_error = NULL;
        bool rejected_missing_fact = false;
        bool rejected_invalid_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "SelectReceiveFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind != MIR_INST_DEF
                        || !inst->requires_channel_receive_statement_emit) {
                        continue;
                    }
                    if (inst->requires_select_receive_statement_emit)
                        select_def_inst = inst;
                    else
                        regular_def_inst = inst;
                }
            }
        }
        if (select_def_inst != NULL) {
            select_def_inst->requires_select_receive_statement_emit = false;
            rejected_missing_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "select receive DEF is missing select receive emit fact") != NULL;
            select_def_inst->requires_select_receive_statement_emit = true;
        }
        free(mir_error);
        mir_error = NULL;
        if (regular_def_inst != NULL) {
            regular_def_inst->requires_select_receive_statement_emit = true;
            rejected_invalid_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "select receive emit fact is invalid") != NULL;
            regular_def_inst->requires_select_receive_statement_emit = false;
        }
        EXPECT(ok
               && routine != NULL
               && select_def_inst != NULL
               && regular_def_inst != NULL
               && rejected_missing_fact
               && rejected_invalid_fact
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects invalid with-slot claim ABI fact");
    {
        const char *src =
            "func WithClaimFact() -> Int {\n"
            "    with slot<Int> as s {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *claim_inst = NULL;
        const MIRTypeLayout *saved_layout = NULL;
        uint32_t saved_layout_id = 0;
        char *mir_error = NULL;
        bool rejected_missing_layout = false;
        bool rejected_invalid_layout = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "WithClaimFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && claim_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_RESOURCE_OP
                        && inst->name != NULL
                        && strcmp(inst->name, "Claim") == 0
                        && inst->source_node_type == AST_WITH_STMT) {
                        claim_inst = inst;
                        break;
                    }
                }
            }
        }
        if (claim_inst != NULL) {
            saved_layout = claim_inst->type_layout;
            saved_layout_id = claim_inst->abi_layout_id;
            claim_inst->type_layout = NULL;
            claim_inst->abi_layout_id = 0;
            rejected_missing_layout =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "with-slot Claim resource op is missing MIR ABI type layout fact") != NULL;
            free(mir_error);
            mir_error = NULL;

            claim_inst->type_layout = mir_abi_lookup("Future");
            claim_inst->abi_layout_id = mir_abi_layout_id(claim_inst->type_layout);
            rejected_invalid_layout =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "with-slot Claim resource op has invalid MIR ABI type layout fact") != NULL;
            claim_inst->type_layout = saved_layout;
            claim_inst->abi_layout_id = saved_layout_id;
        }
        EXPECT(ok
               && routine != NULL
               && claim_inst != NULL
               && saved_layout != NULL
               && rejected_missing_layout
               && rejected_invalid_layout
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR records with-slot scope-exit Release fact");
    {
        const char *src =
            "func WithReleaseFact() -> Void {\n"
            "    with slot<Int> as s {\n"
            "        let i: Int = 0;\n"
            "        while i < 2 {\n"
            "            Write(s, i);\n"
            "            Log(Read(s));\n"
            "            i = i + 1;\n"
            "        }\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const HIRRoutine *hir_routine = NULL;
        const MIRRoutine *routine = NULL;
        size_t claim_block_id = (size_t)-1;
        size_t release_block_id = (size_t)-1;
        bool found_write = false;
        bool found_read = false;
        bool release_has_layout = false;
        bool release_has_scope_exit_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok) {
            routine = find_mir_routine(mir, "WithReleaseFact",
                                       MIR_SCOPE_FUNCTION);
            for (size_t hi = 0; hi < hir->routine_count; hi++) {
                if (hir->routines[hi].name != NULL
                    && strcmp(hir->routines[hi].name,
                              "WithReleaseFact") == 0) {
                    hir_routine = &hir->routines[hi];
                    break;
                }
            }
        }
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count; bi++) {
                const MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    const MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind != MIR_INST_RESOURCE_OP
                        || inst->name == NULL
                        || inst->slot_anchor == NULL
                        || strcmp(inst->slot_anchor, "s") != 0) {
                        continue;
                    }
                    if (strcmp(inst->name, "Claim") == 0) {
                        claim_block_id = bi;
                    } else if (strcmp(inst->name, "Write") == 0) {
                        found_write = true;
                    } else if (strcmp(inst->name, "Read") == 0) {
                        found_read = true;
                    } else if (strcmp(inst->name, "Release") == 0) {
                        release_block_id = bi;
                        release_has_layout =
                            inst->type_layout != NULL
                            && inst->abi_type_name != NULL
                            && strcmp(inst->abi_type_name, "Slot<Int>") == 0;
                    }
                }
            }
        }
        if (routine != NULL
            && hir_routine != NULL
            && release_block_id < routine->block_count) {
            size_t hir_block_id =
                routine->blocks[release_block_id].source_hir_block_id;
            release_has_scope_exit_fact =
                hir_block_id < hir_routine->cfg.block_count
                && hir_routine->cfg.blocks[hir_block_id]
                       .resource_scope_exit_count == 1;
        }
        EXPECT(ok
               && routine != NULL
               && hir_routine != NULL
               && claim_block_id == routine->entry_block
               && found_write
               && found_read
               && release_block_id != (size_t)-1
               && release_block_id != routine->entry_block
               && release_has_layout
               && release_has_scope_exit_fact
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects invalid source-local-decl emit fact");
    {
        const char *src =
            "func LocalDeclEmitFact() -> Int {\n"
            "    let value: Int = 7;\n"
            "    let other: Int = 1;\n"
            "    return value + other;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *let_def = NULL;
        MIRInstruction *other_def = NULL;
        ASTNodeType saved_shape = AST_LET_DECL;
        bool saved_shape_valid = false;
        char *mir_error = NULL;
        bool rejected_missing_local_fact = false;
        bool rejected_invalid_shape = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "LocalDeclEmitFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_DEF
                        && inst->requires_source_local_decl_emit
                        && inst->arg0 != NULL
                        && strcmp(inst->arg0, "value") == 0) {
                        let_def = inst;
                    } else if (inst->kind == MIR_INST_DEF
                        && inst->arg0 != NULL
                        && strcmp(inst->arg0, "other") == 0) {
                        other_def = inst;
                    }
                }
            }
        }
        if (let_def != NULL) {
            let_def->requires_source_local_decl_emit = false;
            rejected_missing_local_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "source-statement LET emit is missing local-decl fact") != NULL;
            let_def->requires_source_local_decl_emit = true;
        }
        free(mir_error);
        mir_error = NULL;
        if (other_def != NULL) {
            saved_shape = other_def->source_node_type;
            saved_shape_valid = true;
            other_def->source_node_type = AST_ASSIGNMENT;
            rejected_invalid_shape =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "source-local-decl emit fact is invalid") != NULL;
            other_def->source_node_type = saved_shape;
        }
        EXPECT(ok
               && routine != NULL
               && let_def != NULL
               && other_def != NULL
               && saved_shape_valid
               && saved_shape == AST_LET_DECL
               && let_def->requires_source_local_decl_emit
               && rejected_missing_local_fact
               && rejected_invalid_shape
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR match branch uses captured pattern fact without payload");
    {
        const char *src =
            "func BranchSourcePayload(x: Int) -> Int {\n"
            "    match x {\n"
            "        case 0:\n"
            "            return 1;\n"
            "        default:\n"
            "            return 0;\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *branch_inst = NULL;
        ASTNode *saved_ast = NULL;
        ASTNode *saved_expr0 = NULL;
        ASTNodeType saved_source_node_type = 0;
        bool saved_has_source_location = false;
        bool saved_requires_source_branch_emit = false;
        char *mir_error = NULL;
        bool rejected_missing_match_subject_fact = false;
        bool rejected_missing_fact = false;
        bool rejected_mismatched_source_type = false;
        bool valid_without_payload = false;
        bool rejected_predicate_without_subject_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "BranchSourcePayload",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && branch_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_BRANCH
                        && inst->branch_shape == MIR_BRANCH_MATCH_CASE) {
                        branch_inst = inst;
                        break;
                    }
                }
            }
        }
        if (branch_inst != NULL) {
            saved_ast = branch_inst->ast;
            saved_expr0 = branch_inst->expr0;
            saved_source_node_type = branch_inst->source_node_type;
            saved_has_source_location = branch_inst->has_source_location;
            saved_requires_source_branch_emit =
                branch_inst->requires_source_branch_emit;
            branch_inst->expr0 = NULL;
            rejected_predicate_without_subject_fact =
                !mir_instruction_has_required_branch_condition_fact(branch_inst);
            rejected_missing_match_subject_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "match-case branch is missing MIR match subject fact") != NULL;
            free(mir_error);
            mir_error = NULL;

            branch_inst->expr0 = saved_expr0;
            branch_inst->requires_source_branch_emit = false;
            rejected_missing_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "branch is missing source-branch emit fact") != NULL;
            free(mir_error);
            mir_error = NULL;

            branch_inst->requires_source_branch_emit = true;
            branch_inst->ast = saved_ast;
            branch_inst->has_source_location = true;
            branch_inst->source_node_type = AST_BLOCK;
            rejected_mismatched_source_type =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "source-branch emit fact is invalid") != NULL;
            free(mir_error);
            mir_error = NULL;

            branch_inst->ast = NULL;
            branch_inst->source_node_type = saved_source_node_type;
            branch_inst->has_source_location = saved_has_source_location;
            valid_without_payload =
                mir_instruction_has_required_source_branch_emit_fact(branch_inst)
                && mir_validate(mir, &mir_error);
            free(mir_error);
            mir_error = NULL;
            branch_inst->ast = saved_ast;
            branch_inst->expr0 = saved_expr0;
            branch_inst->source_node_type = saved_source_node_type;
            branch_inst->has_source_location = saved_has_source_location;
            branch_inst->requires_source_branch_emit =
                saved_requires_source_branch_emit;
        }
        EXPECT(ok
               && routine != NULL
               && branch_inst != NULL
               && saved_ast != NULL
               && saved_expr0 != NULL
               && mir_instruction_match_pattern_count(branch_inst) > 0
               && rejected_predicate_without_subject_fact
               && rejected_missing_match_subject_fact
               && rejected_missing_fact
               && rejected_mismatched_source_type
               && valid_without_payload
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR select dispatch branch uses channel fact without payload");
    {
        const char *src =
            "func SelectBranchFact() -> Void {\n"
            "    let ch: Channel<Int> = Channel(2);\n"
            "    select {\n"
            "        case <- ch:\n"
            "            Log(\"ready\");\n"
            "        default:\n"
            "            Log(\"default\");\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *branch_inst = NULL;
        ASTNode *saved_ast = NULL;
        ASTNode *saved_expr0 = NULL;
        char *mir_error = NULL;
        bool valid_without_payload = false;
        bool rejected_missing_channel_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "SelectBranchFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && branch_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_BRANCH
                        && inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH) {
                        branch_inst = inst;
                        break;
                    }
                }
            }
        }
        if (branch_inst != NULL) {
            saved_ast = branch_inst->ast;
            saved_expr0 = branch_inst->expr0;
            branch_inst->ast = NULL;
            valid_without_payload =
                mir_instruction_has_required_source_branch_emit_fact(branch_inst)
                && mir_validate(mir, &mir_error);
            free(mir_error);
            mir_error = NULL;

            branch_inst->ast = saved_ast;
            branch_inst->expr0 = NULL;
            rejected_missing_channel_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "source-branch emit fact is invalid") != NULL;
            branch_inst->expr0 = saved_expr0;
            free(mir_error);
            mir_error = NULL;
        }
        EXPECT(ok
               && routine != NULL
               && branch_inst != NULL
               && saved_ast != NULL
               && saved_expr0 != NULL
               && valid_without_payload
               && rejected_missing_channel_fact
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

}
