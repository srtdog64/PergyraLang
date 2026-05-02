static bool
test_air_verify_rejects_empty_boundary_evidence(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
        },
    };
    AIREvidenceNode empty_hir_routine[] = {
        {
            .kind = AIR_EVIDENCE_HIR_ROUTINE,
            .boundary_index = 0,
            .provider_name = "reserve",
            .subject_name = "WarehouseZone",
            .fact_count = 0,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = empty_hir_routine,
        .evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "boundary evidence node 0 has no evidence facts") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_authority_evidence_shape_mismatch(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    const char *authority_names[] = { "shipper" };
    AIRBoundaryNode authority_without_boundary[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_required = true,
            .authority_names = authority_names,
            .authority_name_count = 1,
            .has_rir_authority_evidence = true,
            .rir_authority_evidence_name = "shipper",
        },
    };
    AIRBoundaryNode authority_on_non_authority[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .has_rir_boundary_evidence = true,
            .rir_boundary_evidence_scope = "WarehouseZone",
            .has_rir_authority_evidence = true,
            .rir_authority_evidence_name = "shipper",
        },
    };
    AIRBoundaryNode undeclared_authority[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_required = true,
            .authority_names = authority_names,
            .authority_name_count = 1,
            .has_rir_boundary_evidence = true,
            .rir_boundary_evidence_scope = "WarehouseZone",
            .has_rir_authority_evidence = true,
            .rir_authority_evidence_name = "carrier",
        },
    };
    AIRProgram missing_boundary = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = authority_without_boundary,
        .boundary_count = 1,
    };
    AIRProgram non_authority = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = authority_on_non_authority,
        .boundary_count = 1,
    };
    AIRProgram mismatched_authority = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = undeclared_authority,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&missing_boundary, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "authority evidence without boundary evidence") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&non_authority, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "authority evidence on non-authority boundary") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&mismatched_authority, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "authority evidence for undeclared participant") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_cfg_evidence_without_routine(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "Work",
            .step_name = "run",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "Work",
            .source_name = "with",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .has_hir_cfg_evidence = true,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "HIR CFG evidence without routine evidence") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_empty_evidence_provenance(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "Work",
            .step_name = "run",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    const char *authority_names[] = { "worker" };
    AIRBoundaryNode empty_hir[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "Work",
            .source_name = "with",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .has_hir_routine_evidence = true,
            .hir_routine_evidence_name = "",
        },
    };
    AIRBoundaryNode empty_rir_boundary[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "Work",
            .source_name = "WorkZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .has_rir_boundary_evidence = true,
            .rir_boundary_evidence_scope = "",
        },
    };
    AIRBoundaryNode empty_rir_authority[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "Work",
            .source_name = "WorkZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_required = true,
            .authority_names = authority_names,
            .authority_name_count = 1,
            .has_rir_boundary_evidence = true,
            .rir_boundary_evidence_scope = "WorkZone",
            .has_rir_authority_evidence = true,
            .rir_authority_evidence_name = "",
        },
    };
    AIRProgram empty_hir_program = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = empty_hir,
        .boundary_count = 1,
    };
    AIRProgram empty_rir_boundary_program = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = empty_rir_boundary,
        .boundary_count = 1,
    };
    AIRProgram empty_rir_authority_program = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = empty_rir_authority,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&empty_hir_program, &error)
        && error != NULL
        && strstr(error, "HIR evidence without provenance") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&empty_rir_boundary_program, &error)
        && error != NULL
        && strstr(error, "RIR boundary evidence without provenance") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&empty_rir_authority_program, &error)
        && error != NULL
        && strstr(error, "RIR authority evidence without provenance") != NULL;
    free(error);
    return ok;
}

static bool
test_air_check_drift_is_verify_compatibility_wrapper(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool checked = air_check_drift(&air, &error);
    bool ok = checked
        && air.drift_count == 1
        && air.drifts[0].kind == AIR_DRIFT_SYNC_ASYNC_CONFLICT;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_collects_hir_and_rir_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = NULL },
    };
    const char *authorized_by[] = { "shipper" };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "reserve",
            .where_type_name = "WarehouseZone",
            .authorized_by = authorized_by,
            .authorized_by_count = 1,
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
    HIRBasicBlock reserve_blocks[] = {
        { .id = 0, .terminator_kind = HIR_BLOCK_RETURN, .is_reachable = true },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_FUNCTION,
            .owner_name = "ShipOrder",
            .name = "reserve",
            .has_cfg = true,
            .cfg = {
                .blocks = reserve_blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIRFact facts[] = {
        {
            .kind = RIR_FACT_AUTHORITY,
            .name = "shipper",
            .resource_kind = RIR_RESOURCE_AUTHORITY_HANDLE,
            .state = RIR_STATE_AUTHORIZED,
        },
    };
    RIROp ops[] = {
        { .kind = RIR_OP_AUTHORIZE, .subject = "shipper" },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_ZONE,
            .owner_name = "ShipOrder",
            .name = "WarehouseZone",
            .facts = facts,
            .fact_count = 1,
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    const char *dir_owner_before = dir.nodes[0].name;
    const char *dir_step_before = dir.intents[0].steps[0].name;
    const char *dir_where_before = dir.intents[0].steps[0].where_type_name;
    const char *dir_authority_before = dir.intents[0].steps[0].authorized_by[0];
    const char *hir_owner_before = hir.routines[0].owner_name;
    const char *hir_name_before = hir.routines[0].name;
    RIRScopeKind rir_kind_before = rir.scopes[0].kind;
    const char *rir_owner_before = rir.scopes[0].owner_name;
    const char *rir_name_before = rir.scopes[0].name;
    RIROpKind rir_op_before = rir.scopes[0].ops[0].kind;
    const char *rir_op_subject_before = rir.scopes[0].ops[0].subject;
    RIRFactKind rir_fact_before = rir.scopes[0].facts[0].kind;
    const char *rir_fact_name_before = rir.scopes[0].facts[0].name;
    char *error = NULL;
    AIRProgram *air = air_synthesize(&hir, &dir, &rir, &error);
    bool ok = air != NULL
        && air->hir_routine_evidence_count == 1
        && air->hir_cfg_evidence_count == 1
        && air->rir_boundary_evidence_count == 1
        && air->rir_authority_evidence_count == 2
        && air->boundaries[0].has_hir_routine_evidence
        && air->boundaries[0].has_hir_cfg_evidence
        && air->boundaries[0].has_rir_boundary_evidence
        && air->boundaries[0].has_rir_authority_evidence
        && air->boundaries[0].hir_routine_evidence_name != NULL
        && strcmp(air->boundaries[0].hir_routine_evidence_name, "reserve") == 0
        && air->boundaries[0].rir_boundary_evidence_scope != NULL
        && strcmp(air->boundaries[0].rir_boundary_evidence_scope, "WarehouseZone") == 0
        && air->boundaries[0].rir_authority_evidence_name != NULL
        && strcmp(air->boundaries[0].rir_authority_evidence_name, "shipper") == 0
        && dir.node_count == 1
        && dir.intent_count == 1
        && dir.intents[0].step_count == 1
        && dir.nodes[0].name == dir_owner_before
        && dir.intents[0].steps[0].name == dir_step_before
        && dir.intents[0].steps[0].where_type_name == dir_where_before
        && dir.intents[0].steps[0].authorized_by[0] == dir_authority_before
        && hir.routine_count == 1
        && hir.routines[0].owner_name == hir_owner_before
        && hir.routines[0].name == hir_name_before
        && rir.scope_count == 1
        && rir.scopes[0].kind == rir_kind_before
        && rir.scopes[0].owner_name == rir_owner_before
        && rir.scopes[0].name == rir_name_before
        && rir.scopes[0].op_count == 1
        && rir.scopes[0].ops[0].kind == rir_op_before
        && rir.scopes[0].ops[0].subject == rir_op_subject_before
        && rir.scopes[0].fact_count == 1
        && rir.scopes[0].facts[0].kind == rir_fact_before
        && rir.scopes[0].facts[0].name == rir_fact_name_before;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_rejects_mismatched_authority_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = NULL },
    };
    const char *authorized_by[] = { "shipper" };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "reserve",
            .where_type_name = "WarehouseZone",
            .authorized_by = authorized_by,
            .authorized_by_count = 1,
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
    RIRFact facts[] = {
        {
            .kind = RIR_FACT_AUTHORITY,
            .name = "observer",
            .resource_kind = RIR_RESOURCE_AUTHORITY_HANDLE,
            .state = RIR_STATE_AUTHORIZED,
        },
    };
    RIROp ops[] = {
        { .kind = RIR_OP_AUTHORIZE, .subject = "observer" },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_ZONE,
            .owner_name = "ShipOrder",
            .name = "WarehouseZone",
            .facts = facts,
            .fact_count = 1,
            .ops = ops,
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
                && strstr(air->drifts[i].message,
                          "expected authority participant(s): shipper") != NULL) {
                found = true;
                break;
            }
        }
    }
    bool ok = air != NULL
        && air->boundaries[0].has_rir_boundary_evidence
        && !air->boundaries[0].has_rir_authority_evidence
        && air->drift_count >= 1
        && found;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_dump_prints_evidence_provenance(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_required = true,
            .has_hir_routine_evidence = true,
            .has_hir_cfg_evidence = true,
            .has_rir_boundary_evidence = true,
            .has_rir_authority_evidence = true,
            .hir_routine_evidence_name = "reserve",
            .rir_boundary_evidence_scope = "WarehouseZone",
            .rir_authority_evidence_name = "shipper",
        },
    };
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_HIR_ROUTINE,
            .boundary_index = 0,
            .provider_name = "reserve",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
        {
            .kind = AIR_EVIDENCE_HIR_CFG,
            .boundary_index = 0,
            .provider_name = "reserve",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
        {
            .kind = AIR_EVIDENCE_RIR_BOUNDARY,
            .boundary_index = 0,
            .provider_name = "WarehouseZone",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
        {
            .kind = AIR_EVIDENCE_RIR_AUTHORITY,
            .boundary_index = 0,
            .provider_name = "WarehouseZone",
            .subject_name = "shipper",
            .fact_count = 1,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = evidence_nodes,
        .evidence_count = 4,
        .strict_evidence = true,
        .has_hir_input = true,
        .has_rir_input = true,
    };
    char buffer[2048];
    FILE *out = tmpfile();
    size_t bytes;
    bool ok;

    if (out == NULL)
        return false;
    air_dump(&air, out);
    fflush(out);
    rewind(out);
    bytes = fread(buffer, 1, sizeof(buffer) - 1, out);
    buffer[bytes] = '\0';
    fclose(out);

    ok = strstr(buffer, "strict_evidence=yes hir_input=yes rir_input=yes") != NULL
        && strstr(buffer, "evidence hir=yes(reserve) hir_cfg=yes") != NULL
        && strstr(buffer, "rir_boundary=yes(WarehouseZone)") != NULL
        && strstr(buffer, "rir_authority=yes(shipper)") != NULL
        && strstr(buffer, "evidence_node[0] kind=hir_routine") != NULL
        && strstr(buffer, "provider=reserve subject=WarehouseZone facts=1 fallbacks=0") != NULL
        && strstr(buffer, "evidence_node[3] kind=rir_authority") != NULL
        && strstr(buffer, "provider=WarehouseZone subject=shipper facts=1 fallbacks=0") != NULL;
    return ok;
}

static bool
test_air_dump_json_prints_stable_graph_schema(void)
{
    /* Smoke-gated schema literals: pgy.intent.observability.v1, pgy.intent.trace.v1. */
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "ShipOrder",
            .source_name = "pin",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
        },
    };
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_PIN_CLEANUP,
            .boundary_index = 0,
            .provider_name = "reserve",
            .subject_name = "scores",
            .fact_count = 1,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
        .mir_cleanup_evidence_count = 1,
        .mir_pin_cleanup_evidence_count = 1,
        .strict_evidence = true,
        .has_hir_input = true,
    };
    char buffer[4096];
    FILE *out = tmpfile();
    size_t bytes;
    bool ok;

    if (out == NULL)
        return false;
    air_dump_json(&air, out);
    fflush(out);
    rewind(out);
    bytes = fread(buffer, 1, sizeof(buffer) - 1, out);
    buffer[bytes] = '\0';
    fclose(out);

    ok = strstr(buffer, "\"schema\":\"pgy.air.graph.v1\"") != NULL
        && strstr(buffer, "\"summary\"") != NULL
        && strstr(buffer, "\"observability\"") != NULL
        && strstr(buffer, "\"abi_schema\":\"" PGY_OBSERVABILITY_ABI_SCHEMA "\"") != NULL
        && strstr(buffer, "\"trace_schema\":\"" PGY_OBSERVABILITY_TRACE_SCHEMA "\"") != NULL
        && strstr(buffer, "\"surfaces\":[\"last\",\"history\",\"active\",\"recent\"]") != NULL
        && strstr(buffer, "\"" PGY_OBSERVABILITY_SURFACE_LAST "\"") != NULL
        && strstr(buffer, "\"event_kinds\"") != NULL
        && strstr(buffer, "\"" PGY_OBSERVABILITY_EVENT_INTENT_ENTER "\"") != NULL
        && strstr(buffer, "\"transfer\"") != NULL
        && strstr(buffer, "\"history_fields\"") != NULL
        && strstr(buffer, "\"from_zone\"") != NULL
        && strstr(buffer, "\"intent_count\":1") != NULL
        && strstr(buffer, "\"boundaries\"") != NULL
        && strstr(buffer, "\"evidence\"") != NULL
        && strstr(buffer, "\"kind\":\"mir_pin_cleanup\"") != NULL
        && strstr(buffer, "\"mir_pin_cleanup_evidence_count\":1") != NULL
        && strstr(buffer, "\"drifts\"") != NULL;
    return ok;
}

static bool
test_air_collects_mir_pin_cleanup_evidence(void)
{
    ASTNode pin_ast;
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[2];
    MIRInstruction inst;
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

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = "pin-unpin-cleanup-edge";
    inst.slot_anchor = "scores";
    inst.arg0 = "view";
    inst.arg1 = "read";
    inst.ast = &pin_ast;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = true;
    blocks[0].is_pin_region = true;
    blocks[0].pin_source_name = "scores";
    blocks[0].pin_view_name = "view";
    blocks[0].pin_block_ast = &pin_ast;
    blocks[0].has_cleanup_succ = true;
    blocks[0].cleanup_succ = 1;
    blocks[0].instructions = &inst;
    blocks[0].instruction_count = 1;
    blocks[1].id = 1;
    blocks[1].is_cleanup = true;

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
        && air->mir_cleanup_evidence_count == 1
        && air->mir_pin_cleanup_evidence_count == 1
        && air->evidence_count == 2
        && air->evidence_nodes[0].kind == AIR_EVIDENCE_MIR_CLEANUP
        && air->evidence_nodes[1].kind == AIR_EVIDENCE_MIR_PIN_CLEANUP
        && air->evidence_nodes[1].boundary_index == 0
        && strcmp(air->evidence_nodes[1].provider_name, "pin_scores") == 0
        && strcmp(air->evidence_nodes[1].subject_name, "scores") == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_orphan_mir_pin_cleanup_evidence(void)
{
    ASTNode pin_ast;
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock block;
    MIRInstruction inst;
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

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = "pin-unpin-cleanup-edge";
    inst.slot_anchor = "scores";
    inst.ast = &pin_ast;

    memset(&block, 0, sizeof(block));
    block.is_reachable = true;
    block.is_pin_region = true;
    block.pin_source_name = "scores";
    block.pin_view_name = "view";
    block.pin_block_ast = &pin_ast;
    block.instructions = &inst;
    block.instruction_count = 1;

    memset(&routine, 0, sizeof(routine));
    routine.name = "pin_scores";
    routine.blocks = &block;
    routine.block_count = 1;

    memset(&mir, 0, sizeof(mir));
    mir.routines = &routine;
    mir.routine_count = 1;

    ok = air_collect_mir_evidence(air, &mir, &error)
        && air_validate(air, &error)
        && air->has_mir_input
        && air->mir_cleanup_evidence_count == 0
        && air->mir_pin_cleanup_evidence_count == 0
        && air->evidence_count == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_unanchored_mir_pin_cleanup_evidence(void)
{
    ASTNode pin_ast;
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    MIRProgram mir;
    MIRRoutine routine;
    MIRBasicBlock blocks[2];
    MIRInstruction inst;
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

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = "pin-unpin-cleanup-edge";
    inst.ast = &pin_ast;

    memset(blocks, 0, sizeof(blocks));
    blocks[0].is_reachable = true;
    blocks[0].is_pin_region = true;
    blocks[0].pin_source_name = "scores";
    blocks[0].pin_view_name = "view";
    blocks[0].pin_block_ast = &pin_ast;
    blocks[0].has_cleanup_succ = true;
    blocks[0].cleanup_succ = 1;
    blocks[0].instructions = &inst;
    blocks[0].instruction_count = 1;
    blocks[1].id = 1;
    blocks[1].is_cleanup = true;

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
