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

static bool
semantic_ctx_has_embedded_world_zone_name(SemanticContext *ctx, const char *name)
{
    return semantic_ctx_embedded_world_zone_index(ctx, name) != (size_t)-1;
}

static const char *
semantic_ctx_embedded_world_zone_world_name(SemanticContext *ctx, const char *name)
{
    size_t index = semantic_ctx_embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_world_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_world_names[index];
}

static const char *
semantic_ctx_embedded_world_zone_slot_name(SemanticContext *ctx, const char *name)
{
    size_t index = semantic_ctx_embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_slot_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_slot_names[index];
}

static void
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
    if (ctx->scope == NULL || ctx->diagnostics == NULL) {
        scope_destroy(ctx->scope);
        free(ctx->diagnostics);
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
    for (size_t i = 0; i < ctx->alias_resolution_count; i++)
        free(ctx->alias_resolution_stack[i]);
    free(ctx->alias_resolution_stack);
    for (size_t i = 0; i < ctx->type_resolution_graph.node_count; i++)
        free(ctx->type_resolution_graph.nodes[i].label);
    free(ctx->type_resolution_graph.nodes);
    for (size_t i = 0; i < ctx->type_resolution_graph.edge_count; i++)
        free(ctx->type_resolution_graph.edges[i].reason);
    free(ctx->type_resolution_graph.edges);
    free(ctx->resolve_type_cache.keys);
    free(ctx->resolve_type_cache.values);
    free(ctx->diagnostics);
    free(ctx);
}

/* -----------------------------------------------------------------
 * Diagnostic emission
 * ----------------------------------------------------------------- */

static void
emit_diagnostic(SemanticContext *ctx, DiagnosticLevel level,
                 const char *code, const ASTNode *node,
                 const char *fmt, va_list ap)
{
    char *message = NULL;
    va_list ap_copy;
    int len;
    va_copy(ap_copy, ap);
    len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);
    if (len < 0)
        return;
    message = malloc((size_t)len + 1);
    if (message == NULL)
        return;
    vsnprintf(message, (size_t)len + 1, fmt, ap);

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        Diagnostic *existing = ctx->diagnostics[i];
        if (existing == NULL)
            continue;
        if (existing->level != level)
            continue;
        if (existing->line != (node ? node->line : 0)
            || existing->col != (node ? node->column : 0)) {
            continue;
        }
        if (existing->message != NULL && strcmp(existing->message, message) == 0) {
            /* Upgrade legacy entry's code if we now have one to attach. */
            if (existing->code == NULL && code != NULL)
                existing->code = code;
            free(message);
            return;
        }
    }

    if (ctx->diagnostic_count >= ctx->diagnostic_capacity) {
        size_t new_cap = ctx->diagnostic_capacity * 2;
        Diagnostic **grown = realloc(ctx->diagnostics,
                                     new_cap * sizeof(Diagnostic *));
        if (grown == NULL)
            return;
        ctx->diagnostics         = grown;
        ctx->diagnostic_capacity = new_cap;
    }

    Diagnostic *d = calloc(1, sizeof(Diagnostic));
    if (d == NULL)
        return;

    d->level = level;
    d->line  = node ? node->line   : 0;
    d->col   = node ? node->column : 0;
    d->message = message;
    d->code    = code;  /* non-owning; points to static string or NULL */

    ctx->diagnostics[ctx->diagnostic_count++] = d;

    if (level == DIAG_ERROR)
        ctx->has_error = true;
}

void
semantic_error(SemanticContext *ctx, const ASTNode *node,
               const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_ERROR, NULL, node, fmt, ap);
    va_end(ap);
}

void
semantic_error_code(SemanticContext *ctx, const char *code,
                    const ASTNode *node, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_ERROR, code, node, fmt, ap);
    va_end(ap);
}

void
semantic_warning(SemanticContext *ctx, const ASTNode *node,
                  const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_WARNING, NULL, node, fmt, ap);
    va_end(ap);
}

void
semantic_warning_code(SemanticContext *ctx, const char *code,
                      const ASTNode *node, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic(ctx, DIAG_WARNING, code, node, fmt, ap);
    va_end(ap);
}

void
semantic_print_diagnostics(SemanticContext *ctx)
{
    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        Diagnostic *d = ctx->diagnostics[i];
        const char *level = (d->level == DIAG_ERROR) ? "ERROR" : "WARNING";
        if (d->line == 0 || d->col == 0) {
            fprintf(stderr, "[%s] - %s\n", level, d->message);
        } else {
            fprintf(stderr, "[%s] %u:%u - %s\n",
                    level, d->line, d->col, d->message);
        }
    }
}

/* -----------------------------------------------------------------
 * Utility — resolve AST type node to Type*
 * ----------------------------------------------------------------- */

static ASTNode *
