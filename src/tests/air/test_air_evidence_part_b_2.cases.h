static bool
test_air_collects_hir_and_rir_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = NULL },
    };
    const char *authorized_by[] = { "shipper" };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "reserve",
            .where_type_name = "WarehouseZone",
            .authorized_by = authorized_by,
            .authorized_by_count = 1,
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
    HIRBasicBlock reserve_blocks[] = {
        { .id = 0, .terminator_kind = HIR_BLOCK_RETURN, .is_reachable = true },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_FUNCTION,
            .owner_name = "ShipOrder",
            .name = "reserve",
            .has_cfg = true,
            .cfg = {
                .blocks = reserve_blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIRFact facts[] = {
        {
            .kind = RIR_FACT_AUTHORITY,
            .name = "shipper",
            .resource_kind = RIR_RESOURCE_AUTHORITY_HANDLE,
            .state = RIR_STATE_AUTHORIZED,
        },
    };
    RIROp ops[] = {
        { .kind = RIR_OP_AUTHORIZE, .subject = "shipper" },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_ZONE,
            .owner_name = "ShipOrder",
            .name = "WarehouseZone",
            .facts = facts,
            .fact_count = 1,
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    const char *dir_owner_before = dir.nodes[0].name;
    const char *dir_step_before = dir.intents[0].steps[0].name;
    const char *dir_where_before = dir.intents[0].steps[0].where_type_name;
    const char *dir_authority_before = dir.intents[0].steps[0].authorized_by[0];
    const char *hir_owner_before = hir.routines[0].owner_name;
    const char *hir_name_before = hir.routines[0].name;
    RIRScopeKind rir_kind_before = rir.scopes[0].kind;
    const char *rir_owner_before = rir.scopes[0].owner_name;
    const char *rir_name_before = rir.scopes[0].name;
    RIROpKind rir_op_before = rir.scopes[0].ops[0].kind;
    const char *rir_op_subject_before = rir.scopes[0].ops[0].subject;
    RIRFactKind rir_fact_before = rir.scopes[0].facts[0].kind;
    const char *rir_fact_name_before = rir.scopes[0].facts[0].name;
    char *error = NULL;
    AIRProgram *air = air_synthesize(&hir, &dir, &rir, &error);
    bool ok = air != NULL
        && air->hir_routine_evidence_count == 1
        && air->hir_cfg_evidence_count == 1
        && air->rir_boundary_evidence_count == 1
        && air->rir_authority_evidence_count == 2
        && air->boundaries[0].has_hir_routine_evidence
        && air->boundaries[0].has_hir_cfg_evidence
        && air->boundaries[0].has_rir_boundary_evidence
        && air->boundaries[0].has_rir_authority_evidence
        && air->boundaries[0].hir_routine_evidence_name != NULL
        && strcmp(air->boundaries[0].hir_routine_evidence_name, "reserve") == 0
        && air->boundaries[0].rir_boundary_evidence_scope != NULL
        && strcmp(air->boundaries[0].rir_boundary_evidence_scope, "WarehouseZone") == 0
        && air->boundaries[0].rir_authority_evidence_name != NULL
        && strcmp(air->boundaries[0].rir_authority_evidence_name, "shipper") == 0
        && dir.node_count == 1
        && dir.intent_count == 1
        && dir.intents[0].step_count == 1
        && dir.nodes[0].name == dir_owner_before
        && dir.intents[0].steps[0].name == dir_step_before
        && dir.intents[0].steps[0].where_type_name == dir_where_before
        && dir.intents[0].steps[0].authorized_by[0] == dir_authority_before
        && hir.routine_count == 1
        && hir.routines[0].owner_name == hir_owner_before
        && hir.routines[0].name == hir_name_before
        && rir.scope_count == 1
        && rir.scopes[0].kind == rir_kind_before
        && rir.scopes[0].owner_name == rir_owner_before
        && rir.scopes[0].name == rir_name_before
        && rir.scopes[0].op_count == 1
        && rir.scopes[0].ops[0].kind == rir_op_before
        && rir.scopes[0].ops[0].subject == rir_op_subject_before
        && rir.scopes[0].fact_count == 1
        && rir.scopes[0].facts[0].kind == rir_fact_before
        && rir.scopes[0].facts[0].name == rir_fact_name_before;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_collects_all_rir_authority_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = NULL },
    };
    const char *authorized_by[] = { "shipper", "auditor" };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "reserve",
            .where_type_name = "WarehouseZone",
            .authorized_by = authorized_by,
            .authorized_by_count = 2,
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
        {
            .kind = RIR_FACT_AUTHORITY,
            .name = "auditor",
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
            .fact_count = 2,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool has_shipper = false;
    bool has_auditor = false;
    if (air != NULL) {
        for (size_t i = 0; i < air->evidence_count; i++) {
            const AIREvidenceNode *evidence = &air->evidence_nodes[i];
            if (evidence->kind != AIR_EVIDENCE_RIR_AUTHORITY)
                continue;
            has_shipper = has_shipper
                || strcmp(evidence->subject_name, "shipper") == 0;
            has_auditor = has_auditor
                || strcmp(evidence->subject_name, "auditor") == 0;
        }
    }
    bool ok = air != NULL
        && air->drift_count == 0
        && air->rir_authority_evidence_count == 2
        && has_shipper
        && has_auditor;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_rejects_mismatched_authority_evidence(void)
{
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = NULL },
    };
    const char *authorized_by[] = { "shipper" };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "reserve",
            .where_type_name = "WarehouseZone",
            .authorized_by = authorized_by,
            .authorized_by_count = 1,
            .where_inherited_from_action = true,
            .authorized_by_inherited_from_action = true,
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
            .name = "observer",
            .resource_kind = RIR_RESOURCE_AUTHORITY_HANDLE,
            .state = RIR_STATE_AUTHORIZED,
        },
    };
    RIROp ops[] = {
        { .kind = RIR_OP_AUTHORIZE, .subject = "observer" },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_ZONE,
            .owner_name = "ShipOrder",
            .name = "WarehouseZone",
            .facts = facts,
            .fact_count = 1,
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool found = false;
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && strstr(air->drifts[i].message,
                          "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING") != NULL
                && strstr(air->drifts[i].message,
                          "expected authority participant(s): shipper") != NULL
                && strstr(air->drifts[i].message,
                          "source_provenance=action-inherited") != NULL
                && strstr(air->drifts[i].message,
                          "authority_provenance=action-inherited") != NULL) {
                found = true;
                break;
            }
        }
    }
    bool ok = air != NULL
        && air->boundaries[0].has_rir_boundary_evidence
        && !air->boundaries[0].has_rir_authority_evidence
        && air->boundaries[0].source_from_action
        && air->boundaries[0].authority_from_action
        && air->drift_count >= 1
        && found;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_requires_all_authority_participant_evidence(void)
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
    const char *authority_names[] = { "shipper", "auditor" };
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
            .authority_name_count = 2,
            .has_rir_boundary_evidence = true,
            .rir_boundary_evidence_scope = "WarehouseZone",
            .has_rir_authority_evidence = true,
            .rir_authority_evidence_name = "shipper",
        },
    };
    AIREvidenceNode evidence_nodes[] = {
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
            .subject_name = "shipper",
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
        .has_rir_input = true,
    };
    char *error = NULL;
    bool verified = air_verify(&air, &error);
    bool found = false;
    for (size_t i = 0; verified && i < air.drift_count; i++) {
        if (air.drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
            && strstr(air.drifts[i].message, "participant 'auditor'") != NULL
            && strstr(air.drifts[i].message, "shipper, auditor") != NULL
            && strstr(air.drifts[i].message,
                      "every authorized participant") != NULL) {
            found = true;
            break;
        }
    }
    bool ok = verified && error == NULL && found;
    test_air_clear_stack_drifts(&air);
    free(error);
    return ok;
}

static bool
test_air_dump_prints_evidence_provenance(void)
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
            .has_hir_routine_evidence = true,
            .has_hir_cfg_evidence = true,
            .has_rir_boundary_evidence = true,
            .has_rir_authority_evidence = true,
            .hir_routine_evidence_name = "reserve",
            .rir_boundary_evidence_scope = "WarehouseZone",
            .rir_authority_evidence_name = "shipper",
        },
    };
    AIREvidenceNode evidence_nodes[] = {
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
            .provider_name = "reserve",
            .subject_name = "WarehouseZone",
            .fact_count = 1,
        },
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
            .subject_name = "shipper",
            .fact_count = 1,
        },
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
        .evidence_nodes = evidence_nodes,
        .evidence_count = 4,
        .strict_evidence = true,
        .has_hir_input = true,
        .has_rir_input = true,
    };
    char buffer[2048];
    FILE *out = tmpfile();
    size_t bytes;
    bool ok;

    if (out == NULL)
        return false;
    air_dump(&air, out);
    fflush(out);
    rewind(out);
    bytes = fread(buffer, 1, sizeof(buffer) - 1, out);
    buffer[bytes] = '\0';
    fclose(out);

    ok = strstr(buffer, "strict_evidence=yes hir_input=yes rir_input=yes") != NULL
        && strstr(buffer, "evidence hir=yes(reserve) hir_cfg=yes") != NULL
        && strstr(buffer, "rir_boundary=yes(WarehouseZone)") != NULL
        && strstr(buffer, "rir_authority=yes(shipper)") != NULL
        && strstr(buffer, "evidence_node[0] kind=hir_routine") != NULL
        && strstr(buffer, "provider=reserve subject=WarehouseZone facts=1 fallbacks=0") != NULL
        && strstr(buffer, "evidence_node[3] kind=rir_authority") != NULL
        && strstr(buffer, "provider=WarehouseZone subject=shipper facts=1 fallbacks=0") != NULL;
    return ok;
}
