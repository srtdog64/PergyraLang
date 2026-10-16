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
        MIRSourceLocalType facts[1] = {
            { .name = "local", .type_name = NULL }
        };
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

    TEST("MIR validator rejects zero and duplicate local-type identities");
    {
        MIRSourceLocalType facts[2] = {
            { .name = "local", .type_name = "Int", .binding_syntax_id = 0 },
            { .name = "local", .type_name = "String", .binding_syntax_id = 7 }
        };
        MIRRoutine routine = {0};
        MIRProgram mir = {0};
        char *error = NULL;
        routine.name = "InvalidLocalTypeIdentity";
        routine.source_local_type_count = routine.source_local_type_capacity = 2;
        routine.source_local_types = facts;
        mir.routine_count = 1;
        mir.routines = &routine;
        bool rejected = !mir_validate(&mir, &error) && error != NULL
            && strstr(error, "missing binding identity") != NULL;
        free(error);
        error = NULL;
        facts[0].binding_syntax_id = 7;
        rejected &= !mir_validate(&mir, &error) && error != NULL
            && strstr(error, "duplicate binding identity") != NULL;
        EXPECT(rejected);
        free(error);
    }
