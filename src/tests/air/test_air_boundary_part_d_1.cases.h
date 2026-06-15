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
