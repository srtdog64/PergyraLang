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
    bool ok = air_collect_rir_evidence(&air, &rir, NULL, NULL, &error)
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
            .provider_kind = AIR_EVIDENCE_PROVIDER_HIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_ROUTINE,
            .boundary_index = 0,
            .provider_name = "dispatch",
            .subject_name = "spawn",
            .fact_count = 1,
        },
        {
            .kind = AIR_EVIDENCE_HIR_CFG,
            .provider_kind = AIR_EVIDENCE_PROVIDER_HIR,
            .subject_kind = AIR_EVIDENCE_SUBJECT_CFG,
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
    ASTNode *intent_ast = ast_create_intent_declaration("NestedIO");
    ASTNode *step_ast = ast_create_intent_step("read");
    ASTNode *with_stmt = ast_create_with_statement();
    ASTNode *read_call = ast_create_call(ast_create_identifier("ReadFile"));
    DIRNode nodes[] = {
        { .id = 0, .kind = DIR_NODE_INTENT, .name = "NestedIO", .ast = intent_ast },
    };
    DIRIntentStep steps[] = {
        { .index = 0, .name = "read", .ast = step_ast },
    };
    DIRIntentInfo intents[] = {
        { .node_id = 0, .steps = steps, .step_count = 1 },
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

    if (intent_ast == NULL || step_ast == NULL || with_stmt == NULL || read_call == NULL) {
        ast_destroy(intent_ast);
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
        ast_destroy(intent_ast);
        ast_destroy(step_ast);
        ast_destroy(with_stmt);
        ast_destroy(read_call);
        return false;
    }
    ast_add_statement(with_stmt->data.with_stmt.body, read_call);
    read_call = NULL;
    step_ast->data.intent_step.on_exprs = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (step_ast->data.intent_step.on_exprs == NULL) {
        ast_destroy(intent_ast);
        ast_destroy(step_ast);
        ast_destroy(with_stmt);
        return false;
    }
    step_ast->data.intent_step.on_exprs[0] = with_stmt;
    step_ast->data.intent_step.on_expr_count = 1;
    intent_ast->line = 38;
    intent_ast->column = 1;
    intent_ast->data.intent_decl.steps = (ASTNode **)calloc(1, sizeof(ASTNode *));
    if (intent_ast->data.intent_decl.steps == NULL) {
        ast_destroy(intent_ast);
        ast_destroy(step_ast);
        return false;
    }
    intent_ast->data.intent_decl.steps[0] = step_ast;
    intent_ast->data.intent_decl.step_count = 1;
    intent_ast->data.intent_decl.step_capacity = 1;
    if (!ast_assign_stable_ids(intent_ast) || ast_node_stable_id(intent_ast) == 0) {
        ast_destroy(intent_ast);
        return false;
    }
    nodes[0].source_syntax_id = ast_node_stable_id(intent_ast);
    intents[0].ast = intent_ast;
    scopes[0].ast = intent_ast;
    scopes[0].source_syntax_id = ast_node_stable_id(intent_ast);

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
    air = NULL;
    free(error); error = NULL;

    /* Each original projection and the exact IO row is independently required.
       Restore all three before measuring the next missing-fact combination. */
    size_t saved_node_count = dir.node_count;
    size_t saved_intent_count = dir.intent_count;
    size_t saved_scope_op_count = scopes[0].op_count;
    bool dir_masks_denied = true, dir_masks_restored = true;
    bool dir_pair_denied = false, dir_pair_restored = false;
    bool dir_io_pair_denied = false, dir_io_pair_restored = false;
    for (unsigned mask = 1; mask < 8; mask++) {
        dir.node_count = (mask & 1u) != 0 ? 0 : saved_node_count;
        dir.intent_count = (mask & 2u) != 0 ? 0 : saved_intent_count;
        scopes[0].op_count = (mask & 4u) != 0 ? 0 : saved_scope_op_count;
        air = air_synthesize(&hir, &dir, &rir, &error);
        bool synthesized = air != NULL;
        bool collected = air != NULL && air->has_rir_input;
        bool verified = collected && air_verify(air, &error) &&
            air_drift_count(air) == 0;
        bool denied = !verified;
        air_destroy(air); air = NULL;
        free(error); error = NULL;
        dir.node_count = saved_node_count;
        dir.intent_count = saved_intent_count;
        scopes[0].op_count = saved_scope_op_count;
        air = air_synthesize(&hir, &dir, &rir, &error);
        bool restored = air != NULL && air->has_rir_input &&
            air_verify(air, &error) && air_drift_count(air) == 0;
        air_destroy(air); air = NULL;
        free(error); error = NULL;
        printf("readfile DIR node/intent/IO missing mask %u: synth=%d collect=%d verify=%d denied=%d restore=%d\n",
               mask, (int)synthesized, (int)collected, (int)verified,
               (int)denied, (int)restored);
        dir_masks_denied = dir_masks_denied && denied;
        dir_masks_restored = dir_masks_restored && restored;
        if (mask == 3u) {
            dir_pair_denied = denied;
            dir_pair_restored = restored;
        }
        if (mask == 7u) {
            dir_io_pair_denied = denied;
            dir_io_pair_restored = restored;
        }
    }
    ok = ok && dir_masks_denied && dir_masks_restored &&
        dir_pair_denied && dir_pair_restored &&
        dir_io_pair_denied && dir_io_pair_restored;
    free(error);
    ast_destroy(intent_ast);
    return ok;
}

static bool
test_air_deferred_readfile_requires_original_rir_io(void)
{
    const char *source =
        "func Consume(text:String)->Int{return StringLength(text);}"
        "func Outside()->Void{let slot:Slot<Int> = ClaimSlot<Int>();Release(slot);}"
        "subject Hosted { let id:Int; action Fetch(self)->Void { return; } }"
        "func Main()->Void{let risk:Array<String> = [Concat(\"he\",\"ap\")];"
        "let slot:Slot<Int> = ClaimSlot<Int>();Write(slot,1);"
        "defer{Log(Consume(ReadFile(\"tests/cases/backend_compare/string_io/io.txt\")));Release(slot);};"
        "let value=Read(slot);Log(value);Log(risk[0]);}";
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticResult *sem = NULL;
    DIRProgram *dir = NULL;
    HIRProgram *hir = NULL;
    RIRProgram *rir = NULL;
    AIRProgram *air = NULL;
    ASTNode *defer_stmt = NULL, *read_call = NULL;
    RIROp *io = NULL;
    RIRScope *outside_scope = NULL, *main_scope = NULL;
    size_t io_index = SIZE_MAX;
    char *error = NULL;
    bool ok = false;

    if (program == NULL || parser_has_error(parser)) goto cleanup;
    sem = semantic_analyze(program);
    if (sem == NULL || !sem->success) goto cleanup;
    ASTNode *host_decl = ast_program_statement(sem->annotated_ast, 2);
    ASTNode *main_decl = ast_program_statement(sem->annotated_ast, 3);
    defer_stmt = ast_block_statement(ast_func_body(main_decl), 3);
    ASTNode *other_stmt = ast_block_statement(ast_func_body(main_decl), 0);
    ASTNode *log = ast_block_statement(ast_defer_body(defer_stmt), 0);
    ASTNode *consume = ast_call_argument(log, 0);
    read_call = ast_call_argument(consume, 0);
    if (defer_stmt == NULL || defer_stmt->type != AST_DEFER_STMT ||
        read_call == NULL || read_call->type != AST_CALL ||
        ast_node_stable_id(defer_stmt) == 0 || other_stmt == NULL ||
        ast_node_stable_id(other_stmt) == 0 ||
        ast_node_stable_id(other_stmt) == ast_node_stable_id(defer_stmt)) goto cleanup;
    dir = dir_lower(sem->annotated_ast, &error);
    hir = hir_lower(sem->annotated_ast, &error);
    rir = rir_lower(sem->annotated_ast, &error);
    if (dir == NULL || hir == NULL || rir == NULL ||
        !rir_enrich_with_hir_flow(rir, hir, &error)) goto cleanup;
    size_t io_count = 0;
    bool early_resource_transition = false;
    bool outside_release = false;
    for (size_t s = 0; s < rir->scope_count; s++) {
        RIRScope *scope = &rir->scopes[s];
        if (scope->kind == RIR_SCOPE_FUNCTION && scope->name != NULL &&
            strcmp(scope->name, "Outside") == 0) {
            outside_scope = scope;
            for (size_t i = 0; i < scope->op_count; i++)
                if (scope->ops[i].kind == RIR_OP_RELEASE &&
                    scope->ops[i].subject != NULL &&
                    strcmp(scope->ops[i].subject, "slot") == 0)
                    outside_release = true;
        }
        if (scope->kind != RIR_SCOPE_FUNCTION || scope->name == NULL ||
            strcmp(scope->name, "Main") != 0) continue;
        main_scope = scope;
        for (size_t i = 0; i < scope->op_count; i++) {
            RIROp *op = &scope->ops[i];
            if ((op->kind == RIR_OP_RELEASE || op->kind == RIR_OP_MOVE) &&
                op->subject != NULL && strcmp(op->subject, "slot") == 0)
                early_resource_transition = true;
            if (op->kind == RIR_OP_IO && op->subject != NULL &&
                strcmp(op->subject, "ReadFile") == 0 && op->ast == read_call) {
                io = op;
                io_index = i;
                io_count++;
            }
        }
    }
    if (io_count != 1 || io == NULL || io_index == SIZE_MAX ||
        main_scope == NULL || outside_scope == NULL ||
        !outside_release || early_resource_transition ||
        !io->has_source_statement_syntax_id ||
        io->source_statement_syntax_id != ast_node_stable_id(defer_stmt)) goto cleanup;

    /* The original HIR/DIR inventories, not an IO-shaped RIR row alone, own the call. */
    AIRProgram *without_rir = air_synthesize(hir, dir, NULL, &error);
    bool bare_io_present = false;
    for (size_t i = 0; without_rir != NULL && i < without_rir->boundary_count; i++)
        if (without_rir->boundaries[i].kind == AIR_BOUNDARY_IO &&
            without_rir->boundaries[i].ast == read_call) bare_io_present = true;
    if (!bare_io_present) {
        printf("readfile original-owner withdrawal control has no AIR IO boundary\n");
        air_destroy(without_rir);
        goto cleanup;
    }
    bool missing_owner_denied = !air_collect_rir_evidence(
        without_rir, rir, NULL, NULL, &error);
    air_destroy(without_rir);
    free(error); error = NULL;
    without_rir = air_synthesize(hir, dir, NULL, &error);
    bool missing_owner_restored = without_rir != NULL &&
        air_collect_rir_evidence(without_rir, rir, hir, dir, &error) &&
        air_verify(without_rir, &error) && air_drift_count(without_rir) == 0;
    air_destroy(without_rir);
    free(error); error = NULL;
    if (!missing_owner_denied || !missing_owner_restored)
        printf("readfile original HIR/DIR inventory withdrawal: refused=%d restore=%d\n",
               (int)missing_owner_denied, (int)missing_owner_restored);

    /* The parsed Main declaration remains in RIR and HIR routines; only its
       source-function projection is removed. */
    size_t main_function_index = SIZE_MAX;
    for (size_t i = 0; i < hir->function_count; i++)
        if (hir->functions[i] == main_decl) main_function_index = i;
    if (main_function_index == SIZE_MAX || hir->function_count < 2) goto cleanup;
    ASTNode **without_main = (ASTNode **)calloc(hir->function_count,
        sizeof(*without_main));
    if (without_main == NULL) goto cleanup;
    for (size_t i = 0, j = 0; i < hir->function_count; i++)
        if (i != main_function_index) without_main[j++] = hir->functions[i];
    ASTNode **saved_functions = hir->functions;
    size_t saved_function_count = hir->function_count;
    size_t saved_function_capacity = hir->function_capacity;
    hir->functions = without_main;
    hir->function_count = saved_function_count - 1;
    hir->function_capacity = saved_function_count - 1;
    air = air_synthesize(hir, dir, rir, &error);
    bool hir_projection_synthesized = air != NULL;
    bool hir_projection_collected = air != NULL &&
        air_collect_dag_evidence(air, sem, &error);
    bool hir_projection_verified = hir_projection_collected &&
        air_verify(air, &error) && air_drift_count(air) == 0;
    bool hir_projection_denied = !hir_projection_verified;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    hir->functions = saved_functions;
    hir->function_count = saved_function_count;
    hir->function_capacity = saved_function_capacity;
    free(without_main);
    air = air_synthesize(hir, dir, rir, &error);
    bool hir_projection_restored = air != NULL &&
        air_collect_dag_evidence(air, sem, &error) && air_verify(air, &error) &&
        air_drift_count(air) == 0;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    if (!hir_projection_denied || !hir_projection_restored)
        printf("readfile HIR Main function-list erasure: synth=%d collect=%d verify=%d restore=%d\n",
               (int)hir_projection_synthesized, (int)hir_projection_collected,
               (int)hir_projection_verified, (int)hir_projection_restored);

    /* Remove both original HIR declaration and function projection, leaving
       the RIR IO scope and parsed source untouched. */
    size_t main_decl_index = SIZE_MAX;
    for (size_t i = 0; i < hir->decl_count; i++)
        if (hir->decls[i].ast == main_decl) main_decl_index = i;
    HIRRoutine *main_hir_routine = NULL, *host_hir_routine = NULL;
    for (size_t i = 0; i < hir->routine_count; i++) {
        HIRRoutine *routine = &hir->routines[i];
        if (routine->ast == main_decl) main_hir_routine = routine;
        if (host_decl != NULL && routine->is_hosted && routine->owner_name != NULL &&
            strcmp(routine->owner_name, "Hosted") == 0 &&
            routine->owner_ast_type == host_decl->type)
            host_hir_routine = routine;
    }
    if (main_decl_index == SIZE_MAX || hir->decl_count < 1 || host_decl == NULL ||
        main_hir_routine == NULL || host_hir_routine == NULL ||
        host_hir_routine->decl_id >= hir->decl_count ||
        hir->decls[host_hir_routine->decl_id].ast != host_decl ||
        host_hir_routine->decl_id >= main_decl_index) goto cleanup;
    HIRDecl *without_main_decl = (HIRDecl *)calloc(hir->decl_count,
        sizeof(*without_main_decl));
    ASTNode **without_main_function = (ASTNode **)calloc(hir->function_count,
        sizeof(*without_main_function));
    if (without_main_decl == NULL || without_main_function == NULL) {
        free(without_main_decl);
        free(without_main_function);
        goto cleanup;
    }
    for (size_t i = 0, j = 0; i < hir->decl_count; i++)
        if (i != main_decl_index) without_main_decl[j++] = hir->decls[i];
    for (size_t i = 0, j = 0; i < hir->function_count; i++)
        if (i != main_function_index) without_main_function[j++] = hir->functions[i];
    HIRDecl *saved_decls = hir->decls;
    size_t saved_decl_count = hir->decl_count;
    size_t saved_decl_capacity = hir->decl_capacity;
    hir->decls = without_main_decl;
    hir->decl_count = saved_decl_count - 1;
    hir->decl_capacity = saved_decl_count - 1;
    hir->functions = without_main_function;
    hir->function_count = saved_function_count - 1;
    hir->function_capacity = saved_function_count - 1;
    air = air_synthesize(hir, dir, rir, &error);
    bool dual_hir_synthesized = air != NULL;
    bool dual_hir_collected = air != NULL && air_collect_dag_evidence(air, sem, &error);
    bool dual_hir_verified = dual_hir_collected && air_verify(air, &error) &&
        air_drift_count(air) == 0;
    bool dual_hir_denied = !dual_hir_verified;
    air_destroy(air); air = NULL;
    free(error); error = NULL;

    /* The real Hosted declaration stays in the remaining HIR inventory.
       Recasting Main's surviving routine and RIR scope as that host must not
       hide its original function IO when Main's own projections are gone. */
    bool saved_main_hosted = main_hir_routine->is_hosted;
    size_t saved_main_decl_id = main_hir_routine->decl_id;
    ASTNode *saved_main_declaration_ast = main_hir_routine->declaration_ast;
    const char *saved_main_owner_name = main_hir_routine->owner_name;
    ASTNodeType saved_main_owner_type = main_hir_routine->owner_ast_type;
    RIRScope saved_host_scope = *main_scope;
    main_hir_routine->is_hosted = true;
    main_hir_routine->decl_id = host_hir_routine->decl_id;
    main_hir_routine->declaration_ast = host_decl;
    main_hir_routine->owner_name = host_hir_routine->owner_name;
    main_hir_routine->owner_ast_type = host_hir_routine->owner_ast_type;
    main_scope->kind = RIR_SCOPE_METHOD;
    main_scope->owner_name = host_hir_routine->owner_name;
    air = air_synthesize(hir, dir, rir, &error);
    bool coherent_host_synthesized = air != NULL;
    bool coherent_host_collected = air != NULL && air_collect_dag_evidence(air, sem, &error);
    bool coherent_host_verified = coherent_host_collected && air_verify(air, &error) &&
        air_drift_count(air) == 0;
    bool coherent_host_denied = !coherent_host_verified;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    *main_scope = saved_host_scope;
    main_hir_routine->is_hosted = saved_main_hosted;
    main_hir_routine->decl_id = saved_main_decl_id;
    main_hir_routine->declaration_ast = saved_main_declaration_ast;
    main_hir_routine->owner_name = saved_main_owner_name;
    main_hir_routine->owner_ast_type = saved_main_owner_type;

    hir->decls = saved_decls;
    hir->decl_count = saved_decl_count;
    hir->decl_capacity = saved_decl_capacity;
    hir->functions = saved_functions;
    hir->function_count = saved_function_count;
    hir->function_capacity = saved_function_capacity;
    air = air_synthesize(hir, dir, rir, &error);
    bool dual_hir_restored = air != NULL && air_collect_dag_evidence(air, sem, &error) &&
        air_verify(air, &error) && air_drift_count(air) == 0;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    air = air_synthesize(hir, dir, rir, &error);
    bool coherent_host_restored = air != NULL && air_collect_dag_evidence(air, sem, &error) &&
        air_verify(air, &error) && air_drift_count(air) == 0;
    air_destroy(air); air = NULL;
    free(error); error = NULL;

    /* Keep the original AST, Main routine/witness and RIR scope identity.
       Each missing projection is measured once with every other bit state. */
    RIRScope saved_main_inventory_scope = *main_scope;
    RIROp *without_main_io = (RIROp *)calloc(saved_main_inventory_scope.op_count,
                                             sizeof(*without_main_io));
    bool main_masks_denied = without_main_io != NULL;
    bool main_masks_restored = without_main_io != NULL;
    bool triple_owner_denied = false, triple_owner_restored = false;
    if (without_main_io != NULL) {
        for (size_t i = 0, j = 0; i < saved_main_inventory_scope.op_count; i++)
            if (i != io_index) without_main_io[j++] = saved_main_inventory_scope.ops[i];
        for (unsigned mask = 1; mask < 8; mask++) {
            hir->decls = (mask & 1u) != 0 ? without_main_decl : saved_decls;
            hir->decl_count = saved_decl_count - ((mask & 1u) != 0 ? 1u : 0u);
            hir->decl_capacity = (mask & 1u) != 0 ?
                saved_decl_count - 1 : saved_decl_capacity;
            hir->functions = (mask & 2u) != 0 ? without_main_function : saved_functions;
            hir->function_count = saved_function_count - ((mask & 2u) != 0 ? 1u : 0u);
            hir->function_capacity = (mask & 2u) != 0 ?
                saved_function_count - 1 : saved_function_capacity;
            if ((mask & 4u) != 0) {
                main_scope->ops = without_main_io;
                main_scope->op_count = saved_main_inventory_scope.op_count - 1;
                main_scope->op_capacity = main_scope->op_count;
            }
            air = air_synthesize(hir, dir, rir, &error);
            bool synthesized = air != NULL;
            bool collected = air != NULL && air_collect_dag_evidence(air, sem, &error);
            bool verified = collected && air_verify(air, &error) &&
                air_drift_count(air) == 0;
            bool denied = !verified;
            air_destroy(air); air = NULL;
            free(error); error = NULL;
            hir->decls = saved_decls;
            hir->decl_count = saved_decl_count;
            hir->decl_capacity = saved_decl_capacity;
            hir->functions = saved_functions;
            hir->function_count = saved_function_count;
            hir->function_capacity = saved_function_capacity;
            *main_scope = saved_main_inventory_scope;
            air = air_synthesize(hir, dir, rir, &error);
            bool restored = air != NULL && air_collect_dag_evidence(air, sem, &error) &&
                air_verify(air, &error) && air_drift_count(air) == 0;
            air_destroy(air); air = NULL;
            free(error); error = NULL;
            printf("readfile HIR decl/function and RIR IO missing mask %u: synth=%d collect=%d verify=%d denied=%d restore=%d\n",
                   mask, (int)synthesized, (int)collected, (int)verified,
                   (int)denied, (int)restored);
            main_masks_denied = main_masks_denied && denied;
            main_masks_restored = main_masks_restored && restored;
            if (mask == 7u) {
                triple_owner_denied = denied;
                triple_owner_restored = restored;
            }
        }
    }
    free(without_main_io);
    free(without_main_decl);
    free(without_main_function);
    printf("readfile HIR decl+function withdrawal: synth=%d collect=%d verify=%d denied=%d restore=%d\n",
           (int)dual_hir_synthesized, (int)dual_hir_collected,
           (int)dual_hir_verified, (int)dual_hir_denied, (int)dual_hir_restored);
    printf("readfile HIR decl+function and RIR IO withdrawal: denied=%d restore=%d\n",
           (int)triple_owner_denied, (int)triple_owner_restored);
    printf("readfile coherent Hosted Main impersonation: synth=%d collect=%d verify=%d denied=%d restore=%d\n",
           (int)coherent_host_synthesized, (int)coherent_host_collected,
           (int)coherent_host_verified, (int)coherent_host_denied,
           (int)coherent_host_restored);


    bool scope_faults_denied = true;
    bool scope_faults_restored = true;
    RIRScope original_main_scope = *main_scope;
    for (unsigned fault = 0; fault < 3; fault++) {
        if (fault == 0) main_scope->kind = RIR_SCOPE_METHOD;
        if (fault == 1) main_scope->owner_name = "Hosted";
        if (fault == 2) main_scope->name = "Other";
        air = air_synthesize(hir, dir, rir, &error);
        bool synthesized = air != NULL;
        bool collected = air != NULL && air_collect_dag_evidence(air, sem, &error);
        bool verified = collected && air_verify(air, &error) &&
            air_drift_count(air) == 0;
        bool denied = !verified;
        air_destroy(air); air = NULL;
        free(error); error = NULL;
        *main_scope = original_main_scope;
        air = air_synthesize(hir, dir, rir, &error);
        bool restored = air != NULL && air_collect_dag_evidence(air, sem, &error) &&
            air_verify(air, &error) && air_drift_count(air) == 0;
        air_destroy(air); air = NULL;
        free(error); error = NULL;
        printf("readfile Main scope identity fault %u: synth=%d collect=%d verify=%d denied=%d restore=%d\n",
               fault, (int)synthesized, (int)collected, (int)verified,
               (int)denied, (int)restored);
        scope_faults_denied = scope_faults_denied && denied;
        scope_faults_restored = scope_faults_restored && restored;
    }

    /* A top-level action-shaped AST must have freshly issued HIR and RIR
       facts; mutating only the old scope would leave is_action_like stale. */
    bool saved_action = main_decl->data.func_decl.is_action;
    main_decl->data.func_decl.is_action = true;
    DIRProgram *action_dir = dir_lower(sem->annotated_ast, &error);
    HIRProgram *action_hir = hir_lower(sem->annotated_ast, &error);
    RIRProgram *action_rir = rir_lower(sem->annotated_ast, &error);
    bool action_hir_fact = false, action_rir_fact = false;
    for (size_t i = 0; action_hir != NULL && i < action_hir->routine_count; i++)
        if (action_hir->routines[i].ast == main_decl &&
            action_hir->routines[i].is_action_like &&
            !action_hir->routines[i].is_hosted)
            action_hir_fact = true;
    for (size_t i = 0; action_rir != NULL && i < action_rir->scope_count; i++)
        if (action_rir->scopes[i].ast == main_decl &&
            action_rir->scopes[i].kind == RIR_SCOPE_METHOD &&
            action_rir->scopes[i].owner_name == NULL)
            action_rir_fact = true;
    bool action_enriched = action_rir != NULL && action_hir != NULL &&
        rir_enrich_with_hir_flow(action_rir, action_hir, &error);
    air = action_dir != NULL && action_enriched ?
        air_synthesize(action_hir, action_dir, action_rir, &error) : NULL;
    bool top_action_positive = action_hir_fact && action_rir_fact &&
        air != NULL && air_collect_dag_evidence(air, sem, &error) &&
        air_verify(air, &error) && air_drift_count(air) == 0;
    air_destroy(air); air = NULL;
    rir_destroy(action_rir);
    hir_destroy(action_hir);
    dir_destroy(action_dir);
    free(error); error = NULL;
    main_decl->data.func_decl.is_action = saved_action;
    air = air_synthesize(hir, dir, rir, &error);
    bool top_action_restored = air != NULL && air_collect_dag_evidence(air, sem, &error) &&
        air_verify(air, &error) && air_drift_count(air) == 0;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    printf("readfile top-level action-shaped owner: accepted=%d restore=%d\n",
           (int)top_action_positive, (int)top_action_restored);

    const struct {
        const char *label;
        const char *owner;
        ASTNodeType owner_type;
        const char *source;
    } hosted_cases[] = {
        { "subject", "Hosted", AST_CLASS_DECL,
          "func Consume(text:String)->Int{return StringLength(text);}"
          "subject Hosted { let id:Int; action Fetch(self)->Void {"
          "Log(Consume(ReadFile(\"tests/cases/backend_compare/string_io/io.txt\"))); } }"
          "func Main()->Void{} " },
        { "enum", "Route", AST_ENUM_DECL,
          "func Consume(text:String)->Int{return StringLength(text);}"
          "enum Route { One, func Value(self)->Int {"
          "return Consume(ReadFile(\"tests/cases/backend_compare/string_io/io.txt\")); } }"
          "func Main()->Void{let route:Route=One;Log(Route_Value(route));}" },
        { "enum-payload", "Packet", AST_ENUM_DECL,
          "func Consume(text:String)->Int{return StringLength(text);}"
          "enum Packet { Payload(Int), Empty, func Value(self)->Int {"
          "return Consume(ReadFile(\"tests/cases/backend_compare/string_io/io.txt\")); } }"
          "func Main()->Void{let packet:Packet=Payload(23);Log(Packet_Value(packet));}" },
    };
    bool hosted_method_positive = true;
    bool hosted_layout_denied = true;
    bool hosted_layout_not_regenerated = true;
    bool hosted_layout_restored = true;
    bool hosted_identity_denied = true;
    bool hosted_identity_restored = true;
    for (size_t case_i = 0; case_i < sizeof(hosted_cases) / sizeof(hosted_cases[0]); case_i++) {
        const char *hosted_source = hosted_cases[case_i].source;
        Lexer *hosted_lexer = lexer_create(hosted_source);
        Parser *hosted_parser = parser_create(hosted_lexer);
        ASTNode *hosted_program = parser_parse_program(hosted_parser);
        SemanticResult *hosted_sem = NULL;
        DIRProgram *hosted_dir = NULL;
        HIRProgram *hosted_hir = NULL;
        RIRProgram *hosted_rir = NULL;
        AIRProgram *hosted_control = NULL;
        HIRRoutine *hosted_method = NULL;
        HIRDecl *hosted_parent = NULL;
        RIRScope *hosted_scope = NULL;
        RIROp *hosted_readfile = NULL;
        char *hosted_error = NULL;
        bool admitted = false, layout_denied = false;
        bool layout_not_regenerated = false, layout_restored = false;
        bool identity_denied = true, identity_restored = true;
        if (hosted_program != NULL && !parser_has_error(hosted_parser))
            hosted_sem = semantic_analyze(hosted_program);
        if (hosted_sem != NULL && hosted_sem->success) {
            ASTNode *hosted_decl = ast_program_statement(hosted_sem->annotated_ast, 1);
            ASTNode *other_decl = ast_program_statement(hosted_sem->annotated_ast, 0);
            hosted_dir = dir_lower(hosted_sem->annotated_ast, &hosted_error);
            hosted_hir = hir_lower(hosted_sem->annotated_ast, &hosted_error);
            hosted_rir = rir_lower(hosted_sem->annotated_ast, &hosted_error);
            bool enriched = hosted_hir != NULL && hosted_rir != NULL &&
                rir_enrich_with_hir_flow(hosted_rir, hosted_hir, &hosted_error);
            for (size_t i = 0; hosted_hir != NULL && i < hosted_hir->routine_count; i++) {
                HIRRoutine *routine = &hosted_hir->routines[i];
                if (routine->is_hosted && routine->declaration_ast == hosted_decl &&
                    routine->owner_name != NULL &&
                    strcmp(routine->owner_name, hosted_cases[case_i].owner) == 0)
                    hosted_method = routine;
            }
            if (hosted_method != NULL && hosted_method->decl_id < hosted_hir->decl_count)
                hosted_parent = &hosted_hir->decls[hosted_method->decl_id];
            for (size_t i = 0; hosted_rir != NULL && hosted_method != NULL &&
                 i < hosted_rir->scope_count; i++) {
                RIRScope *scope = &hosted_rir->scopes[i];
                if (scope->kind == RIR_SCOPE_METHOD && scope->ast == hosted_method->ast &&
                    scope->owner_name != NULL &&
                    strcmp(scope->owner_name, hosted_cases[case_i].owner) == 0)
                    hosted_scope = scope;
            }
            size_t hosted_io_count = 0;
            for (size_t i = 0; hosted_scope != NULL && i < hosted_scope->op_count; i++) {
                RIROp *op = &hosted_scope->ops[i];
                if (op->kind == RIR_OP_IO && op->subject != NULL &&
                    strcmp(op->subject, "ReadFile") == 0 && op->ast != NULL) {
                    hosted_readfile = op;
                    hosted_io_count++;
                }
            }
            bool issued_owner = hosted_decl != NULL && other_decl != NULL &&
                hosted_decl->type == hosted_cases[case_i].owner_type &&
                hosted_parent != NULL && hosted_parent->ast == hosted_decl &&
                hosted_parent->name != NULL &&
                strcmp(hosted_parent->name, hosted_cases[case_i].owner) == 0 &&
                hosted_method->execution_layout != NULL && hosted_scope != NULL &&
                hosted_io_count == 1 && hosted_readfile != NULL &&
                hosted_scope->source_syntax_id == ast_node_stable_id(hosted_method->ast) &&
                hosted_scope->source_syntax_id != ast_node_stable_id(hosted_decl);
            hosted_control = hosted_dir != NULL && enriched && issued_owner ?
                air_synthesize(hosted_hir, hosted_dir, hosted_rir, &hosted_error) : NULL;
            admitted = hosted_control != NULL &&
                air_collect_dag_evidence(hosted_control, hosted_sem, &hosted_error) &&
                air_verify(hosted_control, &hosted_error) &&
                air_drift_count(hosted_control) == 0;
            bool hosted_io_not_projected = true;
            for (size_t i = 0; hosted_control != NULL &&
                 i < hosted_control->boundary_count; i++) {
                const AIRBoundaryNode *boundary = &hosted_control->boundaries[i];
                if (boundary->kind == AIR_BOUNDARY_IO &&
                    boundary->ast == hosted_readfile->ast)
                    hosted_io_not_projected = false;
            }
            admitted = admitted && hosted_io_not_projected;
            air_destroy(hosted_control); hosted_control = NULL;
            free(hosted_error); hosted_error = NULL;
            if (admitted) {
                HIRCFGExecutionWitness *saved_layout = hosted_method->execution_layout;
                hosted_method->execution_layout = NULL;
                hosted_control = air_synthesize(hosted_hir, hosted_dir, hosted_rir,
                                                &hosted_error);
                bool layout_collected = hosted_control != NULL &&
                    air_collect_dag_evidence(hosted_control, hosted_sem, &hosted_error);
                bool layout_verified = layout_collected &&
                    air_verify(hosted_control, &hosted_error) &&
                    air_drift_count(hosted_control) == 0;
                layout_denied = !layout_verified;
                layout_not_regenerated = hosted_method->execution_layout == NULL;
                air_destroy(hosted_control); hosted_control = NULL;
                free(hosted_error); hosted_error = NULL;
                hosted_method->execution_layout = saved_layout;
                hosted_control = air_synthesize(hosted_hir, hosted_dir, hosted_rir,
                                                &hosted_error);
                layout_restored = hosted_control != NULL &&
                    air_collect_dag_evidence(hosted_control, hosted_sem, &hosted_error) &&
                    air_verify(hosted_control, &hosted_error) &&
                    air_drift_count(hosted_control) == 0;
                air_destroy(hosted_control); hosted_control = NULL;
                free(hosted_error); hosted_error = NULL;

                const char *saved_parent_name = hosted_parent->name;
                size_t saved_decl_id = hosted_method->decl_id;
                ASTNode *saved_declaration_ast = hosted_method->declaration_ast;
                const char *saved_owner_name = hosted_method->owner_name;
                RIRScopeKind saved_scope_kind = hosted_scope->kind;
                const char *saved_scope_owner = hosted_scope->owner_name;
                uint32_t saved_scope_id = hosted_scope->source_syntax_id;
                for (unsigned fault = 0; fault < 8; fault++) {
                    if (fault == 0) hosted_parent->name = NULL;
                    if (fault == 1) hosted_parent->name = "OtherOwner";
                    if (fault == 2) hosted_method->decl_id = hosted_hir->decl_count;
                    if (fault == 3) hosted_method->declaration_ast = other_decl;
                    if (fault == 4) hosted_method->owner_name = "OtherOwner";
                    if (fault == 5) hosted_scope->kind = RIR_SCOPE_FUNCTION;
                    if (fault == 6) hosted_scope->owner_name = "OtherOwner";
                    if (fault == 7)
                        hosted_scope->source_syntax_id = ast_node_stable_id(hosted_decl);
                    hosted_control = air_synthesize(hosted_hir, hosted_dir, hosted_rir,
                                                    &hosted_error);
                    bool synthesized = hosted_control != NULL;
                    bool collected = hosted_control != NULL &&
                        air_collect_dag_evidence(hosted_control, hosted_sem, &hosted_error);
                    bool verified = collected && air_verify(hosted_control, &hosted_error) &&
                        air_drift_count(hosted_control) == 0;
                    bool denied = !verified;
                    air_destroy(hosted_control); hosted_control = NULL;
                    free(hosted_error); hosted_error = NULL;
                    hosted_parent->name = saved_parent_name;
                    hosted_method->decl_id = saved_decl_id;
                    hosted_method->declaration_ast = saved_declaration_ast;
                    hosted_method->owner_name = saved_owner_name;
                    hosted_scope->kind = saved_scope_kind;
                    hosted_scope->owner_name = saved_scope_owner;
                    hosted_scope->source_syntax_id = saved_scope_id;
                    hosted_control = air_synthesize(hosted_hir, hosted_dir, hosted_rir,
                                                    &hosted_error);
                    bool restored = hosted_control != NULL &&
                        air_collect_dag_evidence(hosted_control, hosted_sem, &hosted_error) &&
                        air_verify(hosted_control, &hosted_error) &&
                        air_drift_count(hosted_control) == 0;
                    air_destroy(hosted_control); hosted_control = NULL;
                    free(hosted_error); hosted_error = NULL;
                    printf("readfile hosted %s owner fault %u: synth=%d collect=%d verify=%d denied=%d restore=%d\n",
                           hosted_cases[case_i].label, fault, (int)synthesized,
                           (int)collected, (int)verified, (int)denied, (int)restored);
                    identity_denied = identity_denied && denied;
                    identity_restored = identity_restored && restored;
                }
            } else {
                identity_denied = false;
                identity_restored = false;
            }
        } else {
            identity_denied = false;
            identity_restored = false;
        }
        air_destroy(hosted_control);
        free(hosted_error);
        rir_destroy(hosted_rir);
        hir_destroy(hosted_hir);
        dir_destroy(hosted_dir);
        semantic_result_destroy(hosted_sem);
        ast_destroy(hosted_program);
        parser_destroy(hosted_parser);
        lexer_destroy(hosted_lexer);
        printf("readfile genuine hosted %s method: accepted=%d missing-layout=%d no-regeneration=%d restore=%d identity-denied=%d identity-restored=%d\n",
               hosted_cases[case_i].label, (int)admitted, (int)layout_denied,
               (int)layout_not_regenerated, (int)layout_restored,
               (int)identity_denied, (int)identity_restored);
        hosted_method_positive = hosted_method_positive && admitted;
        hosted_layout_denied = hosted_layout_denied && layout_denied;
        hosted_layout_not_regenerated = hosted_layout_not_regenerated &&
            layout_not_regenerated;
        hosted_layout_restored = hosted_layout_restored && layout_restored;
        hosted_identity_denied = hosted_identity_denied && identity_denied;
        hosted_identity_restored = hosted_identity_restored && identity_restored;
    }

    bool all_faults_denied = true;
    for (unsigned fault = 0; fault < 9; fault++) {
        air = air_synthesize(hir, dir, rir, &error);
        if (air == NULL || !air_collect_dag_evidence(air, sem, &error) ||
            !air_verify(air, &error) || air_drift_count(air) != 0) goto cleanup;
        bool found_io = false;
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_IO && boundary->ast == read_call &&
                boundary->source_name != NULL &&
                strcmp(boundary->source_name, "ReadFile") == 0 &&
                boundary->has_hir_cfg_evidence && boundary->has_rir_boundary_evidence)
                found_io = true;
        }
        air_destroy(air); air = NULL;
        free(error); error = NULL;
        if (!found_io) goto cleanup;

        RIROp saved = *io;
        if (fault == 0) io->kind = RIR_OP_READ;
        if (fault == 1) io->subject = NULL;
        if (fault == 2) io->subject = "WriteFile";
        if (fault == 3) io->ast = NULL;
        if (fault == 4) io->ast = defer_stmt;
        if (fault == 5) io->ast = ast_call_argument(read_call, 0);
        if (fault == 6) io->ast = ast_call_callee(read_call);
        if (fault == 7) io->has_source_statement_syntax_id = false;
        if (fault == 8) io->source_statement_syntax_id = ast_node_stable_id(other_stmt);
        air = air_synthesize(hir, dir, rir, &error);
        bool synthesized = air != NULL;
        bool missing_io = false, evidence_drift = false;
        bool denied = !synthesized;
        if (air != NULL) {
            for (size_t i = 0; i < air->boundary_count; i++) {
                const AIRBoundaryNode *boundary = &air->boundaries[i];
                if (boundary->kind == AIR_BOUNDARY_IO && boundary->ast == read_call &&
                    !boundary->has_rir_boundary_evidence) missing_io = true;
            }
            for (size_t i = 0; i < air->drift_count; i++)
                if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                    evidence_drift = true;
            denied = missing_io && evidence_drift;
        }
        air_destroy(air); air = NULL;
        free(error); error = NULL;
        *io = saved;
        if (!denied) {
            printf("readfile deferred RIR fault %u accepted: air=%d missing_io=%d drift=%d\n",
                   fault, synthesized, missing_io, evidence_drift);
            all_faults_denied = false;
        }
    }
    /* The exact IO tuple in a foreign function cannot witness Main's deferred call. */
    size_t outside_op_count = outside_scope->op_count;
    RIROp *foreign_ops = (RIROp *)calloc(outside_op_count + 1, sizeof(RIROp));
    if (foreign_ops == NULL) goto cleanup;
    for (size_t i = 0; i < outside_op_count; i++)
        foreign_ops[i] = outside_scope->ops[i];
    foreign_ops[outside_op_count] = *io;
    RIRScope saved_outside = *outside_scope;
    RIROp saved_io = *io;
    io->kind = RIR_OP_READ;
    outside_scope->ops = foreign_ops;
    outside_scope->op_count = outside_op_count + 1;
    outside_scope->op_capacity = outside_op_count + 1;
    air = air_synthesize(hir, dir, rir, &error);
    bool foreign_synthesized = air != NULL;
    bool foreign_missing_io = false, foreign_evidence_drift = false;
    bool foreign_denied = !foreign_synthesized;
    if (air != NULL) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            const AIRBoundaryNode *boundary = &air->boundaries[i];
            if (boundary->kind == AIR_BOUNDARY_IO && boundary->ast == read_call &&
                !boundary->has_rir_boundary_evidence) foreign_missing_io = true;
        }
        for (size_t i = 0; i < air->drift_count; i++)
            if (air->drifts[i].kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING)
                foreign_evidence_drift = true;
        foreign_denied = foreign_missing_io && foreign_evidence_drift;
    }
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    *io = saved_io;
    *outside_scope = saved_outside;
    free(foreign_ops);
    if (!foreign_denied)
        printf("readfile deferred foreign RIR accepted: air=%d missing_io=%d drift=%d\n",
               foreign_synthesized, foreign_missing_io, foreign_evidence_drift);

    /* An executable IO row has exactly one matching RIR occurrence in its scope. */
    RIRScope saved_main = *main_scope;
    RIROp *duplicate_io = (RIROp *)calloc(saved_main.op_count + 1, sizeof(*duplicate_io));
    RIROp *without_io = (RIROp *)calloc(saved_main.op_count, sizeof(*without_io));
    if (duplicate_io == NULL || without_io == NULL) {
        free(duplicate_io);
        free(without_io);
        goto cleanup;
    }
    for (size_t i = 0; i < saved_main.op_count; i++)
        duplicate_io[i] = saved_main.ops[i];
    duplicate_io[saved_main.op_count] = saved_main.ops[io_index];
    main_scope->ops = duplicate_io;
    main_scope->op_count = saved_main.op_count + 1;
    main_scope->op_capacity = saved_main.op_count + 1;
    air = air_synthesize(hir, dir, rir, &error);
    bool duplicate_synthesized = air != NULL;
    bool duplicate_collected = air != NULL && air_collect_dag_evidence(air, sem, &error);
    bool duplicate_verified = duplicate_collected && air_verify(air, &error) &&
        air_drift_count(air) == 0;
    bool duplicate_rir_denied = !duplicate_verified;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    *main_scope = saved_main;
    air = air_synthesize(hir, dir, rir, &error);
    bool duplicate_rir_restored = air != NULL &&
        air_collect_dag_evidence(air, sem, &error) && air_verify(air, &error) &&
        air_drift_count(air) == 0;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    if (!duplicate_rir_denied || !duplicate_rir_restored)
        printf("readfile duplicate RIR IO: synth=%d collect=%d verify=%d restore=%d\n",
               duplicate_synthesized, duplicate_collected, duplicate_verified,
               duplicate_rir_restored);

    main_scope->ops = duplicate_io;
    main_scope->op_count = saved_main.op_count + 1;
    main_scope->op_capacity = saved_main.op_count + 1;
    main_scope->kind = RIR_SCOPE_METHOD;
    main_scope->owner_name = "Hosted";
    air = air_synthesize(hir, dir, rir, &error);
    bool method_duplicate_synthesized = air != NULL;
    bool method_duplicate_collected = air != NULL &&
        air_collect_dag_evidence(air, sem, &error);
    bool method_duplicate_verified = method_duplicate_collected &&
        air_verify(air, &error) && air_drift_count(air) == 0;
    bool method_duplicate_denied = !method_duplicate_verified;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    *main_scope = saved_main;
    air = air_synthesize(hir, dir, rir, &error);
    bool method_duplicate_restored = air != NULL &&
        air_collect_dag_evidence(air, sem, &error) && air_verify(air, &error) &&
        air_drift_count(air) == 0;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    if (!method_duplicate_denied || !method_duplicate_restored)
        printf("readfile method-kind duplicate RIR: synth=%d collect=%d verify=%d restore=%d\n",
               (int)method_duplicate_synthesized, (int)method_duplicate_collected,
               (int)method_duplicate_verified, (int)method_duplicate_restored);

    for (size_t i = 0, j = 0; i < saved_main.op_count; i++)
        if (i != io_index) without_io[j++] = saved_main.ops[i];
    main_scope->ops = without_io;
    main_scope->op_count = saved_main.op_count - 1;
    main_scope->op_capacity = saved_main.op_count - 1;
    air = air_synthesize(hir, dir, rir, &error);
    bool missing_synthesized = air != NULL;
    bool missing_collected = air != NULL && air_collect_dag_evidence(air, sem, &error);
    bool missing_verified = missing_collected && air_verify(air, &error) &&
        air_drift_count(air) == 0;
    bool missing_rir_denied = !missing_verified;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    *main_scope = saved_main;
    air = air_synthesize(hir, dir, rir, &error);
    bool missing_rir_restored = air != NULL &&
        air_collect_dag_evidence(air, sem, &error) && air_verify(air, &error) &&
        air_drift_count(air) == 0;
    air_destroy(air); air = NULL;
    free(error); error = NULL;
    free(duplicate_io);
    free(without_io);
    if (!missing_rir_denied || !missing_rir_restored)
        printf("readfile missing RIR IO: synth=%d collect=%d verify=%d restore=%d\n",
               missing_synthesized, missing_collected, missing_verified,
               missing_rir_restored);

    air = air_synthesize(hir, dir, rir, &error);
    if (air == NULL || !air_collect_dag_evidence(air, sem, &error) ||
        !air_verify(air, &error) || air_drift_count(air) != 0) goto cleanup;
    bool restored_io = false;
    for (size_t i = 0; i < air->boundary_count; i++) {
        const AIRBoundaryNode *boundary = &air->boundaries[i];
        if (boundary->kind == AIR_BOUNDARY_IO && boundary->ast == read_call &&
            boundary->source_name != NULL &&
            strcmp(boundary->source_name, "ReadFile") == 0 &&
            boundary->has_hir_cfg_evidence && boundary->has_rir_boundary_evidence)
            restored_io = true;
    }
    if (!restored_io) goto cleanup;
    AIRBoundaryNode *saved_boundaries = air->boundaries;
    size_t saved_boundary_count = air->boundary_count;
    size_t io_boundary_index = SIZE_MAX;
    for (size_t i = 0; i < saved_boundary_count; i++)
        if (saved_boundaries[i].kind == AIR_BOUNDARY_IO &&
            saved_boundaries[i].ast == read_call) io_boundary_index = i;
    if (io_boundary_index == SIZE_MAX) goto cleanup;
    AIRBoundaryNode *duplicate_boundary = (AIRBoundaryNode *)calloc(
        saved_boundary_count + 1, sizeof(*duplicate_boundary));
    if (duplicate_boundary == NULL) goto cleanup;
    for (size_t i = 0; i < saved_boundary_count; i++)
        duplicate_boundary[i] = saved_boundaries[i];
    duplicate_boundary[saved_boundary_count] = saved_boundaries[io_boundary_index];
    air->boundaries = duplicate_boundary;
    air->boundary_count = saved_boundary_count + 1;
    bool duplicate_air_denied = !air_verify(air, &error) ||
        air_drift_count(air) != 0;
    free(error); error = NULL;
    air->boundaries = saved_boundaries;
    air->boundary_count = saved_boundary_count;
    free(duplicate_boundary);
    bool duplicate_air_restored = air_verify(air, &error) &&
        air_drift_count(air) == 0;
    free(error); error = NULL;
    if (!duplicate_air_denied || !duplicate_air_restored)
        printf("readfile duplicate AIR IO boundary: refused=%d restore=%d\n",
               duplicate_air_denied, duplicate_air_restored);
    if (!duplicate_air_restored) goto cleanup;
    AIREvidenceNode *readfile_evidence = NULL;
    for (size_t i = 0; i < air->evidence_count; i++) {
        AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (evidence->kind != AIR_EVIDENCE_RIR_BOUNDARY ||
            evidence->boundary_index >= air->boundary_count ||
            air->boundaries[evidence->boundary_index].ast != read_call ||
            evidence->provider_name == NULL ||
            strcmp(evidence->provider_name, "Main") != 0) continue;
        readfile_evidence = evidence;
        break;
    }
    if (readfile_evidence == NULL) goto cleanup;
    const char *saved_provider_name = readfile_evidence->provider_name;
    readfile_evidence->provider_name = "Outside";
    bool foreign_provider_denied = !air_verify(air, &error) ||
        air_drift_count(air) != 0;
    free(error); error = NULL;
    readfile_evidence->provider_name = saved_provider_name;
    bool provider_restored = air_verify(air, &error) &&
        air_drift_count(air) == 0;
    if (!foreign_provider_denied)
        printf("readfile deferred AIR foreign provider accepted\n");
    printf("readfile duplicate RIR stage: synth=%d collect=%d verify=%d denied=%d restore=%d\n",
           (int)duplicate_synthesized, (int)duplicate_collected,
           (int)duplicate_verified, (int)duplicate_rir_denied,
           (int)duplicate_rir_restored);
    printf("readfile missing RIR stage: synth=%d collect=%d verify=%d denied=%d restore=%d\n",
           (int)missing_synthesized, (int)missing_collected,
           (int)missing_verified, (int)missing_rir_denied,
           (int)missing_rir_restored);
    printf("readfile duplicate AIR boundary: denied=%d restore=%d foreign=%d provider=%d\n",
           (int)duplicate_air_denied, (int)duplicate_air_restored,
           (int)foreign_denied, (int)foreign_provider_denied);
    printf("readfile RIR owner controls: missing_inventory=%d restore=%d method_duplicate=%d restore=%d\n",
           (int)missing_owner_denied, (int)missing_owner_restored,
           (int)method_duplicate_denied, (int)method_duplicate_restored);
    printf("readfile HIR owner projection: denied=%d restore=%d\n",
           (int)hir_projection_denied, (int)hir_projection_restored);
    if (!provider_restored) goto cleanup;
    ok = all_faults_denied && foreign_denied && duplicate_rir_denied &&
        duplicate_rir_restored && missing_rir_denied && missing_rir_restored &&
        duplicate_air_denied && foreign_provider_denied &&
        missing_owner_denied && missing_owner_restored &&
        method_duplicate_denied && method_duplicate_restored &&
        hir_projection_denied && hir_projection_restored &&
        dual_hir_denied && dual_hir_restored &&
        main_masks_denied && main_masks_restored &&
        triple_owner_denied && triple_owner_restored &&
        coherent_host_denied && coherent_host_restored &&
        scope_faults_denied && scope_faults_restored &&
        top_action_positive && top_action_restored && hosted_method_positive &&
        hosted_layout_denied && hosted_layout_not_regenerated &&
        hosted_layout_restored && hosted_identity_denied &&
        hosted_identity_restored;
cleanup:
    air_destroy(air);
    free(error);
    rir_destroy(rir);
    hir_destroy(hir);
    dir_destroy(dir);
    semantic_result_destroy(sem);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return ok;
}

static bool
test_air_original_callable_inventory_cannot_disappear(void)
{
    const struct {
        const char *label;
        unsigned kind;
        const char *source;
    } cases[] = {
        { "Main", 0,
          "func Main()->Void{}" },
        { "intent", 1,
          "func Main()->Void{}"
          "intent ToString(value: Int) { success: value == 7; failure: false; }" },
        { "hosted", 2,
          "func Consume(text:String)->Int{return StringLength(text);}"
          "func Main()->Void{}"
          "subject Hosted { let id:Int; action Fetch(self)->Void {"
          "Log(Consume(ReadFile(\"tests/cases/backend_compare/string_io/io.txt\"))); } }" },
    };
    bool all_ok = true;
    const struct {
        const char *label;
        const char *source;
        bool implicit_main;
    } baseline_sources[] = {
        { "empty", "", false },
        { "top-level", "Log(9);", true },
    };
    for (size_t i = 0; i < sizeof(baseline_sources) / sizeof(baseline_sources[0]); i++) {
        Lexer *lexer = lexer_create(baseline_sources[i].source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *sem = NULL;
        DIRProgram *dir = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        AIRProgram *air = NULL;
        char *error = NULL;
        bool source_shape = false, admitted = false;
        bool census_denied = false, census_not_regenerated = false;
        bool census_restored = false, foreign_program_denied = false;
        bool foreign_source_denied = false, rir_root_denied = false;
        bool rir_root_not_regenerated = false;
        if (program != NULL && !parser_has_error(parser)) {
            size_t count = ast_program_statement_count(program);
            ASTNode *main_decl = count == 1 ? ast_program_statement(program, 0) : NULL;
            source_shape = baseline_sources[i].implicit_main ?
                count == 1 && main_decl != NULL && main_decl->type == AST_FUNC_DECL &&
                    ast_declaration_name(main_decl) != NULL &&
                    strcmp(ast_declaration_name(main_decl), "Main") == 0 :
                count == 0;
        }
        if (source_shape) sem = semantic_analyze(program);
        if (sem != NULL && sem->success) {
            dir = dir_lower(sem->annotated_ast, &error);
            hir = hir_lower(sem->annotated_ast, &error);
            rir = rir_lower(sem->annotated_ast, &error);
            if (dir != NULL && hir != NULL && rir != NULL &&
                rir_enrich_with_hir_flow(rir, hir, &error)) {
                size_t expected = baseline_sources[i].implicit_main ? 1u : 0u;
                source_shape = source_shape && hir->item_count == expected &&
                    hir->decl_count == expected && hir->routine_count == expected &&
                    hir->function_count == expected && rir->scope_count == expected;
                air = air_synthesize(hir, dir, rir, &error);
                admitted = source_shape && air != NULL &&
                    air_collect_dag_evidence(air, sem, &error) &&
                    air_verify(air, &error) && air_drift_count(air) == 0;
            }
        }
        if (admitted) {
            air_destroy(air); air = NULL;
            free(error); error = NULL;
            HIRProgramExecutionWitness *original_census = hir->execution_census;
            hir->execution_census = NULL;
            bool missing_current = !hir_cfg_program_execution_current(hir,
                sem->annotated_ast);
            air = air_synthesize(hir, dir, rir, &error);
            bool missing_verified = air != NULL &&
                air_collect_dag_evidence(air, sem, &error) &&
                air_verify(air, &error) && air_drift_count(air) == 0;
            census_denied = missing_current && !missing_verified;
            census_not_regenerated = hir->execution_census == NULL;
            air_destroy(air); air = NULL;
            free(error); error = NULL;
            hir->execution_census = original_census;

            /* Borrowed copies do not acquire another program's issuance. */
            HIRProgram detached_hir = *hir;
            air = air_synthesize(&detached_hir, dir, rir, &error);
            bool foreign_verified = air != NULL &&
                air_collect_dag_evidence(air, sem, &error) &&
                air_verify(air, &error) && air_drift_count(air) == 0;
            foreign_program_denied = !hir_cfg_program_execution_current(
                &detached_hir, sem->annotated_ast) && !foreign_verified;
            air_destroy(air); air = NULL;
            free(error); error = NULL;
            ASTNode detached_source = *sem->annotated_ast;
            foreign_source_denied = !hir_cfg_program_execution_current(hir,
                &detached_source);

            ASTNode *original_rir_root = rir->program_root;
            rir->program_root = NULL;
            air = air_synthesize(hir, dir, rir, &error);
            bool rootless_verified = air != NULL &&
                air_collect_dag_evidence(air, sem, &error) &&
                air_verify(air, &error) && air_drift_count(air) == 0;
            rir_root_denied = !rootless_verified;
            rir_root_not_regenerated = rir->program_root == NULL;
            air_destroy(air); air = NULL;
            free(error); error = NULL;
            rir->program_root = original_rir_root;

            air = air_synthesize(hir, dir, rir, &error);
            census_restored = hir_cfg_program_execution_current(hir,
                sem->annotated_ast) && air != NULL &&
                air_collect_dag_evidence(air, sem, &error) &&
                air_verify(air, &error) && air_drift_count(air) == 0;
        }
        printf("readfile original %s source baseline: shape=%d admitted=%d\n",
               baseline_sources[i].label, (int)source_shape, (int)admitted);
        printf("readfile original %s program census: missing-denied=%d no-regeneration=%d foreign-program=%d foreign-source=%d missing-rir-root=%d root-no-regeneration=%d restore=%d\n",
               baseline_sources[i].label, (int)census_denied,
               (int)census_not_regenerated, (int)foreign_program_denied,
               (int)foreign_source_denied, (int)rir_root_denied,
               (int)rir_root_not_regenerated, (int)census_restored);
        all_ok = all_ok && source_shape && admitted && census_denied &&
            census_not_regenerated && foreign_program_denied &&
            foreign_source_denied && rir_root_denied &&
            rir_root_not_regenerated && census_restored;
        air_destroy(air);
        free(error);
        rir_destroy(rir);
        hir_destroy(hir);
        dir_destroy(dir);
        semantic_result_destroy(sem);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
    for (size_t case_i = 0; case_i < sizeof(cases) / sizeof(cases[0]); case_i++) {
        Lexer *lexer = lexer_create(cases[case_i].source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *sem = NULL;
        DIRProgram *dir = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        AIRProgram *air = NULL;
        char *error = NULL;
        bool setup = false, baseline = false;
        bool masks_denied = true, masks_not_regenerated = true;
        bool masks_restored = true, masks_executed = false;
        if (program == NULL || parser_has_error(parser)) goto case_cleanup;
        sem = semantic_analyze(program);
        if (sem == NULL || !sem->success) goto case_cleanup;
        size_t statement_count = ast_program_statement_count(sem->annotated_ast);
        ASTNode *owner = statement_count == 0 ? NULL :
            ast_program_statement(sem->annotated_ast, statement_count - 1);
        dir = dir_lower(sem->annotated_ast, &error);
        hir = hir_lower(sem->annotated_ast, &error);
        rir = rir_lower(sem->annotated_ast, &error);
        if (owner == NULL || ast_node_stable_id(owner) == 0 || dir == NULL ||
            hir == NULL || rir == NULL ||
            !rir_enrich_with_hir_flow(rir, hir, &error) ||
            hir->item_count == 0 || hir->decl_count == 0 ||
            hir->routine_count == 0 || rir->scope_count == 0)
            goto case_cleanup;
        const HIRTopLevelItem *item = &hir->items[hir->item_count - 1];
        const HIRDecl *decl = &hir->decls[hir->decl_count - 1];
        const HIRRoutine *routine = &hir->routines[hir->routine_count - 1];
        const RIRScope *scope = &rir->scopes[rir->scope_count - 1];
        setup = item->ast == owner && decl->ast == owner &&
            decl->id == hir->decl_count - 1 &&
            decl->source_syntax_id == ast_node_stable_id(owner) &&
            routine->routine_id == hir->routine_count &&
            routine->ast != NULL && scope->ast != NULL &&
            routine->source_syntax_id == ast_node_stable_id(routine->ast) &&
            scope->source_syntax_id == ast_node_stable_id(scope->ast);
        if (cases[case_i].kind == 0) {
            setup = setup && owner->type == AST_FUNC_DECL &&
                hir->function_count != 0 &&
                hir->functions[hir->function_count - 1] == owner &&
                routine->ast == owner && routine->decl_id == decl->id &&
                scope->ast == owner && scope->kind == RIR_SCOPE_FUNCTION;
        } else if (cases[case_i].kind == 1) {
            setup = setup && owner->type == AST_INTENT_DECL &&
                hir->intent_count != 0 &&
                hir->intents[hir->intent_count - 1] == owner &&
                dir->node_count != 0 && dir->intent_count != 0 &&
                dir->nodes[dir->node_count - 1].ast == owner &&
                dir->intents[dir->intent_count - 1].ast == owner &&
                dir->intents[dir->intent_count - 1].node_id ==
                    dir->nodes[dir->node_count - 1].id &&
                routine->ast == owner && routine->decl_id == decl->id &&
                scope->ast == owner && scope->kind == RIR_SCOPE_INTENT;
        } else {
            setup = setup && owner->type == AST_CLASS_DECL &&
                routine->is_hosted && routine->declaration_ast == owner &&
                routine->decl_id == decl->id && scope->kind == RIR_SCOPE_METHOD &&
                scope->ast == routine->ast && scope->owner_name != NULL &&
                strcmp(scope->owner_name, "Hosted") == 0;
        }
        if (!setup) goto case_cleanup;
        air = air_synthesize(hir, dir, rir, &error);
        baseline = air != NULL && air_collect_dag_evidence(air, sem, &error) &&
            air_verify(air, &error) && air_drift_count(air) == 0;
        air_destroy(air); air = NULL;
        free(error); error = NULL;
        if (!baseline) goto case_cleanup;

        size_t original_function_count = hir->function_count;
        size_t original_hir_intent_count = hir->intent_count;
        size_t original_routine_count = hir->routine_count;
        size_t original_scope_count = rir->scope_count;
        size_t original_node_count = dir->node_count;
        size_t original_intent_count = dir->intent_count;
        unsigned mask_limit = cases[case_i].kind == 0 ? 8u :
            (cases[case_i].kind == 1 ? 32u : 4u);
        for (unsigned mask = 1; mask < mask_limit; mask++) {
            bool remove_function = cases[case_i].kind == 0 && (mask & 1u) != 0;
            bool remove_hir_intent = cases[case_i].kind == 1 && (mask & 1u) != 0;
            bool remove_dir_node = cases[case_i].kind == 1 && (mask & 2u) != 0;
            bool remove_dir_info = cases[case_i].kind == 1 && (mask & 4u) != 0;
            bool remove_routine =
                (cases[case_i].kind == 0 && (mask & 2u) != 0) ||
                (cases[case_i].kind == 1 && (mask & 8u) != 0) ||
                (cases[case_i].kind == 2 && (mask & 1u) != 0);
            bool remove_scope =
                (cases[case_i].kind == 0 && (mask & 4u) != 0) ||
                (cases[case_i].kind == 1 && (mask & 16u) != 0) ||
                (cases[case_i].kind == 2 && (mask & 2u) != 0);
            hir->function_count = original_function_count - (remove_function ? 1u : 0u);
            hir->intent_count = original_hir_intent_count -
                (remove_hir_intent ? 1u : 0u);
            dir->node_count = original_node_count - (remove_dir_node ? 1u : 0u);
            dir->intent_count = original_intent_count - (remove_dir_info ? 1u : 0u);
            hir->routine_count = original_routine_count - (remove_routine ? 1u : 0u);
            rir->scope_count = original_scope_count - (remove_scope ? 1u : 0u);
            air = air_synthesize(hir, dir, rir, &error);
            bool synthesized = air != NULL;
            bool collected = air != NULL && air_collect_dag_evidence(air, sem, &error);
            bool verified = collected && air_verify(air, &error) &&
                air_drift_count(air) == 0;
            bool denied = !verified;
            bool not_regenerated = hir->item_count != 0 && hir->decl_count != 0 &&
                hir->items[hir->item_count - 1].ast == owner &&
                hir->decls[hir->decl_count - 1].ast == owner &&
                hir->function_count == original_function_count -
                    (remove_function ? 1u : 0u) &&
                hir->intent_count == original_hir_intent_count -
                    (remove_hir_intent ? 1u : 0u) &&
                dir->node_count == original_node_count -
                    (remove_dir_node ? 1u : 0u) &&
                dir->intent_count == original_intent_count -
                    (remove_dir_info ? 1u : 0u) &&
                hir->routine_count == original_routine_count -
                    (remove_routine ? 1u : 0u) &&
                rir->scope_count == original_scope_count -
                    (remove_scope ? 1u : 0u);
            air_destroy(air); air = NULL;
            free(error); error = NULL;
            hir->function_count = original_function_count;
            hir->intent_count = original_hir_intent_count;
            dir->node_count = original_node_count;
            dir->intent_count = original_intent_count;
            hir->routine_count = original_routine_count;
            rir->scope_count = original_scope_count;
            air = air_synthesize(hir, dir, rir, &error);
            bool restored = air != NULL &&
                air_collect_dag_evidence(air, sem, &error) &&
                air_verify(air, &error) && air_drift_count(air) == 0;
            air_destroy(air); air = NULL;
            free(error); error = NULL;
            printf("readfile original %s inventory mask %u: synth=%d collect=%d verify=%d denied=%d no-regeneration=%d restore=%d\n",
                   cases[case_i].label, mask, (int)synthesized, (int)collected,
                   (int)verified, (int)denied, (int)not_regenerated, (int)restored);
            masks_denied = masks_denied && denied;
            masks_not_regenerated = masks_not_regenerated && not_regenerated;
            masks_restored = masks_restored && restored;
            masks_executed = true;
        }
case_cleanup:
        printf("readfile original %s inventory census: setup=%d baseline=%d executed=%d all-denied=%d no-regeneration=%d all-restored=%d\n",
               cases[case_i].label, (int)setup, (int)baseline,
               (int)masks_executed, (int)masks_denied,
               (int)masks_not_regenerated, (int)masks_restored);
        all_ok = all_ok && setup && baseline && masks_executed &&
            masks_denied && masks_not_regenerated && masks_restored;
        air_destroy(air);
        free(error);
        rir_destroy(rir);
        hir_destroy(hir);
        dir_destroy(dir);
        semantic_result_destroy(sem);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
    return all_ok;
}

static bool
test_air_rejects_foreign_dir_program_with_matching_numeric_root_id(void)
{
    const struct {
        const char *label;
        const char *source_a;
        const char *source_b;
        bool expects_readfile;
        bool expects_intent;
    } cases[] = {
        { "ReadFile Main versus empty DIR",
          "func Main()->Void{Log(ReadFile(\"tests/cases/backend_compare/string_io/io.txt\"));}",
          "", true, false },
        { "empty versus foreign empty DIR", "", "", false, false },
        { "intent versus foreign intent DIR",
          "func Main()->Void{}"
          "intent ToString(value: Int) { success: value == 7; failure: false; }",
          "func Main()->Void{}"
          "intent Other(value: Int) { success: value == 9; failure: false; }",
          false, true },
    };
    bool all_ok = true;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Lexer *lexer_a = lexer_create(cases[i].source_a);
        Lexer *lexer_b = lexer_create(cases[i].source_b);
        Parser *parser_a = parser_create(lexer_a);
        Parser *parser_b = parser_create(lexer_b);
        ASTNode *program_a = parser_parse_program(parser_a);
        ASTNode *program_b = parser_parse_program(parser_b);
        SemanticResult *sem_a = NULL, *sem_b = NULL;
        DIRProgram *dir_a = NULL, *dir_b = NULL;
        HIRProgram *hir_a = NULL, *hir_b = NULL;
        RIRProgram *rir_a = NULL, *rir_b = NULL;
        AIRProgram *air = NULL;
        char *error_a = NULL, *error_b = NULL;
        bool issued = false, baseline_a = false, baseline_b = false;
        bool cross_synthesized = false, cross_collected = false;
        bool cross_verified = false, cross_denied = false;
        bool original_origin_current = false, foreign_origin_denied = false;
        bool direct_join_ready = false, direct_join_denied = false;
        bool direct_join_restored = false;
        bool faults_executed = false, faults_denied = true;
        bool faults_not_regenerated = true, faults_restored = true;
        bool metadata_unchanged = false, restored = false;
        if (program_a == NULL || program_b == NULL ||
            parser_has_error(parser_a) || parser_has_error(parser_b))
            goto pair_cleanup;
        sem_a = semantic_analyze(program_a);
        sem_b = semantic_analyze(program_b);
        if (sem_a == NULL || sem_b == NULL || !sem_a->success || !sem_b->success)
            goto pair_cleanup;
        dir_a = dir_lower(sem_a->annotated_ast, &error_a);
        hir_a = hir_lower(sem_a->annotated_ast, &error_a);
        rir_a = rir_lower(sem_a->annotated_ast, &error_a);
        dir_b = dir_lower(sem_b->annotated_ast, &error_b);
        hir_b = hir_lower(sem_b->annotated_ast, &error_b);
        rir_b = rir_lower(sem_b->annotated_ast, &error_b);
        if (dir_a == NULL || hir_a == NULL || rir_a == NULL ||
            dir_b == NULL || hir_b == NULL || rir_b == NULL ||
            !rir_enrich_with_hir_flow(rir_a, hir_a, &error_a) ||
            !rir_enrich_with_hir_flow(rir_b, hir_b, &error_b))
            goto pair_cleanup;
        ASTNode *root_a = sem_a->annotated_ast;
        ASTNode *root_b = sem_b->annotated_ast;
        uint32_t root_id = ast_node_stable_id(root_a);
        size_t readfile_count = 0;
        for (size_t s = 0; s < rir_a->scope_count; s++)
            for (size_t op = 0; op < rir_a->scopes[s].op_count; op++) {
                const RIROp *row = &rir_a->scopes[s].ops[op];
                if (row->kind == RIR_OP_IO && row->subject != NULL &&
                    strcmp(row->subject, "ReadFile") == 0 && row->ast != NULL)
                    readfile_count++;
            }
        issued = root_a != root_b && root_id != 0 &&
            ast_node_stable_id(root_b) == root_id &&
            dir_a->source_program_syntax_id == root_id &&
            dir_b->source_program_syntax_id == root_id &&
            dir_a->program_root == root_a && dir_b->program_root == root_b &&
            dir_a->source_origin != NULL && dir_b->source_origin != NULL &&
            hir_a->source_program_syntax_id == root_id &&
            hir_b->source_program_syntax_id == root_id &&
            rir_a->program_root == root_a && rir_b->program_root == root_b &&
            dir_a->intent_count == (cases[i].expects_intent ? 1u : 0u) &&
            dir_b->intent_count == (cases[i].expects_intent ? 1u : 0u) &&
            readfile_count == (cases[i].expects_readfile ? 1u : 0u);
        if (!issued) goto pair_cleanup;
        air = air_synthesize(hir_a, dir_a, rir_a, &error_a);
        baseline_a = air != NULL && air_collect_dag_evidence(air, sem_a, &error_a) &&
            air_verify(air, &error_a) && air_drift_count(air) == 0;
        air_destroy(air); air = NULL;
        free(error_a); error_a = NULL;
        air = air_synthesize(hir_b, dir_b, rir_b, &error_b);
        baseline_b = air != NULL && air_collect_dag_evidence(air, sem_b, &error_b) &&
            air_verify(air, &error_b) && air_drift_count(air) == 0;
        air_destroy(air); air = NULL;
        free(error_b); error_b = NULL;
        if (!baseline_a || !baseline_b) goto pair_cleanup;

        original_origin_current = dir_source_program_current(dir_a, root_a) &&
            dir_source_program_current(dir_b, root_b);
        foreign_origin_denied = !dir_source_program_current(dir_b, root_a) &&
            !dir_source_program_current(dir_a, root_b);
        if (!original_origin_current) goto pair_cleanup;

        uint32_t original_dir_a_id = dir_a->source_program_syntax_id;
        uint32_t original_dir_b_id = dir_b->source_program_syntax_id;
        uint64_t original_dir_a_graph = dir_a->domain_graph_id;
        uint64_t original_dir_b_graph = dir_b->domain_graph_id;
        DIRNode *original_dir_a_nodes = dir_a->nodes;
        DIRNode *original_dir_b_nodes = dir_b->nodes;
        size_t original_dir_a_node_count = dir_a->node_count;
        size_t original_dir_b_node_count = dir_b->node_count;
        DIRIntentInfo *original_dir_a_intents = dir_a->intents;
        DIRIntentInfo *original_dir_b_intents = dir_b->intents;
        size_t original_dir_a_intent_count = dir_a->intent_count;
        size_t original_dir_b_intent_count = dir_b->intent_count;
        const ASTNode *original_dir_a_root = dir_a->program_root;
        const ASTNode *original_dir_b_root = dir_b->program_root;
        DIRSourceOriginWitness *original_dir_a_origin = dir_a->source_origin;
        DIRSourceOriginWitness *original_dir_b_origin = dir_b->source_origin;
        ASTNode *original_rir_a_root = rir_a->program_root;
        ASTNode *original_rir_b_root = rir_b->program_root;

        /* A foreign root has the same numeric syntax ID; only the issued
           program identity and its borrowed witness distinguish it. */
        faults_executed = true;
        for (size_t fault = 0; fault < 4; fault++) {
            if (fault == 0) dir_a->program_root = NULL;
            if (fault == 1) dir_a->program_root = root_b;
            if (fault == 2) dir_a->source_origin = NULL;
            if (fault == 3) dir_a->source_origin = original_dir_b_origin;
            bool current = dir_source_program_current(dir_a, root_a);
            air = air_synthesize(hir_a, dir_a, rir_a, &error_a);
            bool synthesized = air != NULL;
            bool collected = synthesized &&
                air_collect_dag_evidence(air, sem_a, &error_a);
            bool verified = collected && air_verify(air, &error_a) &&
                air_drift_count(air) == 0;
            bool unchanged = dir_a->program_root ==
                    (fault == 0 ? NULL : fault == 1 ? root_b : original_dir_a_root) &&
                dir_a->source_origin ==
                    (fault == 2 ? NULL : fault == 3 ? original_dir_b_origin :
                     original_dir_a_origin);
            printf("readfile foreign DIR %s fault=%zu: current=%d synth=%d collect=%d verify=%d unchanged=%d\n",
                   cases[i].label, fault, (int)current, (int)synthesized,
                   (int)collected, (int)verified, (int)unchanged);
            faults_denied = faults_denied && !current && !verified;
            faults_not_regenerated = faults_not_regenerated && unchanged;
            air_destroy(air); air = NULL;
            free(error_a); error_a = NULL;
            dir_a->program_root = original_dir_a_root;
            dir_a->source_origin = original_dir_a_origin;
            bool current_restored = dir_source_program_current(dir_a, root_a);
            air = air_synthesize(hir_a, dir_a, rir_a, &error_a);
            bool air_restored = air != NULL &&
                air_collect_dag_evidence(air, sem_a, &error_a) &&
                air_verify(air, &error_a) && air_drift_count(air) == 0;
            faults_restored = faults_restored && current_restored && air_restored;
            printf("readfile foreign DIR %s fault=%zu restore: current=%d AIR=%d\n",
                   cases[i].label, fault, (int)current_restored,
                   (int)air_restored);
            air_destroy(air); air = NULL;
            free(error_a); error_a = NULL;
        }

        /* An AIR projection built without RIR must still join that RIR with
           the same original DIR; an empty IO set cannot erase the owner. */
        air = air_synthesize(hir_a, dir_a, NULL, &error_a);
        direct_join_ready = air != NULL;
        direct_join_denied = direct_join_ready &&
            !air_collect_rir_evidence(air, rir_a, hir_a, dir_b, &error_a);
        air_destroy(air); air = NULL;
        free(error_a); error_a = NULL;
        air = air_synthesize(hir_a, dir_a, NULL, &error_a);
        direct_join_restored = air != NULL &&
            air_collect_rir_evidence(air, rir_a, hir_a, dir_a, &error_a) &&
            air_collect_dag_evidence(air, sem_a, &error_a) &&
            air_verify(air, &error_a) && air_drift_count(air) == 0;
        air_destroy(air); air = NULL;
        free(error_a); error_a = NULL;

        air = air_synthesize(hir_a, dir_b, rir_a, &error_a);
        cross_synthesized = air != NULL;
        cross_collected = air != NULL && air_collect_dag_evidence(air, sem_a, &error_a);
        cross_verified = cross_collected && air_verify(air, &error_a) &&
            air_drift_count(air) == 0;
        cross_denied = !cross_verified;
        air_destroy(air); air = NULL;
        free(error_a); error_a = NULL;
        metadata_unchanged = dir_a->source_program_syntax_id == original_dir_a_id &&
            dir_b->source_program_syntax_id == original_dir_b_id &&
            dir_a->program_root == original_dir_a_root &&
            dir_b->program_root == original_dir_b_root &&
            dir_a->source_origin == original_dir_a_origin &&
            dir_b->source_origin == original_dir_b_origin &&
            dir_a->domain_graph_id == original_dir_a_graph &&
            dir_b->domain_graph_id == original_dir_b_graph &&
            dir_a->nodes == original_dir_a_nodes &&
            dir_b->nodes == original_dir_b_nodes &&
            dir_a->node_count == original_dir_a_node_count &&
            dir_b->node_count == original_dir_b_node_count &&
            dir_a->intents == original_dir_a_intents &&
            dir_b->intents == original_dir_b_intents &&
            dir_a->intent_count == original_dir_a_intent_count &&
            dir_b->intent_count == original_dir_b_intent_count &&
            rir_a->program_root == original_rir_a_root &&
            rir_b->program_root == original_rir_b_root &&
            ast_node_stable_id(root_a) == root_id &&
            ast_node_stable_id(root_b) == root_id;
        air = air_synthesize(hir_a, dir_a, rir_a, &error_a);
        restored = air != NULL && air_collect_dag_evidence(air, sem_a, &error_a) &&
            air_verify(air, &error_a) && air_drift_count(air) == 0;
pair_cleanup:
        printf("readfile foreign DIR %s: issued=%d baseline-A=%d baseline-B=%d origin=%d foreign-origin-denied=%d faults=%d/%d/%d/%d join=%d/%d/%d cross-synth=%d cross-collect=%d cross-verify=%d denied=%d unchanged=%d restore=%d\n",
               cases[i].label, (int)issued, (int)baseline_a, (int)baseline_b,
               (int)original_origin_current, (int)foreign_origin_denied,
               (int)faults_executed, (int)faults_denied,
               (int)faults_not_regenerated, (int)faults_restored,
               (int)direct_join_ready, (int)direct_join_denied,
               (int)direct_join_restored,
               (int)cross_synthesized, (int)cross_collected,
               (int)cross_verified, (int)cross_denied,
               (int)metadata_unchanged, (int)restored);
        all_ok = all_ok && issued && baseline_a && baseline_b &&
            original_origin_current && foreign_origin_denied &&
            faults_executed && faults_denied && faults_not_regenerated &&
            faults_restored && direct_join_ready && direct_join_denied &&
            direct_join_restored && cross_denied && metadata_unchanged &&
            restored;
        air_destroy(air);
        free(error_a);
        free(error_b);
        rir_destroy(rir_a);
        rir_destroy(rir_b);
        hir_destroy(hir_a);
        hir_destroy(hir_b);
        dir_destroy(dir_a);
        dir_destroy(dir_b);
        semantic_result_destroy(sem_a);
        semantic_result_destroy(sem_b);
        ast_destroy(program_a);
        ast_destroy(program_b);
        parser_destroy(parser_a);
        parser_destroy(parser_b);
        lexer_destroy(lexer_a);
        lexer_destroy(lexer_b);
    }
    return all_ok;
}

static bool
test_air_late_evidence_keeps_original_program_identity(void)
{
    const char *sources[2] = {
        "func Main()->Void{Log(ReadFile(\"original-a.txt\"));}",
        "func Main()->Void{Log(ReadFile(\"original-b.txt\"));}"
    };
    const char *fault_names[] = {
        "foreign semantic", "semantic root relabeled", "missing semantic",
        "foreign MIR", "MIR source relabeled", "missing MIR snapshot", "missing MIR source"
    };
    Lexer *lexers[2] = {NULL, NULL};
    Parser *parsers[2] = {NULL, NULL};
    ASTNode *programs[2] = {NULL, NULL};
    SemanticResult *sems[2] = {NULL, NULL};
    HIRProgram *hirs[2] = {NULL, NULL};
    DIRProgram *dirs[2] = {NULL, NULL};
    RIRProgram *rirs[2] = {NULL, NULL};
    MIRProgram *mirs[2] = {NULL, NULL};
    AIRProgram *air = NULL;
    char *error = NULL;
    bool setup = true, baselines = true, faults = true, query_faults = true;
    for (size_t i = 0; i < 2; i++) {
        lexers[i] = lexer_create(sources[i]);
        parsers[i] = parser_create(lexers[i]);
        programs[i] = parser_parse_program(parsers[i]);
        if (programs[i] == NULL || parser_has_error(parsers[i])) { setup = false; break; }
        sems[i] = semantic_analyze(programs[i]);
        if (sems[i] == NULL || !sems[i]->success) { setup = false; break; }
        HIRSemanticProjectionFailure failure = HIR_SEMANTIC_PROJECTION_NONE;
        hirs[i] = hir_lower_with_semantic_facts(sems[i], &failure, &error);
        if (hirs[i] == NULL) { setup = false; break; }
        dirs[i] = dir_lower_with_hir_facts(sems[i]->annotated_ast, hirs[i], &error);
        rirs[i] = rir_lower(sems[i]->annotated_ast, &error);
        if (dirs[i] == NULL || rirs[i] == NULL ||
            !rir_enrich_with_hir_flow(rirs[i], hirs[i], &error)) { setup = false; break; }
        MIRLowerRequest request;
        mir_lower_request_init(&request, hirs[i], rirs[i], sems[i]);
        mir_lower_request_bind_dir(&request, dirs[i]);
        mirs[i] = mir_lower(&request, &error);
        if (mirs[i] == NULL) { setup = false; break; }
        air = air_synthesize(hirs[i], dirs[i], rirs[i], &error);
        bool own = air != NULL && air_source_program_current(air, programs[i]) &&
            air_mir_source_current(air, mirs[i]) &&
            air_collect_dag_evidence(air, sems[i], &error) &&
            air_collect_mir_evidence(air, mirs[i], &error) &&
            air_verify(air, &error) && air_drift_count(air) == 0;
        baselines = baselines && own;
        printf("late AIR original pair %zu: MIR=%d own=%d\n", i, mirs[i] != NULL, own);
        air_destroy(air); air = NULL;
        free(error); error = NULL;
    }
    if (!setup || !baselines) goto late_cleanup;
    setup = programs[0] != programs[1] &&
        ast_node_stable_id(programs[0]) != 0 &&
        ast_node_stable_id(programs[0]) == ast_node_stable_id(programs[1]);
    if (!setup) goto late_cleanup;
    for (size_t f = 0; f < sizeof(fault_names) / sizeof(fault_names[0]); f++) {
        air = air_synthesize(hirs[0], dirs[0], rirs[0], &error);
        if (air == NULL) { faults = false; break; }
        size_t evidence = air->evidence_count;
        AIREvidenceNode *nodes = air->evidence_nodes;
        size_t metadata = air->dag_metadata_evidence_count;
        uint32_t capabilities = air->program_capabilities;
        bool started = air->mir_evidence_collection_started;
        bool bound = air->mir_evidence_bound;
        bool certificate = air->verification_certificate_valid;
        uint64_t certificate_id = air->verification_certificate_fingerprint;
        ASTNode *semantic_root = sems[1]->annotated_ast;
        const ASTNode *mir_source = mirs[f == 4 ? 1 : 0]->collection_effect_source_program;
        struct PgyCollectionEffectSnapshot *mir_snapshot = mirs[0]->collection_effect_snapshot;
        if (f == 1) sems[1]->annotated_ast = programs[0];
        if (f == 4) mirs[1]->collection_effect_source_program = programs[0];
        if (f == 5) mirs[0]->collection_effect_snapshot = NULL;
        if (f == 6) mirs[0]->collection_effect_source_program = NULL;
        bool mir_query = f < 3 ? false : air_mir_source_current(air, f < 5 ? mirs[1] : mirs[0]);
        bool collected = f < 3 ? air_collect_dag_evidence(air, f == 2 ? NULL : sems[1], &error) :
            air_collect_mir_evidence(air, f < 5 ? mirs[1] : mirs[0], &error);
        bool unchanged = air->evidence_count == evidence && air->evidence_nodes == nodes &&
            air->dag_metadata_evidence_count == metadata && air->program_capabilities == capabilities &&
            air->mir_evidence_collection_started == started && air->mir_evidence_bound == bound &&
            air->verification_certificate_valid == certificate &&
            air->verification_certificate_fingerprint == certificate_id;
        bool no_regeneration = (f != 1 || sems[1]->annotated_ast == programs[0]) &&
            (f != 4 || mirs[1]->collection_effect_source_program == programs[0]) &&
            (f != 5 || mirs[0]->collection_effect_snapshot == NULL) &&
            (f != 6 || mirs[0]->collection_effect_source_program == NULL);
        sems[1]->annotated_ast = semantic_root;
        if (f == 4) mirs[1]->collection_effect_source_program = mir_source;
        if (f == 5) mirs[0]->collection_effect_snapshot = mir_snapshot;
        if (f == 6) mirs[0]->collection_effect_source_program = mir_source;
        free(error); error = NULL;
        /* Restore on this same AIR object: a failed import must not consume
           its one-shot slot or append any facts before the origin guard. */
        bool restored = air_collect_dag_evidence(air, sems[0], &error) &&
            air_collect_mir_evidence(air, mirs[0], &error) &&
            air_verify(air, &error) && air_drift_count(air) == 0;
        printf("late AIR %s: MIR-query=%d collected=%d unchanged=%d no-regeneration=%d same-object-restore=%d\n",
               fault_names[f], mir_query, collected, unchanged, no_regeneration, restored);
        faults = faults && !mir_query && !collected && unchanged && no_regeneration && restored;
        air_destroy(air); air = NULL;
        free(error); error = NULL;
    }
    air = air_synthesize(hirs[0], dirs[0], rirs[0], &error);
    AIRProgram *foreign = air_synthesize(hirs[1], dirs[1], rirs[1], &error);
    if (air == NULL || foreign == NULL) query_faults = false;
    if (air != NULL && foreign != NULL) {
        AIRSourceOriginWitness *origin = air->source_origin;
        uint32_t source_id = air->source_program_syntax_id;
        for (size_t f = 0; f < 3; f++) {
            if (f == 0) air->source_origin = NULL;
            if (f == 1) air->source_origin = foreign->source_origin;
            if (f == 2) air->source_program_syntax_id = source_id + 1;
            bool denied = !air_source_program_current(air, programs[0]);
            bool untouched = air->source_origin == (f == 0 ? NULL : f == 1 ? foreign->source_origin : origin) &&
                air->source_program_syntax_id == (f == 2 ? source_id + 1 : source_id);
            air->source_origin = origin;
            air->source_program_syntax_id = source_id;
            bool restored = air_source_program_current(air, programs[0]);
            printf("late AIR origin fault %zu: denied=%d unchanged=%d restore=%d\n",
                   f, denied, untouched, restored);
            query_faults = query_faults && denied && untouched && restored;
        }
        AIRProgram detached = *air;
        query_faults = query_faults && !air_source_program_current(&detached, programs[0]) &&
            air_source_program_current(air, programs[0]) &&
            !air_source_program_current(air, programs[1]);

        bool certified = air_collect_dag_evidence(air, sems[0], &error) &&
            air_collect_mir_evidence(air, mirs[0], &error) && air_verify(air, &error) &&
            pgy_air_evidence_certificate_ready(air, NULL);
        const char *plan_error = NULL;
        PgyVerifiedProjectionPlanRow projection = {0};
        PgyVerifiedParallelCapturePlan parallel = {0};
        bool own_plans = certified &&
            pgy_verified_projection_plan_intent_observability_with_air(
                air, mirs[0], PGY_PROJECTION_TARGET_C, &projection, &plan_error) &&
            pgy_verified_parallel_capture_plan_from_air(air, mirs[0], &parallel, &plan_error);
        pgy_verified_parallel_capture_plan_dispose(&parallel);
        bool foreign_projection = pgy_verified_projection_plan_intent_observability_with_air(
            air, mirs[1], PGY_PROJECTION_TARGET_C, &projection, &plan_error);
        bool foreign_parallel = pgy_verified_parallel_capture_plan_from_air(
            air, mirs[1], &parallel, &plan_error);
        pgy_verified_parallel_capture_plan_dispose(&parallel);
        bool plan_restore = pgy_air_evidence_certificate_ready(air, NULL) &&
            pgy_verified_projection_plan_intent_observability_with_air(
                air, mirs[0], PGY_PROJECTION_TARGET_C, &projection, &plan_error) &&
            pgy_verified_parallel_capture_plan_from_air(air, mirs[0], &parallel, &plan_error);
        pgy_verified_parallel_capture_plan_dispose(&parallel);
        printf("late AIR planners: certified=%d own=%d foreign-C=%d foreign-parallel=%d restore=%d\n",
               certified, own_plans, foreign_projection, foreign_parallel, plan_restore);
        query_faults = query_faults && certified && own_plans &&
            !foreign_projection && !foreign_parallel && plan_restore;
        uint64_t original_certificate = air->verification_certificate_fingerprint;
        bool original_certificate_valid = air->verification_certificate_valid;
        const char *certificate_faults[] = {
            "missing certificate", "missing origin", "foreign origin", "changed source ID"
        };
        for (size_t f = 0; certified && f < 4; f++) {
            if (f == 0) air->verification_certificate_valid = false;
            if (f == 1) air->source_origin = NULL;
            if (f == 2) air->source_origin = foreign->source_origin;
            if (f == 3) air->source_program_syntax_id = source_id + 1;
            bool ready = pgy_air_evidence_certificate_ready(air, NULL);
            bool projected = pgy_verified_projection_plan_intent_observability_with_air(
                air, mirs[0], PGY_PROJECTION_TARGET_C, &projection, &plan_error);
            bool parallel_ready = pgy_verified_parallel_capture_plan_from_air(
                air, mirs[0], &parallel, &plan_error);
            pgy_verified_parallel_capture_plan_dispose(&parallel);
            bool unchanged = air->source_origin ==
                    (f == 1 ? NULL : f == 2 ? foreign->source_origin : origin) &&
                air->source_program_syntax_id == (f == 3 ? source_id + 1 : source_id) &&
                air->verification_certificate_valid == (f == 0 ? false : original_certificate_valid) &&
                air->verification_certificate_fingerprint == original_certificate;
            air->source_origin = origin;
            air->source_program_syntax_id = source_id;
            air->verification_certificate_valid = original_certificate_valid;
            bool restored = pgy_air_evidence_certificate_ready(air, NULL) &&
                pgy_verified_projection_plan_intent_observability_with_air(
                    air, mirs[0], PGY_PROJECTION_TARGET_C, &projection, &plan_error) &&
                pgy_verified_parallel_capture_plan_from_air(air, mirs[0], &parallel, &plan_error);
            pgy_verified_parallel_capture_plan_dispose(&parallel);
            printf("late AIR certificate %s: ready=%d C=%d parallel=%d unchanged=%d restore=%d\n",
                   certificate_faults[f], ready, projected, parallel_ready, unchanged, restored);
            query_faults = query_faults && !ready && !projected && !parallel_ready && unchanged && restored;
        }
    }
    air_destroy(foreign);
late_cleanup:
    printf("late AIR original program: setup=%d baselines=%d collectors=%d queries=%d\n",
           setup, baselines, faults, query_faults);
    air_destroy(air);
    free(error);
    for (size_t i = 0; i < 2; i++) {
        mir_destroy(mirs[i]);
        rir_destroy(rirs[i]);
        dir_destroy(dirs[i]);
        hir_destroy(hirs[i]);
        semantic_result_destroy(sems[i]);
        ast_destroy(programs[i]);
        parser_destroy(parsers[i]);
        lexer_destroy(lexers[i]);
    }
    return setup && baselines && faults && query_faults;
}

static bool
test_air_post_certificate_mir_inventory_remains_current(void)
{
    const char *source =
        "func Main() -> Void {\n"
        "    let ch: Channel<Int> = Channel(1);\n"
        "    var x: Int = 1;\n"
        "    var seen: Int = -1;\n"
        "    parallel {\n"
        "        { x = x + 41; ch <- 1; }\n"
        "        { let go: Int = <- ch; seen = x + go - go; }\n"
        "    }\n"
        "}\n";
    const char *fault_names[] = {
        "instruction kind", "instruction source ID", "observability flag",
        "capture count", "capture row name", "capture row kind",
        "capture row writer task", "missing binding token",
        "collection not started", "collection not bound",
        "routine count exceeds storage", "instruction count exceeds storage",
        "missing capture boundary storage", "missing capture row storage",
        "capture boundary count exceeds storage",
        "capture row count exceeds storage"
    };
    Lexer *lexer = lexer_create(source);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *program = parser != NULL ? parser_parse_program(parser) : NULL;
    SemanticResult *sem = NULL;
    HIRProgram *hir = NULL;
    DIRProgram *dir = NULL;
    RIRProgram *rir = NULL;
    MIRProgram *mir = NULL;
    AIRProgram *air = NULL;
    MIRInstruction *instruction = NULL;
    MIRBasicBlock *instruction_block = NULL;
    MIRParallelCaptureBoundaryFact *boundary = NULL;
    MIRParallelCaptureDispositionRow *row = NULL;
    char *error = NULL;
    bool setup = program != NULL && !parser_has_error(parser);
    bool original = false;
    bool faults = true;

    if (setup) {
        sem = semantic_analyze(program);
        setup = sem != NULL && sem->success;
    }
    if (setup) {
        HIRSemanticProjectionFailure failure = HIR_SEMANTIC_PROJECTION_NONE;
        hir = hir_lower_with_semantic_facts(sem, &failure, &error);
        setup = hir != NULL;
    }
    if (setup) {
        dir = dir_lower_with_hir_facts(sem->annotated_ast, hir, &error);
        rir = rir_lower(sem->annotated_ast, &error);
        setup = dir != NULL && rir != NULL
            && rir_enrich_with_hir_flow(rir, hir, &error);
    }
    if (setup) {
        MIRLowerRequest request;
        mir_lower_request_init(&request, hir, rir, sem);
        mir_lower_request_bind_dir(&request, dir);
        mir = mir_lower(&request, &error);
        setup = mir != NULL;
    }
    if (setup) {
        air = air_synthesize(hir, dir, rir, &error);
        setup = air != NULL
            && air_collect_dag_evidence(air, sem, &error)
            && air_collect_mir_evidence(air, mir, &error)
            && air_verify(air, &error)
            && pgy_air_evidence_certificate_ready(air, NULL)
            && air_mir_source_current(air, mir)
            && air_mir_evidence_binding_current(air, mir);
    }
    if (setup && mir->parallel_capture_boundary_count == 1
        && mir->parallel_capture_boundaries != NULL) {
        boundary = &mir->parallel_capture_boundaries[0];
        if (boundary->sealed && boundary->task_count == 2
            && boundary->row_count == 1 && boundary->rows != NULL) {
            row = &boundary->rows[0];
            setup = row->name != NULL
                && row->kind == MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY
                && row->writer_task == 0
                && mir_validate_parallel_capture_facts(mir, NULL);
        } else {
            setup = false;
        }
    } else {
        setup = false;
    }
    if (setup) {
        for (size_t r = 0; r < mir->routine_count && instruction == NULL; r++) {
            MIRRoutine *routine = &mir->routines[r];
            for (size_t b = 0; b < routine->block_count && instruction == NULL; b++) {
                MIRBasicBlock *block = &routine->blocks[b];
                for (size_t i = 0; i < block->instruction_count; i++) {
                    if (block->instructions[i].kind == MIR_INST_DEF
                        && block->instructions[i].source_stable_id != 0) {
                        instruction = &block->instructions[i];
                        instruction_block = block;
                        break;
                    }
                }
            }
        }
        setup = instruction != NULL && instruction_block != NULL
            && mir->routine_count > 0
            && mir->has_inventory_surface_usage_facts;
    }
    if (setup) {
        PgyVerifiedProjectionPlanRow projection = {0};
        PgyVerifiedParallelCapturePlan parallel = {0};
        const char *plan_error = NULL;
        original = pgy_verified_projection_plan_intent_observability_with_air(
                air, mir, PGY_PROJECTION_TARGET_C, &projection, &plan_error)
            && pgy_verified_projection_plan_intent_observability_with_air(
                air, mir, PGY_PROJECTION_TARGET_LLVM, &projection, &plan_error)
            && pgy_verified_parallel_capture_plan_from_air(
                air, mir, &parallel, &plan_error)
            && parallel.row_count == 1;
        pgy_verified_parallel_capture_plan_dispose(&parallel);
    }
    printf("post-certificate original: setup=%d C-LLVM-parallel=%d capture-row=%d\n",
           setup, original, row != NULL);
    if (setup && original) {
        MIRInstKind saved_kind = instruction->kind;
        uint32_t saved_instruction_id = instruction->source_stable_id;
        bool saved_observability = mir->inventory_uses_intent_observability_surface;
        size_t saved_capture_count = mir->parallel_capture_boundary_count;
        size_t saved_row_count = boundary->row_count;
        char *saved_name = row->name;
        MIRParallelCaptureDispositionKind saved_row_kind = row->kind;
        size_t saved_writer_task = row->writer_task;
        const ASTNode *saved_source = mir->collection_effect_source_program;
        struct PgyCollectionEffectSnapshot *saved_snapshot =
            mir->collection_effect_snapshot;
        uint64_t saved_certificate = air->verification_certificate_fingerprint;
        bool saved_certificate_valid = air->verification_certificate_valid;
        uint64_t saved_binding = air->mir_evidence_binding_fingerprint;
        bool saved_started = air->mir_evidence_collection_started;
        bool saved_bound = air->mir_evidence_bound;
        size_t saved_routine_capacity = mir->routine_capacity;
        size_t saved_instruction_capacity = instruction_block->instruction_capacity;
        MIRParallelCaptureBoundaryFact *saved_boundaries =
            mir->parallel_capture_boundaries;
        MIRParallelCaptureDispositionRow *saved_rows = boundary->rows;

        for (size_t f = 0; f < sizeof(fault_names) / sizeof(fault_names[0]); f++) {
            if (f == 0) instruction->kind = MIR_INST_STMT;
            if (f == 1) instruction->source_stable_id =
                saved_instruction_id == UINT32_MAX ? 1 : saved_instruction_id + 1;
            if (f == 2) mir->inventory_uses_intent_observability_surface =
                !saved_observability;
            if (f == 3) mir->parallel_capture_boundary_count = 0;
            if (f == 4) row->name = "renamed_capture";
            if (f == 5) row->kind = MIR_PARALLEL_CAPTURE_JOIN_READONLY;
            if (f == 6) row->writer_task = 1;
            if (f == 7) air->mir_evidence_binding_fingerprint = 0;
            if (f == 8) air->mir_evidence_collection_started = false;
            if (f == 9) air->mir_evidence_bound = false;
            if (f == 10) mir->routine_capacity = mir->routine_count - 1;
            if (f == 11) instruction_block->instruction_capacity =
                instruction_block->instruction_count - 1;
            if (f == 12) mir->parallel_capture_boundaries = NULL;
            if (f == 13) boundary->rows = NULL;
            if (f == 14) mir->parallel_capture_boundary_count =
                mir->parallel_capture_boundary_capacity + 1;
            if (f == 15) boundary->row_count = boundary->row_capacity + 1;

            bool valid_capture_shape = mir_validate_parallel_capture_facts(mir, NULL);
            bool binding_current = air_mir_evidence_binding_current(air, mir);
            bool certificate_ready = pgy_air_evidence_certificate_ready(air, NULL);
            PgyVerifiedProjectionPlanRow projection_c = {0};
            PgyVerifiedProjectionPlanRow projection_llvm = {0};
            PgyVerifiedParallelCapturePlan parallel = {0};
            projection_c.projection_plan_id = 71;
            projection_llvm.projection_plan_id = 73;
            parallel.revision = 79;
            parallel.row_count = 83;
            unsigned char original_projection_c[sizeof(projection_c)];
            unsigned char original_projection_llvm[sizeof(projection_llvm)];
            unsigned char original_parallel[sizeof(parallel)];
            memcpy(original_projection_c, &projection_c, sizeof(projection_c));
            memcpy(original_projection_llvm, &projection_llvm, sizeof(projection_llvm));
            memcpy(original_parallel, &parallel, sizeof(parallel));
            const char *plan_error = NULL;
            bool projected_c =
                pgy_verified_projection_plan_intent_observability_with_air(
                    air, mir, PGY_PROJECTION_TARGET_C, &projection_c, &plan_error);
            bool projected_llvm =
                pgy_verified_projection_plan_intent_observability_with_air(
                    air, mir, PGY_PROJECTION_TARGET_LLVM, &projection_llvm, &plan_error);
            bool captured = pgy_verified_parallel_capture_plan_from_air(
                air, mir, &parallel, &plan_error);
            bool outputs_unchanged =
                memcmp(&projection_c, original_projection_c, sizeof(projection_c)) == 0
                && memcmp(&projection_llvm, original_projection_llvm,
                          sizeof(projection_llvm)) == 0
                && memcmp(&parallel, original_parallel, sizeof(parallel)) == 0;
            if (captured)
                pgy_verified_parallel_capture_plan_dispose(&parallel);
            bool unchanged = mir->collection_effect_source_program == saved_source
                && mir->collection_effect_snapshot == saved_snapshot
                && air->verification_certificate_valid == saved_certificate_valid
                && air->verification_certificate_fingerprint == saved_certificate
                && (f >= 7 && f <= 9 ? !certificate_ready : certificate_ready)
                && ((f != 0 || instruction->kind == MIR_INST_STMT)
                    && (f != 1 || instruction->source_stable_id != saved_instruction_id)
                    && (f != 2 || mir->inventory_uses_intent_observability_surface
                        != saved_observability)
                    && (f != 3 || mir->parallel_capture_boundary_count == 0)
                    && (f != 4 || row->name != saved_name)
                    && (f != 5 || row->kind == MIR_PARALLEL_CAPTURE_JOIN_READONLY)
                    && (f != 6 || row->writer_task == 1)
                    && (f != 7 || air->mir_evidence_binding_fingerprint == 0)
                    && (f != 8 || !air->mir_evidence_collection_started)
                    && (f != 9 || !air->mir_evidence_bound)
                    && (f != 10 || mir->routine_capacity < mir->routine_count)
                    && (f != 11 || instruction_block->instruction_capacity
                        < instruction_block->instruction_count)
                    && (f != 12 || mir->parallel_capture_boundaries == NULL)
                    && (f != 13 || boundary->rows == NULL)
                    && (f != 14 || mir->parallel_capture_boundary_count
                        > mir->parallel_capture_boundary_capacity)
                    && (f != 15 || boundary->row_count
                        > boundary->row_capacity));

            instruction->kind = saved_kind;
            instruction->source_stable_id = saved_instruction_id;
            mir->inventory_uses_intent_observability_surface = saved_observability;
            mir->parallel_capture_boundary_count = saved_capture_count;
            boundary->row_count = saved_row_count;
            row->name = saved_name;
            row->kind = saved_row_kind;
            row->writer_task = saved_writer_task;
            air->mir_evidence_binding_fingerprint = saved_binding;
            air->mir_evidence_collection_started = saved_started;
            air->mir_evidence_bound = saved_bound;
            mir->routine_capacity = saved_routine_capacity;
            instruction_block->instruction_capacity = saved_instruction_capacity;
            mir->parallel_capture_boundaries = saved_boundaries;
            boundary->rows = saved_rows;

            PgyVerifiedProjectionPlanRow projection = {0};
            PgyVerifiedParallelCapturePlan restored_parallel = {0};
            bool restored = mir_validate_parallel_capture_facts(mir, NULL)
                && pgy_air_evidence_certificate_ready(air, NULL)
                && air_mir_evidence_binding_current(air, mir)
                && pgy_verified_projection_plan_intent_observability_with_air(
                    air, mir, PGY_PROJECTION_TARGET_C, &projection, &plan_error)
                && pgy_verified_projection_plan_intent_observability_with_air(
                    air, mir, PGY_PROJECTION_TARGET_LLVM, &projection, &plan_error)
                && pgy_verified_parallel_capture_plan_from_air(
                    air, mir, &restored_parallel, &plan_error)
                && restored_parallel.row_count == 1;
            pgy_verified_parallel_capture_plan_dispose(&restored_parallel);
            printf("post-certificate %s: valid-capture=%d binding=%d ready=%d C=%d LLVM=%d parallel=%d outputs=%d unchanged=%d restore=%d\n",
                   fault_names[f], valid_capture_shape, binding_current,
                   certificate_ready, projected_c, projected_llvm, captured,
                   outputs_unchanged, unchanged, restored);
            faults = faults && (f < 12 ? valid_capture_shape : !valid_capture_shape)
                && !binding_current && !projected_c && !projected_llvm
                && !captured && outputs_unchanged && unchanged && restored;
        }
    } else {
        faults = false;
    }
    air_destroy(air);
    free(error);
    mir_destroy(mir);
    rir_destroy(rir);
    dir_destroy(dir);
    hir_destroy(hir);
    semantic_result_destroy(sem);
    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return setup && original && faults;
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
