static void
test_hir_lowering_part_a(void)
{
    printf("\n[hir]\n");

    TEST("parser-normalized top-level program is bucketed correctly");
    {
        const char *src =
            "event OnHit(damage: Int);\n"
            "extern \"C\" { func SDL_Quit(); }\n"
            "func Helper() -> Int { return 0; }\n"
            "let boot = Helper();\n";
        HIRProgram *hir = lower_from_source(src);
        EXPECT(hir != NULL
               && hir->event_count == 1
               && hir->extern_count == 1
               && hir->function_count == 2
               && hir->executable_count == 0
               && hir->synthetic_executable_func == NULL
               && hir_find_routine(hir, "Main", HIR_TOPLEVEL_FUNCTION) != NULL
               && hir->has_main_function);
        hir_destroy(hir);
    }

    TEST("unsupported root is rejected");
    {
        ASTNode *number = ast_create_number("1");
        char *error = NULL;
        HIRProgram *hir = hir_lower(number, &error);
        EXPECT(hir == NULL && error != NULL);
        free(error);
        ast_destroy(number);
    }

    TEST("event lambda subscription lowers through HIR");
    {
        const char *src =
            "event OnHit(damage: Int);\n"
            "func Main() -> Void {\n"
            "    OnHit += (d: Int) => { Log(d); };\n"
            "    OnHit(77);\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        EXPECT(hir != NULL
               && hir->event_count == 1
               && hir->function_count == 1
               && hir->executable_count == 0
               && hir->has_main_function);
        hir_destroy(hir);
    }

    TEST("HIR exposes declaration index and routine summaries");
    {
        const char *src =
            "subject Member { let hp: Int; }\n"
            "zone PaymentZone { subject slot buyer: Member }\n"
            "func Helper() -> Int { return 1; }\n"
            "intent Purchase(payment: PaymentZone, buyer: Member) {\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        on: Helper();\n"
            "    }\n"
            "}\n"
            "func Main() -> Int {\n"
            "    return Helper();\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRDecl *intent_decl = hir_find_decl(hir, "Purchase", HIR_TOPLEVEL_INTENT);
        const HIRRoutine *main_routine = hir_find_routine(hir, "Main", HIR_TOPLEVEL_FUNCTION);
        const HIRRoutine *helper_routine = hir_find_routine(hir, "Helper", HIR_TOPLEVEL_FUNCTION);
        const HIRRoutine *intent_routine = hir_find_routine(hir, "Purchase", HIR_TOPLEVEL_INTENT);
        EXPECT(hir != NULL
               && hir->decl_count >= 3
               && hir->routine_count >= 2
               && intent_decl != NULL
               && intent_decl->phase == HIR_PHASE_ROUTINE
               && main_routine != NULL
               && main_routine->is_entry_reachable
               && main_routine->signature_type_ref_count >= 1
               && strcmp(main_routine->signature_type_refs[0], "Int") == 0
               && main_routine->direct_call_count == 1
               && main_routine->callee_routine_count == 1
               && helper_routine != NULL
               && main_routine->callee_routine_ids[0]
                    == helper_routine->routine_id
               && hir_find_routine_by_id(hir, helper_routine->routine_id)
                    == helper_routine
               && strcmp(main_routine->direct_calls[0], "Helper") == 0
               && intent_routine != NULL
               && intent_routine->is_entry_reachable
               && intent_routine->has_cfg
               && intent_routine->cfg.block_count >= 1
               && intent_routine->cfg.blocks[0].statement_count >= 2
               && intent_routine->signature_type_ref_count >= 2
               && intent_routine->callee_routine_count == 1
               && intent_routine->direct_call_count == 1
               && strcmp(intent_routine->direct_calls[0], "Helper") == 0);
        hir_destroy(hir);
    }

    TEST("HIR callgraph joins same-name routines by stable identity");
    {
        const char *src =
            "class Holder {\n"
            "    func Helper() -> Int { return 9; }\n"
            "}\n"
            "func Helper() -> Int { return 1; }\n"
            "func Main() -> Int { return Helper(); }\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *hosted = NULL;
        const HIRRoutine *top_level = NULL;
        const HIRRoutine *main_routine = hir_find_routine(
            hir, "Main", HIR_TOPLEVEL_FUNCTION);
        char *validation_error = NULL;

        if (hir != NULL) {
            for (size_t i = 0; i < hir->routine_count; i++) {
                const HIRRoutine *candidate = &hir->routines[i];
                if (candidate->name == NULL
                    || strcmp(candidate->name, "Helper") != 0) {
                    continue;
                }
                if (candidate->is_hosted)
                    hosted = candidate;
                else
                    top_level = candidate;
            }
        }
        EXPECT(hir != NULL
               && hir_validate(hir, &validation_error)
               && validation_error == NULL
               && hosted != NULL
               && top_level != NULL
               && main_routine != NULL
               && main_routine->callee_routine_count == 1
               && main_routine->callee_routine_ids[0]
                    == top_level->routine_id
               && top_level->is_entry_reachable
               && !hosted->is_entry_reachable
               && hir_find_routine(hir, "Helper", HIR_TOPLEVEL_FUNCTION)
                    == NULL);
        free(validation_error);
        hir_destroy(hir);
    }

    TEST("HIR routine pass can filter control-flow routines");
    {
        const char *src =
            "subject Member { let hp: Int; }\n"
            "zone PaymentZone { subject slot buyer: Member }\n"
            "func Helper() -> Int { return 1; }\n"
            "func Looping(flag: Bool) -> Int {\n"
            "    while flag {\n"
            "        return Helper();\n"
            "    }\n"
            "    return 0;\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Member) {\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        on: Helper();\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        HIRPassCounter counter = {0};
        HIRRoutinePass pass;
        memset(&pass, 0, sizeof(pass));
        pass.name = "count-control-flow";
        pass.filter.include_functions = true;
        pass.filter.include_intents = true;
        pass.filter.require_control_flow = true;
        pass.run = count_routines_pass;
        pass.userdata = &counter;
        EXPECT(hir != NULL
               && hir_run_routine_pass(hir, &pass, NULL)
               && pass.routines_visited == 3
               && pass.routines_matched == 2
               && counter.seen == 2
               && counter.with_calls == 2);
        hir_destroy(hir);
    }

    TEST("HIR function CFG lowers basic blocks for if and while");
    {
        const char *src =
            "func Flow(flag: Bool) -> Int {\n"
            "    let base = 1;\n"
            "    if flag {\n"
            "        while flag {\n"
            "            base = base + 1;\n"
            "        }\n"
            "    }\n"
            "    return base;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *flow = hir_find_routine(hir, "Flow", HIR_TOPLEVEL_FUNCTION);
        bool found_loop_header = false;
        bool found_branch = false;
        bool loop_header_has_pred = false;
        bool loop_header_has_idom = false;
        bool found_frontier = false;
        bool found_loop_depth = false;
        bool entry_has_self_idom = false;
        if (flow != NULL && flow->has_cfg) {
            for (size_t i = 0; i < flow->cfg.block_count; i++) {
                const HIRBasicBlock *block = &flow->cfg.blocks[i];
                if (i == flow->cfg.entry_block
                    && block->is_reachable
                    && block->has_immediate_dominator
                    && block->immediate_dominator == i) {
                    entry_has_self_idom = true;
                }
                if (block->is_loop_header) {
                    found_loop_header = true;
                    if (block->predecessor_count > 0)
                        loop_header_has_pred = true;
                    if (block->has_immediate_dominator)
                        loop_header_has_idom = true;
                }
                if (block->dominance_frontier_count > 0)
                    found_frontier = true;
                if (block->loop_depth > 0)
                    found_loop_depth = true;
                if (block->terminator_kind == HIR_BLOCK_BRANCH)
                    found_branch = true;
            }
        }
        EXPECT(hir != NULL
               && flow != NULL
               && flow->has_cfg
               && flow->cfg.block_count >= 6
               && flow->return_block_count == 1
               && flow->normal_exit_block_count == 0
               && flow->cfg.entry_block == 0
               && found_loop_header
               && loop_header_has_pred
               && loop_header_has_idom
               && found_frontier
               && found_loop_depth
               && entry_has_self_idom
               && found_branch);
        hir_destroy(hir);
    }

    TEST("HIR computes phi candidates for joined local defs");
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
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *merge = hir_find_routine(hir, "Merge", HIR_TOPLEVEL_FUNCTION);
        bool found_score_phi = false;
        bool found_defs = false;
        if (merge != NULL && merge->has_cfg) {
            for (size_t i = 0; i < merge->cfg.block_count; i++) {
                const HIRBasicBlock *block = &merge->cfg.blocks[i];
                if (block->local_def_count > 0)
                    found_defs = true;
                for (size_t j = 0; j < block->phi_candidate_count; j++) {
                    if (strcmp(block->phi_candidates[j], "score") == 0)
                        found_score_phi = true;
                }
            }
        }
        EXPECT(hir != NULL
               && merge != NULL
               && merge->has_cfg
               && found_defs
               && merge->return_block_count == 1
               && merge->normal_exit_block_count == 0
               && merge->phi_candidate_count > 0
               && merge->phi_candidate_block_count > 0
               && found_score_phi);
        hir_destroy(hir);
    }

    TEST("HIR records with alias as a CFG local def");
    {
        const char *src =
            "func UseWith() -> Int {\n"
            "    with slot<Int> as s {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *use_with =
            hir_find_routine(hir, "UseWith", HIR_TOPLEVEL_FUNCTION);
        bool found_alias_def = false;
        if (use_with != NULL && use_with->has_cfg) {
            for (size_t i = 0; i < use_with->cfg.block_count; i++) {
                const HIRBasicBlock *block = &use_with->cfg.blocks[i];
                for (size_t j = 0; j < block->local_def_count; j++) {
                    if (strcmp(block->local_defs[j], "s") == 0)
                        found_alias_def = true;
                }
            }
        }
        EXPECT(hir != NULL
               && use_with != NULL
               && use_with->has_cfg
               && found_alias_def);
        hir_destroy(hir);
    }

    TEST("HIR preserves pin block region metadata across CFG blocks");
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
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "PinFlow", HIR_TOPLEVEL_FUNCTION);
        bool found_pin = false;
        bool found_after_pin = false;
        bool found_write_mode = false;
        bool found_source = false;
        bool found_view = false;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->is_pin_region) {
                    found_pin = true;
                    if (block->pin_view_is_write)
                        found_write_mode = true;
                    if (block->pin_source_name != NULL
                        && strcmp(block->pin_source_name, "scores") == 0)
                        found_source = true;
                    if (block->pin_view_name != NULL
                        && strcmp(block->pin_view_name, "view") == 0)
                        found_view = true;
                } else {
                    for (size_t s = 0; s < block->statement_count; s++) {
                        ASTNode *stmt = block->statements[s];
                        if (stmt != NULL && stmt->type == AST_CALL
                            && stmt->data.call.callee != NULL
                            && stmt->data.call.callee->type == AST_IDENTIFIER
                            && strcmp(stmt->data.call.callee->data.identifier.name, "Release") == 0) {
                            found_after_pin = true;
                        }
                    }
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && routine->return_block_count == 0
               && routine->normal_exit_block_count == 1
               && found_pin
               && found_after_pin
               && found_write_mode
               && found_source
               && found_view);
        hir_destroy(hir);
    }

    TEST("HIR validator rejects pin-region without source anchor");
    {
        const char *src =
            "func PinFlow(flag: Bool) -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    pin scores as view: WriteView<Int> {\n"
            "        Write(view, 1);\n"
            "    }\n"
            "    Release(scores);\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        HIRRoutine *routine = NULL;
        char *error = NULL;
        bool corrupted = false;
        bool rejected = false;
        if (hir != NULL) {
            for (size_t i = 0; i < hir->routine_count; i++) {
                if (hir->routines[i].name != NULL
                    && strcmp(hir->routines[i].name, "PinFlow") == 0) {
                    routine = &hir->routines[i];
                    break;
                }
            }
        }
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (!block->is_pin_region)
                    continue;
                block->pin_source_name = NULL;
                corrupted = true;
                break;
            }
        }
        rejected = hir != NULL
                   && corrupted
                   && !hir_validate(hir, &error)
                   && error != NULL
                   && strstr(error, "pin-region") != NULL
                   && strstr(error, "pin source name") != NULL;
        EXPECT(rejected);
        free(error);
        hir_destroy(hir);
    }

    TEST("HIR validator rejects pin-region without view name");
    {
        const char *src =
            "func PinFlow(flag: Bool) -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    pin scores as view: WriteView<Int> {\n"
            "        Write(view, 1);\n"
            "    }\n"
            "    Release(scores);\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        HIRRoutine *routine = NULL;
        char *error = NULL;
        bool corrupted = false;
        bool rejected = false;
        if (hir != NULL) {
            for (size_t i = 0; i < hir->routine_count; i++) {
                if (hir->routines[i].name != NULL
                    && strcmp(hir->routines[i].name, "PinFlow") == 0) {
                    routine = &hir->routines[i];
                    break;
                }
            }
        }
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (!block->is_pin_region)
                    continue;
                block->pin_view_name = NULL;
                corrupted = true;
                break;
            }
        }
        rejected = hir != NULL
                   && corrupted
                   && !hir_validate(hir, &error)
                   && error != NULL
                   && strstr(error, "pin-region") != NULL
                   && strstr(error, "pin view name") != NULL;
        EXPECT(rejected);
        free(error);
        hir_destroy(hir);
    }

}
