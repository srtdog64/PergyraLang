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
test_air_strict_evidence_rejects_summary_flags_with_real_input(void)
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
    bool checked = air_check_drift(&air, &error);
    bool ok = !checked
        && error != NULL
        && strstr(error, "summary without evidence node") != NULL;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_summary_without_inventory(void)
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
            .hir_routine_evidence_name = "dispatch",
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .strict_evidence = true,
        .has_hir_input = true,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "HIR routine evidence summary without evidence node") != NULL;
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_rejects_dag_counter_only(void)
{
    AIRProgram air = {
        .dag_metadata_evidence_count = 1,
        .dag_generic_evidence_count = 1,
        .dag_ability_evidence_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool found_metadata = false;
    bool found_generic = false;
    bool found_ability = false;
    bool ok = air_verify(&air, &error);

    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind != AIR_DRIFT_DAG_DEAD_END_PRESENT
            || strstr(air.drifts[i].message,
                      "AIR DAG evidence counter has no matching evidence node") == NULL) {
            continue;
        }
        if (strstr(air.drifts[i].message, "dag_metadata") != NULL)
            found_metadata = true;
        if (strstr(air.drifts[i].message, "dag_generic") != NULL)
            found_generic = true;
        if (strstr(air.drifts[i].message, "dag_ability") != NULL)
            found_ability = true;
    }

    ok = ok && found_metadata && found_generic && found_ability;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_rejects_rir_propagation_counter_only(void)
{
    AIRProgram air;
    char *error = NULL;
    bool found_effect = false;
    bool found_relation = false;
    bool ok;

    memset(&air, 0, sizeof(air));
    air.strict_evidence = true;
    air.rir_effect_propagation_required_count = 1;
    air.rir_effect_propagation_evidence_count = 1;
    air.rir_relation_propagation_required_count = 1;
    air.rir_relation_propagation_evidence_count = 1;

    ok = air_verify(&air, &error);
    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_EFFECT_PROPAGATION_MISSING
            && strstr(air.drifts[i].message, "effect propagation") != NULL
            && strstr(air.drifts[i].message, "evidence=0") != NULL) {
            found_effect = true;
        }
        if (air.drifts[i].kind == AIR_DRIFT_RELATION_PROPAGATION_MISSING
            && strstr(air.drifts[i].message, "relation propagation") != NULL
            && strstr(air.drifts[i].message, "evidence=0") != NULL) {
            found_relation = true;
        }
    }

    ok = ok && found_effect && found_relation;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}
