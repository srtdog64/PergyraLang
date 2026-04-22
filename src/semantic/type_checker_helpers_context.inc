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
    pgy_arena_destroy(&ctx->scratch_arena);
    free(ctx);
}

/* -----------------------------------------------------------------
 * Diagnostic emission
 * ----------------------------------------------------------------- */

static void
diag_payload_snapshot_destroy_fields(DiagnosticPayloadSnapshot *snapshot)
{
    if (snapshot == NULL)
        return;
    free(snapshot->value_label);
    free(snapshot->provenance_label);
    free(snapshot->replacement_label);
    free(snapshot->transfer_label);
    free(snapshot->borrowed_name);
    free(snapshot->consumer_name);
    free(snapshot->secondary_name);
    free(snapshot->kind_label);
    free(snapshot->extra);
}

DiagnosticPayloadSnapshot *
diag_payload_snapshot_create(const DiagPayload *p)
{
    DiagnosticPayloadSnapshot *snapshot;

    if (p == NULL)
        return NULL;

    snapshot = calloc(1, sizeof(*snapshot));
    if (snapshot == NULL)
        return NULL;

    snapshot->value_label = p->value_label != NULL ? pergyra_strdup(p->value_label) : NULL;
    snapshot->provenance_label = p->provenance_label != NULL ? pergyra_strdup(p->provenance_label) : NULL;
    snapshot->replacement_label = p->replacement_label != NULL ? pergyra_strdup(p->replacement_label) : NULL;
    snapshot->transfer_label = p->transfer_label != NULL ? pergyra_strdup(p->transfer_label) : NULL;
    snapshot->borrowed_name = p->borrowed_name != NULL ? pergyra_strdup(p->borrowed_name) : NULL;
    snapshot->consumer_name = p->consumer_name != NULL ? pergyra_strdup(p->consumer_name) : NULL;
    snapshot->secondary_name = p->secondary_name != NULL ? pergyra_strdup(p->secondary_name) : NULL;
    snapshot->kind_label = p->kind_label != NULL ? pergyra_strdup(p->kind_label) : NULL;
    snapshot->extra = p->extra != NULL ? pergyra_strdup(p->extra) : NULL;

    return snapshot;
}

void
diag_payload_snapshot_destroy(DiagnosticPayloadSnapshot *snapshot)
{
    if (snapshot == NULL)
        return;
    diag_payload_snapshot_destroy_fields(snapshot);
    free(snapshot);
}

static void
emit_diagnostic_full(SemanticContext *ctx, DiagnosticLevel level,
                      const char *code, const char *cause_ir,
                      const char *fix_source, const ASTNode *node,
                      const DiagPayload *payload,
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
            /* Upgrade legacy entry's metadata if the new call provides
             * fields the existing entry lacks. Keeps richer routing info
             * when a plain semantic_error hits the same site first. */
            if (existing->code == NULL && code != NULL)
                existing->code = code;
            if (existing->cause_ir == NULL && cause_ir != NULL)
                existing->cause_ir = cause_ir;
            if (existing->fix_source == NULL && fix_source != NULL)
                existing->fix_source = fix_source;
            if (existing->payload == NULL && payload != NULL)
                existing->payload = diag_payload_snapshot_create(payload);
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

    d->level      = level;
    d->line       = node ? node->line   : 0;
    d->col        = node ? node->column : 0;
    d->message    = message;
    d->code       = code;        /* non-owning; static literal or NULL */
    d->cause_ir   = cause_ir;    /* non-owning; static literal or NULL */
    d->fix_source = fix_source;  /* non-owning; static literal or NULL */
    d->payload    = diag_payload_snapshot_create(payload);

    ctx->diagnostics[ctx->diagnostic_count++] = d;

    if (level == DIAG_ERROR)
        ctx->has_error = true;
}

static void
emit_diagnostic(SemanticContext *ctx, DiagnosticLevel level,
                 const char *code, const ASTNode *node,
                 const char *fmt, va_list ap)
{
    emit_diagnostic_full(ctx, level, code, NULL, NULL, node, NULL, fmt, ap);
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
semantic_error_with_hints(SemanticContext *ctx,
                          const char *code,
                          const char *cause_ir,
                          const char *fix_source,
                          const ASTNode *node,
                          const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic_full(ctx, DIAG_ERROR, code, cause_ir, fix_source,
                          node, NULL, fmt, ap);
    va_end(ap);
}

void
semantic_warning_with_hints(SemanticContext *ctx,
                            const char *code,
                            const char *cause_ir,
                            const char *fix_source,
                            const ASTNode *node,
                            const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    emit_diagnostic_full(ctx, DIAG_WARNING, code, cause_ir, fix_source,
                          node, NULL, fmt, ap);
    va_end(ap);
}

/* DiagPayload renderer (P5 seed).  Caller fills the payload via named
 * fields, then we delegate to emit_diagnostic_full so routing tags and
 * dedupe behavior match every other diagnostic site.  fmt may reference
 * payload fields positionally (the renderer doesn't substitute by
 * name); structured-aware renderers can be added later without
 * touching call sites. */
void
semantic_emit_payload(SemanticContext *ctx,
                      const DiagPayload *p,
                      const char *fmt, ...)
{
    va_list ap;
    if (ctx == NULL || p == NULL || fmt == NULL)
        return;
    va_start(ap, fmt);
    emit_diagnostic_full(ctx, DIAG_ERROR, p->code, p->cause_ir,
                          p->fix_source, p->site, p, fmt, ap);
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
