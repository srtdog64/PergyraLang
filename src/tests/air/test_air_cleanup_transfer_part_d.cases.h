static bool
test_air_synthesizes_boundary_from_let_initializer(void)
{
    ASTNode intent_ast = { .line = 24, .column = 1 };
    ASTNode *step_ast = ast_create_intent_step("load");
    ASTNode *block = ast_create_block();
    ASTNode *let_decl = ast_create_let_declaration("content");
    ASTNode *call = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "LoadIntent", .ast = &intent_ast },
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
    AIRProgram *air;
    char *error = NULL;
    bool found_io = false;
    bool ok;

    if (step_ast == NULL || block == NULL || let_decl == NULL || call == NULL) {
        ast_destroy(step_ast);
        ast_destroy(block);
        ast_destroy(let_decl);
        ast_destroy(call);
        return false;
    }
    block->line = 25;
    let_decl->line = 26;
    call->line = 27;
    let_decl->data.let_decl.initializer = call;
    call = NULL;
    ast_add_statement(block, let_decl);
    let_decl = NULL;

    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        ast_destroy(block);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = block;
    step_ast->data.intent_step.on_expr_count = 1;
    block = NULL;

    air = air_synthesize(NULL, &dir, NULL, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_IO
                && boundary->source_name != NULL
                && strcmp(boundary->source_name, "ReadFile") == 0
                && boundary->ast != NULL
                && boundary->ast->line == 27) {
                found_io = true;
            }
        }
    }
    ok = air != NULL
        && air->boundary_count == 1
        && found_io;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    ast_destroy(block);
    ast_destroy(let_decl);
    ast_destroy(call);
    return ok;
}

static bool
test_air_world_boundary_rejects_mismatched_transfer_ast(void)
{
    ASTNode step_ast = { .type = AST_INTENT_STEP, .line = 31, .column = 5 };
    ASTNode unrelated_ast = { .type = AST_INTENT_STEP, .line = 44, .column = 9 };
    DIRNode nodes[] = {
        { .id = 1, .kind = DIR_NODE_INTENT, .name = "Checkout", .ast = NULL },
    };
    DIRIntentStep steps[] = {
        {
            .index = 0,
            .name = "Handoff",
            .transfer_from_alias = "cart",
            .transfer_to_alias = "payment",
            .ast = &step_ast,
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
    RIROp transfer_ops[] = {
        {
            .kind = RIR_OP_MOVE,
            .subject = "cart",
            .arg0 = "payment",
            .arg1 = "Handoff",
            .ast = &unrelated_ast,
        },
        {
            .kind = RIR_OP_CLAIM,
            .subject = "payment",
            .arg0 = "cart",
            .arg1 = "Handoff",
            .ast = &unrelated_ast,
        },
    };
    RIRScope scopes[] = {
        {
            .kind = RIR_SCOPE_INTENT,
            .name = "Checkout",
            .ops = transfer_ops,
            .op_count = 2,
        },
    };
    RIRProgram rir = {
        .scopes = scopes,
        .scope_count = 1,
    };
    char *error = NULL;
    AIRProgram *air = air_synthesize(NULL, &dir, &rir, &error);
    bool found_missing_transfer_drift = false;

    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
                && air->drifts[i].boundary_index < air->boundary_count
                && air->boundaries[air->drifts[i].boundary_index].kind == AIR_BOUNDARY_WORLD
                && strstr(air->drifts[i].message, "implementation boundary 'payment'") != NULL
                && strstr(air->drifts[i].message, "source_provenance=transfer") != NULL) {
                found_missing_transfer_drift = true;
                break;
            }
        }
    }

    bool ok = air != NULL
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_WORLD
        && air->boundaries[0].ast == &step_ast
        && air->boundaries[0].source_from_transfer
        && !air->boundaries[0].has_rir_boundary_evidence
        && found_missing_transfer_drift;
    air_destroy(air);
    free(error);
    return ok;
}

static bool
test_air_lowers_from_source_without_drift(void)
{
    const char *source =
        "subject Buyer { let hp: Int; action Pay(self) -> Void { return; } }\n"
        "ability Payable { func Pay() -> Void; }\n"
        "role BuyerPay for Buyer {\n"
        "    impl ability Payable { func Pay() -> Void { return; } }\n"
        "}\n"
        "effect PaymentEffect for bearer: Buyer { }\n"
        "zone PaymentZone {\n"
        "    subject slot buyer: Buyer\n"
        "    effect slot paymentFx: PaymentEffect\n"
        "    authority buyer requires Payable\n"
        "}\n"
        "intent Purchase(payment: PaymentZone, buyer: Buyer) {\n"
        "    step pay {\n"
        "        where: PaymentZone;\n"
        "        using: payment;\n"
        "        who: buyer;\n"
        "        requires: Payable;\n"
        "        authorized by: buyer;\n"
        "        causes: PaymentEffect;\n"
        "    }\n"
        "}\n";
    AIRProgram *air = lower_air_from_source(source);
    bool ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == 1
        && air->drift_count == 0
        && air->intents[0].ast != NULL
        && air->intents[0].ast->line > 0
        && air->boundaries[0].ast != NULL
        && air->boundaries[0].ast->line > 0
        && air->rir_authority_evidence_count > 0
        && air->boundaries[0].has_rir_boundary_evidence
        && air->boundaries[0].has_rir_authority_evidence;
    air_destroy(air);
    return ok;
}

static bool
test_air_synthesizes_spawn_boundary_from_step_ast(void)
{
    ASTNode intent_ast = { .line = 20, .column = 1 };
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
    char *error = NULL;
    AIRProgram *air;
    bool found_sync_drift = false;
    bool found_evidence_drift = false;
    bool ok;

    if (step_ast == NULL || call == NULL || spawn == NULL) {
        ast_destroy(step_ast);
        if (spawn == NULL)
            ast_destroy(call);
        ast_destroy(spawn);
        return false;
    }
    step_ast->line = 21;
    step_ast->column = 5;
    spawn->line = 22;
    spawn->column = 9;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(step_ast);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = spawn;
    step_ast->data.intent_step.on_expr_count = 1;

    air = air_synthesize(NULL, &dir, NULL, &error);
    if (air != NULL) {
        for (size_t i = 0; i < air->drift_count; i++) {
            if (air->drifts[i].kind == AIR_DRIFT_SYNC_ASYNC_CONFLICT)
                found_sync_drift = true;
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                found_evidence_drift = true;
        }
    }

    ok = air != NULL
        && air->intent_count == 1
        && air->boundary_count == 1
        && air->boundaries[0].kind == AIR_BOUNDARY_PARALLEL
        && strcmp(air->boundaries[0].source_name, "spawn") == 0
        && air->boundaries[0].ast == spawn
        && air->boundaries[0].sync_class == AIR_SYNC_ASYNC
        && found_sync_drift
        && found_evidence_drift;
    air_destroy(air);
    free(error);
    ast_destroy(step_ast);
    return ok;
}
