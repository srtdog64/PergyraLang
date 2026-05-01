static char *
tc_strdup_fmt(const char *fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) { va_end(ap2); return NULL; }
    char *buf = malloc((size_t)len + 1);
    if (buf != NULL) vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static size_t
semantic_ctx_embedded_world_zone_index(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return (size_t)-1;
    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++) {
        if (ctx->embedded_world_zone_names[i] != NULL
            && strcmp(ctx->embedded_world_zone_names[i], name) == 0) {
            return i;
        }
    }
    return (size_t)-1;
}

__attribute__((unused))
static bool
semantic_ctx_has_embedded_world_zone_name(SemanticContext *ctx, const char *name)
{
    return semantic_ctx_embedded_world_zone_index(ctx, name) != (size_t)-1;
}

__attribute__((unused))
static const char *
semantic_ctx_embedded_world_zone_world_name(SemanticContext *ctx, const char *name)
{
    size_t index = semantic_ctx_embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_world_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_world_names[index];
}

__attribute__((unused))
static const char *
semantic_ctx_embedded_world_zone_slot_name(SemanticContext *ctx, const char *name)
{
    size_t index = semantic_ctx_embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_slot_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_slot_names[index];
}

void
semantic_ctx_mark_embedded_world_zone_name(SemanticContext *ctx,
                                           const char *name,
                                           const char *world_name,
                                           const char *slot_name)
{
    char **grown;
    size_t index;

    if (ctx == NULL || name == NULL || *name == '\0')
        return;
    index = semantic_ctx_embedded_world_zone_index(ctx, name);
    if (index != (size_t)-1) {
        if (world_name != NULL && *world_name != '\0'
            && ctx->embedded_world_zone_world_names[index] == NULL) {
            ctx->embedded_world_zone_world_names[index] = pergyra_strdup(world_name);
        }
        if (slot_name != NULL && *slot_name != '\0'
            && ctx->embedded_world_zone_slot_names[index] == NULL) {
            ctx->embedded_world_zone_slot_names[index] = pergyra_strdup(slot_name);
        }
        return;
    }

    if (ctx->embedded_world_zone_count >= ctx->embedded_world_zone_capacity) {
        size_t new_cap = ctx->embedded_world_zone_capacity == 0
            ? 8
            : ctx->embedded_world_zone_capacity * 2;
        grown = realloc(ctx->embedded_world_zone_names, new_cap * sizeof(char *));
        if (grown == NULL)
            return;
        ctx->embedded_world_zone_names = grown;
        grown = realloc(ctx->embedded_world_zone_world_names, new_cap * sizeof(char *));
        if (grown == NULL)
            return;
        ctx->embedded_world_zone_world_names = grown;
        grown = realloc(ctx->embedded_world_zone_slot_names, new_cap * sizeof(char *));
        if (grown == NULL)
            return;
        ctx->embedded_world_zone_slot_names = grown;
        ctx->embedded_world_zone_capacity = new_cap;
    }

    ctx->embedded_world_zone_names[ctx->embedded_world_zone_count] =
        pergyra_strdup(name);
    ctx->embedded_world_zone_world_names[ctx->embedded_world_zone_count] =
        (world_name != NULL && *world_name != '\0') ? pergyra_strdup(world_name) : NULL;
    ctx->embedded_world_zone_slot_names[ctx->embedded_world_zone_count] =
        (slot_name != NULL && *slot_name != '\0') ? pergyra_strdup(slot_name) : NULL;
    ctx->embedded_world_zone_count++;
}

/* -----------------------------------------------------------------
 * Context lifecycle
 * ----------------------------------------------------------------- */

SemanticContext *
semantic_context_create(void)
{
    SemanticContext *ctx = calloc(1, sizeof(SemanticContext));
    if (ctx == NULL)
        return NULL;

    ctx->scope               = scope_create(NULL, SCOPE_GLOBAL);
    ctx->diagnostic_capacity = INITIAL_DIAG_CAPACITY;
    ctx->diagnostics         = calloc(INITIAL_DIAG_CAPACITY,
                                      sizeof(Diagnostic *));
    pgy_arena_init(&ctx->scratch_arena, 0);
    if (ctx->scope == NULL || ctx->diagnostics == NULL) {
        scope_destroy(ctx->scope);
        free(ctx->diagnostics);
        pgy_arena_destroy(&ctx->scratch_arena);
        free(ctx);
        return NULL;
    }

    /* Register built-in types in global scope */
    type_system_init();

    return ctx;
}

void
semantic_context_destroy(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;

    scope_destroy(ctx->scope);

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        free(ctx->diagnostics[i]->message);
        diag_payload_snapshot_destroy(ctx->diagnostics[i]->payload);
        free(ctx->diagnostics[i]);
    }
    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++)
        free(ctx->embedded_world_zone_names[i]);
    free(ctx->embedded_world_zone_names);
    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++)
        free(ctx->embedded_world_zone_world_names[i]);
    free(ctx->embedded_world_zone_world_names);
    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++)
        free(ctx->embedded_world_zone_slot_names[i]);
    free(ctx->embedded_world_zone_slot_names);
    for (size_t i = 0; i < ctx->type_resolution_stage_alias_diagnostic_name_count; i++)
        free(ctx->type_resolution_stage_alias_diagnostic_names[i]);
    free(ctx->type_resolution_stage_alias_diagnostic_names);
    for (size_t i = 0; i < ctx->type_resolution_graph.node_count; i++)
        free(ctx->type_resolution_graph.nodes[i].label);
    free(ctx->type_resolution_graph.nodes);
    for (size_t i = 0; i < ctx->type_resolution_graph.edge_count; i++)
        free(ctx->type_resolution_graph.edges[i].reason);
    free(ctx->type_resolution_graph.edges);
    semantic_type_resolution_free_metadata(ctx);
    free(ctx->diagnostics);
    pgy_arena_destroy(&ctx->scratch_arena);
    free(ctx);
}

/* -----------------------------------------------------------------
 * Utility — resolve AST type node to Type*
 * ----------------------------------------------------------------- */
