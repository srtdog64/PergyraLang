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
