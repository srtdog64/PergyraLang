static bool
lower_pipeline_from_source_ex(const char *source,
                              ASTNode **program_out,
                              HIRProgram **hir_out,
                              RIRProgram **rir_out,
                              MIRProgram **mir_out,
                              bool report_failure)
{
    bool ok = false;
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *program = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(program);
    char *hir_error = NULL;
    char *dir_error = NULL;
    char *rir_error = NULL;
    char *mir_error = NULL;
    DIRProgram *dir = NULL;

    *program_out = program;
    *hir_out = NULL;
    *rir_out = NULL;
    *mir_out = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success) {
        *hir_out = hir_lower_with_semantic_facts(sem, NULL, &hir_error);
        if (*hir_out != NULL) {
            dir = dir_lower_with_hir_resource_flow_facts(
                sem->annotated_ast, *hir_out, &dir_error);
        }
        *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (*hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
        if (*hir_out != NULL && dir != NULL && *rir_out != NULL
            && dir_validate(dir, &dir_error)) {
            MIRLowerRequest mir_request;
            mir_lower_request_init(&mir_request, *hir_out, *rir_out, sem);
            mir_lower_request_bind_dir(&mir_request, dir);
            *mir_out = mir_lower(&mir_request, &mir_error);
        }
    }

    ok = (*hir_out != NULL && *rir_out != NULL && *mir_out != NULL);
    if (!ok && report_failure) {
        if (hir_error != NULL)
            fprintf(stderr, "HIR lowering failed in test: %s\n", hir_error);
        if (dir_error != NULL)
            fprintf(stderr, "DIR lowering failed in test: %s\n", dir_error);
        if (rir_error != NULL)
            fprintf(stderr, "RIR lowering failed in test: %s\n", rir_error);
        if (mir_error != NULL)
            fprintf(stderr, "MIR lowering failed in test: %s\n", mir_error);
        if (*hir_out == NULL) fprintf(stderr, "HIR is NULL\n");
        if (*rir_out == NULL) fprintf(stderr, "RIR is NULL\n");
        if (*mir_out == NULL) fprintf(stderr, "MIR is NULL\n");
    }

    free(hir_error);
    free(dir_error);
    free(rir_error);
    free(mir_error);
    dir_destroy(dir);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return ok;
}

static bool
lower_pipeline_from_source(const char *source,
                           ASTNode **program_out,
                           HIRProgram **hir_out,
                           RIRProgram **rir_out,
                           MIRProgram **mir_out)
{
    return lower_pipeline_from_source_ex(source, program_out, hir_out, rir_out,
        mir_out, true);
}

static bool
lower_pipeline_from_source_quiet(const char *source,
                                 ASTNode **program_out,
                                 HIRProgram **hir_out,
                                 RIRProgram **rir_out,
                                 MIRProgram **mir_out)
{
    return lower_pipeline_from_source_ex(source, program_out, hir_out, rir_out,
        mir_out, false);
}

/* Backend emission tests must cross the same verified projection boundary as
 * production. The test owns a minimal AIR verification handle because these
 * cases exercise MIR-to-C shape, not AIR synthesis; target and machine facts
 * are still supplied by the real projection-plan owner. */
static PgyVerifiedParallelCapturePlan test_empty_parallel_capture_plan;

static bool
issue_test_c_projection_plan(const MIRProgram *mir,
                             PgyVerifiedProjectionPlanRow *plan,
                             const char **error)
{
    AIRProgram air = {0};

    air.has_hir_input = true;
    air.has_rir_input = true;
    air.has_mir_input = true;
    if (!pgy_air_evidence_certificate_issue(&air, error)
        || !pgy_verified_projection_plan_intent_observability_with_air(
            &air, mir, PGY_PROJECTION_TARGET_C, plan, error))
        return false;
    memset(&test_empty_parallel_capture_plan, 0,
        sizeof(test_empty_parallel_capture_plan));
    test_empty_parallel_capture_plan.revision =
        PGY_VERIFIED_PARALLEL_CAPTURE_PLAN_REVISION;
    test_empty_parallel_capture_plan.air_certificate_fingerprint =
        air.verification_certificate_fingerprint;
    test_empty_parallel_capture_plan.verified = true;
    test_empty_parallel_capture_plan.digest =
        pgy_verified_parallel_capture_plan_digest(
            &test_empty_parallel_capture_plan);
    return true;
}

/* Tests that never reach a spawn site cross the verified spawn-lane boundary
 * with an empty, verified plan; a case that does emit a spawn must register
 * its spawn nodes in its own plan (the emitter is fail-closed per site). */
static const PgySpawnLanePlan test_empty_spawn_lane_plan = {
    PGY_SPAWN_LANE_PLAN_REVISION, NULL, 0, true
};

static TranspileResult *
transpile_mir_with_test_evidence(const MIRProgram *mir,
                                 const char *output_path)
{
    PgyVerifiedProjectionPlanRow plan = {0};
    const char *error = NULL;

    if (!issue_test_c_projection_plan(mir, &plan, &error)) {
        TranspileResult *result = calloc(1, sizeof(*result));
        if (result != NULL) {
            result->success = false;
            result->error_message = pergyra_strdup(
                error != NULL ? error : "test projection plan failed");
        }
        return result;
    }
    return transpile_from_mir_with_projection_plans(mir, &plan,
        &test_empty_parallel_capture_plan, &test_empty_spawn_lane_plan,
        NULL, output_path);
}

static bool
bind_test_c_projection_plan(TranspilerCtx *ctx, const MIRProgram *mir)
{
    static PgyVerifiedProjectionPlanRow plan;
    const char *error = NULL;

    if (ctx == NULL || mir == NULL)
        return false;
    if (!issue_test_c_projection_plan(mir, &plan, &error)) {
        free(ctx->backend_error);
        ctx->backend_error = pergyra_strdup(
            error != NULL ? error : "test projection plan failed");
        return false;
    }
    ctx->projection_plan = &plan;
    ctx->parallel_capture_plan = &test_empty_parallel_capture_plan;
    ctx->spawn_lane_plan = &test_empty_spawn_lane_plan;
    return true;
}

static char *
read_file_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;
    char *data;
    if (f == NULL)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    data = calloc((size_t)size + 1, 1);
    if (data == NULL) {
        fclose(f);
        return NULL;
    }
    if (size > 0) {
        size_t read_bytes = fread(data, 1, (size_t)size, f);
        if (read_bytes != (size_t)size) {
            free(data);
            fclose(f);
            return NULL;
        }
    }
    fclose(f);
    return data;
}

static const ASTNode *
find_hir_function_by_name(const HIRProgram *hir, const char *name)
{
    if (hir == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < hir->function_count; i++) {
        const ASTNode *f = hir->functions[i];
        if (f != NULL && f->type == AST_FUNC_DECL
            && f->data.func_decl.name != NULL
            && strcmp(f->data.func_decl.name, name) == 0) {
            return f;
        }
    }
    return NULL;
}

static const ASTNode *
find_hir_intent_by_name(const HIRProgram *hir, const char *name)
{
    if (hir == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < hir->intent_count; i++) {
        const ASTNode *intent = hir->intents[i];
        if (intent != NULL && intent->type == AST_INTENT_DECL
            && intent->data.intent_decl.name != NULL
            && strcmp(intent->data.intent_decl.name, name) == 0) {
            return intent;
        }
    }
    return NULL;
}

static const MIRRoutine *
find_mir_routine_by_name(const MIRProgram *mir, const char *name)
{
    if (mir == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        if (routine->name != NULL && strcmp(routine->name, name) == 0)
            return routine;
    }
    return NULL;
}

static size_t
mir_count_reachable_non_cleanup_blocks(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block->is_reachable && !block->is_cleanup)
            count++;
    }
    return count;
}

static size_t
mir_count_phi_instructions(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_PHI)
                count++;
        }
    }
    return count;
}

static size_t
mir_count_exceptional_edges(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (!block->is_cleanup && block->is_reachable) {
            if (block->has_cleanup_succ)
                count++;
            if (block->has_rollback_succ)
                count++;
            if (block->has_invalidation_succ)
                count++;
        }
    }
    return count;
}

static bool
check_function_mir_emitability(const HIRProgram *hir, const MIRProgram *mir,
                              const char *function_name)
{
    const ASTNode *func_decl = find_hir_function_by_name(hir, function_name);
    char reason[512];
    if (func_decl == NULL) {
        fprintf(stderr, "  -> no HIR function '%s'\n", function_name);
        return false;
    }
    if (!transpiler_can_emit_function_from_mir_with_reason_for_test(func_decl,
            mir, reason, sizeof(reason))) {
        fprintf(stderr, "  -> MIR emitability rejected for function '%s': %s\n",
                function_name, reason);
        return false;
    }
    return true;
}

static bool
check_intent_mir_emitability(const HIRProgram *hir, const MIRProgram *mir,
                             const char *intent_name)
{
    const ASTNode *intent_decl = find_hir_intent_by_name(hir, intent_name);
    char reason[512];
    if (intent_decl == NULL) {
        fprintf(stderr, "  -> no HIR intent '%s'\n", intent_name);
        return false;
    }
    if (!transpiler_can_emit_intent_cleanup_from_mir_with_reason_for_test(intent_decl,
            mir, reason, sizeof(reason))) {
        fprintf(stderr, "  -> MIR emitability rejected for intent '%s': %s\n",
                intent_name, reason);
        return false;
    }
    return true;
}

static ASTNode *
make_generic_type(const char *name, const char *arg_name)
{
    ASTNode *n = ast_create_type(name);
    n->data.type.generic_args = calloc(1, sizeof(GenericParams));
    n->data.type.generic_args->count = 1;
    n->data.type.generic_args->params = calloc(1, sizeof(GenericParam *));

    GenericParam *gp = calloc(1, sizeof(GenericParam));
    gp->name = pergyra_strdup(arg_name);
    gp->constraint = ast_create_type(arg_name);
    n->data.type.generic_args->params[0] = gp;
    return n;
}

static ASTNode *
make_generic_type_from_node(const char *name, ASTNode *arg_type)
{
    ASTNode *n = ast_create_type(name);
    n->data.type.generic_args = calloc(1, sizeof(GenericParams));
    n->data.type.generic_args->count = 1;
    n->data.type.generic_args->params = calloc(1, sizeof(GenericParam *));

    GenericParam *gp = calloc(1, sizeof(GenericParam));
    gp->name = pergyra_strdup(arg_type->data.type.name);
    gp->constraint = arg_type;
    n->data.type.generic_args->params[0] = gp;
    return n;
}

static ASTNode *
make_match_case(ASTNode *pattern, ASTNode *body)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_MATCH_CASE;
    n->data.match_case.pattern = pattern;
    n->data.match_case.body = body;
    return n;
}

static ASTNode *
make_match(ASTNode *subject, ASTNode **cases, size_t case_count)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_MATCH_STMT;
    n->data.match_stmt.subject = subject;
    if (case_count > 0) {
        n->data.match_stmt.cases = calloc(case_count, sizeof(ASTNode *));
        memcpy(n->data.match_stmt.cases, cases, case_count * sizeof(ASTNode *));
    }
    n->data.match_stmt.case_count = case_count;
    return n;
}

static GenericParams *
make_generic_params1(const char *name)
{
    GenericParams *params = calloc(1, sizeof(GenericParams));
    GenericParam *param = calloc(1, sizeof(GenericParam));

    params->count = 1;
    params->params = calloc(1, sizeof(GenericParam *));
    param->name = pergyra_strdup(name);
    params->params[0] = param;
    return params;
}

/* -----------------------------------------------------------------
 * Helper: emit a single statement into a fresh context, return
 * the output string (caller does NOT free -> points into ctx->out).
 * ----------------------------------------------------------------- */

static const char *
emit_stmt_to_str(ASTNode *node, TranspilerCtx **out_ctx)
{
    TranspilerCtx *ctx = transpiler_ctx_create();
    emit_statement(node, ctx);
    *out_ctx = ctx;
    return ctx->out->data;
}

/* Build a temp file path using the test scratch dir owned by the Makefile. */
static void
make_tmp_path(char *buf, size_t bufsz, const char *filename)
{
    const char *tmpdir = getenv("PGY_TEST_TMPDIR");
    if (tmpdir == NULL || tmpdir[0] == '\0')
        tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL || tmpdir[0] == '\0') {
#ifdef _WIN32
        tmpdir = getenv("TEMP");
        if (tmpdir == NULL || tmpdir[0] == '\0')
            tmpdir = ".";
#else
        tmpdir = "/tmp";
#endif
    }
#ifdef _WIN32
    if (strcmp(tmpdir, "/tmp") == 0 || strcmp(tmpdir, "/tmp/") == 0)
        tmpdir = PGY_PROJECT_ROOT "/build/tmp";
#endif
    snprintf(buf, bufsz, "%s/%s", tmpdir, filename);
}

/* -----------------------------------------------------------------
 * Tests: CodeBuf
 * ----------------------------------------------------------------- */
