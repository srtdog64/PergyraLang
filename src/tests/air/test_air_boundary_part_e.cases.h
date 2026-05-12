static bool
test_air_hir_evidence_accepts_loop_condition_boundary_ast(void)
{
    ASTNode intent_ast = { .line = 42, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("poll");
    ASTNode *while_stmt = ast_create_while_loop();
    ASTNode *read_call = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "LoopIO", .ast = &intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "poll", .ast = step_ast },
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
    ASTNode *statements[] = { while_stmt };
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
            .owner_name = "LoopIO",
            .name = "poll",
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
            .name = "LoopIO",
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
    bool found_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || while_stmt == NULL || read_call == NULL) {
        ast_destroy(step_ast);
        ast_destroy(while_stmt);
        ast_destroy(read_call);
        return false;
    }
    while_stmt->line = 43;
    while_stmt->column = 9;
    read_call->line = 43;
    read_call->column = 16;
    while_stmt->data.while_loop.condition = read_call;
    while_stmt->data.while_loop.body = ast_create_block();
    if (while_stmt->data.while_loop.body == NULL) {
        ast_destroy(step_ast);
        ast_destroy(while_stmt);
        return false;
    }
    read_call = NULL;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(while_stmt);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = while_stmt;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(&hir, &dir, &rir, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
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
        && air->boundary_count == 1
        && found_io
        && !found_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}
