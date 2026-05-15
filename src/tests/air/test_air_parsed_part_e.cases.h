static bool
test_air_synthesizes_stable_execution_boundary_set(void)
{
    ASTNode intent_ast = { .line = 50, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("coordinate");
    ASTNode *parallel = ast_create_parallel_block();
    ASTNode *async_block = ast_create_async_block();
    ASTNode *await_expr = ast_create_await_expression(ast_create_identifier("task"));
    ASTNode *task_group = ast_create_task_group(true);
    ASTNode *send = ast_create_channel_send(ast_create_identifier("ch"),
                                            ast_create_number("1"));
    ASTNode *recv = ast_create_channel_recv(ast_create_identifier("ch"));
    ASTNode *select_stmt = ast_create_select_statement();
    ASTNode *with_stmt = ast_create_with_statement();
    ASTNode *unsafe_block = ast_create_unsafe_block(ast_create_block());
    ASTNode *defer_stmt = ast_create_defer_statement(ast_create_block());
    ASTNode *pin_block = ast_create_block();
    ASTNode *event_subscribe = (ASTNode *)calloc(1, sizeof(ASTNode));
    ASTNode *event_unsubscribe = (ASTNode *)calloc(1, sizeof(ASTNode));
    ASTNode *nested_read = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "CoordinateWork", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "coordinate", .ast = step_ast },
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
    AIRProgram *air = NULL;
    char *error = NULL;
    bool found_parallel = false;
    bool found_async = false;
    bool found_await = false;
    bool found_task_group = false;
    bool found_send = false;
    bool found_recv = false;
    bool found_select = false;
    bool found_with = false;
    bool found_unsafe = false;
    bool found_defer = false;
    bool found_pin = false;
    bool found_event_subscribe = false;
    bool found_event_unsubscribe = false;
    bool found_nested_io = false;
    bool ok;

    if (step_ast == NULL || parallel == NULL || async_block == NULL || await_expr == NULL
        || task_group == NULL
        || send == NULL || recv == NULL || select_stmt == NULL
        || with_stmt == NULL || unsafe_block == NULL || defer_stmt == NULL
        || pin_block == NULL || event_subscribe == NULL
        || event_unsubscribe == NULL || nested_read == NULL) {
        ast_destroy(step_ast);
        ast_destroy(parallel);
        ast_destroy(async_block);
        ast_destroy(await_expr);
        ast_destroy(task_group);
        ast_destroy(send);
        ast_destroy(recv);
        ast_destroy(select_stmt);
        ast_destroy(with_stmt);
        ast_destroy(unsafe_block);
        ast_destroy(defer_stmt);
        ast_destroy(pin_block);
        ast_destroy(event_subscribe);
        ast_destroy(event_unsubscribe);
        ast_destroy(nested_read);
        return false;
    }
    parallel->line = 51;
    async_block->line = 52;
    await_expr->line = 53;
    task_group->line = 54;
    send->line = 55;
    recv->line = 56;
    select_stmt->line = 57;
    with_stmt->line = 58;
    unsafe_block->line = 59;
    defer_stmt->line = 60;
    pin_block->line = 61;
    event_subscribe->type = AST_EVENT_SUBSCRIBE;
    event_subscribe->line = 62;
    event_subscribe->data.event_op.event = ast_create_identifier("OnReady");
    event_subscribe->data.event_op.handler = ast_create_identifier("HandleReady");
    event_unsubscribe->type = AST_EVENT_UNSUBSCRIBE;
    event_unsubscribe->line = 63;
    event_unsubscribe->data.event_op.event = ast_create_identifier("OnReady");
    event_unsubscribe->data.event_op.handler = ast_create_identifier("HandleReady");
    nested_read->line = 64;
    pin_block->data.block.is_pin_block = true;
    with_stmt->data.with_stmt.body = ast_create_block();
    if (with_stmt->data.with_stmt.body == NULL) {
        ast_destroy(step_ast);
        ast_destroy(parallel);
        ast_destroy(async_block);
        ast_destroy(await_expr);
        ast_destroy(task_group);
        ast_destroy(send);
        ast_destroy(recv);
        ast_destroy(select_stmt);
        ast_destroy(with_stmt);
        ast_destroy(unsafe_block);
        ast_destroy(defer_stmt);
        ast_destroy(pin_block);
        ast_destroy(event_subscribe);
        ast_destroy(event_unsubscribe);
        ast_destroy(nested_read);
        return false;
    }
    ast_add_statement(with_stmt->data.with_stmt.body, nested_read);
    nested_read = NULL;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(13, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(parallel);
        ast_destroy(async_block);
        ast_destroy(await_expr);
        ast_destroy(task_group);
        ast_destroy(send);
        ast_destroy(recv);
        ast_destroy(select_stmt);
        ast_destroy(with_stmt);
        ast_destroy(unsafe_block);
        ast_destroy(defer_stmt);
        ast_destroy(pin_block);
        ast_destroy(event_subscribe);
        ast_destroy(event_unsubscribe);
        ast_destroy(nested_read);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = parallel;
    step_ast->data.intent_step.on_exprs[1] = async_block;
    step_ast->data.intent_step.on_exprs[2] = await_expr;
    step_ast->data.intent_step.on_exprs[3] = task_group;
    step_ast->data.intent_step.on_exprs[4] = send;
    step_ast->data.intent_step.on_exprs[5] = recv;
    step_ast->data.intent_step.on_exprs[6] = select_stmt;
    step_ast->data.intent_step.on_exprs[7] = with_stmt;
    step_ast->data.intent_step.on_exprs[8] = unsafe_block;
    step_ast->data.intent_step.on_exprs[9] = defer_stmt;
    step_ast->data.intent_step.on_exprs[10] = pin_block;
    step_ast->data.intent_step.on_exprs[11] = event_subscribe;
    step_ast->data.intent_step.on_exprs[12] = event_unsubscribe;
    step_ast->data.intent_step.on_expr_count = 13;

    air = air_synthesize(NULL, &dir, NULL, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_PARALLEL
                && strcmp(boundary->source_name, "parallel") == 0)
                found_parallel = true;
            if (boundary->kind == AIR_BOUNDARY_PARALLEL
                && strcmp(boundary->source_name, "async") == 0)
                found_async = true;
            if (boundary->kind == AIR_BOUNDARY_PARALLEL
                && strcmp(boundary->source_name, "await") == 0)
                found_await = true;
            if (boundary->kind == AIR_BOUNDARY_PARALLEL
                && strcmp(boundary->source_name, "task-group") == 0)
                found_task_group = true;
            if (boundary->kind == AIR_BOUNDARY_CHANNEL
                && strcmp(boundary->source_name, "channel-send") == 0)
                found_send = true;
            if (boundary->kind == AIR_BOUNDARY_CHANNEL
                && strcmp(boundary->source_name, "channel-recv") == 0)
                found_recv = true;
            if (boundary->kind == AIR_BOUNDARY_CHANNEL
                && strcmp(boundary->source_name, "select") == 0)
                found_select = true;
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "with") == 0)
                found_with = true;
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "unsafe") == 0)
                found_unsafe = true;
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "defer") == 0)
                found_defer = true;
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "pin") == 0)
                found_pin = true;
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "event-subscribe") == 0)
                found_event_subscribe = true;
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "event-unsubscribe") == 0)
                found_event_unsubscribe = true;
            if (boundary->kind == AIR_BOUNDARY_IO
                && boundary->ast != NULL
                && boundary->ast->line == 64
                && strcmp(boundary->source_name, "ReadFile") == 0)
                found_nested_io = true;
        }
    }
    ok = air != NULL
        && air->boundary_count == 14
        && found_parallel
        && found_async
        && found_await
        && found_task_group
        && found_send
        && found_recv
        && found_select
        && found_with
        && found_unsafe
        && found_defer
        && found_pin
        && found_event_subscribe
        && found_event_unsubscribe
        && found_nested_io;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_parsed_io_boundary_accepts_exact_rir_evidence(void)
{
    const char *source =
        "subject Loader { let hp: Int; }\n"
        "zone LoadZone {\n"
        "    subject slot loader: Loader\n"
        "}\n"
        "intent Load(load: LoadZone, loader: Loader) {\n"
        "    step read {\n"
        "        where: LoadZone;\n"
        "        using: load;\n"
        "        who: loader;\n"
        "        on: ReadFile(\"missing.txt\");\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(source);
    bool found_io = false;
    bool found_io_evidence = false;
    bool found_io_drift = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && air->drifts[i].boundary_index < air->boundary_count) {
                const AIRBoundaryNode *boundary =
                    &air->boundaries[air->drifts[i].boundary_index];
                if (air->drifts[i].boundary_index < air->boundary_count) {
                    if (boundary->kind == AIR_BOUNDARY_IO
                        && boundary->source_name != NULL
                        && strcmp(boundary->source_name, "ReadFile") == 0) {
                        found_io_drift = true;
                    }
                }
            }
        }
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_IO
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "ReadFile") == 0
                && boundary->sync_class == AIR_SYNC_EITHER
                && boundary->ast != NULL
                && boundary->ast->type == AST_CALL
                && boundary->ast->line > 0) {
                found_io = true;
                found_io_evidence = boundary->has_hir_cfg_evidence
                    && boundary->has_rir_boundary_evidence;
            }
        }
    }

    bool ok = air != NULL
        && air->boundary_count >= 2
        && found_io
        && found_io_evidence
        && !found_io_drift;
    air_destroy(air);
    return ok;
}

static bool
test_air_parsed_transfer_emits_zone_and_world_boundaries(void)
{
    const char *source =
        "subject Buyer { let hp: Int; action Promote(self) -> Void { hp = hp + 1; } }\n"
        "zone CartZone {\n"
        "    subject slot buyer: Buyer\n"
        "    authority buyer\n"
        "}\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "    authority buyer\n"
        "}\n"
        "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
        "    step Handoff {\n"
        "        transfer: cart -> payment;\n"
        "        who: buyer;\n"
        "        authorized by: buyer;\n"
        "        on: buyer.Promote();\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(source);
    bool found_zone = false;
    bool found_world = false;
    bool found_zone_evidence = false;
    bool found_world_evidence = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            if (air->boundaries[i].kind == AIR_BOUNDARY_ZONE
                && strcmp(air->boundaries[i].source_name, "PaymentZone") == 0) {
                found_zone = true;
                found_zone_evidence = air->boundaries[i].has_rir_boundary_evidence
                    && air->boundaries[i].source_from_transfer
                    && air->boundaries[i].has_rir_authority_evidence
                    && air->boundaries[i].rir_boundary_evidence_scope != NULL
                    && air->boundaries[i].rir_authority_evidence_name != NULL
                    && strcmp(air->boundaries[i].rir_authority_evidence_name, "buyer") == 0;
            }
            if (air->boundaries[i].kind == AIR_BOUNDARY_WORLD
                && strcmp(air->boundaries[i].source_name, "payment") == 0) {
                found_world = true;
                found_world_evidence = air->boundaries[i].has_rir_boundary_evidence
                    && air->boundaries[i].source_from_transfer
                    && air->boundaries[i].has_rir_authority_evidence
                    && air->boundaries[i].rir_boundary_evidence_scope != NULL
                    && air->boundaries[i].rir_authority_evidence_name != NULL
                    && strcmp(air->boundaries[i].rir_authority_evidence_name, "buyer") == 0;
            }
        }
    }

    bool ok = air != NULL
        && air->drift_count == 0
        && air->boundary_count >= 2
        && found_zone
        && found_world
        && found_zone_evidence
        && found_world_evidence;
    air_destroy(air);
    return ok;
}

static void
test_air_remove_boundary_evidence(AIRProgram *air,
                                  size_t boundary_index,
                                  AIREvidenceKind kind)
{
    size_t write = 0;
    size_t removed = 0;

    if (air == NULL)
        return;
    for (size_t read = 0; read < air->evidence_count; read++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[read];
        if (evidence->kind == kind && evidence->boundary_index == boundary_index) {
            removed++;
            continue;
        }
        if (write != read)
            air->evidence_nodes[write] = air->evidence_nodes[read];
        write++;
    }
    air->evidence_count = write;
    if (kind == AIR_EVIDENCE_RIR_BOUNDARY) {
        air->rir_boundary_evidence_count =
            air->rir_boundary_evidence_count > removed
                ? air->rir_boundary_evidence_count - removed
                : 0;
    } else if (kind == AIR_EVIDENCE_RIR_AUTHORITY) {
        air->rir_authority_evidence_count =
            air->rir_authority_evidence_count > removed
                ? air->rir_authority_evidence_count - removed
                : 0;
    }
}

static bool
test_air_parsed_transfer_reports_world_missing_transfer_evidence(void)
{
    const char *source =
        "subject Buyer { let hp: Int; action Promote(self) -> Void { hp = hp + 1; } }\n"
        "zone CartZone {\n"
        "    subject slot buyer: Buyer\n"
        "    authority buyer\n"
        "}\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "    authority buyer\n"
        "}\n"
        "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
        "    step Handoff {\n"
        "        transfer: cart -> payment;\n"
        "        who: buyer;\n"
        "        authorized by: buyer;\n"
        "        on: buyer.Promote();\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(source);
    bool found_world = false;
    bool found_world_drift = false;
    bool verified = false;
    char *error = NULL;

    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_WORLD
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "payment") == 0
                && boundary->source_from_transfer
                && boundary->ast != NULL
                && boundary->ast->line > 0) {
                found_world = true;
                boundary->has_rir_boundary_evidence = false;
                boundary->has_rir_authority_evidence = false;
                boundary->rir_boundary_evidence_scope = NULL;
                boundary->rir_authority_evidence_name = NULL;
                test_air_remove_boundary_evidence(air, i, AIR_EVIDENCE_RIR_AUTHORITY);
                test_air_remove_boundary_evidence(air, i, AIR_EVIDENCE_RIR_BOUNDARY);
                break;
            }
        }
        verified = air_verify(air, &error);
        for (size_t i = 0; i < air->drift_count; i++) {
            const AIRDrift *drift = &air->drifts[i];
            if (drift->kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && drift->boundary_index < air->boundary_count
                && air->boundaries[drift->boundary_index].kind == AIR_BOUNDARY_WORLD
                && drift->message != NULL
                && strstr(drift->message, "source_provenance=transfer") != NULL
                && strstr(drift->message, "payment") != NULL) {
                found_world_drift = true;
            }
        }
    }

    bool ok = air != NULL
        && found_world
        && verified
        && found_world_drift;
    free(error);
    air_destroy(air);
    return ok;
}

static bool
test_air_parsed_transfer_reports_zone_missing_authority_evidence(void)
{
    const char *source =
        "subject Buyer { let hp: Int; action Promote(self) -> Void { hp = hp + 1; } }\n"
        "zone CartZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "}\n"
        "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
        "    step Handoff {\n"
        "        where: PaymentZone;\n"
        "        using: payment;\n"
        "        transfer: cart -> payment;\n"
        "        who: buyer;\n"
        "        authorized by: buyer;\n"
        "        on: buyer.Promote();\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(source);
    bool found_zone = false;
    bool found_zone_authority_drift = false;
    bool found_world_transfer_evidence = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_ZONE
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "PaymentZone") == 0
                && boundary->has_rir_boundary_evidence
                && !boundary->has_rir_authority_evidence) {
                found_zone = true;
            }
            if (boundary->kind == AIR_BOUNDARY_WORLD
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "payment") == 0
                && boundary->has_rir_boundary_evidence
                && boundary->source_from_transfer) {
                found_world_transfer_evidence = true;
            }
        }
        for (size_t i = 0; i < air->drift_count; i++) {
            const AIRDrift *drift = &air->drifts[i];
            if (drift->kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && drift->boundary_index < air->boundary_count
                && air->boundaries[drift->boundary_index].kind == AIR_BOUNDARY_ZONE
                && drift->message != NULL
                && strstr(drift->message, "expected authority participant(s): buyer") != NULL) {
                found_zone_authority_drift = true;
            }
        }
    }

    bool ok = air != NULL
        && air->drift_count >= 1
        && found_zone
        && found_zone_authority_drift
        && found_world_transfer_evidence;
    air_destroy(air);
    return ok;
}

static bool
test_air_parsed_on_receiver_action_contract_provenance(void)
{
    const char *source =
        "ability Prepared {\n"
        "    func Ready() -> Bool;\n"
        "}\n"
        "subject Hero {\n"
        "    let hp: Int;\n"
        "    action Protect(self, healer: Healer) -> Void\n"
        "        requires Prepared\n"
        "        within BattleZone\n"
        "        authorized by healer\n"
        "        causes Protected {\n"
        "        self.hp = self.hp + 1;\n"
        "    }\n"
        "}\n"
        "role HeroPrepared for Hero {\n"
        "    impl ability Prepared {\n"
        "        func Ready() -> Bool { return true; }\n"
        "    }\n"
        "}\n"
        "subject Healer { let level: Int; }\n"
        "effect Protected for bearer: Hero {\n"
        "    subject slot bearer: Hero\n"
        "}\n"
        "zone BattleZone {\n"
        "    subject slot hero: Hero\n"
        "    subject slot healer: Healer\n"
        "    effect slot protected: Protected\n"
        "    authority healer\n"
        "}\n"
        "intent Rescue(battle: BattleZone, hero: Hero, healer: Healer) {\n"
        "    step Verify {\n"
        "        on: hero.Protect(healer);\n"
        "        expect: true;\n"
        "    }\n"
        "    success: true;\n"
        "    failure: false;\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(source);
    bool found_intent = false;
    bool found_boundary = false;
    bool found_rir_authority = false;
    bool found_rir_effect = false;
    bool found_dag_ability = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->intent_count; i++) {
            if (air->intents[i].who_from_on_receiver
                && air->intents[i].requires_from_action
                && air->intents[i].causes_from_action) {
                found_intent = true;
                break;
            }
        }
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_ZONE
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "BattleZone") == 0
                && boundary->source_from_action
                && boundary->authority_from_action
                && !boundary->authority_from_zone
                && boundary->authority_name_count == 1
                && strcmp(boundary->authority_names[0], "healer") == 0
                && boundary->has_rir_authority_evidence
                && boundary->rir_authority_evidence_name != NULL
                && strcmp(boundary->rir_authority_evidence_name, "healer") == 0) {
                found_boundary = true;
            }
        }
        for (size_t i = 0; i < air->evidence_count; i++) {
            const AIREvidenceNode *evidence = &air->evidence_nodes[i];
            if (evidence->kind == AIR_EVIDENCE_RIR_AUTHORITY
                && evidence->subject_name != NULL
                && strcmp(evidence->subject_name, "healer") == 0) {
                found_rir_authority = true;
            }
            if (evidence->kind == AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
                && evidence->subject_name != NULL
                && strcmp(evidence->subject_name, "protected") == 0) {
                found_rir_effect = true;
            }
            if (evidence->kind == AIR_EVIDENCE_DAG_ABILITY
                && evidence->subject_name != NULL
                && strcmp(evidence->subject_name, "ability-consumers") == 0
                && evidence->fact_count > 0
                && evidence->fallback_count == 0) {
                found_dag_ability = true;
            }
        }
    }

    bool ok = air != NULL
        && air->drift_count == 0
        && air->dag_ability_evidence_count == 1
        && air->rir_effect_propagation_required_count == 1
        && air->rir_effect_propagation_evidence_count == 1
        && found_intent
        && found_boundary
        && found_rir_authority
        && found_rir_effect
        && found_dag_ability;
    air_destroy(air);
    return ok;
}
