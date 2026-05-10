static bool
test_air_collects_rir_effect_relation_propagation_evidence(void)
{
    const char *src =
        "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
        "ability Payable { func Pay() -> Void; }\n"
        "role BuyerPay for Buyer {\n"
        "    impl ability Payable { func Pay() -> Void { return; } }\n"
        "}\n"
        "relation CartLink for source: Buyer, target: Buyer { }\n"
        "effect PaymentEffect for bearer: Buyer { }\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "    relation slot cart: CartLink\n"
        "    effect slot fx: PaymentEffect\n"
        "    authority buyer requires Payable\n"
        "    apply fx to buyer by buyer\n"
        "    link cart between buyer, buyer by buyer\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(src);
    bool found_effect = false;
    bool found_relation = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->evidence_count; i++) {
            if (air->evidence_nodes[i].kind == AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
                && strcmp(air->evidence_nodes[i].subject_name, "fx") == 0) {
                found_effect = true;
            }
            if (air->evidence_nodes[i].kind == AIR_EVIDENCE_RIR_RELATION_PROPAGATION
                && strcmp(air->evidence_nodes[i].subject_name, "cart") == 0) {
                found_relation = true;
            }
        }
    }

    bool ok = air != NULL
        && air->rir_effect_propagation_required_count == 1
        && air->rir_effect_propagation_evidence_count == 1
        && air->rir_relation_propagation_required_count == 1
        && air->rir_relation_propagation_evidence_count == 1
        && found_effect
        && found_relation;
    air_destroy(air);
    return ok;
}

static bool
test_air_rejects_rir_evidence_without_scope_provider(void)
{
    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    RIRProgram rir;
    RIRScope scope;
    RIROp op;
    RIRStateSummary summary;
    char *error = NULL;
    bool ok;

    if (air == NULL)
        return false;

    memset(&op, 0, sizeof(op));
    op.kind = RIR_OP_ATTACH_EFFECT;
    op.subject = "fx";

    memset(&summary, 0, sizeof(summary));
    summary.name = "fx";
    summary.resource_kind = RIR_RESOURCE_EFFECT_INSTANCE;

    memset(&scope, 0, sizeof(scope));
    scope.kind = RIR_SCOPE_ZONE;
    scope.ops = &op;
    scope.op_count = 1;
    scope.state_summaries = &summary;
    scope.state_summary_count = 1;

    memset(&rir, 0, sizeof(rir));
    rir.scopes = &scope;
    rir.scope_count = 1;

    ok = !air_collect_rir_evidence(air, &rir, &error)
        && error != NULL
        && strstr(error,
                  "AIR RIR evidence requires scope name or owner provenance") != NULL
        && air->evidence_count == 0;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_reports_missing_effect_relation_propagation_evidence(void)
{
    AIRProgram air;
    char *error = NULL;
    bool found_effect = false;
    bool found_relation = false;
    bool ok;

    memset(&air, 0, sizeof(air));
    air.strict_evidence = true;
    air.rir_effect_propagation_required_count = 2;
    air.rir_effect_propagation_evidence_count = 1;
    air.rir_relation_propagation_required_count = 1;
    air.rir_relation_propagation_evidence_count = 0;

    ok = air_verify(&air, &error);
    for (size_t i = 0; i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_EFFECT_PROPAGATION_MISSING
            && air.drifts[i].intent_index == SIZE_MAX
            && air.drifts[i].boundary_index == SIZE_MAX
            && strstr(air.drifts[i].message, "effect propagation") != NULL) {
            found_effect = true;
        }
        if (air.drifts[i].kind == AIR_DRIFT_RELATION_PROPAGATION_MISSING
            && air.drifts[i].intent_index == SIZE_MAX
            && air.drifts[i].boundary_index == SIZE_MAX
            && strstr(air.drifts[i].message, "relation propagation") != NULL) {
            found_relation = true;
        }
    }

    ok = ok && found_effect && found_relation;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_rejects_empty_rir_propagation_evidence(void)
{
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_RIR_EFFECT_PROPAGATION,
            .boundary_index = SIZE_MAX,
            .provider_name = "PaymentZone",
            .subject_name = "fx",
            .fact_count = 0,
            .fallback_count = 0,
        },
    };
    AIRProgram air = {
        .evidence_nodes = evidence_nodes,
        .evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "RIR propagation evidence node 0 has no propagation facts") != NULL;
    free(error);
    return ok;
}

static bool
test_air_rejects_rir_propagation_counter_mismatch(void)
{
    AIREvidenceNode effect_nodes[] = {
        {
            .kind = AIR_EVIDENCE_RIR_EFFECT_PROPAGATION,
            .boundary_index = SIZE_MAX,
            .provider_name = "PaymentZone",
            .subject_name = "fx",
            .fact_count = 2,
            .fallback_count = 0,
        },
    };
    AIREvidenceNode relation_nodes[] = {
        {
            .kind = AIR_EVIDENCE_RIR_RELATION_PROPAGATION,
            .boundary_index = SIZE_MAX,
            .provider_name = "PaymentZone",
            .subject_name = "cart",
            .fact_count = 2,
            .fallback_count = 0,
        },
    };
    AIRProgram effect_air = {
        .evidence_nodes = effect_nodes,
        .evidence_count = 1,
        .has_rir_input = true,
        .rir_effect_propagation_evidence_count = 1,
    };
    AIRProgram relation_air = {
        .evidence_nodes = relation_nodes,
        .evidence_count = 1,
        .has_rir_input = true,
        .rir_relation_propagation_evidence_count = 1,
    };
    char *error = NULL;
    bool ok = !air_validate(&effect_air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error,
                  "RIR effect propagation evidence counter does not match evidence facts") != NULL;
    free(error);
    error = NULL;
    ok = ok
        && !air_validate(&relation_air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error,
                  "RIR relation propagation evidence counter does not match evidence facts") != NULL;
    free(error);
    return ok;
}
