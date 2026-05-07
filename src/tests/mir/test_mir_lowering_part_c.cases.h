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
        char *mir_error = NULL;
        bool rejected_missing_fact = false;
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
            branch_inst->branch_shape = MIR_BRANCH_MATCH_CASE;
            branch_inst->requires_source_branch_emit = false;
            rejected_missing_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "branch is missing source-branch emit fact") != NULL;
            free(mir_error);
            mir_error = NULL;

            branch_inst->requires_source_branch_emit = true;
            branch_inst->ast = NULL;
            rejected_missing_payload =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "source-branch emit fact is invalid") != NULL;
            branch_inst->ast = saved_ast;
            branch_inst->branch_shape = MIR_BRANCH_EXPR;
            branch_inst->requires_source_branch_emit = false;
        }
        EXPECT(ok
               && routine != NULL
               && branch_inst != NULL
               && saved_ast != NULL
               && rejected_missing_fact
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
                    header->method_metadata[0].param_count++;
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
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
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

    TEST("MIR validator rejects duplicate declaration header names");
    {
        const char *src =
            "class A { let value: Int; }\n"
            "class B { let value: Int; }\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        char *mir_error = NULL;
        size_t mutated = 0;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL && strcmp(header->name, "B") == 0) {
                    header->name = "A";
                    mutated++;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated == 1
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "duplicates declaration header") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
