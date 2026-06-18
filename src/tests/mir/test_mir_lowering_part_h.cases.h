static void
test_mir_lowering_part_h(void)
{
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
                        && inst->source_node_type == AST_WITH_STMT) {
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
        ASTNode *saved_expr0 = NULL;
        ASTNodeType saved_source_node_type = 0;
        bool saved_has_source_location = false;
        char *mir_error = NULL;
        bool rejected_missing_match_subject_fact = false;
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
            saved_expr0 = branch_inst->expr0;
            saved_source_node_type = branch_inst->source_node_type;
            saved_has_source_location = branch_inst->has_source_location;
            branch_inst->branch_shape = MIR_BRANCH_MATCH_CASE;
            branch_inst->expr0 = NULL;
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
            rejected_missing_payload =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "source-branch emit fact is invalid") != NULL;
            branch_inst->ast = saved_ast;
            branch_inst->expr0 = saved_expr0;
            branch_inst->source_node_type = saved_source_node_type;
            branch_inst->has_source_location = saved_has_source_location;
            branch_inst->branch_shape = MIR_BRANCH_EXPR;
            branch_inst->requires_source_branch_emit = false;
        }
        EXPECT(ok
               && routine != NULL
               && branch_inst != NULL
               && saved_ast != NULL
               && rejected_missing_match_subject_fact
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
        char **saved_param_type_names = NULL;
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
                    saved_param_type_names = mutated_method->param_type_names;
                    mutated_method->param_type_names = NULL;
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "type-name storage") != NULL;
        if (mutated_method != NULL)
            mutated_method->param_type_names = saved_param_type_names;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects zone authority metadata drift");
    {
        const char *src =
            "subject Buyer { }\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerPay for Buyer {\n"
            "    impl ability Payable { func Pay() -> Void { return; } }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "    authority buyer requires Payable\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *zone = NULL;
        size_t saved_authority_metadata_count = 0;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "PaymentZone") == 0) {
                    zone = header;
                    saved_authority_metadata_count =
                        header->zone_authority_metadata_count;
                    header->zone_authority_metadata_count = 0;
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error,
                             "zone authority metadata count") != NULL;
        if (zone != NULL)
            zone->zone_authority_metadata_count =
                saved_authority_metadata_count;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects zone refresh metadata drift");
    {
        const char *src =
            "subject Buyer { let hp: Int; }\n"
            "object BuyerCard { totalHp: Int; }\n"
            "zone Lobby {\n"
            "    subject slot buyer: Buyer\n"
            "    object slot card: BuyerCard\n"
            "    refresh card from buyer map { totalHp <- hp; }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRDeclHeader *zone = NULL;
        size_t saved_refresh_metadata_count = 0;
        char *mir_error = NULL;
        bool mutated = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok && mir != NULL) {
            for (size_t i = 0; i < mir->decl_header_count; i++) {
                MIRDeclHeader *header = &mir->decl_headers[i];
                if (header->name != NULL
                    && strcmp(header->name, "Lobby") == 0) {
                    zone = header;
                    saved_refresh_metadata_count =
                        header->zone_refresh_metadata_count;
                    header->zone_refresh_metadata_count = 0;
                    mutated = true;
                    break;
                }
            }
        }
        rejected = ok
                   && mutated
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error,
                             "zone refresh metadata count") != NULL;
        if (zone != NULL)
            zone->zone_refresh_metadata_count =
                saved_refresh_metadata_count;
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

}
