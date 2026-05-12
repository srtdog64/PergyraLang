static bool
test_air_collects_mir_terminator_evidence(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[2];
    MIRInstruction insts[2];
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;

    memset(insts, 0, sizeof(insts));
    insts[0].kind = MIR_INST_BRANCH;
    insts[0].has_source_terminator_kind = true;
    insts[0].source_terminator_kind = HIR_BLOCK_BRANCH;
    insts[1].kind = MIR_INST_RETURN;
    insts[1].has_source_terminator_kind = true;
    insts[1].source_terminator_kind = HIR_BLOCK_RETURN;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = true;
    blocks[0].instructions = &insts[0];
    blocks[0].instruction_count = 1;
    blocks[1].id = 1;
    blocks[1].is_reachable = true;
    blocks[1].instructions = &insts[1];
    blocks[1].instruction_count = 1;

    memset(&routine, 0, sizeof(routine));
    routine.name = "cfg_owner";
    routine.blocks = blocks;
    routine.block_count = 2;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->has_mir_input
        && air->mir_terminator_evidence_count == 1
        && air->evidence_count == 1
        && air->evidence_nodes[0].kind == AIR_EVIDENCE_MIR_TERMINATOR
        && air->evidence_nodes[0].boundary_index == SIZE_MAX
        && air->evidence_nodes[0].fact_count == 2
        && air->evidence_nodes[0].fallback_count == 0
        && strcmp(air->evidence_nodes[0].provider_name, "cfg_owner") == 0
        && strcmp(air->evidence_nodes[0].subject_name, "cfg-terminator") == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_empty_mir_terminator_evidence(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_TERMINATOR,
            .boundary_index = SIZE_MAX,
            .provider_name = "cfg_owner",
            .subject_name = "cfg-terminator",
            .fact_count = 0,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "MIR terminator evidence node 0 has no terminator facts") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_mir_evidence_without_routine_provider(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock block;
    MIRInstruction inst;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_RETURN;
    inst.has_source_terminator_kind = true;
    inst.source_terminator_kind = HIR_BLOCK_RETURN;

    memset(&block, 0, sizeof(block));
    block.is_reachable = true;
    block.instructions = &inst;
    block.instruction_count = 1;

    memset(&routine, 0, sizeof(routine));
    routine.blocks = &block;
    routine.block_count = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = !air_collect_mir_evidence(air, &mir, &error)
        && error != NULL
        && strstr(error,
                  "AIR MIR evidence requires routine name or owner provenance") != NULL
        && air->evidence_count == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_collects_mir_select_receive_evidence(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock block;
    MIRInstruction inst;
    ASTNode source_ast;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;

    memset(&source_ast, 0, sizeof(source_ast));
    source_ast.type = AST_SELECT_STMT;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_DEF;
    inst.ast = &source_ast;
    inst.has_source_location = true;
    inst.source_ast_type = source_ast.type;
    inst.requires_source_statement_emit = true;
    inst.requires_channel_receive_statement_emit = true;
    inst.requires_select_receive_statement_emit = true;

    memset(&block, 0, sizeof(block));
    block.is_reachable = true;
    block.instructions = &inst;
    block.instruction_count = 1;

    memset(&routine, 0, sizeof(routine));
    routine.name = "select_owner";
    routine.blocks = &block;
    routine.block_count = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->has_mir_input
        && air->mir_select_receive_evidence_count == 1
        && air->evidence_count == 1
        && air->evidence_nodes[0].kind == AIR_EVIDENCE_MIR_SELECT_RECEIVE
        && air->evidence_nodes[0].boundary_index == SIZE_MAX
        && air->evidence_nodes[0].fact_count == 1
        && air->evidence_nodes[0].fallback_count == 0
        && strcmp(air->evidence_nodes[0].provider_name, "select_owner") == 0
        && strcmp(air->evidence_nodes[0].subject_name, "select-receive") == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_empty_mir_select_receive_evidence(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_SELECT_RECEIVE,
            .boundary_index = SIZE_MAX,
            .provider_name = "select_owner",
            .subject_name = "select-receive",
            .fact_count = 0,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "MIR select receive evidence node 0 has no select receive facts") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_mir_evidence_counter_mismatch(void)
{
    AIREvidenceNode cleanup_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_CLEANUP,
            .boundary_index = SIZE_MAX,
            .provider_name = "cleanup_owner",
            .subject_name = "cleanup-block",
            .fact_count = 1,
            .fallback_count = 0,
        },
        {
            .kind = AIR_EVIDENCE_MIR_CLEANUP,
            .boundary_index = SIZE_MAX,
            .provider_name = "cleanup_owner_2",
            .subject_name = "cleanup-block",
            .fact_count = 1,
            .fallback_count = 0,
        },
    };
    AIREvidenceNode terminator_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_TERMINATOR,
            .boundary_index = SIZE_MAX,
            .provider_name = "cfg_owner",
            .subject_name = "cfg-terminator",
            .fact_count = 1,
            .fallback_count = 0,
        },
        {
            .kind = AIR_EVIDENCE_MIR_TERMINATOR,
            .boundary_index = SIZE_MAX,
            .provider_name = "cfg_owner_2",
            .subject_name = "cfg-terminator",
            .fact_count = 1,
            .fallback_count = 0,
        },
    };
    AIREvidenceNode select_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_SELECT_RECEIVE,
            .boundary_index = SIZE_MAX,
            .provider_name = "select_owner",
            .subject_name = "select-receive",
            .fact_count = 1,
            .fallback_count = 0,
        },
        {
            .kind = AIR_EVIDENCE_MIR_SELECT_RECEIVE,
            .boundary_index = SIZE_MAX,
            .provider_name = "select_owner_2",
            .subject_name = "select-receive",
            .fact_count = 1,
            .fallback_count = 0,
        },
    };
    AIRProgram cleanup_air = {
        .evidence_nodes = cleanup_nodes,
        .evidence_count = 2,
        .has_mir_input = true,
        .mir_cleanup_evidence_count = 1,
    };
    AIRProgram terminator_air = {
        .evidence_nodes = terminator_nodes,
        .evidence_count = 2,
        .has_mir_input = true,
        .mir_terminator_evidence_count = 1,
    };
    AIRProgram select_air = {
        .evidence_nodes = select_nodes,
        .evidence_count = 2,
        .has_mir_input = true,
        .mir_select_receive_evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&cleanup_air, &error)
        && error != NULL
        && strstr(error,
                  "MIR cleanup evidence counter does not match evidence nodes") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_validate(&terminator_air, &error)
        && error != NULL
        && strstr(error,
                  "MIR terminator evidence counter does not match evidence nodes") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_validate(&select_air, &error)
        && error != NULL
        && strstr(error,
                  "MIR select receive evidence counter does not match evidence nodes") != NULL;
    free(error);
    return ok;
}

static bool
test_air_collects_void_fallthrough_terminator_evidence(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    HIRRoutine hir_routine;
    MIRBasicBlock block;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;

    memset(&block, 0, sizeof(block));
    block.id = 0;
    block.is_reachable = true;
    block.source_hir_block_id = 3;

    memset(&routine, 0, sizeof(routine));
    routine.name = "void_fallthrough";
    memset(&hir_routine, 0, sizeof(hir_routine));
    routine.hir_routine = &hir_routine;
    routine.blocks = &block;
    routine.block_count = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->has_mir_input
        && air->mir_terminator_evidence_count == 1
        && air->evidence_count == 1
        && air->evidence_nodes[0].kind == AIR_EVIDENCE_MIR_TERMINATOR
        && air->evidence_nodes[0].fact_count == 1
        && air->evidence_nodes[0].fallback_count == 0
        && strcmp(air->evidence_nodes[0].provider_name, "void_fallthrough") == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_strict_evidence_requires_mir_terminator_evidence(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "Dispatch",
            .step_name = "run",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "Dispatch",
            .source_name = "run",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
        .has_mir_input = true,
    };
    char *error = NULL;
    bool found = false;
    bool ok = air_verify(&air, &error);

    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && air.drifts[i].intent_index == SIZE_MAX
            && air.drifts[i].boundary_index == SIZE_MAX
            && strstr(air.drifts[i].message,
                      "AIR MIR input has no CFG terminator evidence") != NULL
            && strstr(air.drifts[i].message,
                      "AIR_EVIDENCE_MIR_TERMINATOR") != NULL) {
            found = true;
            break;
        }
    }
    ok = ok && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_rejects_mir_terminator_counter_only(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "Dispatch",
            .step_name = "run",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "Dispatch",
            .source_name = "run",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .mir_terminator_evidence_count = 1,
        .strict_evidence = true,
        .has_mir_input = true,
    };
    char *error = NULL;
    bool found = false;
    bool ok = air_verify(&air, &error);

    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "AIR MIR input has no CFG terminator evidence") != NULL
            && strstr(air.drifts[i].message,
                      "AIR_EVIDENCE_MIR_TERMINATOR") != NULL) {
            found = true;
            break;
        }
    }
    ok = ok && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_rejects_mir_cleanup_counter_only(void)
{
    AIRProgram air = {
        .mir_cleanup_evidence_count = 1,
        .strict_evidence = true,
        .has_mir_input = true,
    };
    char *error = NULL;
    bool found = false;
    bool ok = air_verify(&air, &error);

    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "AIR MIR evidence counter has no matching evidence node") != NULL
            && strstr(air.drifts[i].message,
                      "mir_cleanup") != NULL) {
            found = true;
            break;
        }
    }
    ok = ok && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_rejects_mir_select_receive_counter_only(void)
{
    AIRProgram air = {
        .mir_select_receive_evidence_count = 1,
        .strict_evidence = true,
        .has_mir_input = true,
    };
    char *error = NULL;
    bool found = false;
    bool ok = air_verify(&air, &error);

    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "AIR MIR evidence counter has no matching evidence node") != NULL
            && strstr(air.drifts[i].message,
                      "mir_select_receive") != NULL) {
            found = true;
            break;
        }
    }
    ok = ok && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}
