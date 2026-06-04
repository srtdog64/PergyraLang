static void
test_mir_lowering_part_c(void)
{
    TEST("MIR DCE uses statement shape facts without AST payload");
    {
        MIRInstruction *insts = calloc(1, sizeof(MIRInstruction));
        MIRBasicBlock block = { 0 };
        MIRRoutine routine = { 0 };
        bool changed = false;
        bool kept_effect;
        bool removed_query;

        if (insts != NULL) {
            insts[0].kind = MIR_INST_STMT;
            insts[0].name = "stmt";
            insts[0].arg0 = "Log";
            insts[0].has_source_location = true;
            insts[0].source_ast_type = AST_CALL;
            insts[0].has_surface_usage_facts = true;
            block.instructions = insts;
            block.instruction_count = 1;
            block.instruction_capacity = 1;
            routine.name = "ShapeDce";
            routine.blocks = &block;
            routine.block_count = 1;
        }

        kept_effect = insts != NULL
            && mir_run_dce_on_routine(&routine, &changed)
            && block.instruction_count == 1
            && !changed;
        if (kept_effect) {
            insts[0].arg0 = "ChannelLength";
            routine.dce_removed_count = 0;
            changed = false;
            removed_query = mir_run_dce_on_routine(&routine, &changed)
                && block.instruction_count == 0
                && changed
                && routine.dce_removed_count == 1;
        } else {
            removed_query = false;
        }
        EXPECT(kept_effect && removed_query);
        free(block.instructions);
    }

    TEST("MIR DCE does not preserve user Intent-prefixed statements");
    {
        MIRInstruction *insts = calloc(1, sizeof(MIRInstruction));
        MIRBasicBlock block = { 0 };
        MIRRoutine routine = { 0 };
        bool changed = false;
        bool removed_user_intent;

        if (insts != NULL) {
            insts[0].kind = MIR_INST_STMT;
            insts[0].name = "IntentDomainAction";
            block.instructions = insts;
            block.instruction_count = 1;
            block.instruction_capacity = 1;
            routine.name = "IntentPrefixDce";
            routine.blocks = &block;
            routine.block_count = 1;
        }

        removed_user_intent = insts != NULL
            && mir_run_dce_on_routine(&routine, &changed)
            && block.instruction_count == 0
            && changed
            && routine.dce_removed_count == 1;
        EXPECT(removed_user_intent);
        free(block.instructions);
    }

    TEST("MIR validator rejects CFG-backed non-CFG body fallback state");
    {
        const char *src =
            "func Probe() -> Int {\n"
            "    return 1;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *probe = NULL;
        char *mir_error = NULL;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            probe = find_mir_routine_mut(mir, "Probe", MIR_SCOPE_FUNCTION);
        if (probe != NULL) {
            probe->used_non_cfg_body_fallback = true;
            probe->non_cfg_body_fallback_count = 1;
            rejected =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "used non-CFG body fallback") != NULL;
        }
        EXPECT(ok && probe != NULL && rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR lowering records program-level non-CFG fallback inventory");
    {
        const char *src =
            "func Probe() -> Int {\n"
            "    return 1;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);

        EXPECT(ok
               && mir != NULL
               && mir->has_non_cfg_body_fallback_inventory
               && mir->non_cfg_body_fallback_total == 0
               && mir->non_cfg_body_fallback_routine_count == 0
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects non-CFG fallback flag without count");
    {
        MIRRoutine routine = { 0 };
        MIRProgram mir = { 0 };
        char *mir_error = NULL;
        bool rejected;

        routine.name = "NonCfgFlagOnly";
        routine.used_non_cfg_body_fallback = true;
        routine.non_cfg_body_fallback_count = 0;
        mir.routines = &routine;
        mir.routine_count = 1;

        rejected = !mir_validate(&mir, &mir_error)
            && mir_error != NULL
            && strstr(mir_error, "fallback flag without fallback count") != NULL;
        EXPECT(rejected);
        free(mir_error);
    }

    TEST("MIR validator rejects stale program-level non-CFG fallback inventory");
    {
        MIRBasicBlock block = { 0 };
        MIRRoutine routine = { 0 };
        MIRProgram mir = { 0 };
        char *mir_error = NULL;
        bool rejected;

        block.id = 0;
        block.is_entry = true;
        block.is_reachable = true;
        routine.name = "StaleNonCfgInventory";
        routine.blocks = &block;
        routine.block_count = 1;
        routine.entry_block = 0;
        routine.has_liveness = true;
        routine.has_use_def_summary = true;
        routine.has_dce = true;
        routine.used_non_cfg_body_fallback = true;
        routine.non_cfg_body_fallback_count = 1;
        mir.routines = &routine;
        mir.routine_count = 1;
        mir.has_non_cfg_body_fallback_inventory = true;
        mir.non_cfg_body_fallback_total = 0;
        mir.non_cfg_body_fallback_routine_count = 0;

        rejected = !mir_validate(&mir, &mir_error)
            && mir_error != NULL
            && strstr(mir_error, "non-CFG fallback inventory is stale") != NULL;
        EXPECT(rejected);
        free(mir_error);
    }

    TEST("MIR validator rejects missing routine inventory");
    {
        MIRProgram mir = { 0 };
        char *mir_error = NULL;
        bool rejected;

        mir.routine_count = 1;
        mir.routines = NULL;

        rejected = !mir_validate(&mir, &mir_error)
            && mir_error != NULL
            && strstr(mir_error, "without routine inventory") != NULL;
        EXPECT(rejected);
        free(mir_error);
    }

    TEST("MIR validator rejects missing block inventory");
    {
        MIRRoutine routine = { 0 };
        MIRProgram mir = { 0 };
        char *mir_error = NULL;
        bool rejected;

        routine.name = "MissingBlocks";
        routine.block_count = 1;
        routine.blocks = NULL;
        mir.routine_count = 1;
        mir.routines = &routine;

        rejected = !mir_validate(&mir, &mir_error)
            && mir_error != NULL
            && strstr(mir_error, "without block inventory") != NULL;
        EXPECT(rejected);
        free(mir_error);
    }

    TEST("MIR validator rejects missing value-summary inventory");
    {
        MIRRoutine routine = { 0 };
        MIRProgram mir = { 0 };
        char *mir_error = NULL;
        bool rejected;

        routine.name = "MissingValueSummaries";
        routine.value_summary_count = 1;
        routine.value_summaries = NULL;
        mir.routine_count = 1;
        mir.routines = &routine;

        rejected = !mir_validate(&mir, &mir_error)
            && mir_error != NULL
            && strstr(mir_error, "without value-summary inventory") != NULL;
        EXPECT(rejected);
        free(mir_error);
    }

    TEST("MIR validator rejects invalid source-statement emit fact");
    {
        const char *src =
            "func DefSourceEmitFact() -> Int {\n"
            "    let value: Int = 7;\n"
            "    return value;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *def_inst = NULL;
        ASTNode *saved_ast = NULL;
        char *mir_error = NULL;
        bool rejected_missing_fact = false;
        bool rejected_invalid_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "DefSourceEmitFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && def_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_DEF
                        && inst->requires_source_statement_emit) {
                        def_inst = inst;
                        break;
                    }
                }
            }
        }
        if (def_inst != NULL) {
            def_inst->requires_source_statement_emit = false;
            rejected_missing_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "DEF is missing source-statement emit fact") != NULL;
            free(mir_error);
            mir_error = NULL;
            def_inst->requires_source_statement_emit = true;

            saved_ast = def_inst->ast;
            def_inst->ast = NULL;
            rejected_invalid_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "source-statement emit fact is invalid") != NULL;
            def_inst->ast = saved_ast;
        }
        EXPECT(ok
               && routine != NULL
               && def_inst != NULL
               && saved_ast != NULL
               && rejected_missing_fact
               && rejected_invalid_fact
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR residual STMT policy rejects local dataflow statements");
    {
        ASTNode payload = {0};
        MIRInstruction inst = {0};
        bool rejects_let;
        bool rejects_destructure;
        bool rejects_assignment;
        bool keeps_effect_call;

        payload.type = AST_CALL;
        inst.kind = MIR_INST_STMT;
        inst.name = "stmt";
        inst.ast = &payload;
        inst.has_source_location = true;
        inst.has_source_statement_index = true;
        inst.source_statement_index = 0;

        inst.source_ast_type = AST_LET_DECL;
        rejects_let = !mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_ast_type = AST_LET_DESTRUCTURE;
        rejects_destructure =
            !mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_ast_type = AST_ASSIGNMENT;
        rejects_assignment =
            !mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_ast_type = AST_CALL;
        inst.arg0 = "Log";
        keeps_effect_call =
            mir_instruction_source_stmt_fallback_is_allowed(&inst);

        EXPECT(rejects_let
               && rejects_destructure
               && rejects_assignment
               && keeps_effect_call);
    }

    TEST("MIR validator rejects residual STMT without source inventory fact");
    {
        const char *src =
            "func ResidualStmtInventoryFact() -> Void {\n"
            "    if true {\n"
            "        defer { Log(1); };\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *stmt_inst = NULL;
        char *mir_error = NULL;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "ResidualStmtInventoryFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && stmt_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_STMT
                        && inst->source_ast_type == AST_DEFER_STMT) {
                        stmt_inst = inst;
                        break;
                    }
                }
            }
        }
        if (stmt_inst != NULL) {
            stmt_inst->has_source_statement_index = false;
            rejected =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "STMT fallback is missing source statement inventory fact") != NULL;
            stmt_inst->has_source_statement_index = true;
        }
        EXPECT(ok
               && routine != NULL
               && stmt_inst != NULL
               && rejected
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects missing channel receive emit fact");
    {
        const char *src =
            "func ChannelReceiveFact(ch: Channel<Int>) -> Int {\n"
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
            "func SelectReceiveFact(ch: Channel<Int>) -> Int {\n"
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
                        && inst->source_ast_type == AST_WITH_STMT) {
                        claim_inst = inst;
                        break;
                    }
                }
            }
        }
        if (claim_inst != NULL) {
            saved_layout = claim_inst->type_layout;
            claim_inst->type_layout = NULL;
            rejected_missing_layout =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "with-slot Claim resource op is missing MIR ABI type layout fact") != NULL;
            free(mir_error);
            mir_error = NULL;

            claim_inst->type_layout = mir_abi_lookup("Future");
            rejected_invalid_layout =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "with-slot Claim resource op has invalid MIR ABI type layout fact") != NULL;
            claim_inst->type_layout = saved_layout;
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
            saved_shape = other_def->source_ast_type;
            saved_shape_valid = true;
            other_def->source_ast_type = AST_ASSIGNMENT;
            rejected_invalid_shape =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "source-local-decl emit fact is invalid") != NULL;
            other_def->source_ast_type = saved_shape;
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

    TEST("MIR validator rejects source-compatible branch without payload");
    {
        const char *src =
            "func BranchSourcePayload(x: Int) -> Int {\n"
            "    if x > 0 {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *branch_inst = NULL;
        ASTNode *saved_ast = NULL;
        ASTNodeType saved_source_ast_type = 0;
        bool saved_has_source_location = false;
        char *mir_error = NULL;
        bool rejected_missing_fact = false;
        bool rejected_mismatched_source_type = false;
        bool rejected_missing_payload = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "BranchSourcePayload",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && branch_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_BRANCH) {
                        branch_inst = inst;
                        break;
                    }
                }
            }
        }
        if (branch_inst != NULL) {
            saved_ast = branch_inst->ast;
            saved_source_ast_type = branch_inst->source_ast_type;
            saved_has_source_location = branch_inst->has_source_location;
            branch_inst->branch_shape = MIR_BRANCH_MATCH_CASE;
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
            branch_inst->source_ast_type = AST_BLOCK;
            rejected_mismatched_source_type =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "source-branch emit fact is invalid") != NULL;
            free(mir_error);
            mir_error = NULL;

            branch_inst->ast = NULL;
            rejected_missing_payload =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "source-branch emit fact is invalid") != NULL;
            branch_inst->ast = saved_ast;
            branch_inst->source_ast_type = saved_source_ast_type;
            branch_inst->has_source_location = saved_has_source_location;
            branch_inst->branch_shape = MIR_BRANCH_EXPR;
            branch_inst->requires_source_branch_emit = false;
        }
        EXPECT(ok
               && routine != NULL
               && branch_inst != NULL
               && saved_ast != NULL
               && rejected_missing_fact
               && rejected_mismatched_source_type
               && rejected_missing_payload
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects hosted method signature metadata drift");
    {
        const char *src =
            "enum Status {\n"
            "    Idle,\n"
            "    Busy,\n"
            "    func Code(self) -> Int { return 7; }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclMethod *mutated_method = NULL;
        size_t saved_param_count = 0;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "Status") == 0
                    && header->method_metadata_count > 0) {
                    mutated_method = &header->method_metadata[0];
                    saved_param_count = mutated_method->param_count;
                    mutated_method->param_count++;
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "signature metadata drift") != NULL;
        if (mutated_method != NULL)
            mutated_method->param_count = saved_param_count;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects hosted method routine link metadata drift");
    {
        const char *src =
            "class Item {\n"
            "    func Code(self) -> Int { return 7; }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name == NULL
                    || strcmp(header->name, "Item") != 0
                    || header->method_metadata_count == 0) {
                    continue;
                }
                MIRDeclMethod *method = &header->method_metadata[0];
                if (method->has_routine
                    && method->routine_index < mir->routine_count) {
                    mir->routines[method->routine_index].name = "OtherCode";
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "routine link metadata drift") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR method routine linker requires owner metadata");
    {
        MIRDeclMethod method = { 0 };
        MIRDeclHeader header = { 0 };
        MIRRoutine routine = { 0 };
        MIRProgram mir = { 0 };

        method.name = "Code";
        method.owner_name = "Item";
        header.name = "Item";
        header.method_count = 1;
        header.method_metadata = &method;
        header.method_metadata_count = 1;
        routine.name = "Code";
        routine.owner_name = NULL;
        routine.kind = MIR_SCOPE_METHOD;
        mir.decl_headers = &header;
        mir.decl_header_count = 1;
        mir.routines = &routine;
        mir.routine_count = 1;

        mir_link_decl_method_routines(&mir);
        EXPECT(!method.has_routine);
    }

    TEST("MIR validator rejects declaration header name metadata drift");
    {
        const char *src =
            "class Item {\n"
            "    func Code(self) -> Int { return 7; }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL && strcmp(header->name, "Item") == 0) {
                    header->name = "OtherItem";
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "name metadata drift") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR declaration headers preserve pointer-self ABI shape");
    {
        const char *src =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    func Read(self) -> Int { return hp; }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *player = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                if (mir->decl_headers[i].name != NULL
                    && strcmp(mir->decl_headers[i].name, "Player") == 0) {
                    player = &mir->decl_headers[i];
                    break;
                }
            }
        }
        EXPECT(ok
               && player != NULL
               && player->uses_pointer_self
               && player->method_count == 1
               && player->method_metadata_count == 1
               && mir_validate(mir, NULL));
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects pointer-self ABI metadata drift");
    {
        const char *src =
            "vessel Handle {\n"
            "    let value: Int;\n"
            "    func Read(self) -> Int { return value; }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL && strcmp(header->name, "Handle") == 0) {
                    header->uses_pointer_self = false;
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "pointer-self ABI metadata drift") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

}
