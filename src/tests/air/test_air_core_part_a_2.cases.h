static bool
test_air_has_evidence_ignores_summary_flags_with_real_input(void)
{
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_PARALLEL,
            .owner_name = "ShipOrder",
            .source_name = "spawn",
            .has_hir_routine_evidence = true,
            .has_hir_cfg_evidence = true,
            .has_rir_boundary_evidence = true,
        },
    };
    AIRProgram air = {
        .boundaries = boundaries,
        .boundary_count = 1,
        .has_hir_input = true,
    };

    return !air_boundary_has_evidence(&air, 0, AIR_EVIDENCE_HIR_ROUTINE)
        && !air_boundary_has_evidence(&air, 0, AIR_EVIDENCE_HIR_CFG)
        && !air_boundary_has_evidence(&air, 0, AIR_EVIDENCE_RIR_BOUNDARY);
}

static bool
test_air_parallel_boundary_requires_rir_and_hir_evidence(void)
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
            .source_name = "spawn",
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
            && strstr(air.drifts[i].message, "spawn") != NULL) {
            found_rir_evidence_drift = true;
            break;
        }
    }
    bool ok = checked && air.drift_count >= 1 && found_rir_evidence_drift;
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
    bool first_has_evidence_missing = false;
    for (size_t i = 0; i < first_count; i++) {
        if (air.drifts[i].message != NULL
            && strstr(air.drifts[i].message,
                      "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL) {
            first_has_evidence_missing = true;
            break;
        }
    }
    bool second = air_check_drift(&air, &error);
    bool second_has_evidence_missing = false;
    for (size_t i = 0; i < air.drift_count; i++) {
        if (air.drifts[i].message != NULL
            && strstr(air.drifts[i].message,
                      "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL) {
            second_has_evidence_missing = true;
            break;
        }
    }
    bool ok = first
        && second
        && first_count >= 1
        && air.drift_count == first_count
        && first_has_evidence_missing
        && second_has_evidence_missing;
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
test_air_verify_rejects_authority_name_flag_drift(void)
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
    AIRBoundaryNode names_without_required[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_names = authority_names,
            .authority_name_count = 1,
        },
    };
    AIRBoundaryNode action_without_required[] = {
        {
            .kind = AIR_BOUNDARY_ZONE,
            .owner_name = "ShipOrder",
            .source_name = "WarehouseZone",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .authority_from_action = true,
        },
    };
    AIRProgram names_drift = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = names_without_required,
        .boundary_count = 1,
    };
    AIRProgram action_drift = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = action_without_required,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&names_drift, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "authority participants but does not require authority") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&action_drift, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "action-inherited authority without authority") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_source_provenance_shape_drift(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ShipOrder",
            .step_name = "reserve",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode action_on_parallel[] = {
        {
            .kind = AIR_BOUNDARY_PARALLEL,
            .owner_name = "ShipOrder",
            .source_name = "spawn",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .source_from_action = true,
        },
    };
    AIRBoundaryNode transfer_on_io[] = {
        {
            .kind = AIR_BOUNDARY_IO,
            .owner_name = "ShipOrder",
            .source_name = "ReadFile",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_EITHER,
            .source_from_transfer = true,
        },
    };
    AIRProgram action_drift = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = action_on_parallel,
        .boundary_count = 1,
    };
    AIRProgram transfer_drift = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = transfer_on_io,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&action_drift, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "action-inherited source provenance on non-zone boundary") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&transfer_drift, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "transfer source provenance on non-zone/world boundary") != NULL;
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
