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
            .provider_kind = AIR_EVIDENCE_PROVIDER_RIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_BOUNDARY,
            .boundary_index = 7,
            .provider_name = "WarehouseZone",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode empty_provider[] = {
        {
            .kind = AIR_EVIDENCE_HIR_CFG,
            .provider_kind = AIR_EVIDENCE_PROVIDER_HIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_CFG,
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
            .provider_kind = AIR_EVIDENCE_PROVIDER_RIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_BOUNDARY,
            .boundary_index = 0,
            .provider_name = "WarehouseZone",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
        {
            .kind = AIR_EVIDENCE_RIR_AUTHORITY,
            .provider_kind = AIR_EVIDENCE_PROVIDER_RIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_AUTHORITY,
            .boundary_index = 0,
            .provider_name = "WarehouseZone",
            .subject_name = "observer",
            .fact_count = 1,
        },
    };
    AIREvidenceNode typed_boundary_kind_drift[] = {
        {
            .kind = AIR_EVIDENCE_RIR_BOUNDARY,
            .provider_kind = AIR_EVIDENCE_PROVIDER_RIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_BOUNDARY,
            .boundary_index = 0,
            .provider_name = "WarehouseZone",
            .subject_name = "WarehouseZone",
            .has_boundary_shape = true,
            .boundary_kind = AIR_BOUNDARY_WORLD,
            .boundary_owner_name = "ShipOrder",
            .boundary_source_name = "WarehouseZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode typed_kind_mismatch[] = {
        {
            .kind = AIR_EVIDENCE_RIR_BOUNDARY,
            .provider_kind = AIR_EVIDENCE_PROVIDER_HIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_BOUNDARY,
            .boundary_index = 0,
            .provider_name = "WarehouseZone",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode global_attached_to_boundary[] = {
        {
            .kind = AIR_EVIDENCE_DAG_GENERIC,
            .provider_kind = AIR_EVIDENCE_PROVIDER_DAG,
            .subject_kind = AIR_EVIDENCE_SUBJECT_GENERIC,
            .boundary_index = 0,
            .provider_name = "type-resolution-dag",
            .subject_name = "generic-contracts",
            .fact_count = 1,
        },
    };
    AIREvidenceNode cfg_without_routine[] = {
        {
            .kind = AIR_EVIDENCE_HIR_CFG,
            .provider_kind = AIR_EVIDENCE_PROVIDER_HIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_CFG,
            .boundary_index = 0,
            .provider_name = "reserve",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode hir_subject_mismatch[] = {
        {
            .kind = AIR_EVIDENCE_HIR_ROUTINE,
            .provider_kind = AIR_EVIDENCE_PROVIDER_HIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_ROUTINE,
            .boundary_index = 0,
            .provider_name = "reserve",
            .subject_name = "OtherZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode rir_subject_mismatch[] = {
        {
            .kind = AIR_EVIDENCE_RIR_BOUNDARY,
            .provider_kind = AIR_EVIDENCE_PROVIDER_RIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_BOUNDARY,
            .boundary_index = 0,
            .provider_name = "WarehouseZone",
            .subject_name = "OtherZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode cfg_provider_mismatch[] = {
        {
            .kind = AIR_EVIDENCE_HIR_ROUTINE,
            .provider_kind = AIR_EVIDENCE_PROVIDER_HIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_ROUTINE,
            .boundary_index = 0,
            .provider_name = "reserve",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
        {
            .kind = AIR_EVIDENCE_HIR_CFG,
            .provider_kind = AIR_EVIDENCE_PROVIDER_HIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_CFG,
            .boundary_index = 0,
            .provider_name = "otherRoutine",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
    };
    AIREvidenceNode authority_provider_mismatch[] = {
        {
            .kind = AIR_EVIDENCE_RIR_BOUNDARY,
            .provider_kind = AIR_EVIDENCE_PROVIDER_RIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_BOUNDARY,
            .boundary_index = 0,
            .provider_name = "WarehouseZone",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
        {
            .kind = AIR_EVIDENCE_RIR_AUTHORITY,
            .provider_kind = AIR_EVIDENCE_PROVIDER_RIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_AUTHORITY,
            .boundary_index = 0,
            .provider_name = "OtherScope",
            .subject_name = "shipper",
            .fact_count = 1,
        },
    };
    AIREvidenceNode real_input_missing_summary[] = {
        {
            .kind = AIR_EVIDENCE_HIR_ROUTINE,
            .provider_kind = AIR_EVIDENCE_PROVIDER_HIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_ROUTINE,
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
    AIRProgram bad_typed_boundary_shape = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = typed_boundary_kind_drift,
        .evidence_count = 1,
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
    AIRProgram bad_typed_kind = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = typed_kind_mismatch,
        .evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_verify(&bad_authority_subject, &error)
        && error != NULL
        && strstr(error, "undeclared authority subject") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_verify(&bad_typed_boundary_shape, &error)
        && error != NULL
        && strstr(error, "boundary kind drift") != NULL;
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
    error = NULL;
    ok = ok
        && !air_verify(&bad_typed_kind, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "typed evidence mismatch") != NULL;
    free(error);
    return ok;
}
