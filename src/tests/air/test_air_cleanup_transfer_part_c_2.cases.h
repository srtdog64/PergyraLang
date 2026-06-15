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
test_air_rejects_dag_hits_without_metadata_inventory(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    SemanticResult sem;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;
    memset(&sem, 0, sizeof(sem));
    sem.type_resolution_metadata_hits = 1;

    ok = !air_collect_dag_evidence(air, &sem, &error)
        && error != NULL
        && strstr(error,
            "AIR DAG evidence saw metadata hits without metadata inventory") != NULL;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_reports_dag_dead_end_drift(void)
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

    ok = air_collect_dag_evidence(air, &sem, &error)
        && !air_verify(air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "DAG evidence node 0 has unresolved metadata dead-end facts") != NULL;
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
test_air_rejects_dag_dead_end_evidence(void)
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
        && strstr(error, "DAG evidence node 0 has unresolved metadata dead-end facts") != NULL;
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
