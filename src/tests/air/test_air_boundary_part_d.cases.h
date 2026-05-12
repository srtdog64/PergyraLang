static bool
test_air_synthesizes_boundary_from_event_handler_payload(void)
{
    ASTNode intent_ast = { .line = 28, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("subscribe");
    ASTNode *event_subscribe = (ASTNode *)calloc(1, sizeof(ASTNode));
    ASTNode *event_name = ast_create_identifier("OnLoaded");
    ASTNode *handler = (ASTNode *)calloc(1, sizeof(ASTNode));
    ASTNode *call = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "SubscribeIntent", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "subscribe", .ast = step_ast },
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
    AIRProgram *air;
    char *error = NULL;
    bool found_event = false;
    bool found_io = false;
    bool ok;

    if (step_ast == NULL || event_subscribe == NULL || event_name == NULL
        || handler == NULL || call == NULL) {
        ast_destroy(step_ast);
        ast_destroy(event_subscribe);
        ast_destroy(event_name);
        ast_destroy(handler);
        ast_destroy(call);
        return false;
    }

    event_subscribe->type = AST_EVENT_SUBSCRIBE;
    event_subscribe->line = 29;
    event_subscribe->column = 9;
    event_subscribe->data.event_op.event = event_name;
    event_name = NULL;

    handler->type = AST_LAMBDA_EXPR;
    handler->line = 30;
    handler->column = 13;
    call->line = 31;
    call->column = 17;
    handler->data.lambda_expr.body = call;
    call = NULL;
    event_subscribe->data.event_op.handler = handler;
    handler = NULL;

    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(event_subscribe);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = event_subscribe;
    step_ast->data.intent_step.on_expr_count = 1;
    event_subscribe = NULL;

    air = air_synthesize(NULL, &dir, NULL, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "event-subscribe") == 0
                && boundary->ast != NULL
                && boundary->ast->line == 29) {
                found_event = true;
            }
            if (boundary->kind == AIR_BOUNDARY_IO
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "ReadFile") == 0
                && boundary->ast != NULL
                && boundary->ast->line == 31) {
                found_io = true;
            }
        }
    }

    ok = air != NULL
        && air->boundary_count == 2
        && found_event
        && found_io;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    ast_destroy(event_subscribe);
    ast_destroy(event_name);
    ast_destroy(handler);
    ast_destroy(call);
    return ok;
}

static bool
test_air_rejects_unmatched_top_level_intent_hir_evidence(void)
{
    ASTNode intent_ast = { .line = 24, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("dispatch");
    ASTNode *call = ast_create_call(ast_create_identifier("Worker"));
    ASTNode *spawn = ast_create_spawn_expression(call);
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ShipOrder", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "dispatch", .ast = step_ast },
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
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .name = "OtherIntent",
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        { .kind = RIR_OP_SPAWN, .subject = "spawn", .ast = spawn },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "spawn",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air;
    bool found_hir_routine_drift = false;
    bool found_hir_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || call == NULL || spawn == NULL) {
        ast_destroy(step_ast);
        if (spawn == NULL)
            ast_destroy(call);
        ast_destroy(spawn);
        return false;
    }
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = spawn;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && strstr(air->drifts[i].message,
                          "AIR boundary has no matching HIR routine evidence") != NULL) {
                found_hir_routine_drift = true;
            }
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && strstr(air->drifts[i].message,
                          "AIR implementation boundary has no matching HIR CFG evidence") != NULL) {
                found_hir_evidence_drift = true;
            }
        }
    }

    ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_PARALLEL
        && !air->boundaries[0].has_hir_routine_evidence
        && air->boundaries[0].has_rir_boundary_evidence
        && found_hir_routine_drift
        && found_hir_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_synthesizes_io_boundary_without_sync_drift(void)
{
    ASTNode intent_ast = { .line = 30, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("load");
    ASTNode *call = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "LoadConfig", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "load", .ast = step_ast },
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
    char *error = NULL;
    AIRProgram *air;
    bool found_sync_drift = false;
    bool ok;

    if (step_ast == NULL || call == NULL) {
        ast_destroy(step_ast);
        ast_destroy(call);
        return false;
    }
    step_ast->line = 31;
    step_ast->column = 5;
    call->line = 32;
    call->column = 9;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(call);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = call;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(NULL, &dir, NULL, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_SYNC_ASYNC_CONFLICT)
                found_sync_drift = true;
        }
    }

    ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_IO
        && strcmp(air->boundaries[0].source_name, "ReadFile") == 0
        && air->boundaries[0].ast == call
        && air->boundaries[0].sync_class == AIR_SYNC_EITHER
        && !found_sync_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_await_boundary_accepts_exact_rir_evidence(void)
{
    ASTNode intent_ast = { .line = 34, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("join");
    ASTNode *await_expr = ast_create_await_expression(ast_create_identifier("task"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "JoinWork", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "join", .ast = step_ast },
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
    ASTNode *join_statements[] = { await_expr };
    HIRBasicBlock join_blocks[] = {
        {
            .id = 0,
            .statements = join_statements,
            .statement_count = 1,
            .terminator_kind = HIR_BLOCK_RETURN,
            .is_reachable = true,
        },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .owner_name = "JoinWork",
            .name = "join",
            .has_cfg = true,
            .cfg = {
                .blocks = join_blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        {
            .kind = RIR_OP_AWAIT_REMOTE,
            .subject = "task",
            .ast = await_expr,
        },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .owner_name = "JoinWork",
            .name = "join",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air;
    bool found_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || await_expr == NULL) {
        ast_destroy(step_ast);
        ast_destroy(await_expr);
        return false;
    }
    await_expr->line = 35;
    await_expr->column = 9;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(await_expr);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = await_expr;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                found_evidence_drift = true;
        }
    }

    ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_PARALLEL
        && strcmp(air->boundaries[0].source_name, "await") == 0
        && air->boundaries[0].ast == await_expr
        && air->boundaries[0].has_hir_routine_evidence
        && air->boundaries[0].has_hir_cfg_evidence
        && air->boundaries[0].has_rir_boundary_evidence
        && !found_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_await_boundary_rejects_generic_rir_scope_evidence(void)
{
    ASTNode intent_ast = { .line = 36, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("join");
    ASTNode *await_expr = ast_create_await_expression(ast_create_identifier("task"));
    ASTNode unrelated_await_ast = { .type = AST_AWAIT_EXPR, .line = 37, .column = 9 };
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "JoinWork", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "join", .ast = step_ast },
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
    ASTNode *join_statements[] = { await_expr };
    HIRBasicBlock join_blocks[] = {
        {
            .id = 0,
            .statements = join_statements,
            .statement_count = 1,
            .terminator_kind = HIR_BLOCK_RETURN,
            .is_reachable = true,
        },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .owner_name = "JoinWork",
            .name = "join",
            .has_cfg = true,
            .cfg = {
                .blocks = join_blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        {
            .kind = RIR_OP_AWAIT_REMOTE,
            .subject = "task",
            .ast = &unrelated_await_ast,
        },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .owner_name = "JoinWork",
            .name = "await",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air;
    bool found_rir_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || await_expr == NULL) {
        ast_destroy(step_ast);
        ast_destroy(await_expr);
        return false;
    }
    await_expr->line = 36;
    await_expr->column = 9;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(await_expr);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = await_expr;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && air->drifts[i].message != NULL
                && strstr(air->drifts[i].message, "no matching RIR boundary evidence") != NULL) {
                found_rir_evidence_drift = true;
                break;
            }
        }
    }

    ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_PARALLEL
        && strcmp(air->boundaries[0].source_name, "await") == 0
        && air->boundaries[0].ast == await_expr
        && air->boundaries[0].has_hir_routine_evidence
        && air->boundaries[0].has_hir_cfg_evidence
        && !air->boundaries[0].has_rir_boundary_evidence
        && found_rir_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_parallel_boundary_requires_rir_source_provenance(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "JoinWork",
            .step_name = "join",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_PARALLEL,
            .owner_name = "JoinWork",
            .source_name = "await",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
        },
    };
    RIROp ops[] = {
        {
            .kind = RIR_OP_AWAIT_REMOTE,
            .subject = "task",
        },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .owner_name = "JoinWork",
            .name = "await",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = air_collect_rir_evidence(&air, &rir, &error)
        && air_validate(&air, &error)
        && air.has_rir_input
        && !air.boundaries[0].has_rir_boundary_evidence
        && air.rir_boundary_evidence_count == 0
        && air.evidence_count == 0;
    free(error);
    return ok;
}

static bool
test_air_hir_cfg_requires_boundary_source_provenance(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "DispatchWork",
            .step_name = "dispatch",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_PARALLEL,
            .owner_name = "DispatchWork",
            .source_name = "spawn",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
        },
    };
    HIRBasicBlock blocks[] = {
        {
            .id = 0,
            .terminator_kind = HIR_BLOCK_RETURN,
            .is_reachable = true,
        },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .owner_name = "DispatchWork",
            .name = "dispatch",
            .has_cfg = true,
            .cfg = {
                .blocks = blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    AIRProgram air = {
        .intents = intents,
        .intent_count = 1,
        .boundaries = boundaries,
        .boundary_count = 1,
    };
    char *error = NULL;
    bool ok = air_collect_hir_evidence(&air, &hir, &error)
        && air_validate(&air, &error)
        && air.boundaries[0].has_hir_routine_evidence
        && !air.boundaries[0].has_hir_cfg_evidence
        && air.hir_routine_evidence_count == 1
        && air.hir_cfg_evidence_count == 0
        && air.evidence_count == 1
        && air.evidence_nodes[0].kind == AIR_EVIDENCE_HIR_ROUTINE;
    free(error);
    return ok;
}

static bool
test_air_rejects_hir_cfg_evidence_without_source_provenance(void)
{
    AIRIntentNode intents[] = {
        {
            .intent_owner = "DispatchWork",
            .step_name = "dispatch",
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
            .failure_class = AIR_FAILURE_RECOVERABLE,
        },
    };
    AIRBoundaryNode boundaries[] = {
        {
            .kind = AIR_BOUNDARY_PARALLEL,
            .owner_name = "DispatchWork",
            .source_name = "spawn",
            .intent_index = 0,
            .step_index = 0,
            .sync_class = AIR_SYNC_ASYNC,
        },
    };
    AIREvidenceNode evidence_nodes[] = {
        {
            .kind = AIR_EVIDENCE_HIR_ROUTINE,
            .boundary_index = 0,
            .provider_name = "dispatch",
            .subject_name = "spawn",
            .fact_count = 1,
        },
        {
            .kind = AIR_EVIDENCE_HIR_CFG,
            .boundary_index = 0,
            .provider_name = "dispatch",
            .subject_name = "spawn",
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
    };
    char *error = NULL;
    bool ok = !air_validate(&air, &error)
        && error != NULL
        && strstr(error, "PGY_AIR_INVARIANT_INVALID") != NULL
        && strstr(error, "HIR CFG evidence node") != NULL
        && strstr(error, "no source AST provenance") != NULL;
    free(error);
    return ok;
}

static bool
test_air_channel_boundary_accepts_exact_rir_op_evidence(void)
{
    ASTNode intent_ast = { .line = 36, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("coordinate");
    ASTNode *send = ast_create_channel_send(ast_create_identifier("ch"),
                                            ast_create_number("1"));
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
    ASTNode *statements[] = { send };
    HIRBasicBlock blocks[] = {
        {
            .id = 0,
            .statements = statements,
            .statement_count = 1,
            .terminator_kind = HIR_BLOCK_RETURN,
            .is_reachable = true,
        },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .owner_name = "CoordinateWork",
            .name = "coordinate",
            .has_cfg = true,
            .cfg = {
                .blocks = blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        { .kind = RIR_OP_CHANNEL_SEND, .subject = "ch", .arg0 = "1", .ast = send },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "CoordinateWork",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = NULL;
    bool found_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || send == NULL) {
        ast_destroy(step_ast);
        ast_destroy(send);
        return false;
    }
    send->line = 37;
    send->column = 9;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(send);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = send;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                found_evidence_drift = true;
        }
    }

    ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_CHANNEL
        && strcmp(air->boundaries[0].source_name, "channel-send") == 0
        && air->boundaries[0].has_hir_cfg_evidence
        && air->boundaries[0].has_rir_boundary_evidence
        && !found_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_hir_evidence_accepts_nested_execution_boundary_ast(void)
{
    ASTNode intent_ast = { .line = 38, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("read");
    ASTNode *with_stmt = ast_create_with_statement();
    ASTNode *read_call = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "NestedIO", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "read", .ast = step_ast },
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
    ASTNode *statements[] = { with_stmt };
    HIRBasicBlock blocks[] = {
        {
            .id = 0,
            .statements = statements,
            .statement_count = 1,
            .terminator_kind = HIR_BLOCK_RETURN,
            .is_reachable = true,
        },
    };
    HIRRoutine routines[] = {
        {
            .kind = HIR_TOPLEVEL_INTENT,
            .owner_name = "NestedIO",
            .name = "read",
            .has_cfg = true,
            .cfg = {
                .blocks = blocks,
                .block_count = 1,
                .entry_block = 0,
            },
        },
    };
    HIRProgram hir = {
        .routines = routines,
        .routine_count = 1,
    };
    RIROp ops[] = {
        { .kind = RIR_OP_IO, .subject = "ReadFile", .ast = read_call },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "NestedIO",
            .ops = ops,
            .op_count = 1,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = NULL;
    bool found_io = false;
    bool found_with = false;
    bool found_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || with_stmt == NULL || read_call == NULL) {
        ast_destroy(step_ast);
        ast_destroy(with_stmt);
        ast_destroy(read_call);
        return false;
    }
    with_stmt->line = 39;
    with_stmt->column = 9;
    read_call->line = 40;
    read_call->column = 13;
    with_stmt->data.with_stmt.body = ast_create_block();
    if (with_stmt->data.with_stmt.body == NULL) {
        ast_destroy(step_ast);
        ast_destroy(with_stmt);
        ast_destroy(read_call);
        return false;
    }
    ast_add_statement(with_stmt->data.with_stmt.body, read_call);
    read_call = NULL;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(with_stmt);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = with_stmt;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_EXECUTION
                && strcmp(boundary->source_name, "with") == 0
                && boundary->has_hir_cfg_evidence) {
                found_with = true;
            }
            if (boundary->kind == AIR_BOUNDARY_IO
                && strcmp(boundary->source_name, "ReadFile") == 0
                && boundary->has_hir_cfg_evidence
                && boundary->has_rir_boundary_evidence) {
                found_io = true;
            }
        }
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                found_evidence_drift = true;
        }
    }

    ok = air != NULL
        && air->boundary_count == 2
        && found_with
        && found_io
        && !found_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

static bool
test_air_synthesizes_stable_io_boundary_builtin_set(void)
{
    const char *io_names[] = {
        "FileOpen",
        "FileRead",
        "FileWrite",
        "FileClose",
        "ReadFile",
        "WriteFile",
        "Input",
        "ReadLine",
        "Now",
        "Sleep",
    };
    ASTNode intent_ast = { .line = 40, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("touch");
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "ExternalEffect", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "touch", .ast = step_ast },
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
    char *error = NULL;
    AIRProgram *air;
    bool ok = true;

    if (step_ast == NULL)
        return false;
    step_ast->line = 41;
    step_ast->column = 5;
    step_ast->data.intent_step.on_exprs =
        (ASTNode **)calloc(sizeof(io_names) / sizeof(io_names[0]), sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        return false;
    }
    step_ast->data.intent_step.on_expr_count = sizeof(io_names) / sizeof(io_names[0]);
    for (size_t i = 0; i < sizeof(io_names) / sizeof(io_names[0]); i++) {
        ASTNode *call = ast_create_call(ast_create_identifier(io_names[i]));
        if (call == NULL) {
            ast_destroy(step_ast);
            return false;
        }
        call->line = 42 + (int)i;
        call->column = 9;
        step_ast->data.intent_step.on_exprs[i] = call;
    }

    air = air_synthesize(NULL, &dir, NULL, &error);
    ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == sizeof(io_names) / sizeof(io_names[0]);
    if (ok) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            ok = air->boundaries[i].kind == AIR_BOUNDARY_IO
                && air->boundaries[i].source_name != NULL
                && strcmp(air->boundaries[i].source_name, io_names[i]) == 0
                && air->boundaries[i].sync_class == AIR_SYNC_EITHER
                && air->boundaries[i].ast == step_ast->data.intent_step.on_exprs[i];
            if (!ok)
                break;
        }
    }
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}

