
    TEST("MIR captures builtin call return types for source locals");
    {
        const char *src =
            "func BuiltinLocalFacts() -> Int {\n"
            "    let low = Min(3, 9);\n"
            "    let high = Max(low, 4);\n"
            "    let limited = Clamp(high, 0, 10);\n"
            "    let copied = Clone(limited);\n"
            "    let ratio = Min(1.5, 2.5);\n"
            "    with slot<Int> as cell {\n"
            "        Write(cell, copied);\n"
            "        let budget = Read(cell);\n"
            "        return budget;\n"
            "    }\n"
            "    return copied;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *low_type = NULL;
        const char *high_type = NULL;
        const char *limited_type = NULL;
        const char *copied_type = NULL;
        const char *ratio_type = NULL;
        const char *cell_type = NULL;
        const char *budget_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "BuiltinLocalFacts",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            low_type = mir_routine_source_local_type_name(routine, "low");
            high_type = mir_routine_source_local_type_name(routine, "high");
            limited_type = mir_routine_source_local_type_name(routine,
                "limited");
            copied_type = mir_routine_source_local_type_name(routine,
                "copied");
            ratio_type = mir_routine_source_local_type_name(routine,
                "ratio");
            cell_type = mir_routine_source_local_type_name(routine, "cell");
            budget_type = mir_routine_source_local_type_name(routine,
                "budget");
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && low_type != NULL && strcmp(low_type, "Int") == 0
               && high_type != NULL && strcmp(high_type, "Int") == 0
               && limited_type != NULL && strcmp(limited_type, "Int") == 0
               && copied_type != NULL && strcmp(copied_type, "Int") == 0
               && ratio_type != NULL && strcmp(ratio_type, "Float") == 0
               && cell_type != NULL && strcmp(cell_type, "Slot<Int>") == 0
               && budget_type != NULL && strcmp(budget_type, "Int") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures for-loop variable source-local types");
    {
        const char *src =
            "func ForLocalFacts(values: List<Int>, names: Array<String>) -> Int {\n"
            "    let total: Int = 0;\n"
            "    for value in values {\n"
            "        total = total + value;\n"
            "    }\n"
            "    for name in names {\n"
            "        let size = StringLength(name);\n"
            "        total = total + size;\n"
            "    }\n"
            "    return total;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *value_type = NULL;
        const char *name_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "ForLocalFacts",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            value_type = mir_routine_source_local_type_name(routine, "value");
            name_type = mir_routine_source_local_type_name(routine, "name");
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && value_type != NULL && strcmp(value_type, "Int") == 0
               && name_type != NULL && strcmp(name_type, "String") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures slice source-local types");
    {
        const char *src =
            "func Words() -> Array<Int> {\n"
            "    return [10, 20, 30];\n"
            "}\n"
            "func SliceLocalFacts() -> Int {\n"
            "    let view = Words().Slice(1, 2);\n"
            "    let owned = SliceCopy(view);\n"
            "    return ArrayLength(owned);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *view_type = NULL;
        const char *owned_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "SliceLocalFacts",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            view_type = mir_routine_source_local_type_name(routine, "view");
            owned_type = mir_routine_source_local_type_name(routine, "owned");
        }
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && view_type != NULL && strcmp(view_type, "Slice<Int>") == 0
               && owned_type != NULL && strcmp(owned_type, "Array<Int>") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures inferred function return type names");
    {
        const char *src =
            "func add(a: Int, b: Int) {\n"
            "    return a + b;\n"
            "}\n"
            "func Main() {\n"
            "    let s = add(2, 3);\n"
            "    Log(s);\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *add = NULL;
        const MIRRoutine *main_routine = NULL;
        const char *add_return = NULL;
        const char *main_return = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok) {
            add = find_mir_routine(mir, "add", MIR_SCOPE_FUNCTION);
            main_routine = find_mir_routine(mir, "Main", MIR_SCOPE_FUNCTION);
        }
        if (add != NULL)
            add_return = mir_routine_return_type_name(add);
        if (main_routine != NULL)
            main_return = mir_routine_return_type_name(main_routine);
        EXPECT(ok
               && mir_validate(mir, NULL)
               && add != NULL
               && main_routine != NULL
               && add_return != NULL && strcmp(add_return, "Int") == 0
               && main_return != NULL && strcmp(main_return, "Void") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures callable return source-local facts");
    {
        const char *src =
            "func AddOne(x: Int) -> Int {\n"
            "    return x + 1;\n"
            "}\n"
            "func Pick() -> func(Int) -> Int {\n"
            "    return AddOne;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let f = Pick();\n"
            "    Log(f(4));\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const MIRSourceLocalType *fact = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "Main", MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            fact = mir_routine_source_local_type_fact(routine, "f");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && fact != NULL
               && fact->is_callable
               && fact->type_name != NULL
               && strcmp(fact->type_name, "func(Int)->Int") == 0
               && fact->callable_return_type_name != NULL
               && strcmp(fact->callable_return_type_name, "Int") == 0
               && fact->callable_param_count == 1
               && fact->callable_param_type_names != NULL
               && fact->callable_param_type_names[0] != NULL
               && strcmp(fact->callable_param_type_names[0], "Int") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures owner method call return types for source locals");
    {
        const char *src =
            "world LocalMethodOwner {\n"
            "    func Pick(self, seed: Int) -> Int { return seed + 1; }\n"
            "    func Run(self) -> Int {\n"
            "        let choice = Pick(4);\n"
            "        return choice;\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *choice_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "Run", MIR_SCOPE_METHOD);
        if (routine != NULL)
            choice_type = mir_routine_source_local_type_name(routine,
                "choice");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && choice_type != NULL
               && strcmp(choice_type, "Int") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures self method call return types for source locals");
    {
        const char *src =
            "world LocalSelfMethodOwner {\n"
            "    func Pick(self, seed: Int) -> Int { return seed + 1; }\n"
            "    func Run(self) -> Int {\n"
            "        let choice = self.Pick(4);\n"
            "        return choice;\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *choice_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "Run", MIR_SCOPE_METHOD);
        if (routine != NULL)
            choice_type = mir_routine_source_local_type_name(routine,
                "choice");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && choice_type != NULL
               && strcmp(choice_type, "Int") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures select receive source-local types");
    {
        const char *src =
            "func SelectReceiveLocalFact() -> Int {\n"
            "    let ch: Channel<Int> = Channel(1);\n"
            "    ch <- 7;\n"
            "    select {\n"
            "        case v = <-ch:\n"
            "            return v;\n"
            "        default:\n"
            "            return 0;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *value_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "SelectReceiveLocalFact",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            value_type = mir_routine_source_local_type_name(routine, "v");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && value_type != NULL
               && strcmp(value_type, "Int") == 0);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("MIR captures underscore source-local types in branch blocks");
    {
        const char *src =
            "func BranchLocalFact(flag: Bool) -> Int {\n"
            "    if flag {\n"
            "        let push_fn: String = \"\";\n"
            "        push_fn = \"pgy_ai_push\";\n"
            "        return StringLength(push_fn);\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        const MIRRoutine *routine = NULL;
        const char *push_fn_type = NULL;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine(mir, "BranchLocalFact",
                                       MIR_SCOPE_FUNCTION);
        if (routine != NULL)
            push_fn_type = mir_routine_source_local_type_name(routine,
                "push_fn");
        EXPECT(ok
               && mir_validate(mir, NULL)
               && routine != NULL
               && push_fn_type != NULL
               && strcmp(push_fn_type, "String") == 0);
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
        ASTNodeType saved_source_type = AST_PROGRAM;
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

            saved_source_type = def_inst->source_node_type;
            def_inst->source_node_type = AST_CALL;
            rejected_invalid_fact =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error, "source-statement emit fact is invalid") != NULL;
            def_inst->source_node_type = saved_source_type;
        }
        EXPECT(ok
               && routine != NULL
               && def_inst != NULL
               && saved_source_type == AST_LET_DECL
               && rejected_missing_fact
               && rejected_invalid_fact
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

}
