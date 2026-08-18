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

    TEST("MIR DCE preserves branch-merged inout copy-out phi");
    {
        const char *src =
            "struct Snapshot { count: Int; }\n"
            "func SelectSnapshot(inout snapshot: Snapshot, take_new: Bool) -> Void {\n"
            "    let dead: Int = 0;\n"
            "    if take_new {\n"
            "        snapshot = Snapshot(7);\n"
            "        dead = 1;\n"
            "    } else {\n"
            "        snapshot = Snapshot(9);\n"
            "        dead = 2;\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        bool has_copyout_phi = false;
        bool has_dead_phi = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "SelectSnapshot", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count; bi++) {
                if (block_has_phi_result_prefix(
                        &routine->blocks[bi], "snapshot."))
                    has_copyout_phi = true;
                if (block_has_phi_result_prefix(
                        &routine->blocks[bi], "dead."))
                    has_dead_phi = true;
            }
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && routine->has_dce
               && mir_routine_param_carriage(routine, 0)
                    == MIR_PARAM_CARRIAGE_VALUE_RESULT
               && has_copyout_phi
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
                block->instructions[block->instruction_count].has_source_location = true;
                block->instructions[block->instruction_count].source_node_type = stmt->type;
                block->instructions[block->instruction_count].has_surface_usage_facts = true;
                block->instructions[block->instruction_count].has_source_statement_index = true;
                block->instructions[block->instruction_count].source_statement_index = 0;
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
                        && inst->source_node_type == AST_DEFER_STMT) {
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

    TEST("MIR captures and validates TextBuilder runtime-call ABI facts");
    {
        const char *src =
            "func TextBuilderFacts() -> Void {\n"
            "    let result: Allocator = AllocatorResult();\n"
            "    let builder: TextBuilder = TextBuilderNew(2);\n"
            "    TextBuilderAppend(builder, \"x\");\n"
            "    let text: String = TextBuilderFinish(builder, result);\n"
            "    Print(text);\n"
            "    AllocatorDestroy(result);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *append_inst = NULL;
        bool saw_new = false;
        bool saw_finish = false;
        bool saw_allocator_result = false;
        bool saw_allocator_destroy = false;
        bool rejected_missing_row = false;
        char *mir_error = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(
                mir, "TextBuilderFacts", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    const MIRTextBuilderRuntimeRow *row =
                        inst->text_builder_runtime_row;
                    if (row == NULL || row->operation == NULL)
                        continue;
                    if (strcmp(row->operation, "New") == 0)
                        saw_new = true;
                    else if (strcmp(row->source_name, "AllocatorResult") == 0)
                        saw_allocator_result = true;
                    else if (strcmp(row->source_name, "AllocatorDestroy") == 0)
                        saw_allocator_destroy = true;
                    else if (strcmp(row->operation, "Append") == 0)
                        append_inst = inst;
                    else if (strcmp(row->operation, "Finish") == 0)
                        saw_finish = true;
                }
            }
        }
        if (append_inst != NULL) {
            const MIRTextBuilderRuntimeRow *saved =
                append_inst->text_builder_runtime_row;
            append_inst->text_builder_runtime_row = NULL;
            rejected_missing_row = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "runtime-value runtime-call ABI fact")
                    != NULL;
            append_inst->text_builder_runtime_row = saved;
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && saw_new
               && saw_allocator_result
               && saw_allocator_destroy
               && append_inst != NULL
               && saw_finish
               && rejected_missing_row);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR defer carries and validates its nested runtime-call ABI fact");
    {
        const char *src =
            "func DeferredAllocatorDestroy() -> Void {\n"
            "    let scratch: Allocator = AllocatorScratch();\n"
            "    defer { AllocatorDestroy(scratch); }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *defer_inst = NULL;
        bool rejected_missing_row = false;
        char *mir_error = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(
                mir, "DeferredAllocatorDestroy", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    const MIRTextBuilderRuntimeRow *row =
                        inst->text_builder_runtime_row;
                    if (row != NULL && row->source_name != NULL
                        && strcmp(row->source_name, "AllocatorDestroy") == 0) {
                        defer_inst = inst;
                    }
                }
            }
        }
        if (defer_inst != NULL) {
            const MIRTextBuilderRuntimeRow *saved =
                defer_inst->text_builder_runtime_row;
            defer_inst->text_builder_runtime_row = NULL;
            rejected_missing_row = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "runtime-value runtime-call ABI fact")
                    != NULL;
            defer_inst->text_builder_runtime_row = saved;
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && defer_inst != NULL
               && rejected_missing_row);
        free(mir_error);
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

}
