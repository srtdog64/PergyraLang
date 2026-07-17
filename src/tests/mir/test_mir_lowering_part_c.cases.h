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
            insts[0].source_node_type = AST_CALL;
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

    TEST("MIR validator rejects missing source-local type inventory");
    {
        MIRRoutine routine = { 0 };
        MIRProgram mir = { 0 };
        char *mir_error = NULL;
        bool rejected;

        routine.name = "MissingSourceLocalTypes";
        routine.source_local_type_count = 1;
        routine.source_local_type_capacity = 1;
        routine.source_local_types = NULL;
        mir.routine_count = 1;
        mir.routines = &routine;

        rejected = !mir_validate(&mir, &mir_error)
            && mir_error != NULL
            && strstr(mir_error, "without source-local type inventory") != NULL;
        EXPECT(rejected);
        free(mir_error);
    }

    TEST("MIR validator rejects invalid source-local type fact");
    {
        MIRSourceLocalType facts[1] = { { "local", NULL } };
        MIRRoutine routine = { 0 };
        MIRProgram mir = { 0 };
        char *mir_error = NULL;
        bool rejected;

        routine.name = "InvalidSourceLocalTypes";
        routine.source_local_type_count = 1;
        routine.source_local_type_capacity = 1;
        routine.source_local_types = facts;
        mir.routine_count = 1;
        mir.routines = &routine;

        rejected = !mir_validate(&mir, &mir_error)
            && mir_error != NULL
            && strstr(mir_error, "source-local type fact[0] is incomplete") != NULL;
        EXPECT(rejected);
        free(mir_error);
    }

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
