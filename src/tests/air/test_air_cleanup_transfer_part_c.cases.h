static bool
test_air_rejects_mismatched_mir_pin_cleanup_evidence(void)
{
    ASTNode pin_ast;
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[2];
    MIRInstruction insts[2];
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;
    memset(&pin_ast, 0, sizeof(pin_ast));
    pin_ast.type = AST_BLOCK;
    pin_ast.data.block.is_pin_block = true;

    air->intents = (AIRIntentNode *)calloc(1, sizeof(AIRIntentNode));
    air->boundaries = (AIRBoundaryNode *)calloc(1, sizeof(AIRBoundaryNode));
    if (air->intents == NULL || air->boundaries == NULL) {
        air_destroy(air);
        return false;
    }
    air->intent_count = 1;
    air->boundary_count = 1;
    air->intents[0].intent_owner = "ScoreIntent";
    air->intents[0].step_name = "pin_scores";
    air->intents[0].step_index = 0;
    air->intents[0].sync_class = AIR_SYNC_SYNC;
    air->intents[0].failure_class = AIR_FAILURE_RECOVERABLE;
    air->boundaries[0].kind = AIR_BOUNDARY_EXECUTION;
    air->boundaries[0].owner_name = "ScoreIntent";
    air->boundaries[0].source_name = "pin";
    air->boundaries[0].intent_index = 0;
    air->boundaries[0].step_index = 0;
    air->boundaries[0].sync_class = AIR_SYNC_SYNC;
    air->boundaries[0].ast = &pin_ast;

    memset(insts, 0, sizeof(insts));
    insts[0].kind = MIR_INST_CLEANUP_EDGE;
    insts[0].name = "cleanup-edge";
    insts[0].slot_anchor = "cleanup";
    insts[0].arg0 = "cleanup";
    insts[1].kind = MIR_INST_CLEANUP_EDGE;
    insts[1].name = "pin-unpin-cleanup-edge";
    insts[1].slot_anchor = "scores";
    insts[1].arg0 = "other_view";
    insts[1].arg1 = "read";
    insts[1].ast = &pin_ast;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = true;
    blocks[0].is_pin_region = true;
    blocks[0].pin_source_name = "scores";
    blocks[0].pin_view_name = "view";
    blocks[0].pin_block_ast = &pin_ast;
    blocks[0].has_cleanup_succ = true;
    blocks[0].cleanup_succ = 1;
    blocks[0].instructions = insts;
    blocks[0].instruction_count = 2;
    blocks[1].id = 1;
    blocks[1].is_cleanup = true;
    blocks[1].is_reachable = true;

    memset(&routine, 0, sizeof(routine));
    routine.name = "pin_scores";
    routine.blocks = blocks;
    routine.block_count = 2;
    routine.has_cleanup_block = true;
    routine.cleanup_block = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->has_mir_input
        && air->mir_cleanup_evidence_count == 1
        && air->mir_pin_cleanup_evidence_count == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_pin_cleanup_evidence_without_slot_subject(void)
{
    ASTNode pin_ast = {.type = AST_BLOCK};
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ScoreIntent",
            .step_name = "pin_scores",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "ScoreIntent",
            .source_name = "pin",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .ast = &pin_ast,
        },
    };
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_PIN_CLEANUP,
            .boundary_index = 0,
            .provider_name = "pin_scores",
            .subject_name = "pin",
            .fact_count = 1,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "has no slot anchor subject") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_pin_cleanup_evidence_without_source_provenance(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ScoreIntent",
            .step_name = "pin_scores",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "ScoreIntent",
            .source_name = "pin",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
        },
    };
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_CLEANUP,
            .boundary_index = SIZE_MAX,
            .provider_name = "pin_scores",
            .subject_name = "cleanup-block",
            .fact_count = 1,
            .fallback_count = 0,
        },
        {
            .kind = AIR_EVIDENCE_MIR_PIN_CLEANUP,
            .boundary_index = 0,
            .provider_name = "pin_scores",
            .subject_name = "scores",
            .fact_count = 1,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = evidence_nodes,
        .evidence_count = 2,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "no source AST provenance") != NULL;
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_requires_mir_pin_cleanup(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ScoreIntent",
            .step_name = "pin_scores",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "ScoreIntent",
            .source_name = "pin",
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
    bool ok;

    ok = air_verify(&air, &error);
    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "AIR pin boundary has no matching MIR pin cleanup evidence") != NULL
            && strstr(air.drifts[i].message,
                      "source_provenance=explicit") != NULL
            && strstr(air.drifts[i].message,
                      "who_provenance=explicit") != NULL
            && strstr(air.drifts[i].message,
                      "authority_provenance=none") != NULL) {
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
test_air_rejects_pin_cleanup_evidence_fact_count_mismatch(void)
{
    ASTNode pin_ast = {.type = AST_BLOCK};
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ScoreIntent",
            .step_name = "pin_scores",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "ScoreIntent",
            .source_name = "pin",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .ast = &pin_ast,
        },
    };
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_PIN_CLEANUP,
            .boundary_index = 0,
            .provider_name = "pin_scores",
            .subject_name = "scores",
            .fact_count = 2,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error, "must carry exactly one boundary fact") != NULL;
    free(error);
    return ok;
}
static bool
test_air_rejects_pin_cleanup_without_global_cleanup_evidence(void)
{
    ASTNode pin_ast = {.type = AST_BLOCK};
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ScoreIntent",
            .step_name = "pin_scores",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "ScoreIntent",
            .source_name = "pin",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .ast = &pin_ast,
        },
    };
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_PIN_CLEANUP,
            .boundary_index = 0,
            .provider_name = "pin_scores",
            .subject_name = "scores",
            .fact_count = 1,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "no matching MIR cleanup evidence") != NULL;
    free(error);
    return ok;
}

static bool
test_air_collects_mir_cleanup_block_evidence(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[2];
    MIRInstruction inst;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = "cleanup-edge";
    inst.slot_anchor = "cleanup";
    inst.arg0 = "cleanup";

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = true;
    blocks[0].has_cleanup_succ = true;
    blocks[0].cleanup_succ = 1;
    blocks[0].instructions = &inst;
    blocks[0].instruction_count = 1;
    blocks[1].id = 1;
    blocks[1].is_reachable = true;
    blocks[1].is_cleanup = true;

    memset(&routine, 0, sizeof(routine));
    routine.name = "cleanup_owner";
    routine.blocks = blocks;
    routine.block_count = 2;
    routine.has_cleanup_block = true;
    routine.cleanup_block = 1;
    routine.cleanup_instruction_count = 1;
    routine.cleanup_edge_count = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->mir_cleanup_evidence_count == 1
        && air->mir_pin_cleanup_evidence_count == 0
        && air->evidence_count == 1
        && air->evidence_nodes[0].kind == AIR_EVIDENCE_MIR_CLEANUP
        && air->evidence_nodes[0].boundary_index == SIZE_MAX
        && air->evidence_nodes[0].fact_count == 1
        && air->evidence_nodes[0].fallback_count == 0
        && strcmp(air->evidence_nodes[0].provider_name, "cleanup_owner") == 0
        && strcmp(air->evidence_nodes[0].subject_name, "cleanup-block") == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_ignores_orphan_mir_cleanup_root_evidence(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[3];
    MIRInstruction insts[2];
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;

    memset(insts, 0, sizeof(insts));
    insts[0].kind = MIR_INST_CLEANUP_EDGE;
    insts[0].name = "cleanup-edge";
    insts[0].slot_anchor = "cleanup";
    insts[0].arg0 = "cleanup";
    insts[1].kind = MIR_INST_CLEANUP_EDGE;
    insts[1].name = "cleanup-edge-from-rollback";
    insts[1].slot_anchor = "cleanup";
    insts[1].arg0 = "cleanup";

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = true;
    blocks[0].has_cleanup_succ = true;
    blocks[0].cleanup_succ = 1;
    blocks[0].instructions = &insts[0];
    blocks[0].instruction_count = 1;
    blocks[1].id = 1;
    blocks[1].is_cleanup = true;
    blocks[1].is_reachable = true;
    blocks[2].id = 2;
    blocks[2].is_cleanup = true;
    blocks[2].is_reachable = true;
    blocks[2].instructions = &insts[1];
    blocks[2].instruction_count = 1;

    memset(&routine, 0, sizeof(routine));
    routine.name = "cleanup_owner";
    routine.blocks = blocks;
    routine.block_count = 3;
    routine.has_cleanup_block = true;
    routine.cleanup_block = 1;
    routine.cleanup_instruction_count = 2;
    routine.cleanup_edge_count = 2;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->mir_cleanup_evidence_count == 1
        && air->evidence_count == 1
        && air->evidence_nodes[0].kind == AIR_EVIDENCE_MIR_CLEANUP
        && air->evidence_nodes[0].fact_count == 1
        && air->evidence_nodes[0].fallback_count == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_ignores_unreachable_mir_cleanup_root_evidence(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[2];
    MIRInstruction inst;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = "cleanup-edge";
    inst.slot_anchor = "cleanup";
    inst.arg0 = "cleanup";

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = true;
    blocks[0].has_cleanup_succ = true;
    blocks[0].cleanup_succ = 1;
    blocks[0].instructions = &inst;
    blocks[0].instruction_count = 1;
    blocks[1].id = 1;
    blocks[1].is_cleanup = true;
    blocks[1].is_reachable = false;

    memset(&routine, 0, sizeof(routine));
    routine.name = "cleanup_owner";
    routine.blocks = blocks;
    routine.block_count = 2;
    routine.has_cleanup_block = true;
    routine.cleanup_block = 1;
    routine.cleanup_instruction_count = 1;
    routine.cleanup_edge_count = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->mir_cleanup_evidence_count == 0
        && air->mir_pin_cleanup_evidence_count == 0
        && air->evidence_count == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_ignores_unreachable_mir_cleanup_source_evidence(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[2];
    MIRInstruction inst;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = "cleanup-edge";
    inst.slot_anchor = "cleanup";
    inst.arg0 = "cleanup";

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = false;
    blocks[0].has_cleanup_succ = true;
    blocks[0].cleanup_succ = 1;
    blocks[0].instructions = &inst;
    blocks[0].instruction_count = 1;
    blocks[1].id = 1;
    blocks[1].is_cleanup = true;
    blocks[1].is_reachable = true;

    memset(&routine, 0, sizeof(routine));
    routine.name = "cleanup_owner";
    routine.blocks = blocks;
    routine.block_count = 2;
    routine.has_cleanup_block = true;
    routine.cleanup_block = 1;
    routine.cleanup_instruction_count = 1;
    routine.cleanup_edge_count = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->mir_cleanup_evidence_count == 0
        && air->mir_pin_cleanup_evidence_count == 0
        && air->evidence_count == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_empty_mir_cleanup_evidence(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_CLEANUP,
            .boundary_index = SIZE_MAX,
            .provider_name = "cleanup_owner",
            .subject_name = "cleanup-block",
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
        && strstr(error, "MIR cleanup evidence node 0 has no cleanup facts") != NULL;
    free(error);
    return ok;
}

static bool
test_air_collects_dag_generic_ability_evidence(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    SemanticResult sem;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;
    memset(&sem, 0, sizeof(sem));
    sem.type_resolution_metadata_entries = 33;
    sem.type_resolution_metadata_hits = 49;
    sem.type_resolution_metadata_dead_ends = 0;
    sem.type_resolution_metadata_materializer_fallbacks = 0;
    sem.type_resolution_dag_generic_contract_evidence_count = 7;
    sem.type_resolution_dag_ability_consumer_evidence_count = 5;

    ok = air_collect_dag_evidence(air, &sem, &error)
        && air_validate(air, &error)
        && air->dag_metadata_evidence_count == 1
        && air->dag_generic_evidence_count == 1
        && air->dag_ability_evidence_count == 1
        && air->evidence_count == 3
        && air->evidence_nodes[0].kind == AIR_EVIDENCE_DAG_METADATA
        && air->evidence_nodes[0].boundary_index == SIZE_MAX
        && air->evidence_nodes[0].fact_count == 33
        && air->evidence_nodes[0].fallback_count == 0
        && air->evidence_nodes[1].kind == AIR_EVIDENCE_DAG_GENERIC
        && air->evidence_nodes[1].fact_count == 7
        && air->evidence_nodes[1].fallback_count == 0
        && air->evidence_nodes[2].kind == AIR_EVIDENCE_DAG_ABILITY
        && air->evidence_nodes[2].fact_count == 5
        && air->evidence_nodes[2].fallback_count == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_reports_dag_fallback_drift(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    SemanticResult sem;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;
    memset(&sem, 0, sizeof(sem));
    air->strict_evidence = true;
    sem.type_resolution_dag_generic_contract_evidence_count = 1;
    sem.type_resolution_dag_ability_consumer_evidence_count = 1;
    sem.type_resolution_metadata_dead_ends = 2;
    sem.type_resolution_metadata_materializer_fallbacks = 0;

    ok = air_collect_dag_evidence(air, &sem, &error)
        && !air_verify(air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "DAG evidence node 0 has fallback DAG facts") != NULL;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_invalid_dag_evidence_provider(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_DAG_GENERIC,
            .boundary_index = SIZE_MAX,
            .provider_name = "legacy-resolver",
            .subject_name = "generic-contracts",
            .fact_count = 1,
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
        && strstr(error, "DAG evidence node 0 has invalid provider") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_empty_dag_evidence(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_DAG_GENERIC,
            .boundary_index = SIZE_MAX,
            .provider_name = "type-resolution-dag",
            .subject_name = "generic-contracts",
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
        && strstr(error, "DAG evidence node 0 has no DAG facts") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_dag_fallback_evidence(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_DAG_METADATA,
            .boundary_index = SIZE_MAX,
            .provider_name = "type-resolution-dag",
            .subject_name = "metadata-inventory",
            .fact_count = 1,
            .fallback_count = 1,
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
        && strstr(error, "DAG evidence node 0 has fallback DAG facts") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_invalid_dag_evidence_subject(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_DAG_ABILITY,
            .boundary_index = SIZE_MAX,
            .provider_name = "type-resolution-dag",
            .subject_name = "generic-contracts",
            .fact_count = 1,
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
        && strstr(error, "DAG evidence node 0 has invalid subject") != NULL;
    free(error);
    return ok;
}

static bool
test_air_world_boundary_requires_transfer_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "Checkout", .ast = NULL },
    };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "Handoff",
            .transfer_from_alias = "cart",
            .transfer_to_alias = "payment",
        },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    RIROp unrelated_ops[] = {
        { .kind = RIR_OP_AUTHORIZE, .subject = "buyer" },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "Checkout",
            .ops = unrelated_ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool found = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && strstr(air->drifts[i].message,
                          "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL
                && strstr(air->drifts[i].message, "implementation boundary 'payment'") != NULL) {
                found = true;
                break;
            }
        }
    }

    bool ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_WORLD
        && air->boundaries[0].source_from_transfer
        && !air->boundaries[0].has_rir_boundary_evidence
        && found;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_world_boundary_accepts_transfer_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "Checkout", .ast = NULL },
    };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "Handoff",
            .transfer_from_alias = "cart",
            .transfer_to_alias = "payment",
        },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 1, .steps = steps, .step_count = 1 },
    };
    DIRProgram dir = {
        .nodes = nodes,
        .node_count = 1,
        .intents = intents,
        .intent_count = 1,
    };
    RIROp transfer_ops[] = {
        { .kind = RIR_OP_MOVE, .subject = "cart", .arg0 = "payment", .arg1 = "Handoff" },
        { .kind = RIR_OP_CLAIM, .subject = "payment", .arg0 = "cart", .arg1 = "Handoff" },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "Checkout",
            .ops = transfer_ops,
            .op_count = 2,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_WORLD
        && air->boundaries[0].source_from_transfer
        && air->boundaries[0].has_rir_boundary_evidence
        && air->drift_count == 0;
    air_destroy(air);
    free(error);
    return ok;
}
