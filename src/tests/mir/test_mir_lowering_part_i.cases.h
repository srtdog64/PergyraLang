static void
test_mir_lowering_part_i(void)
{
    TEST("MIR match branch requires captured pattern fact");
    {
        const char *src =
            "func MatchPatternFact(value: Int) -> Int {\n"
            "    match value {\n"
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
        ASTNode *saved_pattern = NULL;
        ASTNode **saved_patterns = NULL;
        size_t saved_pattern_count = 0;
        char *mir_error = NULL;
        bool rejected_missing_pattern_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "MatchPatternFact",
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
            saved_pattern = branch_inst->match_case_pattern;
            saved_patterns = branch_inst->match_case_patterns;
            saved_pattern_count = branch_inst->match_case_pattern_count;
            branch_inst->match_case_pattern = NULL;
            branch_inst->match_case_patterns = NULL;
            branch_inst->match_case_pattern_count = 0;
            rejected_missing_pattern_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "source-branch emit fact is invalid") != NULL;
            branch_inst->match_case_pattern = saved_pattern;
            branch_inst->match_case_patterns = saved_patterns;
            branch_inst->match_case_pattern_count = saved_pattern_count;
        }
        EXPECT(ok
               && routine != NULL
               && branch_inst != NULL
               && branch_inst->expr0 != NULL
               && mir_instruction_match_pattern_count(branch_inst) > 0
               && mir_instruction_match_pattern_at(branch_inst, 0) != NULL
               && rejected_missing_pattern_fact
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR destructure instruction carries binding facts without payload");
    {
        const char *src =
            "func Pair() -> (Int, String) {\n"
            "    return (42, \"hello\");\n"
            "}\n"
            "\n"
            "func DestructureBindingFact() -> Void {\n"
            "    let (n, s) = Pair();\n"
            "    Log(ToString(n));\n"
            "    Log(s);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *destructure_inst = NULL;
        ASTNode *saved_ast = NULL;
        size_t n_index = 99;
        size_t s_index = 99;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "DestructureBindingFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count
                 && destructure_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_DESTRUCTURE) {
                        destructure_inst = inst;
                        break;
                    }
                }
            }
        }
        if (destructure_inst != NULL) {
            saved_ast = destructure_inst->ast;
            destructure_inst->ast = NULL;
        }
        EXPECT(ok
               && routine != NULL
               && destructure_inst != NULL
               && saved_ast != NULL
               && mir_instruction_destructure_binding_count(destructure_inst) == 2
               && strcmp(mir_instruction_destructure_binding_name_at(
                             destructure_inst, 0),
                         "n") == 0
               && strcmp(mir_instruction_destructure_binding_name_at(
                             destructure_inst, 1),
                         "s") == 0
               && mir_instruction_destructure_binding_index(
                      destructure_inst, "n", &n_index)
               && n_index == 0
               && mir_instruction_destructure_binding_index(
                      destructure_inst, "s", &s_index)
               && s_index == 1
               && mir_validate(mir, NULL));
        if (destructure_inst != NULL)
            destructure_inst->ast = saved_ast;
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR assignment instruction carries expression facts without payload");
    {
        const char *src =
            "func AssignmentFact() -> Int {\n"
            "    let x: Int = 1;\n"
            "    x = x + 2;\n"
            "    return x;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *assign_inst = NULL;
        ASTNode *saved_ast = NULL;
        ASTNode *saved_target = NULL;
        char *mir_error = NULL;
        bool rejected_missing_expr_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "AssignmentFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count
                 && assign_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_ASSIGN
                        && mir_instruction_source_is_assignment(inst)) {
                        assign_inst = inst;
                        break;
                    }
                }
            }
        }
        if (assign_inst != NULL) {
            saved_ast = assign_inst->ast;
            saved_target = assign_inst->expr0;
            assign_inst->ast = NULL;
            assign_inst->expr0 = NULL;
            rejected_missing_expr_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "ASSIGN is missing MIR assignment expression facts") != NULL;
            assign_inst->expr0 = saved_target;
        }
        EXPECT(ok
               && routine != NULL
               && assign_inst != NULL
               && saved_ast != NULL
               && saved_target != NULL
               && assign_inst->expr0 != NULL
               && assign_inst->expr1 != NULL
               && assign_inst->expr0->type == AST_IDENTIFIER
               && assign_inst->arg0 != NULL
               && strcmp(assign_inst->arg0, "x") == 0
               && rejected_missing_expr_fact
               && mir_validate(mir, NULL));
        if (assign_inst != NULL)
            assign_inst->ast = saved_ast;
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR validator rejects lifecycle guard without receiver fact");
    {
        const char *src =
            "subject Payment {\n"
            "    func Authorize(self) -> Void { }\n"
            "    func Capture(self) -> Void { }\n"
            "}\n"
            "lifecycle Payment {\n"
            "    Authorize: Pending -> Authorized;\n"
            "    Capture: Authorized -> Captured;\n"
            "}\n"
            "func Flag() -> Bool { return false; }\n"
            "func LifecycleFact() -> Void {\n"
            "    let p: Payment = Payment();\n"
            "    if Flag() {\n"
            "        p.Authorize();\n"
            "    }\n"
            "    p.Capture();\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *guard_inst = NULL;
        char *saved_receiver = NULL;
        char *mir_error = NULL;
        bool rejected_missing_receiver_fact = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "LifecycleFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count
                 && guard_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (mir_instruction_has_lifecycle_guard(inst)) {
                        guard_inst = inst;
                        break;
                    }
                }
            }
        }
        if (guard_inst != NULL) {
            saved_receiver = guard_inst->lifecycle_receiver_name;
            guard_inst->lifecycle_receiver_name = NULL;
            rejected_missing_receiver_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "lifecycle guard is missing receiver fact") != NULL;
            guard_inst->lifecycle_receiver_name = saved_receiver;
        }
        EXPECT(ok
               && routine != NULL
               && guard_inst != NULL
               && saved_receiver != NULL
               && strcmp(saved_receiver, "p") == 0
               && rejected_missing_receiver_fact
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR owns and validates parallel snapshot capture facts");
    {
        const char *src =
            "func Main() -> Void {\n"
            "    let ch: Channel<Int> = Channel(1);\n"
            "    let mut x: Int = 1;\n"
            "    let mut seen: Int = -1;\n"
            "    parallel {\n"
            "        { x = x + 41; ch <- 1; }\n"
            "        { let go: Int = <- ch; seen = x + go - go; }\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRParallelCaptureBoundaryFact *boundary = NULL;
        const MIRParallelCaptureDispositionRow *row = NULL;
        char *mir_error = NULL;
        bool rejected_unsealed = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);

        if (ok && mir_parallel_capture_boundary_count(mir) == 1) {
            boundary = &mir->parallel_capture_boundaries[0];
            row = mir_parallel_capture_disposition_find(
                boundary, "x", MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY);
            boundary->sealed = false;
            rejected_unsealed = !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "MIR parallel capture boundary[0] has invalid shape")
                    != NULL;
            boundary->sealed = true;
        }
        EXPECT(ok
               && boundary != NULL
               && boundary->source_stable_id != 0
               && boundary->task_count == 2
               && boundary->row_count == 1
               && row != NULL
               && row->kind == MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY
               && row->writer_task == 0
               && mir_parallel_capture_disposition_find(
                    boundary, "seen", MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY)
                    == NULL
               && rejected_unsealed
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR owns parallel join index and readonly capture facts");
    {
        const char *src =
            "func Main() -> Void {\n"
            "    let src: Array<Int> = [1, 2, 3, 4];\n"
            "    let mut dst: Array<Int> = [0, 0, 0, 0];\n"
            "    parallel (i in 1..4) {\n"
            "        dst[i] = src[i - 1];\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRParallelCaptureBoundaryFact *boundary = NULL;
        MIRParallelCaptureDispositionRow *index_row = NULL;
        const MIRParallelCaptureDispositionRow *readonly_row = NULL;
        char *mir_error = NULL;
        bool rejected_writer_on_join_row = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);

        if (ok && mir_parallel_capture_boundary_count(mir) == 1) {
            boundary = &mir->parallel_capture_boundaries[0];
            for (size_t i = 0; i < boundary->row_count; i++) {
                if (boundary->rows[i].kind
                        == MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT
                    && strcmp(boundary->rows[i].name, "dst") == 0) {
                    index_row = &boundary->rows[i];
                }
            }
            readonly_row = mir_parallel_capture_disposition_find(
                boundary, "src", MIR_PARALLEL_CAPTURE_JOIN_READONLY);
            if (index_row != NULL) {
                index_row->writer_task = 1;
                rejected_writer_on_join_row = !mir_validate(mir, &mir_error)
                    && mir_error != NULL
                    && strstr(mir_error,
                              "MIR parallel capture boundary[0] row")
                        != NULL;
                index_row->writer_task = 0;
            }
        }
        EXPECT(ok
               && boundary != NULL
               && boundary->sealed
               && boundary->task_count == 1
               && boundary->row_count == 2
               && index_row != NULL
               && readonly_row != NULL
               && rejected_writer_on_join_row
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
