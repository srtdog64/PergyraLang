static bool
test_air_synthesizes_intent_and_boundary(void)
{
    ASTNode intent_ast = { .line = 12, .column = 5 };
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = &intent_ast },
    };
    const char *authorized_by[] = { "shipper" };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "reserve",
            .where_type_name = "WarehouseZone",
            .authorized_by = authorized_by,
            .authorized_by_count = 1,
            .who_inherited_from_intent = true,
            .where_inherited_from_intent = true,
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
            .name = "shipper",
            .resource_kind = RIR_RESOURCE_AUTHORITY_HANDLE,
            .state = RIR_STATE_AUTHORIZED,
        },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_ZONE,
            .owner_name = "ShipOrder",
            .name = "WarehouseZone",
            .facts = facts,
            .fact_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == 1
        && air->drift_count == 0
        && air->strict_evidence
        && strcmp(air->intents[0].intent_owner, "ShipOrder") == 0
        && strcmp(air->intents[0].step_name, "reserve") == 0
        && air->intents[0].ast == &intent_ast
        && air->intents[0].sync_class == AIR_SYNC_SYNC
        && air->intents[0].who_from_intent_default
        && air->boundaries[0].kind == AIR_BOUNDARY_ZONE
        && air->boundaries[0].ast == &intent_ast
        && air->boundaries[0].source_from_intent_default
        && air->boundaries[0].authority_required
        && air->boundaries[0].authority_name_count == 1
        && strcmp(air->boundaries[0].authority_names[0], "shipper") == 0
        && air->boundaries[0].has_rir_boundary_evidence
        && air->boundaries[0].has_rir_authority_evidence;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_who_inference_does_not_imply_authority(void)
{
    ASTNode intent_ast = { .line = 21, .column = 3 };
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "Patrol", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "walk",
            .where_type_name = "CityZone",
            .authorized_by_count = 0,
            .who_inherited_from_intent = true,
            .where_inherited_from_intent = true,
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
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_ZONE,
            .owner_name = "Patrol",
            .name = "CityZone",
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == 1
        && air->drift_count == 0
        && air->intents[0].who_from_intent_default
        && air->boundaries[0].kind == AIR_BOUNDARY_ZONE
        && air->boundaries[0].source_from_intent_default
        && !air->boundaries[0].authority_required
        && air->boundaries[0].authority_name_count == 0
        && !air->boundaries[0].has_rir_authority_evidence;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_detects_sync_async_drift(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
            .who_from_intent_default = true,
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
            .source_from_intent_default = true,
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
        && air.drifts[0].kind == AIR_DRIFT_SYNC_ASYNC_CONFLICT
        && strstr(air.drifts[0].message, "PGY_SEM_INTENT_BOUNDARY_DRIFT") != NULL
        && strstr(air.drifts[0].message, "source_provenance=intent-default") != NULL
        && strstr(air.drifts[0].message, "who_provenance=intent-default") != NULL;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_accepts_async_boundary_match(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "handoff",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_COMPENSABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_WORLD,
            .owner_name = "ShipOrder",
            .source_name = "remote",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .source_from_transfer = true,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = air_check_drift(&air, &error) && air.drift_count == 0;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_reports_missing_boundary(void)
{
    const char *authority_names[] = { "shipper" };
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
            .authority_names = authority_names,
            .authority_name_count = 1,
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
    bool found = false;
    bool checked = air_check_drift(&air, &error);
    for (size_t i = 0; i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL) {
            found = true;
            break;
        }
    }
    bool ok = checked && air.drift_count >= 1 && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_requires_hir_for_implementation_boundary(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "dispatch",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_PARALLEL,
            .owner_name = "ShipOrder",
            .source_name = "spawn",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .has_rir_boundary_evidence = true,
            .rir_boundary_evidence_scope = "spawn",
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
    bool found_hir_evidence_drift = false;
    bool checked = air_check_drift(&air, &error);

    for (size_t i = 0; i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "AIR implementation boundary has no matching HIR CFG evidence") != NULL
            && strstr(air.drifts[i].message, "spawn") != NULL
            && strstr(air.drifts[i].message, "parallel") != NULL) {
            found_hir_evidence_drift = true;
            break;
        }
    }

    bool ok = checked
        && air.drift_count >= 1
        && found_hir_evidence_drift;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_prefers_inventory_over_summary_flags(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "dispatch",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_PARALLEL,
            .owner_name = "ShipOrder",
            .source_name = "spawn",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .has_hir_routine_evidence = true,
            .has_hir_cfg_evidence = true,
            .has_rir_boundary_evidence = true,
            .hir_routine_evidence_name = "dispatch",
            .rir_boundary_evidence_scope = "spawn",
        },
    };
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_HIR_ROUTINE,
            .boundary_index = 0,
            .provider_name = "dispatch",
            .subject_name = "spawn",
            .fact_count = 1,
        },
        {
            .kind = AIR_EVIDENCE_RIR_BOUNDARY,
            .boundary_index = 0,
            .provider_name = "spawn",
            .subject_name = "spawn",
            .fact_count = 1,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = evidence_nodes,
        .evidence_count = 2,
        .strict_evidence = true,
        .has_hir_input = true,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "HIR CFG evidence summary without evidence node") != NULL;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}
