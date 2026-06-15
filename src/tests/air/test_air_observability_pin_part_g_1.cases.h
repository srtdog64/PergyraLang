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
            .kind = AIR_EVIDENCE_MIR_CLEANUP,
            .boundary_index = SIZE_MAX,
            .provider_name = "reserve",
            .subject_name = "cleanup-block",
            .fact_count = 1,
        },
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
        .evidence_count = 2,
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
        && strstr(buffer, "\"runtime_frontier_policy\"") != NULL
        && strstr(buffer, "\"" PGY_FRONTIER_POLICY_SCHEMA "\"") != NULL
        && strstr(buffer, "\"" PGY_FRONTIER_POLICY_SUBJECT "\"") != NULL
        && strstr(buffer, "\"pass_limit_fact_count\":10") != NULL
        && strstr(buffer, "\"overflow_reason_fact_count\":5") != NULL
        && strstr(buffer, "\"fact_count\":15") != NULL
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
test_air_synthesis_collects_observability_schema_evidence(void)
{
    DIRProgram dir;
    RIRProgram rir;
    AIRProgram *air;
    char *error = NULL;
    bool ok;

    memset(&dir, 0, sizeof(dir));
    memset(&rir, 0, sizeof(rir));

    air = air_synthesize(NULL, &dir, &rir, &error);
    ok = air != NULL
        && error == NULL
        && air->observability_schema_evidence_count == 1
        && air->runtime_frontier_policy_evidence_count == 1
        && air->evidence_count == 2
        && air->evidence_nodes[0].kind == AIR_EVIDENCE_OBSERVABILITY_SCHEMA
        && air->evidence_nodes[0].boundary_index == SIZE_MAX
        && strcmp(air->evidence_nodes[0].provider_name,
                  "runtime-observability-schema") == 0
        && strcmp(air->evidence_nodes[0].subject_name,
                  PGY_OBSERVABILITY_ABI_SCHEMA) == 0
        && air->evidence_nodes[0].fact_count
            == PGY_OBSERVABILITY_SCHEMA_FACT_COUNT
        && air->evidence_nodes[0].fallback_count == 0
        && air->evidence_nodes[1].kind == AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY
        && air->evidence_nodes[1].boundary_index == SIZE_MAX
        && strcmp(air->evidence_nodes[1].provider_name,
                  PGY_FRONTIER_POLICY_SCHEMA) == 0
        && strcmp(air->evidence_nodes[1].subject_name,
                  PGY_FRONTIER_POLICY_SUBJECT) == 0
        && air->evidence_nodes[1].fact_count == PGY_FRONTIER_POLICY_FACT_COUNT
        && air->evidence_nodes[1].fallback_count == 0
        && air->drift_count == 0;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_collects_singleton_global_evidence_idempotently(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;

    ok = air_collect_observability_schema_evidence(air, &error)
        && air_collect_observability_schema_evidence(air, &error)
        && air_collect_runtime_frontier_policy_evidence(air, &error)
        && air_collect_runtime_frontier_policy_evidence(air, &error)
        && air_validate(air, &error)
        && air->observability_schema_evidence_count == 1
        && air->runtime_frontier_policy_evidence_count == 1
        && air->evidence_count == 2
        && air->evidence_nodes[0].fact_count
            == PGY_OBSERVABILITY_SCHEMA_FACT_COUNT
        && air->evidence_nodes[1].fact_count
            == PGY_FRONTIER_POLICY_FACT_COUNT;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_conflicting_singleton_global_evidence(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY,
            .boundary_index = SIZE_MAX,
            .provider_name = PGY_FRONTIER_POLICY_SCHEMA,
            .subject_name = PGY_FRONTIER_POLICY_SUBJECT,
            .fact_count = 1,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
        .evidence_capacity = 1,
        .runtime_frontier_policy_evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_collect_runtime_frontier_policy_evidence(&air, &error)
        && error != NULL
        && strstr(error,
                  "AIR singleton global evidence has conflicting counts") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_invalid_observability_schema_provider(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_OBSERVABILITY_SCHEMA,
            .boundary_index = SIZE_MAX,
            .provider_name = "wrong-schema-provider",
            .subject_name = PGY_OBSERVABILITY_ABI_SCHEMA,
            .fact_count = PGY_OBSERVABILITY_SCHEMA_FACT_COUNT,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
        .observability_schema_evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error,
                  "observability schema evidence node 0 has invalid provider") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_empty_observability_schema_evidence(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_OBSERVABILITY_SCHEMA,
            .boundary_index = SIZE_MAX,
            .provider_name = "runtime-observability-schema",
            .subject_name = PGY_OBSERVABILITY_ABI_SCHEMA,
            .fact_count = 0,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
        .observability_schema_evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error,
                  "observability schema evidence node 0 has no schema facts") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_pin_cleanup_counter_mismatch(void)
{
    ASTNode pin_ast = {.type = AST_BLOCK};
    AIRIntentNode intents[] = {
        {
            .intent_owner = "ScoreIntent",
            .step_name = "pin_scores",
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_EXECUTION,
            .owner_name = "ScoreIntent",
            .source_name = "pin",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_SYNC,
            .ast = &pin_ast,
        },
    };
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_MIR_CLEANUP,
            .boundary_index = SIZE_MAX,
            .provider_name = "pin_scores",
            .subject_name = "cleanup-block",
            .fact_count = 1,
            .fallback_count = 0,
        },
        {
            .kind = AIR_EVIDENCE_MIR_PIN_CLEANUP,
            .boundary_index = 0,
            .provider_name = "pin_scores",
            .subject_name = "scores",
            .fact_count = 1,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = evidence_nodes,
        .evidence_count = 2,
        .mir_cleanup_evidence_count = 1,
        .mir_pin_cleanup_evidence_count = 2,
        .has_mir_input = true,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error, "AIR MIR pin cleanup evidence counter does not match evidence nodes") != NULL;
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_rejects_pin_cleanup_counter_only(void)
{
    AIRProgram air = {
        .mir_pin_cleanup_evidence_count = 1,
        .strict_evidence = true,
        .has_mir_input = true,
    };
    char *error = NULL;
    bool found = false;
    bool ok = air_verify(&air, &error);

    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "AIR MIR evidence counter has no matching boundary evidence node") != NULL
            && strstr(air.drifts[i].message,
                      "mir_pin_cleanup") != NULL) {
            found = true;
            break;
        }
    }
    ok = ok && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_requires_observability_schema(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY,
            .boundary_index = SIZE_MAX,
            .provider_name = PGY_FRONTIER_POLICY_SCHEMA,
            .subject_name = PGY_FRONTIER_POLICY_SUBJECT,
            .fact_count = PGY_FRONTIER_POLICY_FACT_COUNT,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
        .runtime_frontier_policy_evidence_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool found = false;
    bool ok = air_verify(&air, &error);

    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && air.drifts[i].intent_index == SIZE_MAX
            && air.drifts[i].boundary_index == SIZE_MAX
            && strstr(air.drifts[i].message,
                      "AIR has no runtime observability schema evidence") != NULL
            && strstr(air.drifts[i].message,
                      "AIR_EVIDENCE_OBSERVABILITY_SCHEMA") != NULL) {
            found = true;
            break;
        }
    }
    ok = ok && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_rejects_observability_counter_only(void)
{
    AIRProgram air = {
        .observability_schema_evidence_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool found = false;
    bool ok = air_verify(&air, &error);

    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message,
                      "AIR has no runtime observability schema evidence") != NULL) {
            found = true;
            break;
        }
    }
    ok = ok && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_rejects_invalid_runtime_frontier_policy_provider(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY,
            .boundary_index = SIZE_MAX,
            .provider_name = "wrong-frontier-policy",
            .subject_name = PGY_FRONTIER_POLICY_SUBJECT,
            .fact_count = PGY_FRONTIER_POLICY_FACT_COUNT,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
        .runtime_frontier_policy_evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error,
                  "runtime frontier policy evidence node 0 has invalid provider") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_empty_runtime_frontier_policy_evidence(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY,
            .boundary_index = SIZE_MAX,
            .provider_name = PGY_FRONTIER_POLICY_SCHEMA,
            .subject_name = PGY_FRONTIER_POLICY_SUBJECT,
            .fact_count = 0,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
        .runtime_frontier_policy_evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error,
                  "runtime frontier policy evidence node 0 has invalid policy fact count") != NULL
        && strstr(error, "expected=15 actual=0") != NULL;
    free(error);
    return ok;
}

static bool
test_air_strict_evidence_requires_runtime_frontier_policy(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_OBSERVABILITY_SCHEMA,
            .boundary_index = SIZE_MAX,
            .provider_name = "runtime-observability-schema",
            .subject_name = PGY_OBSERVABILITY_ABI_SCHEMA,
            .fact_count = PGY_OBSERVABILITY_SCHEMA_FACT_COUNT,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
        .observability_schema_evidence_count = 1,
        .strict_evidence = true,
    };
    char *error = NULL;
    bool found = false;
    bool ok = air_verify(&air, &error);

    for (size_t i = 0; ok && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && air.drifts[i].intent_index == SIZE_MAX
            && air.drifts[i].boundary_index == SIZE_MAX
            && strstr(air.drifts[i].message,
                      "AIR has no runtime frontier policy evidence") != NULL
            && strstr(air.drifts[i].message,
                      "AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY") != NULL) {
            found = true;
            break;
        }
    }
    ok = ok && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}
