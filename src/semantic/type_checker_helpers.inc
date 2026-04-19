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
find_type_alias_decl(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_TYPE_ALIAS
            || stmt->data.type_alias.name == NULL) {
            continue;
        }
        if (strcmp(stmt->data.type_alias.name, name) == 0)
            return stmt;
    }

    return NULL;
}

static int
semantic_ctx_alias_resolution_index(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return -1;

    for (size_t i = 0; i < ctx->alias_resolution_count; i++) {
        if (ctx->alias_resolution_stack[i] != NULL
            && strcmp(ctx->alias_resolution_stack[i], name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static bool
semantic_ctx_alias_resolution_push(SemanticContext *ctx, const char *name)
{
    char **grown;

    if (ctx == NULL || name == NULL)
        return false;

    if (ctx->alias_resolution_count >= ctx->alias_resolution_capacity) {
        size_t new_cap = ctx->alias_resolution_capacity == 0
            ? 8 : ctx->alias_resolution_capacity * 2;
        grown = realloc(ctx->alias_resolution_stack, new_cap * sizeof(char *));
        if (grown == NULL)
            return false;
        ctx->alias_resolution_stack = grown;
        ctx->alias_resolution_capacity = new_cap;
    }

    ctx->alias_resolution_stack[ctx->alias_resolution_count] = pergyra_strdup(name);
    if (ctx->alias_resolution_stack[ctx->alias_resolution_count] == NULL)
        return false;
    ctx->alias_resolution_count++;
    return true;
}

static void
semantic_ctx_alias_resolution_pop(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->alias_resolution_count == 0)
        return;

    free(ctx->alias_resolution_stack[ctx->alias_resolution_count - 1]);
    ctx->alias_resolution_stack[ctx->alias_resolution_count - 1] = NULL;
    ctx->alias_resolution_count--;
}

static char *
semantic_ctx_alias_resolution_cycle_text(SemanticContext *ctx,
                                         size_t start_index,
                                         const char *closing_name)
{
    char *result = NULL;

    if (ctx == NULL || start_index >= ctx->alias_resolution_count)
        return tc_strdup_fmt("<cycle>");

    for (size_t i = start_index; i < ctx->alias_resolution_count; i++) {
        const char *label = ctx->alias_resolution_stack[i] != NULL
            ? ctx->alias_resolution_stack[i] : "<alias>";
        char *next = result == NULL
            ? tc_strdup_fmt("%s", label)
            : tc_strdup_fmt("%s -> %s", result, label);
        free(result);
        result = next;
        if (result == NULL)
            return tc_strdup_fmt("<cycle>");
    }

    {
        char *next = tc_strdup_fmt("%s -> %s",
                                   result != NULL ? result : "<cycle>",
                                   closing_name != NULL ? closing_name : "<alias>");
        free(result);
        result = next;
    }

    return result != NULL ? result : tc_strdup_fmt("<cycle>");
}

static Type *
resolve_type_alias_decl(ASTNode *alias_decl, SemanticContext *ctx, unsigned depth)
{
    const char *alias_name;
    int cycle_index;

    if (alias_decl == NULL || alias_decl->type != AST_TYPE_ALIAS)
        return TYPE_UNKNOWN;

    alias_name = alias_decl->data.type_alias.name != NULL
        ? alias_decl->data.type_alias.name : "<alias>";

    cycle_index = semantic_ctx_alias_resolution_index(ctx, alias_name);
    if (cycle_index >= 0) {
        char *cycle_text = semantic_ctx_alias_resolution_cycle_text(
            ctx, (size_t)cycle_index, alias_name);
        semantic_error(ctx, alias_decl,
            "Type resolution dependency cycle detected around alias '%s'.\n"
            "Reason:\n"
            "- alias resolution re-entered '%s' before the current alias chain completed\n"
            "- cycle path: %s\n"
            "Fix:\n"
            "- break the alias loop so one alias resolves to a concrete type first\n"
            "- or replace one side of the cycle with a non-alias type declaration",
            alias_name,
            alias_name,
            cycle_text != NULL ? cycle_text : "<cycle>");
        free(cycle_text);
        return TYPE_UNKNOWN;
    }

    if (depth > 32) {
        semantic_error(ctx, alias_decl,
            "Type alias resolution exceeded maximum depth near '%s'",
            alias_decl->data.type_alias.name != NULL
                ? alias_decl->data.type_alias.name : "<alias>");
        return TYPE_UNKNOWN;
    }

    if (alias_decl->data.type_alias.target_type == NULL)
        return TYPE_UNKNOWN;

    semantic_type_resolution_record_type_ref_dependency(
        ctx,
        alias_decl,
        alias_decl->data.type_alias.name != NULL
            ? alias_decl->data.type_alias.name : "<alias>",
        alias_decl->data.type_alias.target_type,
        "type-alias target lookup");

    if (!semantic_ctx_alias_resolution_push(ctx, alias_name)) {
        semantic_error(ctx, alias_decl,
            "Out of memory while tracking type alias resolution for '%s'.",
            alias_name);
        return TYPE_UNKNOWN;
    }

    {
        Type *resolved = resolve_type_node(alias_decl->data.type_alias.target_type, ctx);
        semantic_ctx_alias_resolution_pop(ctx);
        return resolved;
    }
}

static ASTNode *
find_type_decl_by_name(ASTNode *program, const char *type_name);

static bool
type_name_is_builtin_provider(const char *name)
{
    return name != NULL
        && (strcmp(name, "Int") == 0
            || strcmp(name, "Long") == 0
            || strcmp(name, "Float") == 0
            || strcmp(name, "Double") == 0
            || strcmp(name, "Bool") == 0
            || strcmp(name, "String") == 0
            || strcmp(name, "QubitSlot") == 0
            || strcmp(name, "Void") == 0
            || strcmp(name, "Array") == 0
            || strcmp(name, "Slice") == 0
            || strcmp(name, "List") == 0
            || strcmp(name, "Queue") == 0
            || strcmp(name, "HashMap") == 0
            || strcmp(name, "Set") == 0
            || strcmp(name, "Box") == 0
            || strcmp(name, "Rc") == 0
            || strcmp(name, "Weak") == 0
            || strcmp(name, "RemoteFuture") == 0
            || strcmp(name, "DeviceSlot") == 0
            || strcmp(name, "Allocator") == 0
            || strcmp(name, "Option") == 0);
}

static void
semantic_type_resolution_record_type_ref_dependency(SemanticContext *ctx,
                                                    const ASTNode *consumer_site,
                                                    const char *consumer_name,
                                                    const ASTNode *provider_type_ref,
                                                    const char *reason)
{
    const char *provider_name;
    ASTNode *alias_decl;
    ASTNode *decl;

    if (ctx == NULL || provider_type_ref == NULL)
        return;

    if (provider_type_ref->type != AST_TYPE
        || provider_type_ref->data.type.name == NULL) {
        semantic_type_resolution_record_named_dependency(
            ctx,
            consumer_site,
            consumer_name,
            TYPE_RES_NODE_TYPE_REF,
            provider_type_ref,
            "<type>",
            reason);
        return;
    }

    provider_name = provider_type_ref->data.type.name;
    if (type_name_is_builtin_provider(provider_name)) {
        semantic_type_resolution_record_named_dependency(
            ctx,
            consumer_site,
            consumer_name,
            TYPE_RES_NODE_BUILTIN,
            NULL,
            provider_name,
            reason);
        return;
    }

    alias_decl = (ctx->program_root != NULL)
        ? find_type_alias_decl(ctx->program_root, provider_name)
        : NULL;
    if (alias_decl != NULL) {
        semantic_type_resolution_record_named_dependency(
            ctx,
            consumer_site,
            consumer_name,
            TYPE_RES_NODE_ALIAS,
            alias_decl,
            provider_name,
            reason);
        return;
    }

    decl = (ctx->program_root != NULL)
        ? find_type_decl_by_name(ctx->program_root, provider_name)
        : NULL;
    if (decl != NULL) {
        semantic_type_resolution_record_named_dependency(
            ctx,
            consumer_site,
            consumer_name,
            TYPE_RES_NODE_DECL,
            decl,
            provider_name,
            reason);
        return;
    }

    semantic_type_resolution_record_named_dependency(
        ctx,
        consumer_site,
        consumer_name,
        TYPE_RES_NODE_TYPE_REF,
        provider_type_ref,
        provider_name,
        reason);
}

static Type *
resolve_named_type(const char *name, SemanticContext *ctx, const ASTNode *site)
{
    Symbol *sym = scope_lookup(ctx->scope, name);
    if (sym != NULL && sym->kind == SYMBOL_CLASS && sym->type != TYPE_UNKNOWN)
    {
        ASTNode *decl = (ctx != NULL && ctx->program_root != NULL)
            ? find_type_decl_by_name(ctx->program_root, name)
            : NULL;
        semantic_type_resolution_record_named_dependency(
            ctx, site, name, TYPE_RES_NODE_DECL, decl, name, "named-type lookup");
        return sym->type;
    }

    if (strcmp(name, "Int")    == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Int", "builtin-type lookup"); return TYPE_INT; }
    if (strcmp(name, "Long")   == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Long", "builtin-type lookup"); return TYPE_LONG; }
    if (strcmp(name, "Float")  == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Float", "builtin-type lookup"); return TYPE_FLOAT; }
    if (strcmp(name, "Double") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Double", "builtin-type lookup"); return TYPE_DOUBLE; }
    if (strcmp(name, "Bool")   == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Bool", "builtin-type lookup"); return TYPE_BOOL; }
    if (strcmp(name, "String") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "String", "builtin-type lookup"); return TYPE_STRING; }
    if (strcmp(name, "QubitSlot") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "QubitSlot", "builtin-type lookup"); return TYPE_QUBIT; }
    if (strcmp(name, "Void")   == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Void", "builtin-type lookup"); return TYPE_VOID; }
    if (strcmp(name, "Array")  == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Array", "builtin-type lookup"); return TYPE_ARRAY; }
    if (strcmp(name, "Slice")  == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Slice", "builtin-type lookup"); return TYPE_SLICE; }
    if (strcmp(name, "List")   == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "List", "builtin-type lookup"); return TYPE_LIST; }
    if (strcmp(name, "Queue")  == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Queue", "builtin-type lookup"); return TYPE_QUEUE; }
    if (strcmp(name, "HashMap")== 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "HashMap", "builtin-type lookup"); return TYPE_HASHMAP; }
    if (strcmp(name, "Set")    == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Set", "builtin-type lookup"); return TYPE_SET; }
    if (strcmp(name, "Box")    == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Box", "builtin-type lookup"); return TYPE_BOX; }
    if (strcmp(name, "Rc")     == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Rc", "builtin-type lookup"); return TYPE_RC; }
    if (strcmp(name, "Weak")   == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Weak", "builtin-type lookup"); return TYPE_WEAK; }
    if (strcmp(name, "RemoteFuture") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "RemoteFuture", "builtin-type lookup"); return TYPE_REMOTE_FUTURE; }
    if (strcmp(name, "DeviceSlot") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "DeviceSlot", "builtin-type lookup"); return TYPE_DEVICE_SLOT; }
    if (strcmp(name, "Allocator") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Allocator", "builtin-type lookup"); return TYPE_ALLOCATOR; }
    if (strcmp(name, "Option") == 0) { semantic_type_resolution_record_named_dependency(ctx, site, name, TYPE_RES_NODE_BUILTIN, NULL, "Option", "builtin-type lookup"); return TYPE_OPTION; }

    if (sym != NULL && sym->type != TYPE_UNKNOWN)
    {
        semantic_type_resolution_record_named_dependency(
            ctx, site, name,
            sym->kind == SYMBOL_TYPE_PARAM ? TYPE_RES_NODE_GENERIC_PARAM : TYPE_RES_NODE_DECL,
            NULL, name, "scope-type lookup");
        return sym->type;
    }

    if (ctx != NULL && ctx->program_root != NULL) {
        ASTNode *alias_decl = find_type_alias_decl(ctx->program_root, name);
        if (alias_decl != NULL) {
            semantic_type_resolution_record_named_dependency(
                ctx, site, name, TYPE_RES_NODE_ALIAS, alias_decl, name, "type-alias lookup");
            Type *resolved = resolve_type_alias_decl(alias_decl, ctx, 0);
            if (sym != NULL && resolved != NULL)
                sym->type = resolved;
            return resolved != NULL ? resolved : TYPE_UNKNOWN;
        }
    }

    semantic_error_code(ctx, "PGY_SEM_UNKNOWN_TYPE", site,
                        "Unknown type '%s'", name);
    return TYPE_UNKNOWN;
}

static Type *
resolve_generic_type_arg(GenericParam *gp, SemanticContext *ctx,
                         const ASTNode *site)
{
    if (gp == NULL)
        return TYPE_UNKNOWN;
    if (gp->constraint != NULL)
        return resolve_type_node(gp->constraint, ctx);
    return resolve_named_type(gp->name, ctx, site);
}

static bool
name_looks_qualified(const char *name)
{
    return name != NULL && strchr(name, '.') != NULL;
}

static const char *
semantic_symbol_kind_label(SymbolKind kind)
{
    switch (kind) {
    case SYMBOL_VARIABLE: return "variable";
    case SYMBOL_FUNCTION: return "function";
    case SYMBOL_CLASS: return "class";
    case SYMBOL_TYPE_PARAM: return "type parameter";
    case SYMBOL_SLOT: return "slot";
    case SYMBOL_TOKEN: return "token";
    case SYMBOL_ABILITY: return "ability";
    case SYMBOL_ROLE: return "role";
    case SYMBOL_PARTY: return "party";
    case SYMBOL_ROSTER: return "roster";
    case SYMBOL_WORLD: return "world";
    case SYMBOL_INTENT: return "intent";
    case SYMBOL_RELATION: return "relation";
    case SYMBOL_EFFECT: return "effect";
    case SYMBOL_ZONE: return "zone";
    default: return "symbol";
    }
}

static const char *
type_name_or_unknown(const Type *type)
{
    if (type == NULL)
        return "<unknown>";
    if (type->name != NULL && type->name[0] != '\0')
        return type->name;

    switch (type->kind) {
    case TYPE_KIND_PRIMITIVE:
    case TYPE_KIND_GENERIC:
    case TYPE_KIND_CONSTRUCTED:
    case TYPE_KIND_TUPLE:
    case TYPE_KIND_SLOT:
    case TYPE_KIND_CLASS:
    case TYPE_KIND_ENUM:
    case TYPE_KIND_TRAIT:
    case TYPE_KIND_ALIAS:
    case TYPE_KIND_FUNCTION:
        return "<unknown>";
    default:
        return "<unknown>";
    }
}

static void
semantic_format_function_signature(const Type *type, char *out, size_t out_cap)
{
    size_t pos = 0;
    if (out == NULL || out_cap == 0)
        return;

    out[0] = '\0';
    if (type == NULL || type->kind != TYPE_KIND_FUNCTION)
        return;

    pos = snprintf(out, out_cap, "fn(");
    for (size_t i = 0; i < type->data.function.param_count; i++) {
        const char *param_name =
            type_name_or_unknown(type->data.function.param_types[i]);
        if (i > 0)
            pos += snprintf(out + pos, out_cap > pos ? out_cap - pos : 0, ", ");
        pos += snprintf(out + pos, out_cap > pos ? out_cap - pos : 0, "%s",
                        param_name);
        if (pos >= out_cap)
            break;
    }
    if (pos < out_cap)
        snprintf(out + pos, out_cap > pos ? out_cap - pos : 0,
                 ")->%s",
                 type_name_or_unknown(type->data.function.return_type));
}

static ASTNode *
constructor_decl_for_symbol_kind(ASTNode *program, SymbolKind kind, const char *name);
static ASTNode *
find_domain_decl_by_name(ASTNode *program, ASTNodeType decl_type, const char *name);

static const char *
root_identifier_name(ASTNode *expr)
{
    if (expr == NULL)
        return NULL;

    switch (expr->type) {
    case AST_IDENTIFIER:
        return expr->data.identifier.name;
    case AST_MEMBER_ACCESS:
        return root_identifier_name(expr->data.member.object);
    case AST_ARRAY_ACCESS:
        return root_identifier_name(expr->data.array_access.array);
    default:
        return NULL;
    }
}

static void
reject_if_embedded_world_zone_mutation(SemanticContext *ctx, ASTNode *site,
                                       ASTNode *target, const char *op_name)
{
    const char *root_name;
    const char *owner_world;
    const char *owner_slot;
    Symbol *root_sym;

    if (ctx == NULL || site == NULL || target == NULL || op_name == NULL)
        return;
    if (ctx->current_world != NULL)
        return;

    root_name = root_identifier_name(target);
    if (root_name == NULL)
        return;

    root_sym = scope_lookup(ctx->scope, root_name);
    if ((root_sym == NULL || !root_sym->embedded_in_world)
        && !semantic_ctx_has_embedded_world_zone_name(ctx, root_name)) {
        return;
    }

    owner_world = semantic_ctx_embedded_world_zone_world_name(ctx, root_name);
    owner_slot = semantic_ctx_embedded_world_zone_slot_name(ctx, root_name);

    semantic_error(ctx, site,
        "Zone '%s' cannot be mutated via %s after it was embedded into world '%s' slot '%s'.\n"
        "Reason:\n"
        "- origin binding is '%s'\n"
        "- derived embedding provenance points to world '%s' slot '%s'\n"
        "- current world construction embeds zone state by value into the world-owned slot\n"
        "- mutating the old binding would diverge from the world-owned zone copy\n"
        "Fix:\n"
        "- finish configuring '%s' before constructing world '%s'\n"
        "- or mutate it through world '%s' slot '%s' after embedding",
        root_name, op_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>");
}

static void
mark_world_embedded_zone_arguments(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *world_decl;
    const char *world_name = NULL;

    if (call == NULL || call->type != AST_CALL || ctx == NULL
        || call->data.call.callee == NULL
        || call->data.call.callee->type != AST_IDENTIFIER) {
        return;
    }

    world_name = call->data.call.callee->data.identifier.name;
    world_decl = find_domain_decl_by_name(ctx->program_root, AST_WORLD_DECL,
        world_name);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return;

    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        ASTNode *arg = call->data.call.arguments[i];
        Symbol *arg_sym;
        bool matched_zone_slot = false;

        if (arg == NULL || arg->type != AST_IDENTIFIER)
            continue;

        arg_sym = scope_lookup(ctx->scope, arg->data.identifier.name);
        if (arg_sym == NULL || arg_sym->kind != SYMBOL_VARIABLE
            || arg_sym->type == NULL || arg_sym->type->name == NULL) {
            if (arg_sym == NULL || arg_sym->kind != SYMBOL_VARIABLE)
                continue;
        }

        if (i < world_decl->data.world_decl.zone_count) {
            ASTNode *zone_slot = world_decl->data.world_decl.zones[i];
            if (zone_slot != NULL && zone_slot->type == AST_WORLD_ZONE
                && zone_slot->data.world_zone.zone_type != NULL) {
                semantic_error(ctx, arg,
                    "World constructor '%s' implicitly copies zone binding '%s' into slot '%s'.\n"
                    "Reason:\n"
                    "- origin binding is '%s'\n"
                    "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
                    "- world embedding currently snapshots the zone binding into a world-owned zone slot\n"
                    "- mutating the original binding afterwards would diverge from the embedded world state\n"
                    "Fix:\n"
                    "- use Clone(%s) for an explicit copy\n"
                    "- or construct the zone inline / mutate it through the owning world after embedding",
                    world_name != NULL ? world_name : "<world>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    zone_slot->data.world_zone.slot_name != NULL
                        ? zone_slot->data.world_zone.slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot->data.world_zone.slot_name != NULL
                        ? zone_slot->data.world_zone.slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>");
                arg_sym->embedded_in_world = true;
                semantic_ctx_mark_embedded_world_zone_name(ctx,
                    arg->data.identifier.name,
                    world_name,
                    zone_slot->data.world_zone.slot_name);
                matched_zone_slot = true;
            }
        }

        if (matched_zone_slot)
            continue;

        for (size_t zi = 0; zi < world_decl->data.world_decl.zone_count; zi++) {
            ASTNode *zone_slot = world_decl->data.world_decl.zones[zi];
            if (zone_slot == NULL || zone_slot->type != AST_WORLD_ZONE
                || zone_slot->data.world_zone.zone_type == NULL) {
                continue;
            }
            if (arg_sym->type != NULL && arg_sym->type->name != NULL
                && strcmp(arg_sym->type->name, zone_slot->data.world_zone.zone_type) == 0) {
                semantic_error(ctx, arg,
                    "World constructor '%s' implicitly copies zone binding '%s' into slot '%s'.\n"
                    "Reason:\n"
                    "- origin binding is '%s'\n"
                    "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
                    "- world embedding currently snapshots the zone binding into a world-owned zone slot\n"
                    "- mutating the original binding afterwards would diverge from the embedded world state\n"
                    "Fix:\n"
                    "- use Clone(%s) for an explicit copy\n"
                    "- or construct the zone inline / mutate it through the owning world after embedding",
                    world_name != NULL ? world_name : "<world>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    zone_slot->data.world_zone.slot_name != NULL
                        ? zone_slot->data.world_zone.slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>",
                    world_name != NULL ? world_name : "<world>",
                    zone_slot->data.world_zone.slot_name != NULL
                        ? zone_slot->data.world_zone.slot_name : "<slot>",
                    arg->data.identifier.name != NULL
                        ? arg->data.identifier.name : "<zone>");
                arg_sym->embedded_in_world = true;
                semantic_ctx_mark_embedded_world_zone_name(ctx,
                    arg->data.identifier.name,
                    world_name,
                    zone_slot->data.world_zone.slot_name);
                break;
            }
        }
    }
}

static uint32_t
parse_effect_word(const char *word)
{
    if (word == NULL || *word == '\0')
        return EFFECT_NONE;
    if (strcmp(word, "secure") == 0)
        return EFFECT_SECURE;
    if (strcmp(word, "remote") == 0)
        return EFFECT_REMOTE;
    if (strcmp(word, "nondeterministic") == 0)
        return EFFECT_NONDETERMINISTIC;
    if (strcmp(word, "collapse") == 0)
        return EFFECT_COLLAPSE;
    if (strcmp(word, "local") == 0)
        return EFFECT_NONE;
    return UINT32_MAX;
}

static void
append_effect_name(char *buf, size_t buf_size, const char *name, bool *first)
{
    if (buf == NULL || buf_size == 0 || name == NULL || first == NULL)
        return;

    if (!*first)
        strncat(buf, ", ", buf_size - strlen(buf) - 1);
    strncat(buf, name, buf_size - strlen(buf) - 1);
    *first = false;
}

static void
effect_mask_to_string(uint32_t mask, char *buf, size_t buf_size)
{
    bool first = true;

    if (buf == NULL || buf_size == 0)
        return;

    buf[0] = '\0';
    mask = type_effect_mask_closure(mask);
    if (mask == EFFECT_NONE) {
        strncat(buf, "local", buf_size - 1);
        return;
    }

    if (type_effect_mask_has(mask, EFFECT_SECURE))
        append_effect_name(buf, buf_size, "secure", &first);
    if (type_effect_mask_has(mask, EFFECT_REMOTE))
        append_effect_name(buf, buf_size, "remote", &first);
    if (type_effect_mask_has(mask, EFFECT_NONDETERMINISTIC))
        append_effect_name(buf, buf_size, "nondeterministic", &first);
    if (type_effect_mask_has(mask, EFFECT_COLLAPSE))
        append_effect_name(buf, buf_size, "collapse", &first);
}

static uint32_t
effects_from_structured_comment(StructuredComment *comment,
                                SemanticContext *ctx,
                                const ASTNode *site)
{
    uint32_t mask = EFFECT_NONE;

    for (StructuredComment *block = comment; block != NULL; block = block->next) {
        for (size_t i = 0; i < block->tag_count; i++) {
            DocTag *tag = block->tags[i];
            if (tag == NULL || tag->type != DOC_TAG_EFFECTS || tag->content == NULL)
                continue;

            char *copy = pergyra_strdup(tag->content);
            char *tok = copy != NULL ? strtok(copy, ", \t\r\n|") : NULL;
            while (tok != NULL) {
                for (char *p = tok; *p != '\0'; p++)
                    *p = (char)tolower((unsigned char)*p);

                uint32_t effect = parse_effect_word(tok);
                if (effect == UINT32_MAX) {
                    semantic_warning(ctx, site,
                        "Unknown effect tag '%s' in structured comment; expected secure, remote, nondeterministic, collapse, or local",
                        tok);
                } else {
                    mask |= effect;
                }
                tok = strtok(NULL, ", \t\r\n|");
            }
            free(copy);
        }
    }

    return mask;
}

static bool
structured_comment_has_effects_tag(StructuredComment *comment)
{
    for (StructuredComment *block = comment; block != NULL; block = block->next) {
        for (size_t i = 0; i < block->tag_count; i++) {
            DocTag *tag = block->tags[i];
            if (tag != NULL && tag->type == DOC_TAG_EFFECTS)
                return true;
        }
    }
    return false;
}

static uint32_t
declared_effects_from_function_node(ASTNode *node, SemanticContext *ctx,
                                    bool *has_contract_out)
{
    bool has_doc_contract;
    bool has_sig_contract;
    uint32_t doc_effects;
    uint32_t sig_effects;

    if (node == NULL)
        return EFFECT_NONE;

    has_doc_contract = structured_comment_has_effects_tag(node->data.func_decl.doc_comment);
    has_sig_contract = node->data.func_decl.has_effects_clause;
    doc_effects = effects_from_structured_comment(node->data.func_decl.doc_comment, ctx, node);
    sig_effects = node->data.func_decl.declared_effects;

    if (has_contract_out != NULL)
        *has_contract_out = has_doc_contract || has_sig_contract;

    return type_effect_mask_closure(doc_effects | sig_effects);
}

void
semantic_record_effect(SemanticContext *ctx, uint32_t effect_mask)
{
    if (ctx == NULL || !ctx->tracking_function_effects)
        return;
    ctx->current_function_effects =
        type_effect_mask_join(ctx->current_function_effects, effect_mask);
}

bool
type_is_constructed_named(const Type *type, const char *name)
{
    return type != NULL
        && type->kind == TYPE_KIND_CONSTRUCTED
        && type->data.constructed.constructor != NULL
        && strcmp(type->data.constructed.constructor->name, name) == 0;
}

bool
type_is_qubit(const Type *type)
{
    if (type == NULL)
        return false;
    if (TYPE_QUBIT != NULL && type_equals(type, TYPE_QUBIT))
        return true;
    return type->name != NULL && strcmp(type->name, "QubitSlot") == 0;
}

bool
type_is_slot_handle(const Type *type)
{
    return type != NULL && type->kind == TYPE_KIND_SLOT;
}

bool
type_is_owned_slot_handle(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_OWNED;
}

bool
type_is_read_view(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_READ_VIEW;
}

bool
type_is_write_view(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_WRITE_VIEW;
}

bool
type_is_move_token(const Type *type)
{
    return type_is_slot_handle(type)
        && type->data.slot.access_mode == SLOT_ACCESS_MOVE_TOKEN;
}

bool
type_is_resource_handle(const Type *type)
{
    return type_is_qubit(type)
        || type_is_owned_slot_handle(type)
        || type_is_constructed_named(type, "DeviceSlot");
}

bool
type_is_anchored_resource_handle(const Type *type)
{
    return type_is_owned_slot_handle(type)
        || type_is_constructed_named(type, "DeviceSlot");
}

bool
type_is_movable_resource_handle(const Type *type)
{
    return type_is_qubit(type);
}

static bool
type_is_subject_type(const Type *type, SemanticContext *ctx);

static bool
type_is_value_nominal_boundary_type(const Type *type, SemanticContext *ctx);

static bool
type_is_general_boundary_type(const Type *type, SemanticContext *ctx)
{
    return type_is_movable_resource_handle(type)
        || type_is_subject_type(type, ctx)
        || type_is_value_nominal_boundary_type(type, ctx);
}

bool
type_requires_boundary_borrow_tracking(const Type *type, SemanticContext *ctx)
{
    if (type == NULL || !type_is_general_boundary_type(type, ctx))
        return false;
    if (type_is_subject_type(type, ctx) || type_is_movable_resource_handle(type))
        return false;

    switch (type->kind) {
    case TYPE_KIND_PRIMITIVE:
    case TYPE_KIND_ENUM:
        return false;
    default:
        return true;
    }
}

bool
type_is_capability_bearing(const Type *type)
{
    return (type_is_owned_slot_handle(type) && type->data.slot.is_secure)
        || type_is_constructed_named(type, "Token");
}

static const char *
resource_handle_display_name(const Type *type)
{
    if (type == NULL)
        return "resource";
    if (type_is_qubit(type))
        return "QubitSlot";
    if (type_is_read_view(type))
        return "ReadView";
    if (type_is_write_view(type))
        return "WriteView";
    if (type_is_move_token(type))
        return "MoveToken";
    if (type_is_owned_slot_handle(type))
        return type->data.slot.is_secure ? "SecureSlot" : "Slot";
    if (type_is_constructed_named(type, "DeviceSlot"))
        return "DeviceSlot";
    return type->name != NULL ? type->name : "resource";
}

static ASTNode *
find_type_decl_by_name(ASTNode *program, const char *type_name)
{
    if (program == NULL || program->type != AST_PROGRAM || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_CLASS_DECL)
            continue;
        if (stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, type_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_ability_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ABILITY_DECL
            || stmt->data.ability_decl.name == NULL)
            continue;
        if (strcmp(stmt->data.ability_decl.name, name) == 0)
            return stmt;
    }

    return NULL;
}

static TypeNominalFlavor
nominal_flavor_from_decl(const ASTNode *decl)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return TYPE_NOMINAL_NONE;

    switch (decl->data.class_decl.nominal_kind) {
    case NOMINAL_DECL_SUBJECT:
        return TYPE_NOMINAL_SUBJECT;
    case NOMINAL_DECL_VESSEL:
        return TYPE_NOMINAL_VESSEL;
    case NOMINAL_DECL_STRUCT:
        return TYPE_NOMINAL_STRUCT;
    case NOMINAL_DECL_OBJECT:
        return TYPE_NOMINAL_OBJECT;
    case NOMINAL_DECL_TOBJECT:
        return TYPE_NOMINAL_TOBJECT;
    case NOMINAL_DECL_CLASS:
    default:
        return TYPE_NOMINAL_CLASS;
    }
}

static bool
decl_is_subject_type(const ASTNode *decl)
{
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && decl->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
}

static bool
decl_is_subject_host(const ASTNode *decl)
{
    return decl_is_subject_type(decl);
}

static ASTNode *
find_subject_host_decl_by_name(ASTNode *program, const char *type_name)
{
    ASTNode *decl = find_type_decl_by_name(program, type_name);
    if (decl_is_subject_host(decl))
        return decl;
    return NULL;
}

static ASTNode *
find_callable_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_FUNC_DECL
            && stmt->data.func_decl.name != NULL
            && strcmp(stmt->data.func_decl.name, name) == 0)
            return stmt;
        if (stmt->type == AST_EVENT_DECL
            && stmt->data.event_decl.name != NULL
            && strcmp(stmt->data.event_decl.name, name) == 0)
            return stmt;
        if (stmt->type == AST_INTENT_DECL
            && stmt->data.intent_decl.name != NULL
            && strcmp(stmt->data.intent_decl.name, name) == 0)
            return stmt;
    }

    return NULL;
}

static bool
decl_is_projection_source(const ASTNode *decl)
{
    if (decl_is_subject_host(decl))
        return true;
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT;
}

static ASTNode *
find_domain_decl_by_name(ASTNode *program, ASTNodeType decl_type,
                         const char *name);

static ASTNode *
find_zone_domain_slot(ASTNode *zone, const char *slot_name);

static bool
subject_type_has_ability(ASTNode *program, const char *type_name,
                         ASTNode *ability_ref);

static ClassField *
subject_host_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    return NULL;
}

static size_t
projection_source_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    return 0;
}

static ClassField *
projection_source_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    return NULL;
}

static int
resolve_projection_source_field_path_rec(ASTNode *program_root,
                                         ASTNode *source_decl,
                                         const char *field_name,
                                         unsigned depth,
                                         SemanticContext *ctx,
                                         char **path_out,
                                         Type **field_type_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;
    Type *resolved_type = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (field_type_out != NULL)
        *field_type_out = NULL;

    if (program_root == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = projection_source_field_count(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = pergyra_strdup(field_name);
            if (field_type_out != NULL)
                *field_type_out = field->type != NULL
                    ? resolve_type_node(field->type, ctx)
                    : TYPE_UNKNOWN;
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at(source_decl, i);
        ASTNode *vessel_decl;
        char *nested_path = NULL;
        Type *nested_type = NULL;
        char *prefixed_path;
        int nested_status;

        if (field == NULL || !field->is_vessel_field
            || field->name == NULL || field->type == NULL
            || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = find_type_decl_by_name(program_root, field->type->data.type.name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = resolve_projection_source_field_path_rec(
            program_root, vessel_decl, field_name, depth + 1, ctx,
            &nested_path, &nested_type);
        if (nested_status != 1) {
            if (nested_path != NULL)
                free(nested_path);
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = tc_strdup_fmt("%s.%s", field->name, nested_path);
        free(nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
            resolved_type = nested_type;
        } else {
            free(prefixed_path);
            free(resolved_path);
            resolved_path = NULL;
            resolved_type = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        else
            free(resolved_path);
        if (field_type_out != NULL)
            *field_type_out = resolved_type;
        return 1;
    }

    if (resolved_path != NULL)
        free(resolved_path);
    return match_count > 1 ? 2 : 0;
}

static int
resolve_projection_source_field_path(ASTNode *program_root,
                                     ASTNode *source_decl,
                                     const char *field_name,
                                     SemanticContext *ctx,
                                     char **path_out,
                                     Type **field_type_out)
{
    return resolve_projection_source_field_path_rec(program_root, source_decl,
        field_name, 0, ctx, path_out, field_type_out);
}

static Type *
create_overlay_nominal_type(const char *name)
{
    Type *type = calloc(1, sizeof(Type));
    if (type == NULL)
        return TYPE_UNKNOWN;
    type->kind = TYPE_KIND_CLASS;
    type->nominal_flavor = TYPE_NOMINAL_CLASS;
    type->name = pergyra_strdup(name);
    return type;
}

static size_t
overlay_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    switch (decl->type) {
    case AST_ROSTER_DECL:
        return decl->data.roster_decl.party_count
            + decl->data.roster_decl.shared_count;
    case AST_WORLD_DECL:
        return decl->data.world_decl.roster_count
            + decl->data.world_decl.zone_count
            + decl->data.world_decl.shared_count;
    case AST_ZONE_DECL:
        return decl->data.zone_decl.slot_count
            + decl->data.zone_decl.shared_count;
    case AST_RELATION_DECL:
        return decl->data.relation_decl.slot_count
            + decl->data.relation_decl.shared_count;
    case AST_EFFECT_DECL:
        return decl->data.effect_decl.slot_count
            + decl->data.effect_decl.shared_count;
    default:
        return 0;
    }
}

static ASTNode *
overlay_field_decl_at(ASTNode *decl, size_t index, const char **field_name_out)
{
    if (field_name_out != NULL)
        *field_name_out = NULL;
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_ROSTER_DECL:
        if (index < decl->data.roster_decl.party_count)
            return NULL;
        index -= decl->data.roster_decl.party_count;
        if (index < decl->data.roster_decl.shared_count) {
            ASTNode *shared = decl->data.roster_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_WORLD_DECL:
        if (index < decl->data.world_decl.roster_count)
            return NULL;
        index -= decl->data.world_decl.roster_count;
        if (index < decl->data.world_decl.zone_count)
            return NULL;
        index -= decl->data.world_decl.zone_count;
        if (index < decl->data.world_decl.shared_count) {
            ASTNode *shared = decl->data.world_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_ZONE_DECL:
        if (index < decl->data.zone_decl.slot_count) {
            ASTNode *slot = decl->data.zone_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.zone_decl.slot_count;
        if (index < decl->data.zone_decl.shared_count) {
            ASTNode *shared = decl->data.zone_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_RELATION_DECL:
        if (index < decl->data.relation_decl.slot_count) {
            ASTNode *slot = decl->data.relation_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.relation_decl.slot_count;
        if (index < decl->data.relation_decl.shared_count) {
            ASTNode *shared = decl->data.relation_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_EFFECT_DECL:
        if (index < decl->data.effect_decl.slot_count) {
            ASTNode *slot = decl->data.effect_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.effect_decl.slot_count;
        if (index < decl->data.effect_decl.shared_count) {
            ASTNode *shared = decl->data.effect_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    default:
        break;
    }

    return NULL;
}

static ASTNode *
current_host_decl(SemanticContext *ctx)
{
    if (ctx == NULL)
        return NULL;
    if (ctx->current_nominal_decl != NULL)
        return ctx->current_nominal_decl;
    if (ctx->current_relation != NULL)
        return ctx->current_relation;
    if (ctx->current_effect != NULL)
        return ctx->current_effect;
    if (ctx->current_zone != NULL)
        return ctx->current_zone;
    if (ctx->current_world != NULL)
        return ctx->current_world;
    return NULL;
}

static Type *
current_host_field_type(SemanticContext *ctx, const char *field_name)
{
    ASTNode *decl = current_host_decl(ctx);
    if (decl == NULL || field_name == NULL)
        return NULL;

    if (decl->type == AST_CLASS_DECL) {
        size_t field_count = projection_source_field_count(decl);
        for (size_t i = 0; i < field_count; i++) {
            ClassField *field = projection_source_field_at(decl, i);
            if (field != NULL && field->name != NULL
                && strcmp(field->name, field_name) == 0) {
                return resolve_type_node(field->type, ctx);
            }
        }
        return NULL;
    }

    if (decl->type == AST_WORLD_DECL) {
        for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
            ASTNode *slot = decl->data.world_decl.rosters[i];
            if (slot != NULL && slot->data.world_roster.slot_name != NULL
                && strcmp(slot->data.world_roster.slot_name, field_name) == 0) {
                return resolve_named_type(slot->data.world_roster.roster_type,
                    ctx, slot);
            }
        }
        for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
            ASTNode *slot = decl->data.world_decl.zones[i];
            if (slot != NULL && slot->data.world_zone.slot_name != NULL
                && strcmp(slot->data.world_zone.slot_name, field_name) == 0) {
                return resolve_named_type(slot->data.world_zone.zone_type, ctx, slot);
            }
        }
    }

    for (size_t i = 0; i < overlay_field_count(decl); i++) {
        const char *name = NULL;
        ASTNode *type_node = overlay_field_decl_at(decl, i, &name);
        if (name != NULL && strcmp(name, field_name) == 0)
            return resolve_type_node(type_node, ctx);
    }

    return NULL;
}

static ASTNode *
current_host_method_decl(SemanticContext *ctx, const char *method_name)
{
    ASTNode *decl = current_host_decl(ctx);
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (decl == NULL || method_name == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        methods = decl->data.class_decl.methods;
        method_count = decl->data.class_decl.method_count;
        break;
    case AST_ENUM_DECL:
        methods = decl->data.enum_decl.methods;
        method_count = decl->data.enum_decl.method_count;
        break;
    case AST_RELATION_DECL:
        methods = decl->data.relation_decl.methods;
        method_count = decl->data.relation_decl.method_count;
        break;
    case AST_EFFECT_DECL:
        methods = decl->data.effect_decl.methods;
        method_count = decl->data.effect_decl.method_count;
        break;
    case AST_ZONE_DECL:
        methods = decl->data.zone_decl.methods;
        method_count = decl->data.zone_decl.method_count;
        break;
    case AST_WORLD_DECL:
        methods = decl->data.world_decl.methods;
        method_count = decl->data.world_decl.method_count;
        break;
    default:
        return NULL;
    }

    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

static Type *
type_check_host_method_call(ASTNode *expr, ASTNode *method, SemanticContext *ctx)
{
    size_t implicit_self = 0;
    size_t expected;
    size_t provided;
    uint32_t declared_effects;

    if (expr == NULL || method == NULL || method->type != AST_FUNC_DECL)
        return TYPE_UNKNOWN;

    declared_effects = declared_effects_from_function_node(method, ctx, NULL);
    semantic_record_effect(ctx, declared_effects);
    if (ctx->in_parallel && type_effect_mask_has(declared_effects, EFFECT_SECURE)) {
        semantic_error(ctx, expr,
            "Parallel context does not permit calling secure-effect method '%s'; serialize capability-bearing operations outside the parallel block",
            method->data.func_decl.name != NULL ? method->data.func_decl.name : "<method>");
        if (method->data.func_decl.return_type != NULL)
            return resolve_type_node(method->data.func_decl.return_type, ctx);
        return TYPE_VOID;
    }

    if (method->data.func_decl.param_count > 0
        && method->data.func_decl.params[0] != NULL
        && method->data.func_decl.params[0]->name != NULL
        && strcmp(method->data.func_decl.params[0]->name, "self") == 0
        && method->data.func_decl.params[0]->type == NULL) {
        implicit_self = 1;
    }

    expected = method->data.func_decl.param_count - implicit_self;
    provided = expr->data.call.arg_count;
    if (provided != expected) {
        semantic_error(ctx, expr,
            "'%s' expects %zu argument(s), got %zu",
            method->data.func_decl.name != NULL ? method->data.func_decl.name : "<method>",
            expected, provided);
        if (method->data.func_decl.return_type != NULL)
            return resolve_type_node(method->data.func_decl.return_type, ctx);
        return TYPE_VOID;
    }

    for (size_t i = 0; i < provided; i++) {
        FuncParam *param = method->data.func_decl.params[i + implicit_self];
        Type *param_type = (param != NULL && param->type != NULL)
            ? resolve_type_node(param->type, ctx)
            : TYPE_UNKNOWN;
        Type *arg_type = type_check_expression(expr->data.call.arguments[i], ctx);
        if (param_type != NULL && arg_type != NULL
            && !type_is_assignable(arg_type, param_type)) {
            semantic_error(ctx, expr->data.call.arguments[i],
                "Argument %zu for '%s' expects '%s', got '%s'",
                i + 1,
                method->data.func_decl.name != NULL ? method->data.func_decl.name : "<method>",
                param_type->name != NULL ? param_type->name : "<type>",
                arg_type->name != NULL ? arg_type->name : "<type>");
        }
    }

    if (method->data.func_decl.return_type != NULL)
        return resolve_type_node(method->data.func_decl.return_type, ctx);
    return TYPE_VOID;
}

static ASTNode *
constructor_decl_for_symbol_kind(ASTNode *program, SymbolKind kind, const char *name)
{
    if (program == NULL || name == NULL)
        return NULL;

    switch (kind) {
    case SYMBOL_CLASS:
        return find_type_decl_by_name(program, name);
    case SYMBOL_ROSTER:
        return find_domain_decl_by_name(program, AST_ROSTER_DECL, name);
    case SYMBOL_WORLD:
        return find_domain_decl_by_name(program, AST_WORLD_DECL, name);
    case SYMBOL_ZONE:
        return find_domain_decl_by_name(program, AST_ZONE_DECL, name);
    case SYMBOL_RELATION:
        return find_domain_decl_by_name(program, AST_RELATION_DECL, name);
    case SYMBOL_EFFECT:
        return find_domain_decl_by_name(program, AST_EFFECT_DECL, name);
    default:
        return NULL;
    }
}

static bool
type_is_subject_type(const Type *type, SemanticContext *ctx);

static bool
type_is_subject_host_slot_handle(const Type *type, SemanticContext *ctx)
{
    if (!type_is_owned_slot_handle(type) || ctx == NULL)
        return false;
    return type_is_subject_type(type->data.slot.inner_type, ctx);
}

static bool
type_is_class_object_type(const Type *type, SemanticContext *ctx)
{
    return type_is_subject_type(type, ctx);
}

static bool
type_is_nominal_host_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;

    if (type == NULL
        || (type->kind != TYPE_KIND_CLASS && type->kind != TYPE_KIND_ENUM)
        || type->name == NULL || ctx == NULL)
        return false;

    decl = find_type_decl_by_name(ctx->program_root, type->name);
    if (decl != NULL && decl->type == AST_ENUM_DECL)
        return true;
    if (decl != NULL && decl->type == AST_CLASS_DECL)
        return !decl->data.class_decl.is_struct
            || decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL
            || decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT;
    return false;
}

static bool
type_is_subject_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;

    if (type == NULL || type->kind != TYPE_KIND_CLASS
        || type->name == NULL || ctx == NULL)
        return false;

    if (type->nominal_flavor == TYPE_NOMINAL_SUBJECT)
        return true;

    decl = find_subject_host_decl_by_name(ctx->program_root, type->name);
    return decl_is_subject_host(decl);
}

static const char *
find_action_binding_type_name(ASTNode *func, ASTNode *enclosing_nominal,
                              SemanticContext *ctx, const char *binding_name)
{
    if (func == NULL || func->type != AST_FUNC_DECL || ctx == NULL
        || binding_name == NULL) {
        return NULL;
    }

    if (strcmp(binding_name, "self") == 0) {
        return enclosing_nominal != NULL
            && enclosing_nominal->type == AST_CLASS_DECL
            && enclosing_nominal->data.class_decl.name != NULL
                ? enclosing_nominal->data.class_decl.name
                : NULL;
    }

    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *param = func->data.func_decl.params[i];
        Type *param_type;
        if (param == NULL || param->name == NULL
            || strcmp(param->name, binding_name) != 0) {
            continue;
        }
        if (param->type == NULL)
            return NULL;
        param_type = resolve_type_node(param->type, ctx);
        if (param_type == NULL || !type_is_subject_type(param_type, ctx))
            return NULL;
        return param_type->name;
    }

    return NULL;
}

static bool
domain_has_subject_slot_type(ASTNode **slots, size_t slot_count,
                             SemanticContext *ctx, const char *type_name)
{
    if (slots == NULL || ctx == NULL || type_name == NULL)
        return false;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject
            || slot->data.domain_slot.type == NULL) {
            continue;
        }
        slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        if (slot_type != NULL && slot_type->name != NULL
            && strcmp(slot_type->name, type_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
type_is_value_nominal_boundary_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;
    TypeNominalFlavor flavor;

    if (type == NULL || ctx == NULL || ctx->program_root == NULL)
        return false;
    if (type_is_subject_type(type, ctx))
        return false;
    switch (type->kind) {
    case TYPE_KIND_PRIMITIVE:
    case TYPE_KIND_ENUM:
        return true;
    case TYPE_KIND_TUPLE:
        return true;
    case TYPE_KIND_CONSTRUCTED:
        return !type_is_movable_resource_handle(type);
    case TYPE_KIND_CLASS:
        break;
    default:
        return false;
    }
    if (type->name == NULL)
        return false;

    decl = find_type_decl_by_name(ctx->program_root, type->name);
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return false;

    flavor = nominal_flavor_from_decl(decl);
    return flavor == TYPE_NOMINAL_CLASS
        || flavor == TYPE_NOMINAL_STRUCT
        || flavor == TYPE_NOMINAL_OBJECT
        || flavor == TYPE_NOMINAL_TOBJECT
        || flavor == TYPE_NOMINAL_VESSEL;
}

static bool
zone_has_authority_for_subject_type(ASTNode *zone, SemanticContext *ctx,
                                    const char *type_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || ctx == NULL
        || type_name == NULL) {
        return false;
    }

    for (size_t i = 0; i < zone->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone->data.zone_decl.authorities[i];
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL
            || authority->data.zone_authority.subject_slot_name == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(zone,
            authority->data.zone_authority.subject_slot_name);
        if (slot == NULL || slot->data.domain_slot.type == NULL)
            continue;
        slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        if (slot_type != NULL && slot_type->name != NULL
            && strcmp(slot_type->name, type_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
zone_has_effect_layer_type(ASTNode *zone, const char *effect_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || effect_name == NULL)
        return false;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.layer_type == NULL
            || slot->data.zone_layer_slot.is_relation) {
            continue;
        }
        if (strcmp(slot->data.zone_layer_slot.layer_type, effect_name) == 0)
            return true;
    }

    return false;
}

static bool
expr_is_class_constructor_call(const ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *decl;

    if (expr == NULL || expr->type != AST_CALL
        || expr->data.call.callee == NULL
        || expr->data.call.callee->type != AST_IDENTIFIER
        || expr->data.call.callee->data.identifier.name == NULL
        || ctx == NULL) {
        return false;
    }

    decl = find_type_decl_by_name(ctx->program_root,
        expr->data.call.callee->data.identifier.name);
    if (decl != NULL)
        return !decl->data.class_decl.is_struct;
    return false;
}

static bool
expr_is_qubit_claim(const ASTNode *expr)
{
    return expr != NULL
        && expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_IDENTIFIER
        && expr->data.call.callee->data.identifier.name != NULL
        && strcmp(expr->data.call.callee->data.identifier.name, "ClaimQubit") == 0;
}

static bool
expr_is_device_slot_claim(const ASTNode *expr)
{
    return expr != NULL
        && expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_IDENTIFIER
        && expr->data.call.callee->data.identifier.name != NULL
        && strcmp(expr->data.call.callee->data.identifier.name, "ClaimDeviceSlot") == 0;
}

static bool
expr_is_movable_resource_transfer_source(const ASTNode *expr)
{
    if (expr == NULL)
        return false;

    switch (expr->type) {
    case AST_IDENTIFIER:
    case AST_CALL:
    case AST_CHANNEL_RECV:
    case AST_AWAIT_EXPR:
        return true;
    default:
        return false;
    }
}

static bool
slot_transfer_compatible(const Type *from, const Type *to)
{
    if (from == NULL || to == NULL)
        return false;
    if (from->kind != TYPE_KIND_SLOT || to->kind != TYPE_KIND_SLOT)
        return false;
    if (from->data.slot.access_mode != SLOT_ACCESS_MOVE_TOKEN)
        return false;
    if (to->data.slot.access_mode != SLOT_ACCESS_OWNED)
        return false;
    if (from->data.slot.is_secure != to->data.slot.is_secure)
        return false;
    return type_is_assignable(from->data.slot.inner_type, to->data.slot.inner_type)
        && type_is_assignable(to->data.slot.inner_type, from->data.slot.inner_type);
}

static bool
expr_is_movable_resource_boundary(const ASTNode *expr)
{
    return expr != NULL
        && (expr->type == AST_CHANNEL_RECV
            || expr->type == AST_AWAIT_EXPR);
}

static bool
expr_is_static_member_access(const ASTNode *expr)
{
    if (expr == NULL || expr->type != AST_MEMBER_ACCESS
        || expr->data.member.object == NULL) {
        return false;
    }

    if (expr->data.member.object->type == AST_IDENTIFIER) {
        const char *name = expr->data.member.object->data.identifier.name;
        return name != NULL && name[0] >= 'A' && name[0] <= 'Z';
    }

    return expr_is_static_member_access(expr->data.member.object);
}

static char *
flatten_static_member_access(const ASTNode *expr, char separator)
{
    if (expr == NULL)
        return NULL;

    if (expr->type == AST_IDENTIFIER)
        return expr->data.identifier.name != NULL
            ? pergyra_strdup(expr->data.identifier.name) : NULL;

    if (expr->type != AST_MEMBER_ACCESS
        || expr->data.member.object == NULL
        || expr->data.member.name == NULL) {
        return NULL;
    }

    char *lhs = flatten_static_member_access(expr->data.member.object, separator);
    if (lhs == NULL)
        return NULL;

    size_t lhs_len = strlen(lhs);
    size_t rhs_len = strlen(expr->data.member.name);
    char *result = malloc(lhs_len + rhs_len + 2);
    if (result == NULL) {
        free(lhs);
        return NULL;
    }

    memcpy(result, lhs, lhs_len);
    result[lhs_len] = separator;
    memcpy(result + lhs_len + 1, expr->data.member.name, rhs_len);
    result[lhs_len + rhs_len + 1] = '\0';
    free(lhs);
    return result;
}

static uint32_t
declared_effects_from_function_node(ASTNode *node, SemanticContext *ctx,
                                    bool *has_contract_out);

static Type *
type_check_function_symbol_call(ASTNode *expr, Symbol *sym,
                                const char *display_name,
                                SemanticContext *ctx)
{
    if (sym == NULL) {
        if (name_looks_qualified(display_name)) {
            semantic_error(ctx, expr,
                "Undefined function '%s' (expected a callable symbol). "
                "Check namespace spelling/export visibility or define/import a callable declaration before use.",
                display_name);
        } else {
            semantic_error(ctx, expr,
                "Undefined function '%s' (expected a callable symbol in current scope). "
                "Declare '%s' first, or use a qualified name for a namespaced declaration.",
                display_name, display_name);
        }
        return TYPE_UNKNOWN;
    }

    if (sym->kind == SYMBOL_CLASS || sym->kind == SYMBOL_PARTY
        || sym->kind == SYMBOL_RELATION
        || sym->kind == SYMBOL_EFFECT || sym->kind == SYMBOL_ROSTER
        || sym->kind == SYMBOL_WORLD || sym->kind == SYMBOL_ZONE) {
        if (ctx->program_root != NULL) {
            ASTNode *decl = constructor_decl_for_symbol_kind(ctx->program_root,
                sym->kind, display_name);
            size_t field_count = 0;
            bool decl_is_generic = false;
            if (decl != NULL
                && !explicit_type_reference_allowed(decl, expr, ctx)) {
                semantic_error(ctx, expr,
                    "Constructor '%s' is not accessible across the current visibility boundary",
                    display_name);
                return TYPE_UNKNOWN;
            }
            if (decl != NULL && decl->type == AST_CLASS_DECL) {
                field_count = decl->data.class_decl.field_count;
                decl_is_generic =
                    (decl->data.class_decl.generic_params != NULL
                     && decl->data.class_decl.generic_params->count > 0);
            } else if (decl != NULL
                       && (decl->type == AST_RELATION_DECL
                           || decl->type == AST_EFFECT_DECL
                           || decl->type == AST_ROSTER_DECL
                           || decl->type == AST_WORLD_DECL
                           || decl->type == AST_ZONE_DECL)) {
                field_count = overlay_field_count(decl);
            }
            if (decl != NULL) {
                size_t provided = expr->data.call.arg_count;
                /* Skip field-type validation for generic classes — the
                 * generic params (T, U) aren't in scope at the call site.
                 * Type safety is handled by the let-annotation type. */
                if (provided > field_count) {
                    semantic_error(ctx, expr,
                        "Constructor '%s' accepts at most %zu positional field argument(s), got %zu",
                        display_name, field_count, provided);
                } else if (!decl_is_generic) {
                    for (size_t i = 0; i < provided; i++) {
                        const char *field_name = NULL;
                        ASTNode *field_type_node = NULL;
                        if (decl->type == AST_CLASS_DECL) {
                            ClassField *field = subject_host_field_at(decl, i);
                            if (field != NULL) {
                                field_name = field->name;
                                field_type_node = field->type;
                            }
                        } else {
                            field_type_node = overlay_field_decl_at(decl, i, &field_name);
                        }
                        if (field_type_node == NULL)
                            continue;
                        Type *field_type = resolve_type_node(field_type_node, ctx);
                        Type *arg_type = type_check_expression(expr->data.call.arguments[i], ctx);
                        if (field_type != NULL && arg_type != NULL
                            && !type_is_assignable(arg_type, field_type)) {
                            semantic_error(ctx, expr->data.call.arguments[i],
                                "Constructor '%s' argument %zu initializes field '%s' of type '%s', got '%s'",
                                display_name, i + 1,
                                field_name != NULL ? field_name : "<field>",
                                field_type->name != NULL ? field_type->name : "<type>",
                                arg_type->name != NULL ? arg_type->name : "<type>");
                        }
                        if (sym->kind == SYMBOL_WORLD
                            && decl->type == AST_WORLD_DECL
                            && expr->data.call.arguments[i] != NULL
                            && expr->data.call.arguments[i]->type == AST_IDENTIFIER) {
                            Symbol *arg_sym = scope_lookup(ctx->scope,
                                expr->data.call.arguments[i]->data.identifier.name);
                            if (arg_sym != NULL && arg_sym->kind == SYMBOL_VARIABLE) {
                                const char *arg_type_name = NULL;
                                if (arg_sym->type != NULL
                                    && arg_sym->type->kind == TYPE_KIND_CLASS
                                    && arg_sym->type->name != NULL) {
                                    arg_type_name = arg_sym->type->name;
                                } else if (field_type != NULL
                                           && field_type->kind == TYPE_KIND_CLASS
                                           && field_type->name != NULL) {
                                    arg_type_name = field_type->name;
                                }
                                if (arg_type_name == NULL)
                                    continue;
                                for (size_t zi = 0; zi < decl->data.world_decl.zone_count; zi++) {
                                    ASTNode *zone_slot = decl->data.world_decl.zones[zi];
                                    if (zone_slot == NULL
                                        || zone_slot->type != AST_WORLD_ZONE
                                        || zone_slot->data.world_zone.zone_type == NULL) {
                                        continue;
                                    }
                                    if (strcmp(arg_type_name,
                                            zone_slot->data.world_zone.zone_type) == 0) {
                                        semantic_error(ctx, expr->data.call.arguments[i],
                                            "World constructor '%s' implicitly copies zone binding '%s' into slot '%s'.\n"
                                            "Reason:\n"
                                            "- origin binding is '%s'\n"
                                            "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
                                            "- world embedding currently snapshots the zone binding into a world-owned zone slot\n"
                                            "- mutating the original binding afterwards would diverge from the embedded world state\n"
                                            "Fix:\n"
                                            "- use Clone(%s) for an explicit copy\n"
                                            "- or construct the zone inline / mutate it through the owning world after embedding",
                                            display_name != NULL ? display_name : "<world>",
                                            expr->data.call.arguments[i]->data.identifier.name != NULL
                                                ? expr->data.call.arguments[i]->data.identifier.name
                                                : "<zone>",
                                            zone_slot->data.world_zone.slot_name != NULL
                                                ? zone_slot->data.world_zone.slot_name
                                                : "<slot>",
                                            expr->data.call.arguments[i]->data.identifier.name != NULL
                                                ? expr->data.call.arguments[i]->data.identifier.name
                                                : "<zone>",
                                            expr->data.call.arguments[i]->data.identifier.name != NULL
                                                ? expr->data.call.arguments[i]->data.identifier.name
                                                : "<zone>",
                                            display_name != NULL ? display_name : "<world>",
                                            zone_slot->data.world_zone.slot_name != NULL
                                                ? zone_slot->data.world_zone.slot_name
                                                : "<slot>",
                                            expr->data.call.arguments[i]->data.identifier.name != NULL
                                                ? expr->data.call.arguments[i]->data.identifier.name
                                                : "<zone>");
                                        arg_sym->embedded_in_world = true;
                                        semantic_ctx_mark_embedded_world_zone_name(ctx,
                                            expr->data.call.arguments[i]->data.identifier.name,
                                            display_name,
                                            zone_slot->data.world_zone.slot_name);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    /* For generic classes, still type-check expressions */
                    for (size_t i = 0; i < provided; i++)
                        type_check_expression(expr->data.call.arguments[i], ctx);
                }
            }
        }
        sym->is_used = true;
        return sym->type;
    }

    if (sym->type->kind != TYPE_KIND_FUNCTION) {
        char sig_buf[256];
        semantic_format_function_signature(sym->type, sig_buf, sizeof(sig_buf));
        if (sig_buf[0] == '\0')
            snprintf(sig_buf, sizeof(sig_buf), "fn(<signature unavailable>)");
        semantic_error(ctx, expr,
            "'%s' resolves to a %s '%s' of type '%s', but function calls require a callable declaration. "
            "Expected a function signature matching %s. Use a function name, a callable variable, "
            "or adjust this symbol declaration before calling.",
            display_name,
            semantic_symbol_kind_label(sym->kind),
            display_name,
            type_name_or_unknown(sym->type),
            sig_buf);
        return TYPE_UNKNOWN;
    }
    if (ctx->program_root != NULL
        && (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_INTENT)) {
        ASTNode *decl = find_callable_decl_by_name(ctx->program_root, display_name);
        if (decl != NULL && !explicit_type_reference_allowed(decl, expr, ctx)) {
            semantic_error(ctx, expr,
                "Callable '%s' is not accessible across the current visibility boundary",
                display_name);
            return sym->type->data.function.return_type;
        }
    }
    sym->is_used = true;
    semantic_record_effect(ctx, type_function_effects(sym->type));
    if (ctx->in_parallel
        && type_effect_mask_has(type_function_effects(sym->type), EFFECT_SECURE)) {
        semantic_error(ctx, expr,
            "Parallel context does not permit calling secure-effect function '%s'; serialize capability-bearing operations outside the parallel block",
            display_name);
        return sym->type->data.function.return_type;
    }

    size_t expected = sym->type->data.function.param_count;
    size_t provided = expr->data.call.arg_count;
    ASTNode *callable_decl = NULL;
    GenericParams *callable_generic_params = NULL;
    Type **effective_generic_types = NULL;
    if (provided != expected) {
        char sig_buf[256];
        semantic_format_function_signature(sym->type, sig_buf, sizeof(sig_buf));
        if (sig_buf[0] == '\0')
            snprintf(sig_buf, sizeof(sig_buf), "fn(<signature unavailable>)");
        semantic_error(ctx, expr,
            "'%s' expects %zu argument(s), got %zu. Declared signature: %s. "
            "Adjust caller arguments to satisfy the declaration.",
            display_name, expected, provided, sig_buf);
        return sym->type->data.function.return_type;
    }

    Type **call_arg_types = calloc(provided > 0 ? provided : 1, sizeof(Type *));
    if (ctx->program_root != NULL
        && (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_INTENT)) {
        callable_decl = find_callable_decl_by_name(ctx->program_root, display_name);
        if (callable_decl != NULL
            && callable_decl->type == AST_FUNC_DECL
            && callable_decl->data.func_decl.generic_params != NULL
            && callable_decl->data.func_decl.generic_params->count > 0) {
            callable_generic_params = callable_decl->data.func_decl.generic_params;
            effective_generic_types = calloc(
                callable_generic_params->count > 0
                    ? callable_generic_params->count : 1,
                sizeof(Type *));
            if (effective_generic_types != NULL) {
                for (size_t gi = 0; gi < callable_generic_params->count; gi++) {
                    GenericParam *gp = callable_generic_params->params[gi];
                    if (gp != NULL && gp->default_type != NULL)
                        effective_generic_types[gi] =
                            resolve_type_node(gp->default_type, ctx);
                }
            }
        }
    }

    for (size_t i = 0; i < provided; i++) {
        Type *param_type = sym->type->data.function.param_types[i];
        Type *arg_type = type_is_movable_resource_handle(param_type)
            ? type_check_qubit_use(expr->data.call.arguments[i], ctx)
            : type_check_expression(expr->data.call.arguments[i], ctx);
        if (effective_generic_types != NULL
            && callable_decl != NULL
            && i < callable_decl->data.func_decl.param_count) {
            FuncParam *fp = callable_decl->data.func_decl.params[i];
            int param_index = -1;
            if (fp != NULL && fp->type != NULL
                && fp->type->type == AST_TYPE
                && fp->type->data.type.name != NULL) {
                param_index = find_generic_param_index(
                    callable_generic_params,
                    fp->type->data.type.name);
            }
            if (param_index >= 0
                && (size_t)param_index < callable_generic_params->count) {
                effective_generic_types[param_index] = arg_type;
                if (arg_type != NULL) {
                    param_type = arg_type;
                }
            }
        }
        if (call_arg_types != NULL)
            call_arg_types[i] = arg_type;

        if (type_is_movable_resource_handle(arg_type)
            || type_is_movable_resource_handle(param_type)) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            if (!type_is_movable_resource_handle(arg_type)
                || !type_is_movable_resource_handle(param_type)) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Movable resource argument type mismatch: expected '%s', got '%s'",
                    resource_handle_display_name(param_type),
                    resource_handle_display_name(arg_type));
                continue;
            }
            if (expr->data.call.arguments[i]->type != AST_IDENTIFIER) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "%s arguments must be moved from a named variable; bind the value first, then pass that variable",
                    resource_handle_display_name(param_type));
                continue;
            }
            if (ctx->program_root != NULL) {
                ASTNode *prog = ctx->program_root;
                for (size_t si = 0; si < prog->data.program.count; si++) {
                    ASTNode *stmt = prog->data.program.statements[si];
                    if (stmt != NULL && stmt->type == AST_FUNC_DECL
                        && stmt->data.func_decl.name != NULL
                        && strcmp(stmt->data.func_decl.name, display_name) == 0
                        && i < stmt->data.func_decl.param_count) {
                        callee_decl = stmt;
                        pmode = stmt->data.func_decl.params[i]->mode;
                        break;
                    }
                }
            }
            if (pmode == PARAM_MODE_REF
                && expr->data.call.arguments[i]->type == AST_IDENTIFIER
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)
                && callee_decl != NULL
                && callee_decl->data.func_decl.body != NULL) {
                unsigned callee_mask = slot_analyze_param_summary_in_program(
                    callee_decl->data.func_decl.body,
                    callee_decl->data.func_decl.params[i] != NULL
                        ? callee_decl->data.func_decl.params[i]->name
                        : NULL,
                    ctx->program_root);
                if ((callee_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0) {
                    semantic_error(ctx, expr->data.call.arguments[i],
                        "Borrowed ref movable resource '%s' cannot escape through helper/function call to '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                        "- callee '%s' derives a transitive call-escape path for its borrowed parameter\n"
                        "- forwarding this borrow into that helper would let it escape beyond the current call boundary\n"
                        "Fix:\n"
                        "- perform the movable-resource operation locally in this function\n"
                        "- or change the current parameter to 'own' if transfer/forwarding is intended",
                        expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>",
                        ctx->current_function_decl != NULL
                            && ctx->current_function_decl->data.func_decl.name != NULL
                                ? ctx->current_function_decl->data.func_decl.name
                                : "<anonymous>",
                        expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>");
                    continue;
                }
            }
            if (pmode != PARAM_MODE_REF
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)) {
                    semantic_error(ctx, expr->data.call.arguments[i],
                    "Borrowed ref movable resource '%s' cannot escape through helper/function call to '%s'.\n"
                    "Reason:\n"
                    "- consumer path is function '%s'\n"
                    "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                    "- forwarding it to '%s' as '%s' would create a transitive transfer beyond the current call boundary\n"
                    "Fix:\n"
                    "- call a 'ref' helper instead\n"
                    "- or change the current parameter to 'own' if forwarding/transfer is intended",
                    expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    ctx->current_function_decl != NULL
                        && ctx->current_function_decl->data.func_decl.name != NULL
                            ? ctx->current_function_decl->data.func_decl.name
                            : "<anonymous>",
                    expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    pmode == PARAM_MODE_OWN ? "own" : "default");
                continue;
            }
            if (pmode == PARAM_MODE_REF)
                continue;
            consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
            continue;
        }

        if (type_is_subject_type(param_type, ctx)) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            if (!type_is_subject_type(arg_type, ctx)) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Subject argument type mismatch: expected '%s', got '%s'.\n"
                    "Reason:\n"
                    "- subject boundary passing requires the caller value and callee contract to agree on the same subject type\n"
                    "- ownership/borrow provenance cannot be preserved across mismatched subject types\n"
                    "Fix:\n"
                    "- pass a value of type '%s'\n"
                    "- or change the callee contract to accept '%s'",
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type),
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type));
                continue;
            }
            if (ctx->program_root != NULL) {
                ASTNode *prog = ctx->program_root;
                for (size_t si = 0; si < prog->data.program.count; si++) {
                    ASTNode *stmt = prog->data.program.statements[si];
                    if (stmt != NULL && stmt->type == AST_FUNC_DECL
                        && stmt->data.func_decl.name != NULL
                        && strcmp(stmt->data.func_decl.name, display_name) == 0
                        && i < stmt->data.func_decl.param_count) {
                        callee_decl = stmt;
                        pmode = stmt->data.func_decl.params[i]->mode;
                        break;
                    }
                }
            }
            if (expr->data.call.arguments[i]->type != AST_IDENTIFIER) {
                if (pmode == PARAM_MODE_DEFAULT)
                    continue;
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Subject boundary arguments must use a named variable.\n"
                    "Reason:\n"
                    "- own/ref subject boundaries need one stable source binding for moved-here / borrowed-here provenance\n"
                    "- unnamed subject expressions would make later ownership diagnostics ambiguous\n"
                    "Fix:\n"
                    "- bind the subject to a local variable first\n"
                    "- then pass that named variable through the boundary");
                continue;
            }
            if (pmode == PARAM_MODE_REF
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)
                && callee_decl != NULL
                && callee_decl->data.func_decl.body != NULL) {
                unsigned callee_mask = slot_analyze_param_summary_in_program(
                    callee_decl->data.func_decl.body,
                    callee_decl->data.func_decl.params[i] != NULL
                        ? callee_decl->data.func_decl.params[i]->name
                        : NULL,
                    ctx->program_root);
                if ((callee_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0) {
                    semantic_error(ctx, expr->data.call.arguments[i],
                        "Borrowed ref subject '%s' cannot escape through helper/function call to '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' subject\n"
                        "- callee '%s' derives a transitive call-escape path for its borrowed parameter\n"
                        "- forwarding this borrow into that helper would let it escape beyond the current call boundary\n"
                        "Fix:\n"
                        "- perform the subject operation locally in this function\n"
                        "- or change the current parameter to 'own' if transfer/forwarding is intended",
                        expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>",
                        ctx->current_function_decl != NULL
                            && ctx->current_function_decl->data.func_decl.name != NULL
                                ? ctx->current_function_decl->data.func_decl.name
                                : "<anonymous>",
                        expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>");
                    continue;
                }
            }
            if (pmode != PARAM_MODE_REF
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Borrowed ref subject '%s' cannot escape through helper/function call to '%s'.\n"
                    "Reason:\n"
                    "- consumer path is function '%s'\n"
                    "- '%s' entered this function as a borrowed 'ref' subject\n"
                    "- forwarding it to '%s' as '%s' would create a transitive transfer beyond the current call boundary\n"
                    "Fix:\n"
                    "- call a 'ref' helper instead\n"
                    "- or change the current parameter to 'own' if forwarding/transfer is intended",
                    expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    ctx->current_function_decl != NULL
                        && ctx->current_function_decl->data.func_decl.name != NULL
                            ? ctx->current_function_decl->data.func_decl.name
                            : "<anonymous>",
                    expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    pmode == PARAM_MODE_OWN ? "own" : "default");
                continue;
            }
            if (pmode == PARAM_MODE_OWN) {
                consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
            }
            continue;
        }

        if (type_is_general_boundary_type(param_type, ctx)
            && !type_is_subject_type(param_type, ctx)
            && !type_is_movable_resource_handle(param_type)) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            if (!type_is_general_boundary_type(arg_type, ctx)
                || type_is_subject_type(arg_type, ctx)
                || type_is_movable_resource_handle(arg_type)
                || !type_is_assignable(arg_type, param_type)
                || !type_is_assignable(param_type, arg_type)) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Boundary value argument type mismatch: expected '%s', got '%s'.\n"
                    "Reason:\n"
                    "- own/ref value boundaries require the caller value and callee contract to agree on the same boundary value type\n"
                    "- provenance cannot be preserved across mismatched boundary value types\n"
                    "Fix:\n"
                    "- pass a value of type '%s'\n"
                    "- or change the callee contract to accept '%s'",
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type),
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type));
                continue;
            }
            if (ctx->program_root != NULL) {
                ASTNode *prog = ctx->program_root;
                for (size_t si = 0; si < prog->data.program.count; si++) {
                    ASTNode *stmt = prog->data.program.statements[si];
                    if (stmt != NULL && stmt->type == AST_FUNC_DECL
                        && stmt->data.func_decl.name != NULL
                        && strcmp(stmt->data.func_decl.name, display_name) == 0
                        && i < stmt->data.func_decl.param_count) {
                        callee_decl = stmt;
                        pmode = stmt->data.func_decl.params[i]->mode;
                        break;
                    }
                }
            }
            if (expr->data.call.arguments[i]->type != AST_IDENTIFIER) {
                if (pmode == PARAM_MODE_DEFAULT)
                    continue;
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Value boundary arguments must use a named variable.\n"
                    "Reason:\n"
                    "- own/ref value boundaries need one stable source binding for moved-here / borrowed-here provenance\n"
                    "- unnamed expressions would make later ownership diagnostics ambiguous\n"
                    "Fix:\n"
                    "- bind the value to a local variable first\n"
                    "- then pass that named variable through the boundary");
                continue;
            }
            if (pmode == PARAM_MODE_REF
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)
                && type_requires_boundary_borrow_tracking(param_type, ctx)
                && callee_decl != NULL
                && callee_decl->data.func_decl.body != NULL) {
                unsigned callee_mask = slot_analyze_param_summary_in_program(
                    callee_decl->data.func_decl.body,
                    callee_decl->data.func_decl.params[i] != NULL
                        ? callee_decl->data.func_decl.params[i]->name
                        : NULL,
                    ctx->program_root);
                if ((callee_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0) {
                    semantic_error(ctx, expr->data.call.arguments[i],
                        "Borrowed ref boundary value '%s' cannot escape through helper/function call to '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                        "- callee '%s' derives a transitive call-escape path for its borrowed parameter\n"
                        "- forwarding this borrow into that helper would let it escape beyond the current call boundary\n"
                        "Fix:\n"
                        "- perform the value operation locally in this function\n"
                        "- or change the current parameter to 'own' if transfer/forwarding is intended",
                        expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>",
                        ctx->current_function_decl != NULL
                            && ctx->current_function_decl->data.func_decl.name != NULL
                                ? ctx->current_function_decl->data.func_decl.name
                                : "<anonymous>",
                        expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>");
                    continue;
                }
            }
            if (pmode != PARAM_MODE_REF
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)) {
                if (!type_requires_boundary_borrow_tracking(param_type, ctx))
                    continue;
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Borrowed ref boundary value '%s' cannot escape through helper/function call to '%s'.\n"
                    "Reason:\n"
                    "- consumer path is function '%s'\n"
                    "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                    "- forwarding it to '%s' as '%s' would create a transitive transfer beyond the current call boundary\n"
                    "Fix:\n"
                    "- call a 'ref' helper instead\n"
                    "- or change the current parameter to 'own' if forwarding/transfer is intended",
                    expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    ctx->current_function_decl != NULL
                        && ctx->current_function_decl->data.func_decl.name != NULL
                            ? ctx->current_function_decl->data.func_decl.name
                            : "<anonymous>",
                    expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    pmode == PARAM_MODE_OWN ? "own" : "default");
                continue;
            }
            if (pmode == PARAM_MODE_OWN) {
                consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
            }
            continue;
        }

        if (type_is_anchored_resource_handle(arg_type)
            || type_is_anchored_resource_handle(param_type)) {
            if (!type_is_anchored_resource_handle(arg_type)
                || !type_is_anchored_resource_handle(param_type)) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Resource handle argument type mismatch: expected '%s', got '%s'",
                    resource_handle_display_name(param_type),
                    resource_handle_display_name(arg_type));
                continue;
            }
            /* Check own/ref qualifier from function declaration AST */
            {
                ParamMode pmode = PARAM_MODE_DEFAULT;
                if (ctx->program_root != NULL) {
                    ASTNode *prog = ctx->program_root;
                    for (size_t si = 0; si < prog->data.program.count; si++) {
                        ASTNode *stmt = prog->data.program.statements[si];
                        if (stmt != NULL && stmt->type == AST_FUNC_DECL
                            && stmt->data.func_decl.name != NULL
                            && strcmp(stmt->data.func_decl.name, display_name) == 0
                            && i < stmt->data.func_decl.param_count) {
                            pmode = stmt->data.func_decl.params[i]->mode;
                            break;
                        }
                    }
                }
                if (pmode == PARAM_MODE_OWN) {
                    /* own: move — consume the source slot */
                    if (expr->data.call.arguments[i]->type == AST_IDENTIFIER) {
                        const char *src = expr->data.call.arguments[i]->data.identifier.name;
                        Symbol *src_sym = scope_lookup(ctx->scope, src);
                        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT) {
                            if (src_sym->slot_info.state == SLOT_STATE_RELEASED)
                                semantic_error_code(ctx, "PGY_SEM_MOVE_FROM_RELEASED",
                                    expr->data.call.arguments[i],
                                    "Cannot move from released slot '%s'", src);
                            else
                                src_sym->slot_info.state = SLOT_STATE_RELEASED;
                        }
                    }
                    continue;
                } else if (pmode == PARAM_MODE_REF) {
                    /* ref: borrow — source remains valid */
                    if (expr->data.call.arguments[i]->type == AST_IDENTIFIER) {
                        const char *src = expr->data.call.arguments[i]->data.identifier.name;
                        Symbol *src_sym = scope_lookup(ctx->scope, src);
                        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT
                            && src_sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error(ctx, expr->data.call.arguments[i],
                                "Cannot borrow released slot '%s'.\n"
                                "Reason:\n"
                                "- slot '%s' was already released or invalidated earlier in this scope\n"
                                "- a 'ref' borrow requires a live source handle\n"
                                "Fix:\n"
                                "- reacquire the slot before borrowing it\n"
                                "- or remove the earlier release/invalidating transfer",
                                src,
                                src);
                        }
                    }
                    continue;
                }
            }
            semantic_error(ctx, expr->data.call.arguments[i],
                "Slot arguments require 'own' or 'ref' qualifier in function signature; "
                "use 'func F(own s: Slot<T>)' to take ownership or "
                "'func F(ref s: Slot<T>)' to borrow");
            continue;
        }

        require_assignable(arg_type, param_type, expr->data.call.arguments[i], ctx);
    }

    /* Validate generic function where-clause constraints */
    if (ctx->program_root != NULL) {
        ASTNode *prog = ctx->program_root;
        for (size_t si = 0; si < prog->data.program.count; si++) {
            ASTNode *stmt = prog->data.program.statements[si];
            if (stmt != NULL && stmt->type == AST_FUNC_DECL
                && stmt->data.func_decl.name != NULL
                && strcmp(stmt->data.func_decl.name, display_name) == 0
                && stmt->data.func_decl.generic_params != NULL
                && stmt->data.func_decl.generic_params->count > 0
                && stmt->data.func_decl.where_clause != NULL) {
                /* Generic function with where clause — validate constraints */
                GenericParams *decl_gp = stmt->data.func_decl.generic_params;
                WhereClause *wc = stmt->data.func_decl.where_clause;
                char *expected_sig = format_generic_subject_signature(display_name, decl_gp);
                Type **effective_generic_types =
                    calloc(decl_gp->count > 0 ? decl_gp->count : 1, sizeof(Type *));
                if (effective_generic_types == NULL) {
                    free(expected_sig);
                    break;
                }

                for (size_t gi = 0; gi < decl_gp->count; gi++) {
                    GenericParam *gp = decl_gp->params[gi];
                    if (gp != NULL && gp->default_type != NULL)
                        effective_generic_types[gi] = resolve_type_node(gp->default_type, ctx);
                }
                for (size_t ai = 0; ai < provided; ai++) {
                    FuncParam *fp = (ai < stmt->data.func_decl.param_count)
                        ? stmt->data.func_decl.params[ai] : NULL;
                    int param_index;
                    if (fp == NULL || fp->type == NULL
                        || fp->type->type != AST_TYPE
                        || fp->type->data.type.name == NULL) {
                        continue;
                    }
                    param_index = find_generic_param_index(
                        decl_gp, fp->type->data.type.name);
                    if (param_index < 0)
                        continue;
                    effective_generic_types[param_index] =
                        (call_arg_types != NULL) ? call_arg_types[ai] : NULL;
                }

                for (size_t ci = 0; ci < wc->count; ci++) {
                    TypeConstraint *tc = wc->constraints[ci];
                    int param_index;
                    const char *param_name;
                    Type *concrete_type;
                    char *actual_sig = format_effective_generic_type_list(
                        display_name, effective_generic_types, decl_gp->count);

                    if (tc == NULL || tc->type_param == NULL)
                    {
                        free(actual_sig);
                        continue;
                    }

                    param_name = tc->type_param;
                    param_index = find_generic_param_index(decl_gp, param_name);
                    if (param_index < 0
                        || (size_t)param_index >= decl_gp->count) {
                        semantic_error(ctx, expr,
                            "Call site of '%s' could not validate generic parameter '%s'.\n"
                            "Reason:\n"
                            "- function '%s' where-clause references '%s'\n"
                            "- that parameter does not exist in the function generic parameter list\n"
                            "Fix:\n"
                            "- change the function where-clause to reference an existing generic parameter\n"
                            "- or add generic parameter '%s' to function '%s'",
                            display_name,
                            param_name != NULL ? param_name : "<param>",
                            display_name,
                            param_name != NULL ? param_name : "<param>",
                            param_name != NULL ? param_name : "<param>",
                            display_name);
                        free(actual_sig);
                        continue;
                    }
                    concrete_type = effective_generic_types[param_index];
                    if (concrete_type == NULL) {
                        semantic_error(ctx, expr,
                            "Call site of '%s' could not materialize generic argument for '%s'.\n"
                            "Reason:\n"
                            "- function '%s' where-clause validation depends on an effective type argument for '%s'\n"
                            "- neither explicit call-site evidence nor declaration defaults produced a concrete type\n"
                            "Fix:\n"
                            "- pass arguments that make '%s' concrete\n"
                            "- or add/fix a default type argument for '%s'",
                            display_name,
                            param_name != NULL ? param_name : "<param>",
                            display_name,
                            param_name != NULL ? param_name : "<param>",
                            param_name != NULL ? param_name : "<param>",
                            param_name != NULL ? param_name : "<param>");
                        free(actual_sig);
                        continue;
                    }
                    /* Check each bound */
                    for (size_t bi = 0; bi < tc->bound_count; bi++) {
                        char *bounds_text = format_type_constraint_bounds(tc);
                        const char *bound_name =
                            (tc->bounds[bi] != NULL
                             && tc->bounds[bi]->type == AST_TYPE
                             && tc->bounds[bi]->data.type.name != NULL)
                                ? tc->bounds[bi]->data.type.name
                                : NULL;
                        bool satisfies = concrete_type_satisfies_bound(
                            concrete_type, tc->bounds[bi], ctx);
                        if (!satisfies) {
                            semantic_error(ctx, expr,
                                "Type '%s' does not satisfy constraint '%s' for generic parameter '%s'.\n"
                                "Reason:\n"
                                "- generic subject is function '%s'\n"
                                "- function '%s' requires '%s: %s'\n"
                                "- full bound set is '%s: %s'\n"
                                "- expected type args are '%s'\n"
                                "- actual type args are '%s'\n"
                                "- actual type argument is '%s'\n"
                                "- consumer path is call site of '%s'\n"
                                "- call-site derived type arguments do not satisfy bound '%s'\n"
                                "- expected type args are constrained by '%s: %s'\n"
                                "Fix:\n"
                                "- pass an argument whose type satisfies '%s'\n"
                                "- or relax the function where-clause",
                                concrete_type->name != NULL ? concrete_type->name : "<type>",
                                bound_name != NULL ? bound_name : "<constraint>",
                                param_name,
                                display_name,
                                display_name,
                                param_name,
                                bound_name != NULL ? bound_name : "<constraint>",
                                param_name,
                                bounds_text != NULL ? bounds_text : "<constraint>",
                                expected_sig != NULL ? expected_sig : display_name,
                                actual_sig != NULL ? actual_sig : display_name,
                                concrete_type->name != NULL ? concrete_type->name : "<type>",
                                display_name,
                                bound_name != NULL ? bound_name : "<constraint>",
                                param_name,
                                bound_name != NULL ? bound_name : "<constraint>",
                                bound_name != NULL ? bound_name : "<constraint>");
                        }
                        free(bounds_text);
                    }
                    free(actual_sig);
                }
                free(expected_sig);
                free(effective_generic_types);
                break;
            }
        }
    }

    Type *return_type = sym->type->data.function.return_type;
    if (effective_generic_types != NULL
        && callable_decl != NULL
        && callable_decl->type == AST_FUNC_DECL
        && callable_decl->data.func_decl.return_type != NULL
        && callable_decl->data.func_decl.return_type->type == AST_TYPE
        && callable_decl->data.func_decl.return_type->data.type.name != NULL) {
        int return_index = find_generic_param_index(
            callable_generic_params,
            callable_decl->data.func_decl.return_type->data.type.name);
        if (return_index >= 0
            && (size_t)return_index < callable_generic_params->count
            && effective_generic_types[return_index] != NULL) {
            return_type = effective_generic_types[return_index];
        }
    }

    free(effective_generic_types);
    free(call_arg_types);
    return return_type;
}

static Symbol *
lookup_identifier_symbol(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL || expr->type != AST_IDENTIFIER
        || expr->data.identifier.name == NULL) {
        return NULL;
    }
    return scope_lookup(ctx->scope, expr->data.identifier.name);
}

bool
consume_qubit_value(ASTNode *expr, SemanticContext *ctx, const char *action)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL
        || !type_is_general_boundary_type(sym->type, ctx))
        return false;

    if (sym->is_consumed) {
        return false;
    }

    sym->is_consumed = true;
    sym->is_used = true;
    (void)action;
    return true;
}
