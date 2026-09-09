static void
test_mir_lowering_part_c_2(void)
{
    TEST("MIR residual STMT policy rejects local dataflow statements");
    {
        MIRInstruction inst = {0};
        bool rejects_let;
        bool rejects_destructure;
        bool rejects_assignment;
        bool keeps_fail;
        bool rejects_with;
        bool rejects_pure_call;
        bool keeps_effect_call;
        bool keeps_defer;
        bool keeps_intent_step;
        bool redundant_unordered;
        bool redundant_return;
        bool redundant_destructure;
        bool effect_call_emit_allowed;
        bool non_call_emit_rejected;
        ASTNode call_expr = {0};

        inst.kind = MIR_INST_STMT;
        inst.name = "stmt";
        inst.has_source_location = true;
        inst.has_source_statement_index = true;
        inst.source_statement_index = 0;

        inst.source_node_type = AST_LET_DECL;
        rejects_let =
            !mir_instruction_source_stmt_residual_emit_is_allowed(&inst);
        inst.source_node_type = AST_LET_DESTRUCTURE;
        rejects_destructure =
            !mir_instruction_source_stmt_residual_emit_is_allowed(&inst);
        inst.source_node_type = AST_ASSIGNMENT;
        rejects_assignment =
            !mir_instruction_source_stmt_residual_emit_is_allowed(&inst);
        inst.source_node_type = AST_FAIL_STMT;
        keeps_fail =
            mir_instruction_source_stmt_residual_emit_is_allowed(&inst);
        inst.source_node_type = AST_WITH_STMT;
        rejects_with =
            !mir_instruction_source_stmt_residual_emit_is_allowed(&inst);
        inst.source_node_type = AST_CALL;
        inst.arg0 = "HasZone";
        rejects_pure_call =
            !mir_instruction_source_stmt_residual_emit_is_allowed(&inst);
        inst.source_node_type = AST_CALL;
        inst.arg0 = "Log";
        keeps_effect_call =
            mir_instruction_source_stmt_residual_emit_is_allowed(&inst);
        inst.source_node_type = AST_DEFER_STMT;
        inst.arg0 = NULL;
        keeps_defer =
            mir_instruction_source_stmt_residual_emit_is_allowed(&inst);
        inst.source_node_type = AST_INTENT_STEP;
        keeps_intent_step =
            mir_instruction_source_stmt_residual_emit_is_allowed(&inst);
        inst.source_node_type = AST_BLOCK;
        inst.has_source_statement_index = false;
        redundant_unordered =
            mir_instruction_source_stmt_reemit_is_redundant(&inst);
        inst.has_source_statement_index = true;
        inst.source_node_type = AST_RETURN;
        redundant_return =
            mir_instruction_source_stmt_reemit_is_redundant(&inst);
        inst.source_node_type = AST_LET_DESTRUCTURE;
        redundant_destructure =
            mir_instruction_source_stmt_reemit_is_redundant(&inst);
        inst.source_node_type = AST_CALL;
        inst.expr0 = &call_expr;
        effect_call_emit_allowed =
            mir_instruction_source_stmt_call_emit_is_allowed(&inst);
        inst.source_node_type = AST_FAIL_STMT;
        non_call_emit_rejected =
            !mir_instruction_source_stmt_call_emit_is_allowed(&inst);

        EXPECT(rejects_let
               && rejects_destructure
               && rejects_assignment
               && keeps_fail
               && rejects_with
               && rejects_pure_call
               && keeps_effect_call
               && keeps_defer
               && keeps_intent_step
               && redundant_unordered
               && redundant_return
               && redundant_destructure
               && effect_call_emit_allowed
               && non_call_emit_rejected);
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
            for (size_t bi = 0; bi < routine->block_count
                    && stmt_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_STMT
                        && inst->source_node_type == AST_DEFER_STMT) {
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
                && strstr(mir_error,
                          "residual STMT emit is missing source statement inventory fact") != NULL;
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
}

static void
test_mir_lowering_part_c_3(void)
{
    TEST("MIR captures array literal source-local types");
    {
        const char *src =
            "func ArrayLiteralLocalFacts() -> Int {\n"
            "    let values = [1, 2, 3];\n"
            "    return ArrayLength(values);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *values_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "ArrayLiteralLocalFacts",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            values_type = mir_routine_source_local_type_name(routine,
                "values");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && values_type != NULL
               && strcmp(values_type, "Array<Int>") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures zone constructor source-local types");
    {
        const char *src =
            "subject Driver {\n"
            "    let hp: Int;\n"
            "}\n"
            "object DriverView {\n"
            "    let hp: Int;\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
              "    object slot dashboard: DriverView\n"
            "}\n"
            "func ZoneConstructorLocalFacts() -> Int {\n"
              "    let cockpit = CockpitZone(Driver(5));\n"
            "    return cockpit.driver.hp;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *cockpit_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "ZoneConstructorLocalFacts",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            cockpit_type = mir_routine_source_local_type_name(routine,
                "cockpit");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && cockpit_type != NULL
               && strcmp(cockpit_type, "CockpitZone") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures intent call source-local types");
    {
        const char *src =
            "subject Driver {\n"
            "    let hp: Int;\n"
            "}\n"
            "object DriverView {\n"
            "    let hp: Int;\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
              "    object slot dashboard: DriverView\n"
            "}\n"
            "intent SyncDrive(cockpit: CockpitZone, driver: Driver) {\n"
            "    step verify {\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "func IntentCallLocalFacts() -> Int {\n"
              "    let cockpit = CockpitZone(Driver(5));\n"
            "    let ok = SyncDrive(cockpit, cockpit.driver);\n"
            "    if ok {\n"
            "        return 1;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *ok_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "IntentCallLocalFacts",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            ok_type = mir_routine_source_local_type_name(routine, "ok");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && ok_type != NULL
               && strcmp(ok_type, "Bool") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures generic spawn and await source-local types");
    {
        const char *src =
            "func Identity<T>(x: T) -> T {\n"
            "    return x;\n"
            "}\n"
            "async func Main() -> Void {\n"
            "    let task = spawn Identity(42);\n"
            "    let value = await task;\n"
            "    Log(value);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *task_type = NULL;
        const char *value_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "Main", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            task_type = mir_routine_source_local_type_name(routine, "task");
            value_type = mir_routine_source_local_type_name(routine, "value");
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && task_type != NULL && strcmp(task_type, "Future<Int>") == 0
               && value_type != NULL && strcmp(value_type, "Int") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures extern call source-local types");
    {
        const char *src =
            "extern \"c\" {\n"
            "    func pgy_now_ms() -> Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let ignored = pgy_now_ms();\n"
            "    Log(1);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *ignored_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "Main", MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            ignored_type = mir_routine_source_local_type_name(routine,
                "ignored");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && ignored_type != NULL
               && strcmp(ignored_type, "Int") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
