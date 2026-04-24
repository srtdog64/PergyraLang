static const char *infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr);
static char *escape_c_string_literal(const char *src);
static char *render_type_name(ASTNode *type_node);
static char *render_ability_ref_vtable_tag(ASTNode *ability_ref);
static bool transpiler_can_forward_declare_type_early(TranspilerCtx *ctx, ASTNode *type_node);
static bool transpiler_can_forward_declare_func_early(TranspilerCtx *ctx, ASTNode *func);
static bool transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx, ASTNode *func);
static void transpiler_bind_current_host_decl_local(TranspilerCtx *ctx, ASTNode *decl);
static ASTNode *transpiler_current_host_decl_local(TranspilerCtx *ctx);
static ASTNode *transpiler_find_decl_in_inventory_local(TranspilerCtx *ctx,
                                                        ASTNodeType decl_type,
                                                        const char *name);
static bool resolve_world_zone_subject_receiver(TranspilerCtx *ctx, ASTNode *receiver,
                                                const char **zone_slot_name_out,
                                                const char **zone_type_name_out,
                                                const char **slot_name_out,
                                                const char **type_name_out);
/* -----------------------------------------------------------------
 * CodeBuf
 * ----------------------------------------------------------------- */

#define CODEBUF_INITIAL_CAP 4096

CodeBuf *
codebuf_create(void)
{
    CodeBuf *b = calloc(1, sizeof(CodeBuf));
    if (b == NULL)
        return NULL;
    b->data = malloc(CODEBUF_INITIAL_CAP);
    if (b->data == NULL) {
        free(b);
        return NULL;
    }
    b->data[0] = '\0';
    b->cap     = CODEBUF_INITIAL_CAP;
    b->len     = 0;
    return b;
}

void
codebuf_destroy(CodeBuf *buf)
{
    if (buf == NULL)
        return;
    free(buf->data);
    free(buf);
}

void
codebuf_write(CodeBuf *buf, const char *fmt, ...)
{
    static const char null_text[] = "(null)";
    va_list ap;
    const char *s;
    char ch;

    if (buf == NULL || fmt == NULL)
        return;

    if (strchr(fmt, '%') == NULL) {
        codebuf_write_raw(buf, fmt, strlen(fmt));
        return;
    }

    va_start(ap, fmt);

    if (strcmp(fmt, "%s") == 0) {
        s = va_arg(ap, const char *);
        if (s != NULL)
            codebuf_write_raw(buf, s, strlen(s));
        else
            codebuf_write_raw(buf, null_text, sizeof(null_text) - 1);
        va_end(ap);
        return;
    }

    if (strcmp(fmt, "%c") == 0) {
        ch = (char)va_arg(ap, int);
        codebuf_write_raw(buf, &ch, 1);
        va_end(ap);
        return;
    }

    /* Measure required space */
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);

    if (needed < 0) {
        va_end(ap);
        return;
    }

    /* Grow if needed */
    while (buf->len + (size_t)needed + 1 > buf->cap) {
        size_t new_cap = buf->cap * 2;
        char  *grown   = realloc(buf->data, new_cap);
        if (grown == NULL) {
            va_end(ap);
            return;
        }
        buf->data = grown;
        buf->cap  = new_cap;
    }

    vsnprintf(buf->data + buf->len, buf->cap - buf->len, fmt, ap);
    buf->len += (size_t)needed;
    va_end(ap);
}

void
codebuf_write_raw(CodeBuf *buf, const char *s, size_t n)
{
    while (buf->len + n + 1 > buf->cap) {
        size_t new_cap = buf->cap * 2;
        char  *grown   = realloc(buf->data, new_cap);
        if (grown == NULL)
            return;
        buf->data = grown;
        buf->cap  = new_cap;
    }
    memcpy(buf->data + buf->len, s, n);
    buf->len += n;
    buf->data[buf->len] = '\0';
}

bool
codebuf_dump_file(const CodeBuf *buf, const char *path)
{
    FILE *f = fopen(path, "w");
    if (f == NULL)
        return false;
    fwrite(buf->data, 1, buf->len, f);
    fclose(f);
    return true;
}

/* -----------------------------------------------------------------
 * TranspilerCtx
 * ----------------------------------------------------------------- */

TranspilerCtx *
transpiler_ctx_create(void)
{
    TranspilerCtx *ctx = calloc(1, sizeof(TranspilerCtx));
    if (ctx == NULL)
        return NULL;
    pgy_arena_init(&ctx->arena, 0);
    ctx->out   = codebuf_create();
    ctx->decls = codebuf_create();
    ctx->helpers = codebuf_create();
    if (ctx->out == NULL || ctx->decls == NULL || ctx->helpers == NULL) {
        codebuf_destroy(ctx->out);
        codebuf_destroy(ctx->decls);
        codebuf_destroy(ctx->helpers);
        pgy_arena_destroy(&ctx->arena);
        free(ctx);
        return NULL;
    }
    return ctx;
}

void
transpiler_ctx_destroy(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return;
    codebuf_destroy(ctx->out);
    codebuf_destroy(ctx->decls);
    codebuf_destroy(ctx->helpers);
    free(ctx->backend_error);
    pgy_arena_destroy(&ctx->arena);
    free(ctx);
}

/* -----------------------------------------------------------------
 * Indent helper
 * ----------------------------------------------------------------- */

static void
write_indent(TranspilerCtx *ctx)
{
    static const char indent_chunk[] =
        "                                                                ";
    size_t remaining;

    if (ctx == NULL || ctx->out == NULL || ctx->indent <= 0)
        return;

    remaining = (size_t)ctx->indent * 4;
    while (remaining > 0) {
        size_t chunk = remaining;
        if (chunk > sizeof(indent_chunk) - 1)
            chunk = sizeof(indent_chunk) - 1;
        codebuf_write_raw(ctx->out, indent_chunk, chunk);
        remaining -= chunk;
    }
}

static void
write_indent_to(CodeBuf *buf, int indent)
{
    static const char indent_chunk[] =
        "                                                                ";
    size_t remaining;

    if (buf == NULL || indent <= 0)
        return;

    remaining = (size_t)indent * 4;
    while (remaining > 0) {
        size_t chunk = remaining;
        if (chunk > sizeof(indent_chunk) - 1)
            chunk = sizeof(indent_chunk) - 1;
        codebuf_write_raw(buf, indent_chunk, chunk);
        remaining -= chunk;
    }
}

static TranspilerCtx *g_type_render_ctx = NULL;
static char *strdup_fmt(const char *fmt, ...);
static char *transpiler_scratch_strdup(TranspilerCtx *ctx, const char *s);
static char *transpiler_scratch_fmt(TranspilerCtx *ctx, const char *fmt, ...);

static void
transpiler_set_backend_error_v(TranspilerCtx *ctx, const char *code,
                               const char *fmt, va_list ap)
{
    va_list ap2;
    int needed;
    char *msg;

    if (ctx == NULL || fmt == NULL || ctx->backend_error != NULL)
        return;

    va_copy(ap2, ap);
    needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (needed < 0)
        return;

    msg = malloc((size_t)needed + 1);
    if (msg == NULL)
        return;

    vsnprintf(msg, (size_t)needed + 1, fmt, ap);
    ctx->backend_error = msg;
    /* `code` must be a string literal (non-owning). NULL preserves the
     * legacy path — emit the error without a stable code. */
    if (code != NULL && ctx->backend_error_code == NULL)
        ctx->backend_error_code = code;
}

static void
transpiler_set_backend_error(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    transpiler_set_backend_error_v(ctx, NULL, fmt, ap);
    va_end(ap);
}

static char *
transpiler_scratch_strdup(TranspilerCtx *ctx, const char *s)
{
    if (ctx == NULL)
        return NULL;
    return pgy_arena_strdup(&ctx->arena, s);
}

static char *
transpiler_scratch_fmt(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;
    char *result;

    if (ctx == NULL || fmt == NULL)
        return NULL;

    va_start(ap, fmt);
    result = pgy_arena_vfmt(&ctx->arena, fmt, ap);
    va_end(ap);
    return result;
}

/* Full variant that records `cause_ir` + `fix_source` tags alongside the
 * code. All three must be non-owning string literals; NULL is allowed
 * per-field to omit. First error wins (matches legacy setter). */
static void
transpiler_set_backend_error_with_hints(TranspilerCtx *ctx, const char *code,
                                         const char *cause_ir,
                                         const char *fix_source,
                                         const char *fmt, ...)
{
    va_list ap;
    if (ctx == NULL || fmt == NULL || ctx->backend_error != NULL)
        return;
    if (cause_ir != NULL)
        ctx->backend_error_cause_ir = cause_ir;
    if (fix_source != NULL)
        ctx->backend_error_fix_source = fix_source;
    va_start(ap, fmt);
    transpiler_set_backend_error_v(ctx, code, fmt, ap);
    va_end(ap);
}

/* -----------------------------------------------------------------
 * Slot variable tracking
 * ----------------------------------------------------------------- */

static void
register_slot_var(TranspilerCtx *ctx, const char *name,
                  const char *inner_type, bool is_secure, bool is_indirect)
{
    if (ctx->slot_var_count >= MAX_SLOT_VARS)
        return;
    SlotVarEntry *e = &ctx->slot_vars[ctx->slot_var_count++];
    ctx->last_slot_var_index = ctx->slot_var_count - 1;
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    strncpy(e->inner_type, inner_type, sizeof(e->inner_type) - 1);
    e->inner_type[sizeof(e->inner_type) - 1] = '\0';
    if (is_secure) {
        snprintf(e->token_name, sizeof(e->token_name), "%s_token", name);
    } else {
        e->token_name[0] = '\0';
    }
    e->is_secure = is_secure;
    e->is_indirect = is_indirect;
}

static void
set_slot_token_name(TranspilerCtx *ctx, const char *slot_name, const char *token_name)
{
    if (ctx == NULL || slot_name == NULL || token_name == NULL)
        return;
    if (ctx->last_slot_var_index >= 0
        && ctx->last_slot_var_index < ctx->slot_var_count
        && strcmp(ctx->slot_vars[ctx->last_slot_var_index].name, slot_name) == 0) {
        strncpy(ctx->slot_vars[ctx->last_slot_var_index].token_name, token_name,
                sizeof(ctx->slot_vars[ctx->last_slot_var_index].token_name) - 1);
        ctx->slot_vars[ctx->last_slot_var_index]
            .token_name[sizeof(ctx->slot_vars[ctx->last_slot_var_index].token_name) - 1] = '\0';
        return;
    }
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, slot_name) == 0) {
            ctx->last_slot_var_index = i;
            strncpy(ctx->slot_vars[i].token_name, token_name,
                    sizeof(ctx->slot_vars[i].token_name) - 1);
            ctx->slot_vars[i].token_name[sizeof(ctx->slot_vars[i].token_name) - 1] = '\0';
            return;
        }
    }
}

static const char *
lookup_slot_type(TranspilerCtx *ctx, const char *var_name)
{
    const char *typed_name;

    if (ctx == NULL || var_name == NULL)
        return NULL;

    if (ctx->last_slot_var_index >= 0
        && ctx->last_slot_var_index < ctx->slot_var_count
        && strcmp(ctx->slot_vars[ctx->last_slot_var_index].name, var_name) == 0) {
        return ctx->slot_vars[ctx->last_slot_var_index].inner_type;
    }

    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0) {
            ctx->last_slot_var_index = i;
            return ctx->slot_vars[i].inner_type;
        }
    }
    typed_name = lookup_typed_var(ctx, var_name);
    if (typed_name != NULL) {
        if (strncmp(typed_name, "Slot<", 5) == 0
            || strncmp(typed_name, "SecureSlot<", 11) == 0
            || strncmp(typed_name, "DeviceSlot<", 11) == 0) {
            return slot_inner_type_name(typed_name);
        }
        return typed_name;
    }
    if (ctx->active_type_hint != NULL) {
        if (strncmp(ctx->active_type_hint, "Slot<", 5) == 0
            || strncmp(ctx->active_type_hint, "SecureSlot<", 11) == 0
            || strncmp(ctx->active_type_hint, "DeviceSlot<", 11) == 0) {
            return slot_inner_type_name(ctx->active_type_hint);
        }
        return ctx->active_type_hint;
    }
    return NULL;
}

static bool
lookup_slot_is_secure(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx != NULL
        && ctx->last_slot_var_index >= 0
        && ctx->last_slot_var_index < ctx->slot_var_count
        && strcmp(ctx->slot_vars[ctx->last_slot_var_index].name, var_name) == 0)
        return ctx->slot_vars[ctx->last_slot_var_index].is_secure;
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0) {
            ctx->last_slot_var_index = i;
            return ctx->slot_vars[i].is_secure;
        }
    }
    return false;
}

static const char *
lookup_slot_token_name(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx != NULL
        && ctx->last_slot_var_index >= 0
        && ctx->last_slot_var_index < ctx->slot_var_count
        && strcmp(ctx->slot_vars[ctx->last_slot_var_index].name, var_name) == 0) {
        if (ctx->slot_vars[ctx->last_slot_var_index].token_name[0] != '\0')
            return ctx->slot_vars[ctx->last_slot_var_index].token_name;
        return NULL;
    }
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0) {
            ctx->last_slot_var_index = i;
            if (ctx->slot_vars[i].token_name[0] != '\0')
                return ctx->slot_vars[i].token_name;
            break;
        }
    }
    return NULL;
}

static bool
lookup_slot_is_indirect(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx != NULL
        && ctx->last_slot_var_index >= 0
        && ctx->last_slot_var_index < ctx->slot_var_count
        && strcmp(ctx->slot_vars[ctx->last_slot_var_index].name, var_name) == 0)
        return ctx->slot_vars[ctx->last_slot_var_index].is_indirect;
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0) {
            ctx->last_slot_var_index = i;
            return ctx->slot_vars[i].is_indirect;
        }
    }
    return false;
}

static char *
slot_ref_expr(TranspilerCtx *ctx, const char *slot_name, const char *slot_expr)
{
    (void)ctx;
    if (slot_name != NULL && lookup_slot_is_indirect(ctx, slot_name))
        return pergyra_strdup(slot_expr);
    return strdup_fmt("&%s", slot_expr);
}

static bool
is_slot_var(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx != NULL
        && ctx->last_slot_var_index >= 0
        && ctx->last_slot_var_index < ctx->slot_var_count
        && strcmp(ctx->slot_vars[ctx->last_slot_var_index].name, var_name) == 0)
        return true;
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0) {
            ctx->last_slot_var_index = i;
            return true;
        }
    }
    return false;
}

static void
register_typed_var(TranspilerCtx *ctx, const char *name, const char *type_name)
{
    if (ctx->typed_var_count >= MAX_SLOT_VARS || name == NULL || type_name == NULL)
        return;
    TypedVarEntry *e = &ctx->typed_vars[ctx->typed_var_count++];
    ctx->last_typed_var_index = ctx->typed_var_count - 1;
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    strncpy(e->type_name, type_name, sizeof(e->type_name) - 1);
    e->type_name[sizeof(e->type_name) - 1] = '\0';
}

static void
register_alias_var(TranspilerCtx *ctx, const char *name, ASTNode *target_expr)
{
    if (ctx == NULL || name == NULL || target_expr == NULL
        || ctx->alias_var_count >= MAX_ALIAS_VARS)
        return;

    AliasVarEntry *e = &ctx->alias_vars[ctx->alias_var_count++];
    ctx->last_alias_var_index = ctx->alias_var_count - 1;
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    e->target_expr = target_expr;
}

static ASTNode *
lookup_alias_expr(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx == NULL || var_name == NULL)
        return NULL;

    if (ctx->last_alias_var_index >= 0
        && ctx->last_alias_var_index < ctx->alias_var_count
        && strcmp(ctx->alias_vars[ctx->last_alias_var_index].name, var_name) == 0)
        return ctx->alias_vars[ctx->last_alias_var_index].target_expr;

    for (int i = ctx->alias_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->alias_vars[i].name, var_name) == 0) {
            ctx->last_alias_var_index = i;
            return ctx->alias_vars[i].target_expr;
        }
    }
    return NULL;
}

static TypedVarEntry *
lookup_typed_entry(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx != NULL
        && ctx->last_typed_var_index >= 0
        && ctx->last_typed_var_index < ctx->typed_var_count
        && strcmp(ctx->typed_vars[ctx->last_typed_var_index].name, var_name) == 0)
        return &ctx->typed_vars[ctx->last_typed_var_index];

    for (int i = ctx->typed_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->typed_vars[i].name, var_name) == 0) {
            ctx->last_typed_var_index = i;
            return &ctx->typed_vars[i];
        }
    }
    return NULL;
}

static const char *
lookup_typed_var(TranspilerCtx *ctx, const char *var_name)
{
    TypedVarEntry *entry = lookup_typed_entry(ctx, var_name);
    return entry != NULL ? entry->type_name : NULL;
}

static void
register_view_like_var(TranspilerCtx *ctx, const char *name, const char *type_name,
                       const char *source_slot, bool source_secure,
                       bool is_move_token)
{
    if (ctx->typed_var_count >= MAX_SLOT_VARS || name == NULL || type_name == NULL)
        return;
    TypedVarEntry *e = &ctx->typed_vars[ctx->typed_var_count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    strncpy(e->type_name, type_name, sizeof(e->type_name) - 1);
    e->type_name[sizeof(e->type_name) - 1] = '\0';
    if (source_slot != NULL) {
        strncpy(e->source_slot, source_slot, sizeof(e->source_slot) - 1);
        e->source_slot[sizeof(e->source_slot) - 1] = '\0';
    }
    e->is_view = !is_move_token;
    e->is_move_token = is_move_token;
    e->source_secure = source_secure;
}

static void
register_projection_borrow_var(TranspilerCtx *ctx, const char *name,
                               const char *type_name,
                               const char *source_name)
{
    if (ctx->typed_var_count >= MAX_SLOT_VARS
        || name == NULL || type_name == NULL || source_name == NULL)
        return;

    TypedVarEntry *e = &ctx->typed_vars[ctx->typed_var_count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    strncpy(e->type_name, type_name, sizeof(e->type_name) - 1);
    e->type_name[sizeof(e->type_name) - 1] = '\0';
    strncpy(e->source_slot, source_name, sizeof(e->source_slot) - 1);
    e->source_slot[sizeof(e->source_slot) - 1] = '\0';
    e->is_projection_borrow = true;
}

static ASTNode *
transpiler_find_named_decl_local(TranspilerCtx *ctx, ASTNodeType decl_type,
                                 const char *name);

static ASTNode *
find_extern_function_decl(TranspilerCtx *ctx, const char *function_name)
{
    ASTNode **decls = NULL;
    size_t decl_count = 0;

    if (ctx == NULL || function_name == NULL)
        return NULL;

    transpiler_active_inventory(ctx, AST_EXTERN_BLOCK, &decls, &decl_count);
    for (size_t i = 0; decls != NULL && i < decl_count; i++) {
        ASTNode *block = decls[i];
        if (block == NULL || block->type != AST_EXTERN_BLOCK)
            continue;
        for (size_t j = 0; j < block->data.extern_block.count; j++) {
            ASTNode *stmt = block->data.extern_block.declarations[j];
            if (stmt != NULL && stmt->type == AST_FUNC_DECL
                && stmt->data.func_decl.name != NULL
                && strcmp(stmt->data.func_decl.name, function_name) == 0) {
                return stmt;
            }
        }
    }
    return NULL;
}

static ASTNode *
find_role_decl(TranspilerCtx *ctx, const char *role_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ROLE_DECL, role_name);
}

static ASTNode *
find_function_decl(TranspilerCtx *ctx, const char *function_name)
{
    ASTNode *decl = transpiler_find_named_decl_local(ctx, AST_FUNC_DECL, function_name);
    if (decl != NULL)
        return decl;
    return find_extern_function_decl(ctx, function_name);
}

static ASTNode *
find_intent_decl(TranspilerCtx *ctx, const char *intent_name)
{
    return transpiler_find_named_decl_local(ctx, AST_INTENT_DECL, intent_name);
}

static ASTNode *
find_callable_decl(TranspilerCtx *ctx, const char *name)
{
    ASTNode *decl = find_function_decl(ctx, name);
    if (decl != NULL)
        return decl;
    return find_intent_decl(ctx, name);
}

static bool
transpiler_named_decl_matches(ASTNode *stmt, ASTNodeType decl_type, const char *name)
{
    if (stmt == NULL || name == NULL || stmt->type != decl_type)
        return false;

    switch (decl_type) {
    case AST_PARTY_DECL:
        return stmt->data.party_decl.name != NULL
            && strcmp(stmt->data.party_decl.name, name) == 0;
    case AST_ROSTER_DECL:
        return stmt->data.roster_decl.name != NULL
            && strcmp(stmt->data.roster_decl.name, name) == 0;
    case AST_ENUM_DECL:
        return stmt->data.enum_decl.name != NULL
            && strcmp(stmt->data.enum_decl.name, name) == 0;
    case AST_ROLE_DECL:
        return stmt->data.role_decl.name != NULL
            && strcmp(stmt->data.role_decl.name, name) == 0;
    case AST_CLASS_DECL:
        return stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, name) == 0;
    case AST_FUNC_DECL:
        return stmt->data.func_decl.name != NULL
            && strcmp(stmt->data.func_decl.name, name) == 0;
    case AST_INTENT_DECL:
        return stmt->data.intent_decl.name != NULL
            && strcmp(stmt->data.intent_decl.name, name) == 0;
    case AST_TYPE_ALIAS:
        return stmt->data.type_alias.name != NULL
            && strcmp(stmt->data.type_alias.name, name) == 0;
    case AST_ABILITY_DECL:
        return stmt->data.ability_decl.name != NULL
            && strcmp(stmt->data.ability_decl.name, name) == 0;
    case AST_EVENT_DECL:
        return stmt->data.event_decl.name != NULL
            && strcmp(stmt->data.event_decl.name, name) == 0;
    case AST_ZONE_DECL:
        return stmt->data.zone_decl.name != NULL
            && strcmp(stmt->data.zone_decl.name, name) == 0;
    case AST_WORLD_DECL:
        return stmt->data.world_decl.name != NULL
            && strcmp(stmt->data.world_decl.name, name) == 0;
    case AST_RELATION_DECL:
        return stmt->data.relation_decl.name != NULL
            && strcmp(stmt->data.relation_decl.name, name) == 0;
    case AST_EFFECT_DECL:
        return stmt->data.effect_decl.name != NULL
            && strcmp(stmt->data.effect_decl.name, name) == 0;
    default:
        return false;
    }
}

static ASTNode *
transpiler_find_named_decl_local(TranspilerCtx *ctx, ASTNodeType decl_type,
                                 const char *name)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode **decls = NULL;
    size_t decl_count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;
    if (ctx->mir != NULL) {
        decl_header = mir_find_decl_header(ctx->mir, name);
        if (decl_header != NULL && decl_header->ast_type == decl_type)
            return decl_header->ast;
    }
    transpiler_active_inventory(ctx, decl_type, &decls, &decl_count);
    if (decls == NULL)
        return NULL;

    for (size_t i = 0; i < decl_count; i++) {
        ASTNode *stmt = decls[i];
        if (transpiler_named_decl_matches(stmt, decl_type, name))
            return stmt;
    }
    return NULL;
}

static ASTNode *
find_party_decl(TranspilerCtx *ctx, const char *party_name)
{
    return transpiler_find_named_decl_local(ctx, AST_PARTY_DECL, party_name);
}

static ASTNode *
find_roster_decl(TranspilerCtx *ctx, const char *roster_name);

static ASTNode *
find_zone_domain_slot(ASTNode *zone_decl, const char *slot_name);

static ASTNode *
find_world_decl(TranspilerCtx *ctx, const char *world_name);

static ASTNode *
find_relation_decl(TranspilerCtx *ctx, const char *relation_name);

static ASTNode *
find_effect_decl(TranspilerCtx *ctx, const char *effect_name);

static ASTNode *
find_nominal_host_method_decl(TranspilerCtx *ctx, const char *host_type_name,
                              const char *method_name);

static ASTNode *
find_enum_decl(TranspilerCtx *ctx, const char *enum_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ENUM_DECL, enum_name);
}

static ASTNode *
find_class_decl(TranspilerCtx *ctx, const char *class_name)
{
    return transpiler_find_named_decl_local(ctx, AST_CLASS_DECL, class_name);
}

static ASTNode *
find_type_alias_decl(TranspilerCtx *ctx, const char *alias_name)
{
    return transpiler_find_named_decl_local(ctx, AST_TYPE_ALIAS, alias_name);
}

static ASTNode *
resolve_type_alias_target(TranspilerCtx *ctx, ASTNode *type_node)
{
    unsigned depth = 0;

    while (ctx != NULL && type_node != NULL
           && type_node->type == AST_TYPE
           && type_node->data.type.name != NULL
           && depth < 32) {
        ASTNode *alias_decl = find_type_alias_decl(ctx, type_node->data.type.name);
        if (alias_decl == NULL || alias_decl->data.type_alias.target_type == NULL)
            break;
        type_node = alias_decl->data.type_alias.target_type;
        depth++;
    }

    return type_node;
}

static ASTNode *
find_subject_host_decl(TranspilerCtx *ctx, const char *subject_name)
{
    ASTNode *decl = find_class_decl(ctx, subject_name);
    if (decl != NULL && decl->type == AST_CLASS_DECL
        && decl->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT) {
        return decl;
    }
    return NULL;
}

static ASTNode *
find_zone_decl(TranspilerCtx *ctx, const char *zone_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ZONE_DECL, zone_name);
}

static bool
transpiler_has_known_nominal_type(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;

    if (find_class_decl(ctx, name) != NULL
        || find_zone_decl(ctx, name) != NULL
        || find_party_decl(ctx, name) != NULL
        || find_roster_decl(ctx, name) != NULL
        || find_world_decl(ctx, name) != NULL
        || find_relation_decl(ctx, name) != NULL
        || find_effect_decl(ctx, name) != NULL)
        return true;
    return false;
}

static ASTNode *
find_world_decl(TranspilerCtx *ctx, const char *world_name)
{
    return transpiler_find_named_decl_local(ctx, AST_WORLD_DECL, world_name);
}

static ASTNode *
find_relation_decl(TranspilerCtx *ctx, const char *relation_name)
{
    return transpiler_find_named_decl_local(ctx, AST_RELATION_DECL,
                                            relation_name);
}

static ASTNode *
find_effect_decl(TranspilerCtx *ctx, const char *effect_name)
{
    return transpiler_find_named_decl_local(ctx, AST_EFFECT_DECL, effect_name);
}

static size_t
projection_source_field_count_local(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    return 0;
}

static ClassField *
projection_source_field_at_local(ASTNode *decl, size_t index)
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
resolve_projection_source_path_rec(TranspilerCtx *ctx, ASTNode *source_decl,
                                   const char *field_name, unsigned depth,
                                   char **path_out)
{
    size_t field_count;
    int match_count = 0;
    char *resolved_path = NULL;

    if (path_out != NULL)
        *path_out = NULL;
    if (ctx == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = projection_source_field_count_local(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at_local(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = pergyra_strdup(field_name);
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at_local(source_decl, i);
        ASTNode *vessel_decl;
        char *nested_path = NULL;
        char *prefixed_path;
        int nested_status;

        if (field == NULL
            || field->type == NULL || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = find_class_decl(ctx, field->type->data.type.name);
        if (vessel_decl == NULL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = resolve_projection_source_path_rec(
            ctx, vessel_decl, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_path != NULL)
                free(nested_path);
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = strdup_fmt("%s.%s", field->name, nested_path);
        free(nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
        } else {
            free(prefixed_path);
            free(resolved_path);
            resolved_path = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        else
            free(resolved_path);
        return 1;
    }

    if (resolved_path != NULL)
        free(resolved_path);
    return match_count > 1 ? 2 : 0;
}

static bool
is_subject_type_name(TranspilerCtx *ctx, const char *type_name)
{
    ASTNode *decl = find_class_decl(ctx, type_name);
    if (decl != NULL && !decl->data.class_decl.is_struct)
        return decl->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
    /* Also check monomorphized generic class specializations */
    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, type_name) == 0) {
            const ASTNode *orig = ctx->generic_class_specs[i].class_decl;
            return orig != NULL && !orig->data.class_decl.is_struct
                && orig->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
        }
    }
    return false;
}

static bool
is_nominal_host_type_name(TranspilerCtx *ctx, const char *type_name)
{
    if (find_enum_decl(ctx, type_name) != NULL)
        return true;
    ASTNode *decl = find_class_decl(ctx, type_name);
    if (decl != NULL && (!decl->data.class_decl.is_struct
        || decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL
        || decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT))
        return true;
    if (find_relation_decl(ctx, type_name) != NULL
        || find_effect_decl(ctx, type_name) != NULL
        || find_zone_decl(ctx, type_name) != NULL
        || find_world_decl(ctx, type_name) != NULL)
        return true;
    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, type_name) == 0) {
            const ASTNode *orig = ctx->generic_class_specs[i].class_decl;
            return orig != NULL && !orig->data.class_decl.is_struct;
        }
    }
    return false;
}

static bool
is_pointer_self_host_type_name(TranspilerCtx *ctx, const char *type_name)
{
    ASTNode *decl;

    if (type_name == NULL)
        return false;
    if (is_subject_type_name(ctx, type_name))
        return true;
    decl = find_class_decl(ctx, type_name);
    if (decl != NULL
        && decl->type == AST_CLASS_DECL
        && decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL)
        return true;
    return find_relation_decl(ctx, type_name) != NULL
        || find_effect_decl(ctx, type_name) != NULL
        || find_zone_decl(ctx, type_name) != NULL
        || find_world_decl(ctx, type_name) != NULL;
}

static bool
current_class_uses_self_cell(TranspilerCtx *ctx)
{
    ASTNode *host_decl = NULL;
    const char *class_name = NULL;

    if (ctx == NULL)
        return false;
    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl != NULL && host_decl->type == AST_CLASS_DECL)
        class_name = host_decl->data.class_decl.name;
    return ctx != NULL
        && class_name != NULL
        && is_pointer_self_host_type_name(ctx, class_name);
}

static bool
current_class_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_CLASS_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
        ClassField *field = decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
current_zone_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_ZONE_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = decl->data.zone_decl.slots[i];
        if (slot != NULL && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = decl->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
        ASTNode *shared = decl->data.zone_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
current_relation_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_RELATION_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
        ASTNode *slot = decl->data.relation_decl.slots[i];
        if (slot != NULL && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.relation_decl.shared_count; i++) {
        ASTNode *shared = decl->data.relation_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
current_effect_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_EFFECT_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
        ASTNode *slot = decl->data.effect_decl.slots[i];
        if (slot != NULL && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.effect_decl.shared_count; i++) {
        ASTNode *shared = decl->data.effect_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
domain_slot_is_projection_target_local(ASTNode *slot,
                                       ASTNode **refreshes,
                                       size_t refresh_count);

static bool
domain_slot_is_projection_target_local(ASTNode *slot,
                                       ASTNode **refreshes,
                                       size_t refresh_count)
{
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
        || slot->data.domain_slot.slot_name == NULL) {
        return false;
    }

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH
            || refresh->data.zone_refresh.object_slot_name == NULL) {
            continue;
        }
        if (strcmp(slot->data.domain_slot.slot_name,
                   refresh->data.zone_refresh.object_slot_name) == 0) {
            return true;
        }
    }

    return false;
}

static ASTNode *
find_nth_bindable_domain_slot_local(ASTNode **slots, size_t slot_count,
                                    ASTNode **refreshes, size_t refresh_count,
                                    size_t nth)
{
    size_t seen = 0;
    (void)refreshes;
    (void)refresh_count;

    if (slots == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_binding) {
            continue;
        }
        if (seen == nth)
            return slot;
        seen++;
    }

    return NULL;
}

static void
emit_zone_bind_effect_layer(CodeBuf *out, ASTNode *zone, const char *layer_slot_name,
                            const char *target_slot_name, TranspilerCtx *ctx)
{
    ASTNode *layer_slot;
    ASTNode *effect_decl;
    ASTNode *target_slot;

    if (out == NULL || zone == NULL || layer_slot_name == NULL
        || target_slot_name == NULL || ctx == NULL) {
        return;
    }

    layer_slot = NULL;
    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && !slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, layer_slot_name) == 0) {
            layer_slot = slot;
            break;
        }
    }
    if (layer_slot == NULL)
        return;

    effect_decl = find_effect_decl(ctx, layer_slot->data.zone_layer_slot.layer_type);
    if (effect_decl == NULL)
        return;

    target_slot = find_nth_bindable_domain_slot_local(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count,
        effect_decl->data.effect_decl.refreshes,
        effect_decl->data.effect_decl.refresh_count, 0);
    if (target_slot == NULL || target_slot->data.domain_slot.slot_name == NULL)
        return;

    if (layer_slot->data.zone_layer_slot.is_pool) {
        write_indent(ctx);
        codebuf_write(out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(out, "%s _pgy_%s_instance = (%s){0};\n",
            effect_decl->data.effect_decl.name,
            layer_slot_name,
            effect_decl->data.effect_decl.name);
        write_indent(ctx);
        codebuf_write(out, "_pgy_%s_instance.%s = self->%s;\n",
            layer_slot_name,
            target_slot->data.domain_slot.slot_name,
            target_slot_name);
        for (size_t i = 0; i < effect_decl->data.effect_decl.refresh_count; i++) {
            ASTNode *refresh = effect_decl->data.effect_decl.refreshes[i];
            const char *projection_name;
            const char *source_name;

            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            projection_name = refresh->data.zone_refresh.object_slot_name;
            source_name = refresh->data.zone_refresh.source_slot_name;
            if (projection_name == NULL || source_name == NULL
                || strcmp(source_name, target_slot->data.domain_slot.slot_name) != 0) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(out,
                "_pgy_%s_instance.__projection_dirty_%s = true;\n",
                layer_slot_name, projection_name);
            write_indent(ctx);
            codebuf_write(out,
                "_pgy_%s_instance.__projection_ready_%s = false;\n",
                layer_slot_name, projection_name);
        }
        write_indent(ctx);
        codebuf_write(out, "%s_sync(&_pgy_%s_instance);\n",
            effect_decl->data.effect_decl.name,
            layer_slot_name);
        write_indent(ctx);
        codebuf_write(out, "PGY_EFFECT_POOL_APPLY(self->%s, _pgy_%s_instance);\n",
            layer_slot_name,
            layer_slot_name);
        write_indent(ctx);
        codebuf_write(out, "self->__layer_active_%s = PGY_EFFECT_POOL_ACTIVE_COUNT(self->%s) > 0;\n",
            layer_slot_name,
            layer_slot_name);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(out, "}\n");
        return;
    }

    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name,
        target_slot->data.domain_slot.slot_name,
        target_slot_name);
    for (size_t i = 0; i < effect_decl->data.effect_decl.refresh_count; i++) {
        ASTNode *refresh = effect_decl->data.effect_decl.refreshes[i];
        const char *projection_name;
        const char *source_name;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        projection_name = refresh->data.zone_refresh.object_slot_name;
        source_name = refresh->data.zone_refresh.source_slot_name;
        if (projection_name == NULL || source_name == NULL
            || strcmp(source_name, target_slot->data.domain_slot.slot_name) != 0) {
            continue;
        }
        write_indent(ctx);
        codebuf_write(out,
            "self->%s.__projection_dirty_%s = true;\n",
            layer_slot_name, projection_name);
        write_indent(ctx);
        codebuf_write(out,
            "self->%s.__projection_ready_%s = false;\n",
            layer_slot_name, projection_name);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(&self->%s);\n",
        effect_decl->data.effect_decl.name,
        layer_slot_name);
}

static void
emit_zone_bind_relation_layer(CodeBuf *out, ASTNode *zone, const char *layer_slot_name,
                              const char *left_slot_name, const char *right_slot_name,
                              TranspilerCtx *ctx)
{
    ASTNode *layer_slot;
    ASTNode *relation_decl;
    ASTNode *left_target;
    ASTNode *right_target;

    if (out == NULL || zone == NULL || layer_slot_name == NULL
        || left_slot_name == NULL || right_slot_name == NULL || ctx == NULL) {
        return;
    }

    layer_slot = NULL;
    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, layer_slot_name) == 0) {
            layer_slot = slot;
            break;
        }
    }
    if (layer_slot == NULL)
        return;

    relation_decl = find_relation_decl(ctx, layer_slot->data.zone_layer_slot.layer_type);
    if (relation_decl == NULL)
        return;

    left_target = find_nth_bindable_domain_slot_local(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 0);
    right_target = find_nth_bindable_domain_slot_local(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 1);
    if (left_target == NULL || right_target == NULL
        || left_target->data.domain_slot.slot_name == NULL
        || right_target->data.domain_slot.slot_name == NULL) {
        return;
    }

    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name, left_target->data.domain_slot.slot_name, left_slot_name);
    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name, right_target->data.domain_slot.slot_name, right_slot_name);
    for (size_t i = 0; i < relation_decl->data.relation_decl.refresh_count; i++) {
        ASTNode *refresh = relation_decl->data.relation_decl.refreshes[i];
        const char *projection_name;
        const char *source_name;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        projection_name = refresh->data.zone_refresh.object_slot_name;
        source_name = refresh->data.zone_refresh.source_slot_name;
        if (projection_name == NULL || source_name == NULL)
            continue;
        if (strcmp(source_name, left_target->data.domain_slot.slot_name) != 0
            && strcmp(source_name, right_target->data.domain_slot.slot_name) != 0) {
            continue;
        }
        write_indent(ctx);
        codebuf_write(out,
            "self->%s.__projection_dirty_%s = true;\n",
            layer_slot_name, projection_name);
        write_indent(ctx);
        codebuf_write(out,
            "self->%s.__projection_ready_%s = false;\n",
            layer_slot_name, projection_name);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(&self->%s);\n",
        relation_decl->data.relation_decl.name,
        layer_slot_name);
}

static ASTNode **
current_overlay_refresh_list(TranspilerCtx *ctx, size_t *refresh_count_out)
{
    ASTNode *decl;

    if (refresh_count_out != NULL)
        *refresh_count_out = 0;
    if (ctx == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type == AST_RELATION_DECL) {
        if (refresh_count_out != NULL)
            *refresh_count_out = decl->data.relation_decl.refresh_count;
        return decl->data.relation_decl.refreshes;
    }
    if (decl != NULL && decl->type == AST_EFFECT_DECL) {
        if (refresh_count_out != NULL)
            *refresh_count_out = decl->data.effect_decl.refresh_count;
        return decl->data.effect_decl.refreshes;
    }
    if (decl != NULL && decl->type == AST_ZONE_DECL) {
        if (refresh_count_out != NULL)
            *refresh_count_out = decl->data.zone_decl.refresh_count;
        return decl->data.zone_decl.refreshes;
    }

    return NULL;
}

static ASTNode *
find_roster_decl(TranspilerCtx *ctx, const char *roster_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ROSTER_DECL, roster_name);
}

static const char *
assignment_target_root_slot_name(ASTNode *target)
{
    if (target == NULL)
        return NULL;
    if (target->type == AST_IDENTIFIER)
        return target->data.identifier.name;
    if (target->type == AST_MEMBER_ACCESS) {
        if (target->data.member.object != NULL
            && target->data.member.object->type == AST_IDENTIFIER
            && target->data.member.object->data.identifier.name != NULL
            && strcmp(target->data.member.object->data.identifier.name, "self") == 0) {
            return target->data.member.name;
        }
        return assignment_target_root_slot_name(target->data.member.object);
    }
    return NULL;
}

static const char *
assignment_target_root_subfield_name(ASTNode *target)
{
    if (target == NULL || target->type != AST_MEMBER_ACCESS)
        return NULL;

    if (target->data.member.object != NULL
        && target->data.member.object->type == AST_MEMBER_ACCESS) {
        ASTNode *inner = target->data.member.object;
        if (inner->data.member.object != NULL
            && inner->data.member.object->type == AST_IDENTIFIER
            && inner->data.member.object->data.identifier.name != NULL
            && strcmp(inner->data.member.object->data.identifier.name, "self") == 0) {
            return target->data.member.name;
        }
        return assignment_target_root_subfield_name(inner);
    }

    return NULL;
}

static ASTNode *
current_overlay_domain_slot_decl(TranspilerCtx *ctx, const char *slot_name)
{
    ASTNode *decl;

    if (ctx == NULL || slot_name == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type == AST_RELATION_DECL) {
        for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
            ASTNode *slot = decl->data.relation_decl.slots[i];
            if (slot != NULL && slot->type == AST_DOMAIN_SLOT
                && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
                return slot;
            }
        }
    }
    if (decl != NULL && decl->type == AST_EFFECT_DECL) {
        for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
            ASTNode *slot = decl->data.effect_decl.slots[i];
            if (slot != NULL && slot->type == AST_DOMAIN_SLOT
                && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
                return slot;
            }
        }
    }
    if (decl != NULL && decl->type == AST_ZONE_DECL)
        return find_zone_domain_slot(decl, slot_name);

    return NULL;
}

static bool
projection_target_mentions_source_field(TranspilerCtx *ctx,
                                        ASTNode *refresh,
                                        const char *target_slot_name,
                                        const char *source_field_name)
{
    ASTNode *slot_decl;
    ASTNode *target_decl;
    const char *target_type_name;

    if (ctx == NULL || target_slot_name == NULL)
        return false;
    if (source_field_name == NULL)
        return true;

    slot_decl = current_overlay_domain_slot_decl(ctx, target_slot_name);
    if (slot_decl == NULL
        || slot_decl->data.domain_slot.type == NULL
        || slot_decl->data.domain_slot.type->type != AST_TYPE
        || slot_decl->data.domain_slot.type->data.type.name == NULL) {
        return true;
    }

    target_type_name = slot_decl->data.domain_slot.type->data.type.name;
    target_decl = find_class_decl(ctx, target_type_name);
    if (target_decl == NULL || target_decl->type != AST_CLASS_DECL)
        return true;

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *field = target_decl->data.class_decl.fields[i];
        const char *mapped_source_name = field != NULL ? field->name : NULL;
        if (refresh != NULL && refresh->type == AST_ZONE_REFRESH && field != NULL
            && field->name != NULL) {
            for (size_t j = 0; j < refresh->data.zone_refresh.field_map_count; j++) {
                const char *mapped_target =
                    refresh->data.zone_refresh.mapped_target_fields[j];
                const char *mapped_source =
                    refresh->data.zone_refresh.mapped_source_fields[j];
                if (mapped_target != NULL && mapped_source != NULL
                    && strcmp(mapped_target, field->name) == 0) {
                    mapped_source_name = mapped_source;
                    break;
                }
            }
        }
        if (mapped_source_name != NULL
            && strcmp(mapped_source_name, source_field_name) == 0) {
            return true;
        }
    }

    return false;
}

static char *
emit_current_overlay_projection_invalidation(TranspilerCtx *ctx,
                                            const char *source_slot_name,
                                            const char *source_field_name)
{
    ASTNode **refreshes;
    size_t refresh_count = 0;
    CodeBuf *buf;
    const char *receiver_expr;

    if (ctx == NULL || source_slot_name == NULL)
        return NULL;

    receiver_expr = ctx->current_overlay_receiver_expr != NULL
        ? ctx->current_overlay_receiver_expr
        : "self";

    refreshes = current_overlay_refresh_list(ctx, &refresh_count);
    if (refreshes == NULL || refresh_count == 0)
        return NULL;

    buf = codebuf_create();
    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        const char *target_name;
        const char *refresh_source_name;

        if (refresh == NULL)
            continue;
        target_name = refresh->data.zone_refresh.object_slot_name;
        refresh_source_name = refresh->data.zone_refresh.source_slot_name;
        if (target_name == NULL || refresh_source_name == NULL
            || strcmp(refresh_source_name, source_slot_name) != 0) {
            continue;
        }
        if (!projection_target_mentions_source_field(ctx, refresh,
                target_name, source_field_name))
            continue;
        codebuf_write(buf,
            "%s->__projection_dirty_%s = true; "
            "%s->__projection_ready_%s = false; ",
            receiver_expr,
            target_name,
            receiver_expr,
            target_name);
    }

    if (buf->len == 0) {
        codebuf_destroy(buf);
        return NULL;
    }

    {
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }
}

static bool
resolve_world_embedded_projection_invalidation(TranspilerCtx *ctx,
                                               ASTNode *target,
                                               const char **zone_slot_name_out,
                                               const char **zone_type_name_out,
                                               const char **source_slot_name_out,
                                               const char **source_field_name_out)
{
    ASTNode *cursor = target;
    const char *source_field_name = NULL;

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = NULL;
    if (zone_type_name_out != NULL)
        *zone_type_name_out = NULL;
    if (source_slot_name_out != NULL)
        *source_slot_name_out = NULL;
    if (source_field_name_out != NULL)
        *source_field_name_out = NULL;

    if (ctx == NULL || target == NULL)
        return false;

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        const char *zone_slot_name = NULL;
        const char *zone_type_name = NULL;
        const char *source_slot_name = NULL;
        const char *source_type_name = NULL;

        if (resolve_world_zone_subject_receiver(ctx, cursor,
                &zone_slot_name, &zone_type_name,
                &source_slot_name, &source_type_name)
            && zone_slot_name != NULL
            && zone_type_name != NULL
            && source_slot_name != NULL) {
            if (zone_slot_name_out != NULL)
                *zone_slot_name_out = zone_slot_name;
            if (zone_type_name_out != NULL)
                *zone_type_name_out = zone_type_name;
            if (source_slot_name_out != NULL)
                *source_slot_name_out = source_slot_name;
            if (source_field_name_out != NULL)
                *source_field_name_out = source_field_name;
            return true;
        }

        source_field_name = cursor->data.member.name;
        cursor = cursor->data.member.object;
    }

    return false;
}

static char *
emit_assignment_projection_invalidation(TranspilerCtx *ctx, ASTNode *target)
{
    const char *root_slot_name;
    const char *root_subfield_name;
    char *invalidation;

    if (ctx == NULL || target == NULL)
        return NULL;

    root_slot_name = assignment_target_root_slot_name(target);
    root_subfield_name = assignment_target_root_subfield_name(target);
    invalidation = emit_current_overlay_projection_invalidation(
        ctx, root_slot_name, root_subfield_name);
    if (invalidation != NULL)
        return invalidation;

    {
        ASTNode *saved_host_decl = transpiler_current_host_decl_local(ctx);
        const char *saved_receiver_expr = ctx->current_overlay_receiver_expr;

        if (saved_host_decl != NULL && saved_host_decl->type == AST_WORLD_DECL) {
            const char *zone_slot_name = NULL;
            const char *zone_type_name = NULL;
            const char *source_slot_name = NULL;
            const char *source_field_name = NULL;

            if (resolve_world_embedded_projection_invalidation(ctx, target,
                    &zone_slot_name, &zone_type_name,
                    &source_slot_name, &source_field_name)
                && zone_slot_name != NULL
                && zone_type_name != NULL
                && source_slot_name != NULL) {
                ASTNode *zone_decl = find_zone_decl(ctx, zone_type_name);
                if (zone_decl != NULL)
                    transpiler_bind_current_host_decl_local(ctx, zone_decl);
                ctx->current_overlay_receiver_expr =
                    strdup_fmt("(&self->%s)", zone_slot_name);
                invalidation = emit_current_overlay_projection_invalidation(
                    ctx, source_slot_name, source_field_name);
                if (ctx->current_overlay_receiver_expr != NULL
                    && ctx->current_overlay_receiver_expr != saved_receiver_expr) {
                    free((char *)ctx->current_overlay_receiver_expr);
                }
                ctx->current_overlay_receiver_expr = saved_receiver_expr;
                transpiler_bind_current_host_decl_local(ctx, saved_host_decl);
                return invalidation;
            }
        }
    }

    return NULL;
}

static char *
emit_world_embedded_assignment_sync(TranspilerCtx *ctx, ASTNode *target)
{
    ASTNode *host_decl;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *source_slot_name = NULL;
    const char *source_field_name = NULL;

    if (ctx == NULL || target == NULL)
        return NULL;

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return NULL;

    if (!resolve_world_embedded_projection_invalidation(ctx, target,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_field_name)
        || zone_slot_name == NULL
        || zone_type_name == NULL
        || source_slot_name == NULL) {
        return NULL;
    }

    return strdup_fmt("%s_sync(&self->%s); ", zone_type_name, zone_slot_name);
}

static char *
emit_world_embedded_receiver_projection_sync(TranspilerCtx *ctx, ASTNode *receiver)
{
    ASTNode *host_decl;
    ASTNode *zone_decl;
    CodeBuf *buf;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *source_slot_name = NULL;
    const char *source_type_name = NULL;

    if (ctx == NULL || receiver == NULL)
        return NULL;

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return NULL;

    if (!resolve_world_zone_subject_receiver(ctx, receiver,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_type_name)
        || zone_slot_name == NULL
        || zone_type_name == NULL) {
        return NULL;
    }

    zone_decl = find_zone_decl(ctx, zone_type_name);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return NULL;

    buf = codebuf_create();
    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.slot_name == NULL
            || slot->data.domain_slot.is_subject) {
            continue;
        }
        codebuf_write(buf,
            "self->%s.__projection_dirty_%s = true; "
            "self->%s.__projection_ready_%s = false; ",
            zone_slot_name, slot->data.domain_slot.slot_name,
            zone_slot_name, slot->data.domain_slot.slot_name);
    }
    codebuf_write(buf,
        "%s_sync(&self->%s); "
        "self->__zone_dirty_%s = true; "
        "self->__world_derived_dirty = true; "
        "%s_sync(self); ",
        zone_type_name, zone_slot_name,
        zone_slot_name,
        host_decl->data.world_decl.name != NULL
            ? host_decl->data.world_decl.name : "World");

    {
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }
}

static bool
host_projection_relevant_field_exists(TranspilerCtx *ctx,
                                      const char *host_type_name,
                                      const char *field_name)
{
    ASTNode *host_decl;

    if (ctx == NULL || host_type_name == NULL || field_name == NULL)
        return false;

    host_decl = find_class_decl(ctx, host_type_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return false;

    for (size_t i = 0; i < host_decl->data.class_decl.field_count; i++) {
        ClassField *field = host_decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return !field->is_vessel_field;
        }
    }

    return false;
}

static ClassField *
find_host_field_by_name_local(ASTNode *host_decl, const char *field_name)
{
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL
        || field_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < host_decl->data.class_decl.field_count; i++) {
        ClassField *field = host_decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return field;
        }
    }

    return NULL;
}

static const char *
method_assignment_projection_field_name(TranspilerCtx *ctx,
                                        const char *host_type_name,
                                        ASTNode *target)
{
    ASTNode *cursor = target;
    ASTNode *host_decl;

    if (ctx == NULL || host_type_name == NULL || target == NULL)
        return NULL;

    host_decl = find_class_decl(ctx, host_type_name);
    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL)
        return NULL;

    if (target->type == AST_IDENTIFIER
        && host_projection_relevant_field_exists(ctx, host_type_name,
            target->data.identifier.name)) {
        return target->data.identifier.name;
    }

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        ASTNode *obj = cursor->data.member.object;
        if (obj != NULL && obj->type == AST_IDENTIFIER
            && obj->data.identifier.name != NULL) {
            ClassField *field = find_host_field_by_name_local(
                host_decl, obj->data.identifier.name);
            if (field != NULL) {
                if (field->is_vessel_field)
                    return cursor->data.member.name;
                return obj->data.identifier.name;
            }
        }
        cursor = obj;
    }

    return NULL;
}

static void
append_overlay_method_projection_invalidations(CodeBuf *buf,
                                               TranspilerCtx *ctx,
                                               const char *source_slot_name,
                                               const char *host_type_name,
                                               ASTNode *node,
                                               int depth)
{
    if (buf == NULL || ctx == NULL || source_slot_name == NULL
        || host_type_name == NULL || node == NULL || depth > 8) {
        return;
    }

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.block.statements[i], depth + 1);
        }
        break;
    case AST_IF_STMT:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.if_stmt.then_branch, depth + 1);
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.if_stmt.else_branch, depth + 1);
        break;
    case AST_FOR_LOOP:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.for_loop.body, depth + 1);
        break;
    case AST_WHILE_LOOP:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.while_loop.body, depth + 1);
        break;
    case AST_MATCH_STMT:
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.match_stmt.cases[i], depth + 1);
        }
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.match_stmt.default_body, depth + 1);
        break;
    case AST_MATCH_CASE:
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.match_case.body, depth + 1);
        break;
    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.select_stmt.cases[i], depth + 1);
        }
        append_overlay_method_projection_invalidations(
            buf, ctx, source_slot_name, host_type_name,
            node->data.select_stmt.default_case, depth + 1);
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
            append_overlay_method_projection_invalidations(
                buf, ctx, source_slot_name, host_type_name,
                node->data.async_block.statements[i], depth + 1);
        }
        break;
    case AST_ASSIGNMENT: {
        const char *field_name = method_assignment_projection_field_name(
            ctx, host_type_name, node->data.assignment.target);
        if (field_name != NULL) {
            char *invalidation = emit_current_overlay_projection_invalidation(
                ctx, source_slot_name, field_name);
            if (invalidation != NULL) {
                codebuf_write(buf, "%s", invalidation);
                free(invalidation);
            }
        }
        break;
    }
    case AST_CALL:
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_MEMBER_ACCESS
            && node->data.call.callee->data.member.object != NULL
            && node->data.call.callee->data.member.object->type == AST_IDENTIFIER
            && node->data.call.callee->data.member.object->data.identifier.name != NULL
            && node->data.call.callee->data.member.name != NULL) {
            ASTNode *host_decl = find_class_decl(ctx, host_type_name);
            ClassField *field = NULL;

            if (host_decl != NULL && host_decl->type == AST_CLASS_DECL) {
                field = find_host_field_by_name_local(host_decl,
                    node->data.call.callee->data.member.object->data.identifier.name);
            }
            if (field != NULL && field->is_vessel_field
                && field->type != NULL
                && field->type->type == AST_TYPE
                && field->type->data.type.name != NULL) {
                ASTNode *method_decl = find_nominal_host_method_decl(
                    ctx, field->type->data.type.name,
                    node->data.call.callee->data.member.name);
                if (method_decl != NULL) {
                    append_overlay_method_projection_invalidations(
                        buf, ctx, source_slot_name,
                        field->type->data.type.name,
                        method_decl->data.func_decl.body, depth + 1);
                }
            }
        }
        break;
    default:
        break;
    }
}

static char *
emit_current_overlay_method_projection_invalidation(TranspilerCtx *ctx,
                                                    const char *source_slot_name,
                                                    const char *host_type_name,
                                                    ASTNode *method_decl)
{
    CodeBuf *buf;

    if (ctx == NULL || source_slot_name == NULL || host_type_name == NULL
        || method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->data.func_decl.body == NULL) {
        return NULL;
    }

    buf = codebuf_create();
    append_overlay_method_projection_invalidations(
        buf, ctx, source_slot_name, host_type_name,
        method_decl->data.func_decl.body, 0);

    if (buf->len == 0) {
        codebuf_destroy(buf);
        return NULL;
    }

    {
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }
}

static bool
current_world_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_WORLD_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
        ASTNode *slot = decl->data.world_decl.rosters[i];
        if (slot != NULL && slot->data.world_roster.slot_name != NULL
            && strcmp(slot->data.world_roster.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
        ASTNode *slot = decl->data.world_decl.zones[i];
        if (slot != NULL && slot->data.world_zone.slot_name != NULL
            && strcmp(slot->data.world_zone.slot_name, field_name) == 0) {
            return true;
        }
    }
    for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
        ASTNode *shared = decl->data.world_decl.shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL
            && strcmp(shared->data.party_shared.name, field_name) == 0) {
            return true;
        }
    }

    return false;
}

static ASTNode *
find_zone_state_decl(ASTNode *zone_decl, const char *state_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.state_count; i++) {
        ASTNode *state = zone_decl->data.zone_decl.states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_domain_slot(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_layer_slot(ASTNode *zone_decl, const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
resolve_world_zone_decl(TranspilerCtx *ctx, ASTNode *world_decl, const char *slot_name);

static const char *
zone_subject_slot_type_name(ASTNode *zone_decl, const char *slot_name)
{
    ASTNode *slot = find_zone_domain_slot(zone_decl, slot_name);
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
        || !slot->data.domain_slot.is_subject
        || slot->data.domain_slot.type == NULL
        || slot->data.domain_slot.type->type != AST_TYPE) {
        return NULL;
    }
    return slot->data.domain_slot.type->data.type.name;
}

static ASTNode *
find_subject_host_method_decl(TranspilerCtx *ctx, const char *type_name,
                              const char *method_name)
{
    ASTNode *decl;

    if (ctx == NULL || type_name == NULL || method_name == NULL)
        return NULL;

    decl = find_class_decl(ctx, type_name);
    if (decl != NULL && decl->type == AST_CLASS_DECL) {
        for (size_t i = 0; i < decl->data.class_decl.method_count; i++) {
            ASTNode *method = decl->data.class_decl.methods[i];
            if (method != NULL && method->type == AST_FUNC_DECL
                && method->data.func_decl.name != NULL
                && strcmp(method->data.func_decl.name, method_name) == 0) {
                return method;
            }
        }
    }

    return NULL;
}

static bool
resolve_zone_subject_receiver(TranspilerCtx *ctx, ASTNode *receiver,
                              const char **slot_name_out,
                              const char **type_name_out)
{
    ASTNode *zone_decl;
    const char *slot_name = NULL;
    const char *type_name = NULL;

    if (slot_name_out != NULL)
        *slot_name_out = NULL;
    if (type_name_out != NULL)
        *type_name_out = NULL;

    if (ctx == NULL || receiver == NULL)
        return false;

    zone_decl = transpiler_current_host_decl_local(ctx);
    if (zone_decl != NULL && zone_decl->type != AST_ZONE_DECL)
        zone_decl = NULL;
    if (zone_decl == NULL)
        return false;

    if (receiver->type == AST_IDENTIFIER && receiver->data.identifier.name != NULL) {
        slot_name = receiver->data.identifier.name;
        type_name = zone_subject_slot_type_name(zone_decl, slot_name);
        if (type_name == NULL) {
            const char *var_type = lookup_typed_var(ctx, slot_name);
            if (var_type != NULL && is_subject_type_name(ctx, var_type))
                type_name = var_type;
        }
    } else if (receiver->type == AST_MEMBER_ACCESS
               && receiver->data.member.object != NULL
               && receiver->data.member.object->type == AST_IDENTIFIER
               && receiver->data.member.object->data.identifier.name != NULL
               && strcmp(receiver->data.member.object->data.identifier.name, "self") == 0
               && receiver->data.member.name != NULL) {
        slot_name = receiver->data.member.name;
        type_name = zone_subject_slot_type_name(zone_decl, slot_name);
    }

    if (slot_name == NULL || type_name == NULL)
        return false;

    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (type_name_out != NULL)
        *type_name_out = type_name;
    return true;
}

static bool
resolve_world_zone_subject_receiver(TranspilerCtx *ctx, ASTNode *receiver,
                                    const char **zone_slot_name_out,
                                    const char **zone_type_name_out,
                                    const char **slot_name_out,
                                    const char **type_name_out)
{
    ASTNode *world_decl;
    ASTNode *zone_decl;
    ASTNode *zone_expr;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *slot_name = NULL;
    const char *type_name = NULL;

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = NULL;
    if (zone_type_name_out != NULL)
        *zone_type_name_out = NULL;
    if (slot_name_out != NULL)
        *slot_name_out = NULL;
    if (type_name_out != NULL)
        *type_name_out = NULL;

    if (ctx == NULL
        || receiver == NULL || receiver->type != AST_MEMBER_ACCESS) {
        return false;
    }

    zone_expr = receiver->data.member.object;
    slot_name = receiver->data.member.name;
    if (zone_expr == NULL || slot_name == NULL)
        return false;

    if (zone_expr->type == AST_IDENTIFIER && zone_expr->data.identifier.name != NULL) {
        zone_slot_name = zone_expr->data.identifier.name;
    } else if (zone_expr->type == AST_MEMBER_ACCESS
               && zone_expr->data.member.object != NULL
               && zone_expr->data.member.object->type == AST_IDENTIFIER
               && zone_expr->data.member.object->data.identifier.name != NULL
               && strcmp(zone_expr->data.member.object->data.identifier.name, "self") == 0
               && zone_expr->data.member.name != NULL) {
        zone_slot_name = zone_expr->data.member.name;
    } else {
        return false;
    }

    world_decl = transpiler_current_host_decl_local(ctx);
    if (world_decl != NULL && world_decl->type != AST_WORLD_DECL)
        world_decl = NULL;
    if (world_decl == NULL)
        return false;

    zone_decl = resolve_world_zone_decl(ctx, world_decl, zone_slot_name);
    if (zone_decl == NULL)
        return false;

    zone_type_name = zone_decl->data.zone_decl.name;
    type_name = zone_subject_slot_type_name(zone_decl, slot_name);
    if (zone_type_name == NULL || type_name == NULL)
        return false;

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = zone_slot_name;
    if (zone_type_name_out != NULL)
        *zone_type_name_out = zone_type_name;
    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (type_name_out != NULL)
        *type_name_out = type_name;
    return true;
}

static void
emit_zone_action_effect_runtime(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *callee;
    ASTNode *receiver;
    ASTNode *host_decl;
    ASTNode *zone_decl;
    ASTNode *method_decl;
    const char *method_name;
    const char *receiver_slot_name = NULL;
    const char *receiver_type_name = NULL;
    const char *effect_name;
    const char *active_zone_name = NULL;

    if (ctx == NULL || call == NULL
        || call->type != AST_CALL) {
        return;
    }

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_ZONE_DECL)
        return;
    active_zone_name = host_decl->data.zone_decl.name;

    callee = call->data.call.callee;
    if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
        return;

    receiver = callee->data.member.object;
    method_name = callee->data.member.name;
    if (receiver == NULL || method_name == NULL)
        return;

    if (!resolve_zone_subject_receiver(ctx, receiver,
            &receiver_slot_name, &receiver_type_name)) {
        return;
    }

    method_decl = find_subject_host_method_decl(ctx, receiver_type_name, method_name);
    if (method_decl == NULL || method_decl->type != AST_FUNC_DECL
        || method_decl->is_async_decl
        || !method_decl->data.func_decl.is_action
        || method_decl->data.func_decl.within_zone == NULL
        || method_decl->data.func_decl.causes_effect == NULL
        || strcmp(method_decl->data.func_decl.within_zone, active_zone_name) != 0) {
        return;
    }

    zone_decl = host_decl;
    if (zone_decl == NULL)
        return;

    effect_name = method_decl->data.func_decl.causes_effect;
    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = zone_decl->data.zone_decl.layer_slots[i];
        const char *layer_name;

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || layer_slot->data.zone_layer_slot.is_relation
            || layer_slot->data.zone_layer_slot.layer_type == NULL
            || strcmp(layer_slot->data.zone_layer_slot.layer_type, effect_name) != 0) {
            continue;
        }

        layer_name = layer_slot->data.zone_layer_slot.slot_name;
        if (layer_name == NULL)
            continue;

        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n", layer_name);
        emit_zone_bind_effect_layer(ctx->out, zone_decl, layer_name, receiver_slot_name, ctx);
    }
}

static char *
emit_world_embedded_action_effect_sync(TranspilerCtx *ctx,
                                       ASTNode *receiver,
                                       ASTNode *method_decl)
{
    ASTNode *world_decl;
    ASTNode *zone_decl;
    ASTNode *effect_decl;
    CodeBuf *buf;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *source_slot_name = NULL;
    const char *source_type_name = NULL;
    const char *effect_name;

    if (ctx == NULL || receiver == NULL || method_decl == NULL
        || method_decl->type != AST_FUNC_DECL
        || method_decl->is_async_decl
        || !method_decl->data.func_decl.is_action
        || method_decl->data.func_decl.within_zone == NULL
        || method_decl->data.func_decl.causes_effect == NULL) {
        return NULL;
    }

    world_decl = transpiler_current_host_decl_local(ctx);
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL)
        return NULL;

    if (!resolve_world_zone_subject_receiver(ctx, receiver,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_type_name)
        || zone_slot_name == NULL || zone_type_name == NULL
        || source_slot_name == NULL
        || strcmp(method_decl->data.func_decl.within_zone, zone_type_name) != 0) {
        return NULL;
    }

    zone_decl = find_zone_decl(ctx, zone_type_name);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return NULL;

    effect_name = method_decl->data.func_decl.causes_effect;
    effect_decl = find_effect_decl(ctx, effect_name);
    if (effect_decl == NULL || effect_decl->type != AST_EFFECT_DECL)
        return NULL;

    buf = codebuf_create();
    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = zone_decl->data.zone_decl.layer_slots[i];
        ASTNode *target_slot;
        const char *layer_name;
        int tmp_id;

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || layer_slot->data.zone_layer_slot.is_relation
            || layer_slot->data.zone_layer_slot.layer_type == NULL
            || strcmp(layer_slot->data.zone_layer_slot.layer_type, effect_name) != 0) {
            continue;
        }

        layer_name = layer_slot->data.zone_layer_slot.slot_name;
        if (layer_name == NULL)
            continue;

        target_slot = find_nth_bindable_domain_slot_local(
            effect_decl->data.effect_decl.slots,
            effect_decl->data.effect_decl.slot_count,
            effect_decl->data.effect_decl.refreshes,
            effect_decl->data.effect_decl.refresh_count, 0);
        if (target_slot == NULL || target_slot->data.domain_slot.slot_name == NULL)
            continue;

        codebuf_write(buf, "self->%s.__layer_active_%s = true; ",
            zone_slot_name, layer_name);
        codebuf_write(buf,
            "self->%s.__layer_epoch_%s++; "
            "self->%s.__layer_cause_%s = 11; ",
            zone_slot_name, layer_name,
            zone_slot_name, layer_name);
        for (size_t si = 0; si < zone_decl->data.zone_decl.state_count; si++) {
            ASTNode *state = zone_decl->data.zone_decl.states[si];
            if (state == NULL || state->type != AST_ZONE_STATE
                || state->data.zone_state.is_relation
                || state->data.zone_state.state_name == NULL
                || state->data.zone_state.layer_slot_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name, layer_name) != 0) {
                continue;
            }
            codebuf_write(buf,
                "self->%s.__state_epoch_%s++; "
                "self->%s.__state_cause_%s = 11; ",
                zone_slot_name, state->data.zone_state.state_name,
                zone_slot_name, state->data.zone_state.state_name);
        }

        if (layer_slot->data.zone_layer_slot.is_pool) {
            tmp_id = ++ctx->tmp_counter;
            codebuf_write(buf,
                "{ %s _pgy_world_effect_%d = (%s){0}; "
                "_pgy_world_effect_%d.%s = self->%s.%s; ",
                effect_decl->data.effect_decl.name, tmp_id,
                effect_decl->data.effect_decl.name,
                tmp_id, target_slot->data.domain_slot.slot_name,
                zone_slot_name, source_slot_name);
            for (size_t ri = 0; ri < effect_decl->data.effect_decl.refresh_count; ri++) {
                ASTNode *refresh = effect_decl->data.effect_decl.refreshes[ri];
                const char *projection_name;
                const char *refresh_source;
                if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                    continue;
                projection_name = refresh->data.zone_refresh.object_slot_name;
                refresh_source = refresh->data.zone_refresh.source_slot_name;
                if (projection_name == NULL || refresh_source == NULL
                    || strcmp(refresh_source, target_slot->data.domain_slot.slot_name) != 0) {
                    continue;
                }
                codebuf_write(buf,
                    "_pgy_world_effect_%d.__projection_dirty_%s = true; "
                    "_pgy_world_effect_%d.__projection_ready_%s = false; ",
                    tmp_id, projection_name,
                    tmp_id, projection_name);
            }
            codebuf_write(buf,
                "%s_sync(&_pgy_world_effect_%d); "
                "PGY_EFFECT_POOL_APPLY(self->%s.%s, _pgy_world_effect_%d); "
                "self->%s.__layer_active_%s = PGY_EFFECT_POOL_ACTIVE_COUNT(self->%s.%s) > 0; } ",
                effect_decl->data.effect_decl.name, tmp_id,
                zone_slot_name, layer_name, tmp_id,
                zone_slot_name, layer_name,
                zone_slot_name, layer_name);
            continue;
        }

        codebuf_write(buf, "self->%s.%s.%s = self->%s.%s; ",
            zone_slot_name,
            layer_name,
            target_slot->data.domain_slot.slot_name,
            zone_slot_name,
            source_slot_name);
        for (size_t ri = 0; ri < effect_decl->data.effect_decl.refresh_count; ri++) {
            ASTNode *refresh = effect_decl->data.effect_decl.refreshes[ri];
            const char *projection_name;
            const char *refresh_source;
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            projection_name = refresh->data.zone_refresh.object_slot_name;
            refresh_source = refresh->data.zone_refresh.source_slot_name;
            if (projection_name == NULL || refresh_source == NULL
                || strcmp(refresh_source, target_slot->data.domain_slot.slot_name) != 0) {
                continue;
            }
            codebuf_write(buf,
                "self->%s.%s.__projection_dirty_%s = true; "
                "self->%s.%s.__projection_ready_%s = false; ",
                zone_slot_name, layer_name, projection_name,
                zone_slot_name, layer_name, projection_name);
        }
        codebuf_write(buf, "%s_sync(&self->%s.%s); ",
            effect_decl->data.effect_decl.name,
            zone_slot_name,
            layer_name);
    }

    if (buf->len == 0) {
        codebuf_destroy(buf);
        return NULL;
    }

    {
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }
}

static ASTNode *
find_world_state_decl(ASTNode *world_decl, const char *state_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < world_decl->data.world_decl.state_count; i++) {
        ASTNode *state = world_decl->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }
    return NULL;
}

static const char *
