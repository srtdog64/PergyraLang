static bool
test_air_verify_rejects_invalid_drift_inventory(void)
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
    AIRDrift drifts[] = {
        {
            .kind = AIR_DRIFT_NONE,
            .intent_index = 0,
            .boundary_index = 0,
            .message = "stale placeholder",
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .drifts = drifts,
        .drift_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "drift node 0 has invalid kind") != NULL;
    free(error);
    return ok;
}

static bool
test_air_verify_rejects_duplicate_evidence_nodes(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_DAG_METADATA,
            .boundary_index = SIZE_MAX,
            .provider_name = "type-resolution-dag",
            .subject_name = "metadata-inventory",
            .fact_count = 7,
        },
        {
            .kind = AIR_EVIDENCE_DAG_METADATA,
            .boundary_index = SIZE_MAX,
            .provider_name = "type-resolution-dag",
            .subject_name = "metadata-inventory",
            .fact_count = 7,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 2,
    };
    char *error = NULL;
    bool ok = !air_verify(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "duplicates evidence node") != NULL;
    free(error);
    return ok;
}

static bool
test_air_append_merges_duplicate_evidence_nodes(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    char *error = NULL;
    bool ok = air != NULL
        && air_append_evidence_node_ex(air,
                                       AIR_EVIDENCE_DAG_METADATA,
                                       SIZE_MAX,
                                       "type-resolution-dag",
                                       "metadata-inventory",
                                       7,
                                       0,
                                       &error)
        && air_append_evidence_node_ex(air,
                                       AIR_EVIDENCE_DAG_METADATA,
                                       SIZE_MAX,
                                       "type-resolution-dag",
                                       "metadata-inventory",
                                       3,
                                       0,
                                       &error)
        && air->evidence_count == 1
        && air->evidence_nodes != NULL
        && air->evidence_nodes[0].fact_count == 10
        && air_verify(air, &error);
    free(error);
    air_destroy(air);
    return ok;
}
