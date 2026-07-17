static void
test_hir_lowering_part_b(void)
{
    TEST("HIR validator rejects out-of-range CFG successor");
    {
        const char *src =
            "func BranchFlow(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        HIRRoutine *routine = NULL;
        char *error = NULL;
        bool corrupted = false;
        bool rejected = false;
        if (hir != NULL) {
            for (size_t i = 0; i < hir->routine_count; i++) {
                if (hir->routines[i].name != NULL
                    && strcmp(hir->routines[i].name, "BranchFlow") == 0) {
                    routine = &hir->routines[i];
                    break;
                }
            }
        }
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (!block->has_succ_true)
                    continue;
                block->succ_true = routine->cfg.block_count + 1;
                corrupted = true;
                break;
            }
        }
        rejected = hir != NULL
                   && corrupted
                   && !hir_validate(hir, &error)
                   && error != NULL
                   && strstr(error, "out-of-range successor") != NULL;
        EXPECT(rejected);
        free(error);
        hir_destroy(hir);
    }

    TEST("HIR validator rejects successor without reciprocal predecessor");
    {
        const char *src =
            "func BranchFlow(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        HIRRoutine *routine = NULL;
        char *error = NULL;
        bool corrupted = false;
        bool rejected = false;
        if (hir != NULL) {
            for (size_t i = 0; i < hir->routine_count; i++) {
                if (hir->routines[i].name != NULL
                    && strcmp(hir->routines[i].name, "BranchFlow") == 0) {
                    routine = &hir->routines[i];
                    break;
                }
            }
        }
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                HIRBasicBlock *block = &routine->cfg.blocks[i];
                HIRBasicBlock *target = NULL;
                if (!block->has_succ_true)
                    continue;
                target = &routine->cfg.blocks[block->succ_true];
                target->predecessor_count = 0;
                corrupted = true;
                break;
            }
        }
        rejected = hir != NULL
                   && corrupted
                   && !hir_validate(hir, &error)
                   && error != NULL
                   && strstr(error, "reciprocal predecessor") != NULL;
        EXPECT(rejected);
        free(error);
        hir_destroy(hir);
    }

    TEST("HIR validator rejects out-of-range CFG predecessor");
    {
        const char *src =
            "func BranchFlow(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        HIRRoutine *routine = NULL;
        char *error = NULL;
        bool corrupted = false;
        bool rejected = false;
        if (hir != NULL) {
            for (size_t i = 0; i < hir->routine_count; i++) {
                if (hir->routines[i].name != NULL
                    && strcmp(hir->routines[i].name, "BranchFlow") == 0) {
                    routine = &hir->routines[i];
                    break;
                }
            }
        }
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->predecessor_count == 0 || block->predecessors == NULL)
                    continue;
                block->predecessors[0] = routine->cfg.block_count + 1;
                corrupted = true;
                break;
            }
        }
        rejected = hir != NULL
                   && corrupted
                   && !hir_validate(hir, &error)
                   && error != NULL
                   && strstr(error, "out-of-range predecessor") != NULL;
        EXPECT(rejected);
        free(error);
        hir_destroy(hir);
    }

    TEST("HIR validator rejects predecessor count above capacity");
    {
        const char *src =
            "func BranchFlow(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        HIRRoutine *routine = NULL;
        char *error = NULL;
        bool corrupted = false;
        bool rejected = false;
        if (hir != NULL) {
            for (size_t i = 0; i < hir->routine_count; i++) {
                if (hir->routines[i].name != NULL
                    && strcmp(hir->routines[i].name, "BranchFlow") == 0) {
                    routine = &hir->routines[i];
                    break;
                }
            }
        }
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->predecessor_count == 0 || block->predecessors == NULL)
                    continue;
                block->predecessor_capacity = block->predecessor_count - 1;
                corrupted = true;
                break;
            }
        }
        rejected = hir != NULL
                   && corrupted
                   && !hir_validate(hir, &error)
                   && error != NULL
                   && strstr(error, "predecessor count above predecessor capacity") != NULL;
        EXPECT(rejected);
        free(error);
        hir_destroy(hir);
    }

    TEST("HIR CFG lowers loop break and continue edges explicitly");
    {
        const char *src =
            "func LoopEdges(flag: Bool) -> Int {\n"
            "    let i = 0;\n"
            "    while flag {\n"
            "        if flag {\n"
            "            continue;\n"
            "        }\n"
            "        break;\n"
            "    }\n"
            "    return i;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "LoopEdges", HIR_TOPLEVEL_FUNCTION);
        bool found_loop_header = false;
        bool found_break_edge = false;
        bool found_continue_edge = false;
        size_t loop_header = 0;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->is_loop_header) {
                    found_loop_header = true;
                    loop_header = i;
                    break;
                }
            }
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->terminator_kind != HIR_BLOCK_GOTO || !block->has_succ_true)
                    continue;
                for (size_t j = 0; j < block->statement_count; j++) {
                    ASTNode *stmt = block->statements[j];
                    if (stmt != NULL && stmt->type == AST_CONTINUE
                        && found_loop_header && block->succ_true == loop_header) {
                        found_continue_edge = true;
                    }
                    if (stmt != NULL && stmt->type == AST_BREAK
                        && found_loop_header && block->succ_true != loop_header) {
                        found_break_edge = true;
                    }
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && routine->return_block_count == 1
               && routine->normal_exit_block_count == 0
               && found_loop_header
               && found_break_edge
               && found_continue_edge);
        hir_destroy(hir);
    }

    TEST("HIR CFG lowers match cases and default as explicit edges");
    {
        const char *src =
            "func MatchEdges(value: Int) -> Int {\n"
            "    match value {\n"
            "        case 0:\n"
            "            return 1;\n"
            "        case 1:\n"
            "            value = value + 1;\n"
            "        default:\n"
            "            value = value + 2;\n"
            "    }\n"
            "    return value;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "MatchEdges", HIR_TOPLEVEL_FUNCTION);
        bool found_match_dispatch = false;
        bool found_match_subject_fact = false;
        bool found_case_return = false;
        bool found_case_join = false;
        bool found_default_join = false;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->terminator_kind == HIR_BLOCK_BRANCH
                    && block->terminator_condition != NULL
                    && block->terminator_condition->type == AST_MATCH_CASE) {
                    found_match_dispatch = true;
                    if (block->terminator_value != NULL
                        && block->terminator_value->type == AST_IDENTIFIER
                        && strcmp(block->terminator_value->data.identifier.name,
                                  "value") == 0) {
                        found_match_subject_fact = true;
                    }
                }
                if (block->terminator_kind == HIR_BLOCK_RETURN) {
                    for (size_t j = 0; j < block->statement_count; j++) {
                        ASTNode *stmt = block->statements[j];
                        if (stmt != NULL && stmt->type == AST_RETURN
                            && stmt->data.return_stmt.value != NULL
                            && stmt->data.return_stmt.value->type == AST_NUMBER
                            && stmt->data.return_stmt.value->data.number.value == 1) {
                            found_case_return = true;
                        }
                    }
                }
                if (block->terminator_kind == HIR_BLOCK_GOTO && block->has_succ_true) {
                    for (size_t j = 0; j < block->statement_count; j++) {
                        ASTNode *stmt = block->statements[j];
                        if (stmt != NULL && stmt->type == AST_ASSIGNMENT) {
                            if (stmt->data.assignment.value != NULL
                                && stmt->data.assignment.value->type == AST_BINARY
                                && stmt->data.assignment.value->data.binary.right != NULL
                                && stmt->data.assignment.value->data.binary.right->type == AST_NUMBER
                                && stmt->data.assignment.value->data.binary.right->data.number.value == 1) {
                                found_case_join = true;
                            }
                            if (stmt->data.assignment.value != NULL
                                && stmt->data.assignment.value->type == AST_BINARY
                                && stmt->data.assignment.value->data.binary.right != NULL
                                && stmt->data.assignment.value->data.binary.right->type == AST_NUMBER
                                && stmt->data.assignment.value->data.binary.right->data.number.value == 2) {
                                found_default_join = true;
                            }
                        }
                    }
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && found_match_dispatch
               && found_match_subject_fact
               && found_case_return
               && found_case_join
               && found_default_join);
        hir_destroy(hir);
    }

    TEST("HIR CFG lowers select cases and default as explicit edges");
    {
        const char *src =
            "func SelectEdges() -> Int {\n"
            "    let ch: Channel<Int> = Channel(1);\n"
            "    select {\n"
            "        case v = <-ch:\n"
            "            return v;\n"
            "        default:\n"
            "            Log(0);\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "SelectEdges", HIR_TOPLEVEL_FUNCTION);
        bool found_select_dispatch = false;
        bool found_case_receive = false;
        bool found_case_return = false;
        bool found_default_join = false;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->terminator_kind == HIR_BLOCK_BRANCH
                    && block->terminator_condition != NULL
                    && block->terminator_condition->type == AST_BLOCK) {
                    ASTNode *case_block = block->terminator_condition;
                    if (case_block->data.block.count > 0
                        && case_block->data.block.statements[0] != NULL
                        && case_block->data.block.statements[0]->type == AST_ASSIGNMENT) {
                        found_select_dispatch = true;
                    }
                }
                for (size_t j = 0; j < block->statement_count; j++) {
                    ASTNode *stmt = block->statements[j];
                    if (stmt != NULL && stmt->type == AST_ASSIGNMENT
                        && stmt->data.assignment.value != NULL
                        && stmt->data.assignment.value->type == AST_CHANNEL_RECV) {
                        found_case_receive = true;
                    }
                    if (stmt != NULL && stmt->type == AST_CALL
                        && block->terminator_kind == HIR_BLOCK_GOTO
                        && block->has_succ_true) {
                        found_default_join = true;
                    }
                }
                if (block->terminator_kind == HIR_BLOCK_RETURN
                    && block->terminator_value != NULL
                    && block->terminator_value->type == AST_IDENTIFIER
                    && strcmp(block->terminator_value->data.identifier.name, "v") == 0) {
                    found_case_return = true;
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && found_select_dispatch
               && found_case_receive
               && found_case_return
               && found_default_join);
        hir_destroy(hir);
    }

    TEST("HIR CFG lowers unsafe block body control flow");
    {
        const char *src =
            "func UnsafeReturn(flag: Bool) -> Int {\n"
            "    unsafe {\n"
            "        if flag {\n"
            "            return 1;\n"
            "        }\n"
            "    }\n"
            "    return 2;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "UnsafeReturn", HIR_TOPLEVEL_FUNCTION);
        bool found_unsafe_payload = false;
        bool found_nested_return = false;
        bool found_after_return = false;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                for (size_t j = 0; j < block->statement_count; j++) {
                    ASTNode *stmt = block->statements[j];
                    if (stmt != NULL && stmt->type == AST_UNSAFE_BLOCK)
                        found_unsafe_payload = true;
                }
                if (block->terminator_kind == HIR_BLOCK_RETURN
                    && block->terminator_value != NULL
                    && block->terminator_value->type == AST_NUMBER) {
                    if (block->terminator_value->data.number.value == 1)
                        found_nested_return = true;
                    if (block->terminator_value->data.number.value == 2)
                        found_after_return = true;
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && found_unsafe_payload
               && found_nested_return
               && found_after_return);
        hir_destroy(hir);
    }

    TEST("HIR CFG resolves labeled loop control to the named loop");
    {
        const char *src =
            "func LabeledLoopEdges(flag: Bool) -> Int {\n"
            "    let i = 0;\n"
            "    outer: while flag {\n"
            "        while flag {\n"
            "            if flag {\n"
            "                continue outer;\n"
            "            }\n"
            "            break outer;\n"
            "        }\n"
            "        i = i + 1;\n"
            "    }\n"
            "    return i;\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        const HIRRoutine *routine = hir_find_routine(hir, "LabeledLoopEdges", HIR_TOPLEVEL_FUNCTION);
        bool found_outer_header = false;
        bool found_continue_outer = false;
        bool found_break_outer_exit = false;
        size_t outer_header = 0;
        if (routine != NULL && routine->has_cfg) {
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->is_loop_header) {
                    outer_header = i;
                    found_outer_header = true;
                    break;
                }
            }
            for (size_t i = 0; i < routine->cfg.block_count; i++) {
                const HIRBasicBlock *block = &routine->cfg.blocks[i];
                if (block->terminator_kind != HIR_BLOCK_GOTO || !block->has_succ_true)
                    continue;
                for (size_t j = 0; j < block->statement_count; j++) {
                    ASTNode *stmt = block->statements[j];
                    if (stmt != NULL && stmt->type == AST_CONTINUE
                        && stmt->data.continue_stmt.label != NULL
                        && strcmp(stmt->data.continue_stmt.label, "outer") == 0
                        && found_outer_header
                        && block->succ_true == outer_header) {
                        found_continue_outer = true;
                    }
                    if (stmt != NULL && stmt->type == AST_BREAK
                        && stmt->data.break_stmt.label != NULL
                        && strcmp(stmt->data.break_stmt.label, "outer") == 0
                        && block->succ_true < routine->cfg.block_count) {
                        const HIRBasicBlock *target = &routine->cfg.blocks[block->succ_true];
                        if (target->terminator_kind == HIR_BLOCK_RETURN)
                            found_break_outer_exit = true;
                    }
                }
            }
        }
        EXPECT(hir != NULL
               && routine != NULL
               && routine->has_cfg
               && found_outer_header
               && found_continue_outer
               && found_break_outer_exit);
        hir_destroy(hir);
    }

    TEST("HIR block pass can walk SSA-prep reachable blocks");
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
        HIRBlockCounter counter = {0};
        HIRBlockPass pass;
        memset(&pass, 0, sizeof(pass));
        pass.name = "count-ssa-blocks";
        pass.filter.include_functions = true;
        pass.filter.require_cfg = true;
        pass.filter.include_reachable_blocks = true;
        pass.run = count_blocks_pass;
        pass.userdata = &counter;
        EXPECT(hir != NULL
               && hir_run_block_pass(hir, &pass, NULL)
               && pass.routines_visited >= 1
               && pass.blocks_visited >= pass.blocks_matched
               && pass.blocks_matched == counter.reachable_blocks
               && counter.blocks_with_phi > 0
               && counter.blocks_with_dom_children > 0);
        hir_destroy(hir);
    }
}
