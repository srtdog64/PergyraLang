static bool
mir_block_slice_contains(const char *output, const char *label, const char *needle)
{
    const char *start;
    const char *end;
    const char *hit;
    size_t label_len;

    if (output == NULL || label == NULL || needle == NULL)
        return false;
    start = strstr(output, label);
    if (start == NULL)
        return false;
    label_len = strlen(label);
    /* Find next block label (but not within goto target of current block) */
    end = strstr(start + label_len, "\n    _pgy_mir_bb_");
    if (end == NULL)
        end = strstr(start + label_len, "\n_pgy_mir_bb_");
    if (end == NULL)
        end = output + strlen(output);
    hit = strstr(start, needle);
    return hit != NULL && hit < end;
}

static ASTNode **
test_active_inventory(MIRProgram *mir, ASTNodeType decl_type, size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (mir != NULL) {
        switch (decl_type) {
        case AST_ABILITY_DECL: nodes = mir->abilities; count = mir->ability_count; break;
        case AST_FUNC_DECL: nodes = mir->functions; count = mir->function_count; break;
        case AST_INTENT_DECL: nodes = mir->intents; count = mir->intent_count; break;
        case AST_ROLE_DECL: nodes = mir->roles; count = mir->role_count; break;
        case AST_PARTY_DECL: nodes = mir->parties; count = mir->party_count; break;
        case AST_ROSTER_DECL: nodes = mir->rosters; count = mir->roster_count; break;
        case AST_WORLD_DECL: nodes = mir->worlds; count = mir->world_count; break;
        case AST_RELATION_DECL: nodes = mir->relations; count = mir->relation_count; break;
        case AST_EFFECT_DECL: nodes = mir->effects; count = mir->effect_count; break;
        case AST_ZONE_DECL: nodes = mir->zones; count = mir->zone_count; break;
        case AST_EVENT_DECL: nodes = mir->events; count = mir->event_count; break;
        case AST_CLASS_DECL:
        case AST_ENUM_DECL:
        case AST_TYPE_ALIAS:
            nodes = mir->types; count = mir->type_count; break;
        default:
            break;
        }
    }

    if (count_out != NULL)
        *count_out = count;
    return nodes;
}

static const char *
test_decl_name(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        return decl->data.class_decl.name;
    case AST_ENUM_DECL:
        return decl->data.enum_decl.name;
    case AST_ABILITY_DECL:
        return decl->data.ability_decl.name;
    case AST_FUNC_DECL:
        return decl->data.func_decl.name;
    case AST_INTENT_DECL:
        return decl->data.intent_decl.name;
    case AST_ROLE_DECL:
        return decl->data.role_decl.name;
    case AST_PARTY_DECL:
        return decl->data.party_decl.name;
    case AST_ROSTER_DECL:
        return decl->data.roster_decl.name;
    case AST_WORLD_DECL:
        return decl->data.world_decl.name;
    case AST_RELATION_DECL:
        return decl->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return decl->data.effect_decl.name;
    case AST_ZONE_DECL:
        return decl->data.zone_decl.name;
    case AST_EVENT_DECL:
        return decl->data.event_decl.name;
    case AST_TYPE_ALIAS:
        return decl->data.type_alias.name;
    default:
        return NULL;
    }
}

static ASTNode *
find_test_decl(MIRProgram *mir, ASTNodeType decl_type, const char *name)
{
    ASTNode **nodes;
    size_t count = 0;

    if (mir == NULL || name == NULL)
        return NULL;

    nodes = test_active_inventory(mir, decl_type, &count);
    for (size_t i = 0; i < count; i++) {
        ASTNode *decl = nodes[i];
        const char *decl_name = test_decl_name(decl);
        if (decl_name != NULL && strcmp(decl_name, name) == 0)
            return decl;
    }

    return NULL;
}

/* -----------------------------------------------------------------
 * Minimal AST node builders (same helpers as test_semantic.c)
 * ----------------------------------------------------------------- */

static ASTNode *
make_number(double v, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type  = AST_NUMBER;
    n->line  = line;
    n->data.number.value = v;
    return n;
}

static ASTNode *
make_string_lit(const char *s, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_STRING;
    n->line = line;
    n->data.string.value = pergyra_strdup(s);
    return n;
}

static ASTNode *
make_identifier(const char *name, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_IDENTIFIER;
    n->line = line;
    n->data.identifier.name = pergyra_strdup(name);
    return n;
}

static ASTNode *
make_call(const char *callee_name, ASTNode **args, size_t arg_count,
           uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_CALL;
    n->line = line;
    n->data.call.callee    = make_identifier(callee_name, line);
    if (arg_count > 0) {
        n->data.call.arguments = calloc(arg_count, sizeof(ASTNode *));
        memcpy(n->data.call.arguments, args, arg_count * sizeof(ASTNode *));
    }
    n->data.call.arg_count = arg_count;
    return n;
}

static ASTNode *
make_call_generic1(const char *callee_name, const char *type_name,
                   ASTNode **args, size_t arg_count, uint32_t line)
{
    ASTNode *n = make_call(callee_name, args, arg_count, line);
    GenericParams *params = calloc(1, sizeof(GenericParams));
    GenericParam *param = calloc(1, sizeof(GenericParam));
    params->count = 1;
    params->params = calloc(1, sizeof(GenericParam *));
    param->name = pergyra_strdup(type_name);
    param->constraint = ast_create_type(type_name);
    params->params[0] = param;
    n->data.call.generic_args = params;
    return n;
}

static ASTNode *
make_type_node(const char *type_name)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_TYPE;
    n->data.type.name = pergyra_strdup(type_name);
    return n;
}

static FuncParam *
make_func_param(const char *name, const char *type_name)
{
    FuncParam *param = calloc(1, sizeof(FuncParam));
    param->name = pergyra_strdup(name);
    param->type = make_type_node(type_name);
    return param;
}

static ClassField *
make_class_field(const char *name, const char *type_name)
{
    ClassField *field = calloc(1, sizeof(ClassField));
    field->name = pergyra_strdup(name);
    field->type = make_type_node(type_name);
    return field;
}

static ASTNode *
make_let(const char *name, ASTNode *ann, ASTNode *init, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_LET_DECL;
    n->line = line;
    n->data.let_decl.name        = pergyra_strdup(name);
    n->data.let_decl.type        = ann;
    n->data.let_decl.initializer = init;
    return n;
}

static ASTNode *
make_return(ASTNode *value, uint32_t line)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_RETURN;
    n->line = line;
    n->data.return_stmt.value = value;
    return n;
}

static ASTNode *
make_block(ASTNode **stmts, size_t count)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_BLOCK;
    if (count > 0) {
        n->data.block.statements = calloc(count, sizeof(ASTNode *));
        memcpy(n->data.block.statements, stmts, count * sizeof(ASTNode *));
    }
    n->data.block.count      = count;
    return n;
}

static ASTNode *
make_program(ASTNode **stmts, size_t count)
{
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = AST_PROGRAM;
    if (count > 0) {
        n->data.program.statements = calloc(count, sizeof(ASTNode *));
        memcpy(n->data.program.statements, stmts, count * sizeof(ASTNode *));
    }
    n->data.program.count      = count;
    return n;
}

static MIRProgram *
mir_program_from_ast(ASTNode *program);

static MIRProgram *
lower_program_to_mir_ex(ASTNode *program,
                        HIRProgram **hir_out,
                        RIRProgram **rir_out,
                        bool allow_synthetic_fallback)
{
    SemanticResult *sem = semantic_analyze(program);
    char *hir_error = NULL;
    char *rir_error = NULL;
    char *mir_error = NULL;
    MIRProgram *mir = NULL;

    if (hir_out != NULL)
        *hir_out = NULL;
    if (rir_out != NULL)
        *rir_out = NULL;

    if (sem != NULL && sem->success) {
        if (hir_out != NULL)
            *hir_out = hir_lower(sem->annotated_ast, &hir_error);
        if (rir_out != NULL)
            *rir_out = rir_lower(sem->annotated_ast, &rir_error);
        if (hir_out != NULL && rir_out != NULL
            && *hir_out != NULL && *rir_out != NULL)
            (void)rir_enrich_with_hir_flow(*rir_out, *hir_out, &rir_error);
        if (hir_out != NULL && rir_out != NULL
            && *hir_out != NULL && *rir_out != NULL)
            mir = mir_lower(*hir_out, *rir_out, sem, &mir_error);
    }

    if (mir == NULL && allow_synthetic_fallback) {
        /* Some inventory-driven transpile tests only need a synthetic MIR view
         * of the declarations. Keep the fallback, but do not leak debug stderr
         * into otherwise successful regression output. */
        mir = mir_program_from_ast(program);
    }

    free(hir_error);
    free(rir_error);
    free(mir_error);
    g_last_mir = mir;
    return mir;
}

static MIRProgram *
lower_program_to_mir(ASTNode *program, HIRProgram **hir_out, RIRProgram **rir_out)
{
    return lower_program_to_mir_ex(program, hir_out, rir_out, true);
}

static MIRProgram *
lower_program_to_mir_strict(ASTNode *program,
                            HIRProgram **hir_out,
                            RIRProgram **rir_out)
{
    return lower_program_to_mir_ex(program, hir_out, rir_out, false);
}

static MIRProgram *
mir_program_from_ast(ASTNode *program)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return NULL;

    MIRProgram *mir = calloc(1, sizeof(MIRProgram));
    if (mir == NULL)
        return NULL;

    size_t stmt_count = program->data.program.count;
    ASTNode **stmts = program->data.program.statements;

    for (size_t i = 0; i < stmt_count; i++) {
        ASTNode *stmt = stmts[i];
        if (stmt == NULL)
            continue;
        switch (stmt->type) {
        case AST_EXTERN_BLOCK:  mir->extern_count++; break;
        case AST_CLASS_DECL:
        case AST_ENUM_DECL:     mir->type_count++; break;
        case AST_FUNC_DECL:     mir->function_count++; break;
        case AST_INTENT_DECL:   mir->intent_count++; break;
        case AST_ABILITY_DECL:  mir->ability_count++; break;
        case AST_ROLE_DECL:     mir->role_count++; break;
        case AST_PARTY_DECL:    mir->party_count++; break;
        case AST_ROSTER_DECL:   mir->roster_count++; break;
        case AST_WORLD_DECL:    mir->world_count++; break;
        case AST_RELATION_DECL: mir->relation_count++; break;
        case AST_EFFECT_DECL:   mir->effect_count++; break;
        case AST_ZONE_DECL:     mir->zone_count++; break;
        case AST_EVENT_DECL:    mir->event_count++; break;
        default: break;
        }
    }

    if (mir->extern_count)  mir->externs  = calloc(mir->extern_count, sizeof(ASTNode *));
    if (mir->type_count)    mir->types    = calloc(mir->type_count, sizeof(ASTNode *));
    if (mir->function_count)mir->functions= calloc(mir->function_count, sizeof(ASTNode *));
    if (mir->intent_count)  mir->intents  = calloc(mir->intent_count, sizeof(ASTNode *));
    if (mir->ability_count) mir->abilities= calloc(mir->ability_count, sizeof(ASTNode *));
    if (mir->role_count)    mir->roles    = calloc(mir->role_count, sizeof(ASTNode *));
    if (mir->party_count)   mir->parties  = calloc(mir->party_count, sizeof(ASTNode *));
    if (mir->roster_count)  mir->rosters  = calloc(mir->roster_count, sizeof(ASTNode *));
    if (mir->world_count)   mir->worlds   = calloc(mir->world_count, sizeof(ASTNode *));
    if (mir->relation_count)mir->relations= calloc(mir->relation_count, sizeof(ASTNode *));
    if (mir->effect_count)  mir->effects  = calloc(mir->effect_count, sizeof(ASTNode *));
    if (mir->zone_count)    mir->zones    = calloc(mir->zone_count, sizeof(ASTNode *));
    if (mir->event_count)   mir->events   = calloc(mir->event_count, sizeof(ASTNode *));

    size_t ext_i = 0, type_i = 0, func_i = 0, intent_i = 0;
    size_t ability_i = 0, role_i = 0, party_i = 0, roster_i = 0;
    size_t world_i = 0, relation_i = 0, effect_i = 0, zone_i = 0, event_i = 0;

    for (size_t i = 0; i < stmt_count; i++) {
        ASTNode *stmt = stmts[i];
        if (stmt == NULL)
            continue;
        switch (stmt->type) {
        case AST_EXTERN_BLOCK:  mir->externs[ext_i++] = stmt; break;
        case AST_CLASS_DECL:
        case AST_ENUM_DECL:     mir->types[type_i++] = stmt; break;
        case AST_FUNC_DECL:
            mir->functions[func_i++] = stmt;
            if (stmt->data.func_decl.name != NULL
                && strcmp(stmt->data.func_decl.name, "Main") == 0)
                mir->has_main_function = true;
            break;
        case AST_INTENT_DECL:   mir->intents[intent_i++] = stmt; break;
        case AST_ABILITY_DECL:  mir->abilities[ability_i++] = stmt; break;
        case AST_ROLE_DECL:     mir->roles[role_i++] = stmt; break;
        case AST_PARTY_DECL:    mir->parties[party_i++] = stmt; break;
        case AST_ROSTER_DECL:   mir->rosters[roster_i++] = stmt; break;
        case AST_WORLD_DECL:    mir->worlds[world_i++] = stmt; break;
        case AST_RELATION_DECL: mir->relations[relation_i++] = stmt; break;
        case AST_EFFECT_DECL:   mir->effects[effect_i++] = stmt; break;
        case AST_ZONE_DECL:     mir->zones[zone_i++] = stmt; break;
        case AST_EVENT_DECL:    mir->events[event_i++] = stmt; break;
        default: break;
        }
    }

    for (size_t i = 0; i < stmt_count; i++) {
        ASTNode *stmt = stmts[i];
        if (stmt != NULL && !mir_record_decl_header(mir, stmt)) {
            mir_destroy(mir);
            return NULL;
        }
    }

    return mir;
}

static MIRProgram *
mir_program_from_decl_for_test(ASTNode *decl)
{
    ASTNode program;
    ASTNode *stmts[1] = { decl };

    memset(&program, 0, sizeof(program));
    program.type = AST_PROGRAM;
    program.data.program.statements = stmts;
    program.data.program.count = decl != NULL ? 1 : 0;
    return mir_program_from_ast(&program);
}

static TranspilerCtx *
transpiler_ctx_create_for_test(void)
{
    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx != NULL && g_last_mir != NULL)
        ctx->mir = g_last_mir;
    return ctx;
}

#define transpiler_ctx_create transpiler_ctx_create_for_test
