static bool
test_air_append_rejects_unknown_evidence_kind(void)
{
    AIRProgram air = { 0 };
    char *error = NULL;
    bool ok = !air_append_evidence_node(&air,
                                        AIR_EVIDENCE_KIND_COUNT,
                                        SIZE_MAX,
                                        "unknown-provider",
                                        "unknown-subject",
                                        &error)
        && error != NULL
        && strstr(error, "known evidence kind") != NULL
        && air.evidence_count == 0;
    free(error);
    return ok;
}

static bool
test_air_append_rejects_empty_evidence_provenance(void)
{
    AIRProgram air = { 0 };
    char *error = NULL;
    bool ok = !air_append_evidence_node(&air,
                                        AIR_EVIDENCE_HIR_ROUTINE,
                                        0,
                                        "",
                                        "with",
                                        &error)
        && error != NULL
        && strstr(error, "non-empty provider and subject provenance") != NULL
        && air.evidence_count == 0;
    free(error);
    error = NULL;
    ok = ok
        && !air_append_evidence_node(&air,
                                     AIR_EVIDENCE_HIR_ROUTINE,
                                     0,
                                     "run",
                                     NULL,
                                     &error)
        && error != NULL
        && strstr(error, "non-empty provider and subject provenance") != NULL
        && air.evidence_count == 0;
    free(error);
    return ok;
}

static bool
test_air_append_rejects_empty_evidence_counts(void)
{
    AIRProgram air = { 0 };
    char *error = NULL;
    bool ok = !air_append_evidence_node_ex(&air,
                                           AIR_EVIDENCE_DAG_METADATA,
                                           SIZE_MAX,
                                           "type-resolution-dag",
                                           "metadata-inventory",
                                           0,
                                           0,
                                           &error)
        && error != NULL
        && strstr(error, "at least one fact or fallback fact") != NULL
        && air.evidence_count == 0;
    free(error);
    return ok;
}

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
