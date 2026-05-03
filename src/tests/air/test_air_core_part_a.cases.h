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
        && air.drift_count == 1
        && found_hir_evidence_drift;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_prefers_inventory_over_legacy_flags(void)
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

static bool
test_air_task_group_boundary_requires_rir_and_hir_evidence(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "CoordinateWork",
            .step_name = "coordinate",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_PARALLEL,
            .owner_name = "CoordinateWork",
            .source_name = "task-group",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .has_hir_routine_evidence = true,
            .has_hir_cfg_evidence = true,
            .hir_routine_evidence_name = "coordinate",
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
    bool checked = air_check_drift(&air, &error);
    bool found_rir_evidence_drift = false;
    for (size_t i = 0; i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && air.drifts[i].message != NULL
            && strstr(air.drifts[i].message, "no matching RIR boundary evidence") != NULL
            && strstr(air.drifts[i].message, "task-group") != NULL) {
            found_rir_evidence_drift = true;
            break;
        }
    }
    bool ok = checked && air.drift_count == 1 && found_rir_evidence_drift;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_recheck_clears_owned_drift_messages(void)
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
    bool first = air_check_drift(&air, &error);
    size_t first_count = air.drift_count;
    const char *first_message = first_count > 0 ? air.drifts[0].message : NULL;
    bool second = air_check_drift(&air, &error);
    bool ok = first
        && second
        && first_count >= 1
        && first_message != NULL
        && air.drift_count == first_count
        && air.drifts[0].message != NULL
        && strstr(air.drifts[0].message,
                  "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_invalid_boundary_inventory(void)
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
            .authority_name_count = 1,
            .authority_names = NULL,
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
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "authority count without names") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_missing_inventory_arrays(void)
{
    AIRProgram missing_intents = {
        .intent_count = 1,
    };
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRProgram missing_boundaries = {
        .intents = intents,
        .intent_count = 1,
        .boundary_count = 1,
    };
    AIRProgram missing_drifts = {
        .intents = intents,
        .intent_count = 1,
        .drift_count = 1,
    };
    AIRProgram missing_evidence = {
        .intents = intents,
        .intent_count = 1,
        .evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&missing_intents, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "intent count without intent array") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&missing_boundaries, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "boundary count without boundary array") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&missing_drifts, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "drift count without drift array") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&missing_evidence, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "evidence count without evidence array") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_boundary_step_mismatch(void)
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
            .step_index = 7,
            .sync_class = AIR_SYNC_SYNC,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "step index does not match intent") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_boundary_owner_mismatch(void)
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
            .owner_name = "OtherIntent",
            .source_name = "WarehouseZone",
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
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "owner does not match intent") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_boundary_sync_shape_mismatch(void)
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
            .sync_class = AIR_SYNC_SYNC,
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
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "invalid sync class sync for world boundary") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_invalid_evidence_inventory(void)
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
    AIREvidenceNode missing_boundary[] = {
        {
            .kind = AIR_EVIDENCE_RIR_BOUNDARY,
            .boundary_index = 7,
            .provider_name = "WarehouseZone",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode empty_provider[] = {
        {
            .kind = AIR_EVIDENCE_HIR_CFG,
            .boundary_index = 0,
            .provider_name = "",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
    };
    AIRProgram bad_boundary = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = missing_boundary,
        .evidence_count = 1,
    };
    AIRProgram bad_provider = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = empty_provider,
        .evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&bad_boundary, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "references missing boundary node") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&bad_provider, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "has no provider provenance") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_evidence_boundary_shape_mismatch(void)
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
    AIREvidenceNode wrong_authority_subject[] = {
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
            .subject_name = "observer",
            .fact_count = 1,
        },
    };
    AIREvidenceNode global_attached_to_boundary[] = {
        {
            .kind = AIR_EVIDENCE_DAG_GENERIC,
            .boundary_index = 0,
            .provider_name = "type-resolution-dag",
            .subject_name = "generic-contracts",
            .fact_count = 1,
        },
    };
    AIREvidenceNode cfg_without_routine[] = {
        {
            .kind = AIR_EVIDENCE_HIR_CFG,
            .boundary_index = 0,
            .provider_name = "reserve",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode hir_subject_mismatch[] = {
        {
            .kind = AIR_EVIDENCE_HIR_ROUTINE,
            .boundary_index = 0,
            .provider_name = "reserve",
            .subject_name = "OtherZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode rir_subject_mismatch[] = {
        {
            .kind = AIR_EVIDENCE_RIR_BOUNDARY,
            .boundary_index = 0,
            .provider_name = "WarehouseZone",
            .subject_name = "OtherZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode cfg_provider_mismatch[] = {
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
            .provider_name = "otherRoutine",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode authority_provider_mismatch[] = {
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
            .provider_name = "OtherScope",
            .subject_name = "shipper",
            .fact_count = 1,
        },
    };
    AIREvidenceNode real_input_missing_summary[] = {
        {
            .kind = AIR_EVIDENCE_HIR_ROUTINE,
            .boundary_index = 0,
            .provider_name = "reserve",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
    };
    AIRProgram bad_authority_subject = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = wrong_authority_subject,
        .evidence_count = 2,
    };
    AIRProgram bad_global = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = global_attached_to_boundary,
        .evidence_count = 1,
    };
    AIRProgram bad_cfg = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = cfg_without_routine,
        .evidence_count = 1,
    };
    AIRProgram bad_hir_subject = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = hir_subject_mismatch,
        .evidence_count = 1,
    };
    AIRProgram bad_rir_subject = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = rir_subject_mismatch,
        .evidence_count = 1,
    };
    AIRProgram bad_cfg_provider = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = cfg_provider_mismatch,
        .evidence_count = 2,
    };
    AIRProgram bad_authority_provider = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = authority_provider_mismatch,
        .evidence_count = 2,
    };
    AIRProgram bad_real_input_summary = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = real_input_missing_summary,
        .evidence_count = 1,
        .has_hir_input = true,
    };
    char *error = NULL;
    bool ok = !air_verify(&bad_authority_subject, &error)
        && error != NULL
        && strstr(error, "undeclared authority subject") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&bad_global, &error)
        && error != NULL
        && strstr(error, "global evidence node") != NULL
        && strstr(error, "attached to boundary") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&bad_cfg, &error)
        && error != NULL
        && strstr(error, "HIR CFG evidence node") != NULL
        && strstr(error, "no matching HIR routine evidence") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&bad_hir_subject, &error)
        && error != NULL
        && strstr(error, "HIR routine evidence node") != NULL
        && strstr(error, "subject/source mismatch") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&bad_rir_subject, &error)
        && error != NULL
        && strstr(error, "RIR boundary evidence node") != NULL
        && strstr(error, "subject/source mismatch") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&bad_cfg_provider, &error)
        && error != NULL
        && strstr(error, "HIR CFG evidence node") != NULL
        && strstr(error, "no matching HIR routine evidence") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&bad_authority_provider, &error)
        && error != NULL
        && strstr(error, "RIR authority evidence node") != NULL
        && strstr(error, "no matching RIR boundary evidence") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&bad_real_input_summary, &error)
        && error != NULL
        && strstr(error, "boundary evidence node 0") != NULL
        && strstr(error, "no matching boundary summary flag") != NULL;
    free(error);
    return ok;
}
