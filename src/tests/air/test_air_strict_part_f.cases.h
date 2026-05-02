static bool
test_air_verify_rejects_world_boundary_without_transfer_provenance(void)
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
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "world boundary node 0 has no transfer provenance") != NULL;
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_rejects_legacy_flags_with_real_input(void)
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
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
        .has_hir_input = true,
        .has_rir_input = true,
    };
    char *error = NULL;
    bool found_rir_boundary_drift = false;
    bool found_hir_routine_drift = false;
    bool found_hir_cfg_drift = false;
    bool checked = air_check_drift(&air, &error);

    for (size_t i = 0; i < air.drift_count; i++) {
        if (air.drifts[i].kind != AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
            continue;
        if (strstr(air.drifts[i].message,
                   "AIR boundary has no matching RIR boundary evidence") != NULL)
            found_rir_boundary_drift = true;
        if (strstr(air.drifts[i].message,
                   "AIR boundary has no matching HIR routine evidence") != NULL)
            found_hir_routine_drift = true;
        if (strstr(air.drifts[i].message,
                   "AIR implementation boundary has no matching HIR CFG evidence") != NULL)
            found_hir_cfg_drift = true;
    }

    bool ok = checked
        && air.drift_count == 3
        && found_rir_boundary_drift
        && found_hir_routine_drift
        && found_hir_cfg_drift;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}
