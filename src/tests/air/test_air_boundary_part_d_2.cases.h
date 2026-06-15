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
        "FileClose",
        "FileExists",
        "FileOpen",
        "FileRead",
        "FileWrite",
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
