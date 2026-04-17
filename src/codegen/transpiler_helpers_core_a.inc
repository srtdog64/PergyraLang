static const char *infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr);
static char *escape_c_string_literal(const char *src);
static char *render_type_name(ASTNode *type_node);
static char *render_ability_ref_vtable_tag(ASTNode *ability_ref);
static bool transpiler_can_forward_declare_type_early(TranspilerCtx *ctx, ASTNode *type_node);
static bool transpiler_can_forward_declare_func_early(TranspilerCtx *ctx, ASTNode *func);
static bool transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx, ASTNode *func);

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
    va_list ap;
    va_start(ap, fmt);

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
    for (int i = 0; i < ctx->indent; i++)
        codebuf_write(ctx->out, "    ");
}

static void
write_indent_to(CodeBuf *buf, int indent)
{
    for (int i = 0; i < indent; i++)
        codebuf_write(buf, "    ");
}

static TranspilerCtx *g_type_render_ctx = NULL;
static char *strdup_fmt(const char *fmt, ...);

static void
transpiler_set_backend_error(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int needed;
    char *msg;

    if (ctx == NULL || fmt == NULL || ctx->backend_error != NULL)
        return;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    needed = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (needed < 0) {
        va_end(ap);
        return;
    }

    msg = malloc((size_t)needed + 1);
    if (msg == NULL) {
        va_end(ap);
        return;
    }

    vsnprintf(msg, (size_t)needed + 1, fmt, ap);
    va_end(ap);
    ctx->backend_error = msg;
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
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    strncpy(e->inner_type, inner_type, sizeof(e->inner_type) - 1);
    e->inner_type[sizeof(e->inner_type) - 1] = '\0';
    e->is_secure = is_secure;
    e->is_indirect = is_indirect;
}

static const char *
lookup_slot_type(TranspilerCtx *ctx, const char *var_name)
{
    const char *typed_name;

    if (ctx == NULL || var_name == NULL)
        return NULL;

    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0)
            return ctx->slot_vars[i].inner_type;
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
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0)
            return ctx->slot_vars[i].is_secure;
    }
    return false;
}

static bool
lookup_slot_is_indirect(TranspilerCtx *ctx, const char *var_name)
{
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0)
            return ctx->slot_vars[i].is_indirect;
    }
    return false;
}

static char *
slot_ref_expr(TranspilerCtx *ctx, const char *slot_name, const char *slot_expr)
{
    if (slot_name != NULL && lookup_slot_is_indirect(ctx, slot_name))
        return pergyra_strdup(slot_expr);
    return strdup_fmt("&%s", slot_expr);
}

static bool
is_slot_var(TranspilerCtx *ctx, const char *var_name)
{
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0)
            return true;
    }
    return false;
}

static void
register_typed_var(TranspilerCtx *ctx, const char *name, const char *type_name)
{
    if (ctx->typed_var_count >= MAX_SLOT_VARS || name == NULL || type_name == NULL)
        return;
    TypedVarEntry *e = &ctx->typed_vars[ctx->typed_var_count++];
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

    for (int i = ctx->alias_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->alias_vars[i].name, var_name) == 0)
            return ctx->alias_vars[i].target_expr;
    }
    return NULL;
}

static TypedVarEntry *
lookup_typed_entry(TranspilerCtx *ctx, const char *var_name)
{
    for (int i = ctx->typed_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->typed_vars[i].name, var_name) == 0)
            return &ctx->typed_vars[i];
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
find_role_decl(TranspilerCtx *ctx, const char *role_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ROLE_DECL, role_name);
}

static ASTNode *
find_function_decl(TranspilerCtx *ctx, const char *function_name)
{
    return transpiler_find_named_decl_local(ctx, AST_FUNC_DECL, function_name);
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
    const char *class_name = NULL;

    if (ctx == NULL)
        return false;
    if (ctx->current_host_decl != NULL
        && ctx->current_host_decl->type == AST_CLASS_DECL) {
        class_name = ctx->current_host_decl->data.class_decl.name;
    }
    if (class_name == NULL)
        class_name = ctx->current_class_name;
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

    decl = (ctx->current_host_decl != NULL
            && ctx->current_host_decl->type == AST_CLASS_DECL)
        ? ctx->current_host_decl
        : NULL;
    if (decl == NULL && ctx->current_class_name != NULL)
        decl = find_class_decl(ctx, ctx->current_class_name);
    /* For monomorphized generic classes, look up the original class decl */
    if (decl == NULL && ctx->current_class_name != NULL) {
        for (int gi = 0; gi < ctx->generic_class_spec_count; gi++) {
            if (strcmp(ctx->generic_class_specs[gi].specialized_name,
                       ctx->current_class_name) == 0) {
                decl = (ASTNode *)ctx->generic_class_specs[gi].class_decl;
                break;
            }
        }
    }
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

    decl = (transpiler_current_host_decl_local(ctx) != NULL
            && transpiler_current_host_decl_local(ctx)->type == AST_ZONE_DECL)
        ? transpiler_current_host_decl_local(ctx)
        : NULL;
    if (decl == NULL && ctx->current_zone_name != NULL)
        decl = find_zone_decl(ctx, ctx->current_zone_name);
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

    decl = (transpiler_current_host_decl_local(ctx) != NULL
            && transpiler_current_host_decl_local(ctx)->type == AST_RELATION_DECL)
        ? transpiler_current_host_decl_local(ctx)
        : NULL;
    if (decl == NULL && ctx->current_relation_name != NULL)
        decl = find_relation_decl(ctx, ctx->current_relation_name);
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

    decl = (transpiler_current_host_decl_local(ctx) != NULL
            && transpiler_current_host_decl_local(ctx)->type == AST_EFFECT_DECL)
        ? transpiler_current_host_decl_local(ctx)
        : NULL;
    if (decl == NULL && ctx->current_effect_name != NULL)
        decl = find_effect_decl(ctx, ctx->current_effect_name);
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

    if (ctx->current_relation_name != NULL) {
        decl = find_relation_decl(ctx, ctx->current_relation_name);
        if (decl != NULL && decl->type == AST_RELATION_DECL) {
            if (refresh_count_out != NULL)
                *refresh_count_out = decl->data.relation_decl.refresh_count;
            return decl->data.relation_decl.refreshes;
        }
    }

    if (ctx->current_effect_name != NULL) {
        decl = find_effect_decl(ctx, ctx->current_effect_name);
        if (decl != NULL && decl->type == AST_EFFECT_DECL) {
            if (refresh_count_out != NULL)
                *refresh_count_out = decl->data.effect_decl.refresh_count;
            return decl->data.effect_decl.refreshes;
        }
    }

    if (ctx->current_zone_name != NULL) {
        decl = find_zone_decl(ctx, ctx->current_zone_name);
        if (decl != NULL && decl->type == AST_ZONE_DECL) {
            if (refresh_count_out != NULL)
                *refresh_count_out = decl->data.zone_decl.refresh_count;
            return decl->data.zone_decl.refreshes;
        }
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

    if (ctx->current_relation_name != NULL) {
        decl = find_relation_decl(ctx, ctx->current_relation_name);
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
    }

    if (ctx->current_effect_name != NULL) {
        decl = find_effect_decl(ctx, ctx->current_effect_name);
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
    }

    if (ctx->current_zone_name != NULL) {
        decl = find_zone_decl(ctx, ctx->current_zone_name);
        if (decl != NULL && decl->type == AST_ZONE_DECL)
            return find_zone_domain_slot(decl, slot_name);
    }

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

    decl = (transpiler_current_host_decl_local(ctx) != NULL
            && transpiler_current_host_decl_local(ctx)->type == AST_WORLD_DECL)
        ? transpiler_current_host_decl_local(ctx)
        : NULL;
    if (decl == NULL && ctx->current_world_name != NULL)
        decl = find_world_decl(ctx, ctx->current_world_name);
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

    zone_decl = (transpiler_current_host_decl_local(ctx) != NULL
                 && transpiler_current_host_decl_local(ctx)->type == AST_ZONE_DECL)
        ? transpiler_current_host_decl_local(ctx)
        : find_zone_decl(ctx, ctx->current_zone_name);
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

    world_decl = (transpiler_current_host_decl_local(ctx) != NULL
                  && transpiler_current_host_decl_local(ctx)->type == AST_WORLD_DECL)
        ? transpiler_current_host_decl_local(ctx)
        : find_world_decl(ctx, ctx->current_world_name);
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

    if (transpiler_current_host_decl_local(ctx) != NULL
        && transpiler_current_host_decl_local(ctx)->type == AST_ZONE_DECL) {
        active_zone_name = transpiler_current_host_decl_local(ctx)->data.zone_decl.name;
    }
    if (active_zone_name == NULL)
        active_zone_name = ctx->current_zone_name;
    if (active_zone_name == NULL)
        return;

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

    zone_decl = (transpiler_current_host_decl_local(ctx) != NULL
                 && transpiler_current_host_decl_local(ctx)->type == AST_ZONE_DECL)
        ? transpiler_current_host_decl_local(ctx)
        : find_zone_decl(ctx, ctx->current_zone_name);
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
