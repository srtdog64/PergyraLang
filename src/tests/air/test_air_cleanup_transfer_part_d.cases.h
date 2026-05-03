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
