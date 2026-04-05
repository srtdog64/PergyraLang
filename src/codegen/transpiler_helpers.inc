static const char *infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr);
static char *escape_c_string_literal(const char *src);
static char *render_type_name(ASTNode *type_node);
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
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, var_name) == 0)
            return ctx->slot_vars[i].inner_type;
    }
    return "Int"; /* fallback */
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

static ASTNode *
find_role_decl(TranspilerCtx *ctx, const char *role_name)
{
    if (ctx == NULL || ctx->hir == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->role_count; i++) {
        ASTNode *stmt = ctx->hir->roles[i];
        if (stmt->type == AST_ROLE_DECL
            && stmt->data.role_decl.name != NULL
            && strcmp(stmt->data.role_decl.name, role_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_function_decl(TranspilerCtx *ctx, const char *function_name)
{
    if (ctx == NULL || ctx->hir == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->function_count; i++) {
        ASTNode *stmt = ctx->hir->functions[i];
        if (stmt->type == AST_FUNC_DECL
            && stmt->data.func_decl.name != NULL
            && strcmp(stmt->data.func_decl.name, function_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_intent_decl(TranspilerCtx *ctx, const char *intent_name)
{
    if (ctx == NULL || ctx->hir == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->intent_count; i++) {
        ASTNode *stmt = ctx->hir->intents[i];
        if (stmt->type == AST_INTENT_DECL
            && stmt->data.intent_decl.name != NULL
            && strcmp(stmt->data.intent_decl.name, intent_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_callable_decl(TranspilerCtx *ctx, const char *name)
{
    ASTNode *decl = find_function_decl(ctx, name);
    if (decl != NULL)
        return decl;
    return find_intent_decl(ctx, name);
}

static ASTNode *
find_party_decl(TranspilerCtx *ctx, const char *party_name)
{
    if (ctx == NULL || ctx->hir == NULL || party_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->party_count; i++) {
        ASTNode *stmt = ctx->hir->parties[i];
        if (stmt != NULL && stmt->type == AST_PARTY_DECL
            && stmt->data.party_decl.name != NULL
            && strcmp(stmt->data.party_decl.name, party_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_class_decl(TranspilerCtx *ctx, const char *class_name)
{
    if (ctx == NULL || ctx->hir == NULL || class_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->type_count; i++) {
        ASTNode *stmt = ctx->hir->types[i];
        if (stmt != NULL && stmt->type == AST_CLASS_DECL
            && stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, class_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_actor_decl(TranspilerCtx *ctx, const char *actor_name)
{
    if (ctx == NULL || ctx->hir == NULL || actor_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->actor_count; i++) {
        ASTNode *stmt = ctx->hir->actors[i];
        if (stmt != NULL && stmt->type == AST_ACTOR_DECL
            && stmt->data.actor_decl.name != NULL
            && strcmp(stmt->data.actor_decl.name, actor_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_zone_decl(TranspilerCtx *ctx, const char *zone_name)
{
    if (ctx == NULL || ctx->hir == NULL || zone_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->zone_count; i++) {
        ASTNode *stmt = ctx->hir->zones[i];
        if (stmt != NULL && stmt->type == AST_ZONE_DECL
            && stmt->data.zone_decl.name != NULL
            && strcmp(stmt->data.zone_decl.name, zone_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static bool
transpiler_has_known_nominal_type(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || ctx->hir == NULL || name == NULL)
        return false;

    if (find_class_decl(ctx, name) != NULL
        || find_zone_decl(ctx, name) != NULL
        || find_party_decl(ctx, name) != NULL
        || find_actor_decl(ctx, name) != NULL)
        return true;

    for (size_t i = 0; i < ctx->hir->systemic_count; i++) {
        ASTNode *stmt = ctx->hir->systemics[i];
        if (stmt != NULL && stmt->data.systemic_decl.name != NULL
            && strcmp(stmt->data.systemic_decl.name, name) == 0)
            return true;
    }
    for (size_t i = 0; i < ctx->hir->world_count; i++) {
        (void)i;
    }
    for (size_t i = 0; i < ctx->hir->relation_count; i++) {
        ASTNode *stmt = ctx->hir->relations[i];
        if (stmt != NULL && stmt->data.relation_decl.name != NULL
            && strcmp(stmt->data.relation_decl.name, name) == 0)
            return true;
    }
    for (size_t i = 0; i < ctx->hir->effect_count; i++) {
        ASTNode *stmt = ctx->hir->effects[i];
        if (stmt != NULL && stmt->data.effect_decl.name != NULL
            && strcmp(stmt->data.effect_decl.name, name) == 0)
            return true;
    }

    return false;
}

static ASTNode *
find_world_decl(TranspilerCtx *ctx, const char *world_name)
{
    if (ctx == NULL || ctx->hir == NULL || world_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->world_count; i++) {
        ASTNode *stmt = ctx->hir->worlds[i];
        if (stmt != NULL && stmt->type == AST_WORLD_DECL
            && stmt->data.world_decl.name != NULL
            && strcmp(stmt->data.world_decl.name, world_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_relation_decl(TranspilerCtx *ctx, const char *relation_name)
{
    if (ctx == NULL || ctx->hir == NULL || relation_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->relation_count; i++) {
        ASTNode *stmt = ctx->hir->relations[i];
        if (stmt != NULL && stmt->type == AST_RELATION_DECL
            && stmt->data.relation_decl.name != NULL
            && strcmp(stmt->data.relation_decl.name, relation_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_effect_decl(TranspilerCtx *ctx, const char *effect_name)
{
    if (ctx == NULL || ctx->hir == NULL || effect_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->effect_count; i++) {
        ASTNode *stmt = ctx->hir->effects[i];
        if (stmt != NULL && stmt->type == AST_EFFECT_DECL
            && stmt->data.effect_decl.name != NULL
            && strcmp(stmt->data.effect_decl.name, effect_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static size_t
projection_source_field_count_local(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    if (decl->type == AST_ACTOR_DECL)
        return decl->data.actor_decl.field_count;
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
    if (decl->type == AST_ACTOR_DECL) {
        if (index < decl->data.actor_decl.field_count)
            return decl->data.actor_decl.fields[index];
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

        if (field == NULL || !field->is_vessel_field
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
    return ctx != NULL
        && ctx->current_class_name != NULL
        && is_pointer_self_host_type_name(ctx, ctx->current_class_name);
}

static bool
current_class_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || ctx->current_class_name == NULL || field_name == NULL)
        return false;

    decl = find_class_decl(ctx, ctx->current_class_name);
    /* For monomorphized generic classes, look up the original class decl */
    if (decl == NULL) {
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

    if (ctx == NULL || ctx->current_zone_name == NULL || field_name == NULL)
        return false;

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

    if (ctx == NULL || ctx->current_relation_name == NULL || field_name == NULL)
        return false;

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

    if (ctx == NULL || ctx->current_effect_name == NULL || field_name == NULL)
        return false;

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
domain_has_projection_slot(ASTNode **slots, size_t slot_count,
                           ASTNode **refreshes, size_t refresh_count,
                           const char *slot_name)
{
    if (slots == NULL || slot_name == NULL)
        return false;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.slot_name == NULL
            || strcmp(slot->data.domain_slot.slot_name, slot_name) != 0) {
            continue;
        }
        if (slot->data.domain_slot.is_dto)
            return true;
        if (slot->data.domain_slot.is_subject)
            return false;
        return domain_slot_is_projection_target_local(slot, refreshes, refresh_count);
    }

    return false;
}

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

static bool
current_zone_has_projection_slot(TranspilerCtx *ctx, const char *slot_name)
{
    ASTNode *decl;
    if (ctx == NULL || ctx->current_zone_name == NULL || slot_name == NULL)
        return false;
    decl = find_zone_decl(ctx, ctx->current_zone_name);
    if (decl == NULL)
        return false;
    return domain_has_projection_slot(decl->data.zone_decl.slots,
        decl->data.zone_decl.slot_count,
        decl->data.zone_decl.refreshes,
        decl->data.zone_decl.refresh_count, slot_name);
}

static bool
current_relation_has_projection_slot(TranspilerCtx *ctx, const char *slot_name)
{
    ASTNode *decl;
    if (ctx == NULL || ctx->current_relation_name == NULL || slot_name == NULL)
        return false;
    decl = find_relation_decl(ctx, ctx->current_relation_name);
    if (decl == NULL)
        return false;
    return domain_has_projection_slot(decl->data.relation_decl.slots,
        decl->data.relation_decl.slot_count,
        decl->data.relation_decl.refreshes,
        decl->data.relation_decl.refresh_count, slot_name);
}

static bool
current_effect_has_projection_slot(TranspilerCtx *ctx, const char *slot_name)
{
    ASTNode *decl;
    if (ctx == NULL || ctx->current_effect_name == NULL || slot_name == NULL)
        return false;
    decl = find_effect_decl(ctx, ctx->current_effect_name);
    if (decl == NULL)
        return false;
    return domain_has_projection_slot(decl->data.effect_decl.slots,
        decl->data.effect_decl.slot_count,
        decl->data.effect_decl.refreshes,
        decl->data.effect_decl.refresh_count, slot_name);
}

static bool
current_world_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || ctx->current_world_name == NULL || field_name == NULL)
        return false;

    decl = find_world_decl(ctx, ctx->current_world_name);
    if (decl == NULL)
        return false;

    for (size_t i = 0; i < decl->data.world_decl.systemic_count; i++) {
        ASTNode *slot = decl->data.world_decl.systemics[i];
        if (slot != NULL && slot->data.world_systemic.slot_name != NULL
            && strcmp(slot->data.world_systemic.slot_name, field_name) == 0) {
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

    decl = find_actor_decl(ctx, type_name);
    if (decl != NULL && decl->type == AST_ACTOR_DECL) {
        for (size_t i = 0; i < decl->data.actor_decl.method_count; i++) {
            ASTNode *method = decl->data.actor_decl.methods[i];
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

    if (ctx == NULL || ctx->current_zone_name == NULL || receiver == NULL)
        return false;

    zone_decl = find_zone_decl(ctx, ctx->current_zone_name);
    if (zone_decl == NULL)
        return false;

    if (receiver->type == AST_IDENTIFIER && receiver->data.identifier.name != NULL) {
        slot_name = receiver->data.identifier.name;
        type_name = zone_subject_slot_type_name(zone_decl, slot_name);
        if (type_name == NULL) {
            const char *var_type = lookup_typed_var(ctx, slot_name);
            if (var_type != NULL && is_subject_type_name(ctx, var_type))
                type_name = var_type;
            else if (var_type != NULL) {
                ASTNode *actor_decl = find_actor_decl(ctx, var_type);
                if (actor_decl != NULL)
                    type_name = var_type;
            }
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

    if (ctx == NULL || ctx->current_zone_name == NULL || call == NULL
        || call->type != AST_CALL) {
        return;
    }

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
        || strcmp(method_decl->data.func_decl.within_zone, ctx->current_zone_name) != 0) {
        return;
    }

    zone_decl = find_zone_decl(ctx, ctx->current_zone_name);
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
current_field_type_name(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;

    if (ctx == NULL || field_name == NULL)
        return NULL;

    if (ctx->current_class_name != NULL) {
        decl = find_class_decl(ctx, ctx->current_class_name);
        if (decl != NULL && decl->type == AST_CLASS_DECL) {
            for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
                ClassField *field = decl->data.class_decl.fields[i];
                if (field != NULL && field->name != NULL
                    && strcmp(field->name, field_name) == 0
                    && field->type != NULL && field->type->type == AST_TYPE) {
                    return field->type->data.type.name;
                }
            }
        }
    }

    if (ctx->current_zone_name != NULL) {
        decl = find_zone_decl(ctx, ctx->current_zone_name);
        if (decl != NULL && decl->type == AST_ZONE_DECL) {
            ASTNode *slot = find_zone_domain_slot(decl, field_name);
            if (slot != NULL && slot->data.domain_slot.type != NULL
                && slot->data.domain_slot.type->type == AST_TYPE) {
                return slot->data.domain_slot.type->data.type.name;
            }
            for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
                ASTNode *layer = decl->data.zone_decl.layer_slots[i];
                if (layer != NULL && layer->type == AST_ZONE_LAYER_SLOT
                    && layer->data.zone_layer_slot.slot_name != NULL
                    && strcmp(layer->data.zone_layer_slot.slot_name, field_name) == 0) {
                    return layer->data.zone_layer_slot.layer_type;
                }
            }
            for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
                ASTNode *shared = decl->data.zone_decl.shared_fields[i];
                if (shared != NULL && shared->data.party_shared.name != NULL
                    && strcmp(shared->data.party_shared.name, field_name) == 0
                    && shared->data.party_shared.type != NULL
                    && shared->data.party_shared.type->type == AST_TYPE) {
                    return shared->data.party_shared.type->data.type.name;
                }
            }
        }
    }

    if (ctx->current_relation_name != NULL) {
        decl = find_relation_decl(ctx, ctx->current_relation_name);
        if (decl != NULL && decl->type == AST_RELATION_DECL) {
            for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
                ASTNode *slot = decl->data.relation_decl.slots[i];
                if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, field_name) == 0
                    && slot->data.domain_slot.type != NULL
                    && slot->data.domain_slot.type->type == AST_TYPE) {
                    return slot->data.domain_slot.type->data.type.name;
                }
            }
        }
    }

    if (ctx->current_effect_name != NULL) {
        decl = find_effect_decl(ctx, ctx->current_effect_name);
        if (decl != NULL && decl->type == AST_EFFECT_DECL) {
            for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
                ASTNode *slot = decl->data.effect_decl.slots[i];
                if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, field_name) == 0
                    && slot->data.domain_slot.type != NULL
                    && slot->data.domain_slot.type->type == AST_TYPE) {
                    return slot->data.domain_slot.type->data.type.name;
                }
            }
        }
    }

    if (ctx->current_world_name != NULL) {
        decl = find_world_decl(ctx, ctx->current_world_name);
        if (decl != NULL && decl->type == AST_WORLD_DECL) {
            for (size_t i = 0; i < decl->data.world_decl.systemic_count; i++) {
                ASTNode *slot = decl->data.world_decl.systemics[i];
                if (slot != NULL && slot->data.world_systemic.slot_name != NULL
                    && strcmp(slot->data.world_systemic.slot_name, field_name) == 0) {
                    return slot->data.world_systemic.systemic_type;
                }
            }
            for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
                ASTNode *slot = decl->data.world_decl.zones[i];
                if (slot != NULL && slot->data.world_zone.slot_name != NULL
                    && strcmp(slot->data.world_zone.slot_name, field_name) == 0) {
                    return slot->data.world_zone.zone_type;
                }
            }
            for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
                ASTNode *shared = decl->data.world_decl.shared_fields[i];
                if (shared != NULL && shared->data.party_shared.name != NULL
                    && strcmp(shared->data.party_shared.name, field_name) == 0
                    && shared->data.party_shared.type != NULL
                    && shared->data.party_shared.type->type == AST_TYPE) {
                    return shared->data.party_shared.type->data.type.name;
                }
            }
        }
    }

    return NULL;
}

static const char *
current_host_decl_name(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return NULL;
    if (ctx->current_relation_name != NULL)
        return ctx->current_relation_name;
    if (ctx->current_effect_name != NULL)
        return ctx->current_effect_name;
    if (ctx->current_zone_name != NULL)
        return ctx->current_zone_name;
    if (ctx->current_world_name != NULL)
        return ctx->current_world_name;
    return ctx->current_class_name;
}

static ASTNode *
current_host_method_decl(TranspilerCtx *ctx, const char *method_name)
{
    ASTNode *decl = NULL;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (ctx == NULL || method_name == NULL)
        return NULL;

    if (ctx->current_relation_name != NULL) {
        decl = find_relation_decl(ctx, ctx->current_relation_name);
        if (decl != NULL) {
            methods = decl->data.relation_decl.methods;
            method_count = decl->data.relation_decl.method_count;
        }
    } else if (ctx->current_effect_name != NULL) {
        decl = find_effect_decl(ctx, ctx->current_effect_name);
        if (decl != NULL) {
            methods = decl->data.effect_decl.methods;
            method_count = decl->data.effect_decl.method_count;
        }
    } else if (ctx->current_zone_name != NULL) {
        decl = find_zone_decl(ctx, ctx->current_zone_name);
        if (decl != NULL) {
            methods = decl->data.zone_decl.methods;
            method_count = decl->data.zone_decl.method_count;
        }
    } else if (ctx->current_world_name != NULL) {
        decl = find_world_decl(ctx, ctx->current_world_name);
        if (decl != NULL) {
            methods = decl->data.world_decl.methods;
            method_count = decl->data.world_decl.method_count;
        }
    } else if (ctx->current_class_name != NULL) {
        decl = find_class_decl(ctx, ctx->current_class_name);
        if (decl != NULL && decl->type == AST_CLASS_DECL) {
            methods = decl->data.class_decl.methods;
            method_count = decl->data.class_decl.method_count;
        } else {
            decl = find_actor_decl(ctx, ctx->current_class_name);
            if (decl != NULL && decl->type == AST_ACTOR_DECL) {
                methods = decl->data.actor_decl.methods;
                method_count = decl->data.actor_decl.method_count;
            }
        }
    }

    for (size_t i = 0; methods != NULL && i < method_count; i++) {
        ASTNode *method = methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

static ASTNode *
find_nominal_host_method_decl(TranspilerCtx *ctx, const char *host_type_name,
                              const char *method_name)
{
    ASTNode *decl = NULL;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    decl = find_relation_decl(ctx, host_type_name);
    if (decl != NULL) {
        methods = decl->data.relation_decl.methods;
        method_count = decl->data.relation_decl.method_count;
    } else {
        decl = find_effect_decl(ctx, host_type_name);
        if (decl != NULL) {
            methods = decl->data.effect_decl.methods;
            method_count = decl->data.effect_decl.method_count;
        } else {
            decl = find_zone_decl(ctx, host_type_name);
            if (decl != NULL) {
                methods = decl->data.zone_decl.methods;
                method_count = decl->data.zone_decl.method_count;
            } else {
                decl = find_world_decl(ctx, host_type_name);
                if (decl != NULL) {
                    methods = decl->data.world_decl.methods;
                    method_count = decl->data.world_decl.method_count;
                } else {
                    decl = find_class_decl(ctx, host_type_name);
                    if (decl != NULL && decl->type == AST_CLASS_DECL) {
                        methods = decl->data.class_decl.methods;
                        method_count = decl->data.class_decl.method_count;
                    } else {
                        decl = find_actor_decl(ctx, host_type_name);
                        if (decl != NULL && decl->type == AST_ACTOR_DECL) {
                            methods = decl->data.actor_decl.methods;
                            method_count = decl->data.actor_decl.method_count;
                        }
                    }
                }
            }
        }
    }

    for (size_t i = 0; methods != NULL && i < method_count; i++) {
        ASTNode *method = methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

static ASTNode *
find_nominal_host_decl_local(TranspilerCtx *ctx, const char *host_type_name)
{
    ASTNode *decl;

    if (ctx == NULL || host_type_name == NULL)
        return NULL;

    decl = find_relation_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    decl = find_effect_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    decl = find_zone_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    decl = find_world_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    decl = find_class_decl(ctx, host_type_name);
    if (decl != NULL)
        return decl;
    return find_actor_decl(ctx, host_type_name);
}

static const char *
lookup_nominal_host_member_type_name(TranspilerCtx *ctx, const char *host_type_name,
                                     const char *member_name)
{
    ASTNode *decl;

    if (ctx == NULL || host_type_name == NULL || member_name == NULL)
        return NULL;

    decl = find_nominal_host_decl_local(ctx, host_type_name);
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
            ClassField *field = decl->data.class_decl.fields[i];
            if (field != NULL && field->name != NULL
                && strcmp(field->name, member_name) == 0
                && field->type != NULL && field->type->type == AST_TYPE) {
                return field->type->data.type.name;
            }
        }
        break;
    case AST_ACTOR_DECL:
        for (size_t i = 0; i < decl->data.actor_decl.field_count; i++) {
            ClassField *field = decl->data.actor_decl.fields[i];
            if (field != NULL && field->name != NULL
                && strcmp(field->name, member_name) == 0
                && field->type != NULL && field->type->type == AST_TYPE) {
                return field->type->data.type.name;
            }
        }
        break;
    case AST_ZONE_DECL: {
        ASTNode *slot = find_zone_domain_slot(decl, member_name);
        if (slot != NULL && slot->data.domain_slot.type != NULL
            && slot->data.domain_slot.type->type == AST_TYPE) {
            return slot->data.domain_slot.type->data.type.name;
        }
        for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
            ASTNode *layer = decl->data.zone_decl.layer_slots[i];
            if (layer != NULL && layer->type == AST_ZONE_LAYER_SLOT
                && layer->data.zone_layer_slot.slot_name != NULL
                && strcmp(layer->data.zone_layer_slot.slot_name, member_name) == 0) {
                return layer->data.zone_layer_slot.layer_type;
            }
        }
        for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
            ASTNode *shared = decl->data.zone_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0
                && shared->data.party_shared.type != NULL
                && shared->data.party_shared.type->type == AST_TYPE) {
                return shared->data.party_shared.type->data.type.name;
            }
        }
        break;
    }
    case AST_RELATION_DECL:
        for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
            ASTNode *slot = decl->data.relation_decl.slots[i];
            if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, member_name) == 0
                && slot->data.domain_slot.type != NULL
                && slot->data.domain_slot.type->type == AST_TYPE) {
                return slot->data.domain_slot.type->data.type.name;
            }
        }
        for (size_t i = 0; i < decl->data.relation_decl.shared_count; i++) {
            ASTNode *shared = decl->data.relation_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0
                && shared->data.party_shared.type != NULL
                && shared->data.party_shared.type->type == AST_TYPE) {
                return shared->data.party_shared.type->data.type.name;
            }
        }
        break;
    case AST_EFFECT_DECL:
        for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
            ASTNode *slot = decl->data.effect_decl.slots[i];
            if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, member_name) == 0
                && slot->data.domain_slot.type != NULL
                && slot->data.domain_slot.type->type == AST_TYPE) {
                return slot->data.domain_slot.type->data.type.name;
            }
        }
        for (size_t i = 0; i < decl->data.effect_decl.shared_count; i++) {
            ASTNode *shared = decl->data.effect_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0
                && shared->data.party_shared.type != NULL
                && shared->data.party_shared.type->type == AST_TYPE) {
                return shared->data.party_shared.type->data.type.name;
            }
        }
        break;
    case AST_WORLD_DECL:
        for (size_t i = 0; i < decl->data.world_decl.systemic_count; i++) {
            ASTNode *slot = decl->data.world_decl.systemics[i];
            if (slot != NULL && slot->data.world_systemic.slot_name != NULL
                && strcmp(slot->data.world_systemic.slot_name, member_name) == 0) {
                return slot->data.world_systemic.systemic_type;
            }
        }
        for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
            ASTNode *slot = decl->data.world_decl.zones[i];
            if (slot != NULL && slot->data.world_zone.slot_name != NULL
                && strcmp(slot->data.world_zone.slot_name, member_name) == 0) {
                return slot->data.world_zone.zone_type;
            }
        }
        for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
            ASTNode *shared = decl->data.world_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0
                && shared->data.party_shared.type != NULL
                && shared->data.party_shared.type->type == AST_TYPE) {
                return shared->data.party_shared.type->data.type.name;
            }
        }
        break;
    default:
        break;
    }

    return NULL;
}

static const char *
resolve_nominal_host_expr_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    if (ctx == NULL || expr == NULL)
        return NULL;

    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL)
            return type_name;
        return current_field_type_name(ctx, expr->data.identifier.name);
    }

    if (expr->type == AST_MEMBER_ACCESS
        && expr->data.member.object != NULL
        && expr->data.member.name != NULL) {
        /* self.field */
        if (expr->data.member.object->type == AST_IDENTIFIER
            && strcmp(expr->data.member.object->data.identifier.name, "self") == 0) {
            return current_field_type_name(ctx, expr->data.member.name);
        }
        {
            const char *obj_type = resolve_nominal_host_expr_type_name(ctx,
                expr->data.member.object);
            const char *member_type = lookup_nominal_host_member_type_name(ctx,
                obj_type, expr->data.member.name);
            if (member_type != NULL)
                return member_type;
        }
    }

    return NULL;
}

static bool
world_has_zone_slot(ASTNode *world_decl, const char *slot_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || slot_name == NULL)
        return false;

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return true;
        }
    }

    return false;
}

static ASTNode *
find_world_zone_slot_decl(ASTNode *world_decl, const char *slot_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

static ASTNode *
resolve_world_zone_decl(TranspilerCtx *ctx, ASTNode *world_decl, const char *slot_name)
{
    ASTNode *zone_slot = find_world_zone_slot_decl(world_decl, slot_name);
    if (ctx == NULL || zone_slot == NULL || zone_slot->data.world_zone.zone_type == NULL)
        return NULL;
    return find_zone_decl(ctx, zone_slot->data.world_zone.zone_type);
}

static char *
emit_builtin_to_dto(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode *target_arg;
    ASTNode *source_arg;
    ASTNode *target_decl;
    ASTNode *source_decl;
    const char *source_type_name;
    char *source_expr;
    CodeBuf *buf;
    char *result;
    bool first = true;

    if (call->data.call.arg_count != 2)
        return pergyra_strdup("/* ToDto: invalid args */");

    target_arg = call->data.call.arguments[0];
    source_arg = call->data.call.arguments[1];
    if (target_arg == NULL || target_arg->type != AST_IDENTIFIER
        || target_arg->data.identifier.name == NULL) {
        return pergyra_strdup("/* ToDto: need target dto type */");
    }
    if (source_arg == NULL || source_arg->type != AST_IDENTIFIER) {
        return pergyra_strdup("/* ToDto: need named subject source */");
    }

    target_decl = find_class_decl(ctx, target_arg->data.identifier.name);
    if (target_decl == NULL || !target_decl->data.class_decl.is_struct)
        return pergyra_strdup("/* ToDto: target must be dto/struct */");

    source_type_name = infer_expression_type_name(ctx, source_arg);
    source_decl = find_class_decl(ctx, source_type_name);
    if (source_decl == NULL)
        return pergyra_strdup("/* ToDto: source subject type not found */");

    source_expr = emit_expression(source_arg, ctx);
    buf = codebuf_create();
    codebuf_write(buf, "(%s){ ", target_arg->data.identifier.name);

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        char *source_path = NULL;
        int source_status;

        if (target_field == NULL || target_field->name == NULL)
            continue;

        source_status = resolve_projection_source_path_rec(
            ctx, source_decl, target_field->name, 0, &source_path);
        if (!first)
            codebuf_write(buf, ", ");
        first = false;

        if (source_status == 1 && source_path != NULL) {
            codebuf_write(buf, ".%s = %s.%s",
                target_field->name, source_expr, source_path);
        } else {
            codebuf_write(buf, ".%s = 0", target_field->name);
        }
        free(source_path);
    }

    codebuf_write(buf, " }");
    free(source_expr);
    result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static char *
emit_projection_literal(TranspilerCtx *ctx, ASTNode *target_decl, ASTNode *source_decl,
                        const char *target_type_name, const char *source_expr)
{
    CodeBuf *buf;
    char *result;
    bool first = true;

    if (target_decl == NULL || source_decl == NULL
        || target_type_name == NULL || source_expr == NULL) {
        return pergyra_strdup("0");
    }

    buf = codebuf_create();
    codebuf_write(buf, "(%s){ ", target_type_name);

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        char *source_path = NULL;
        int source_status;

        if (target_field == NULL || target_field->name == NULL)
            continue;

        source_status = resolve_projection_source_path_rec(
            ctx, source_decl, target_field->name, 0, &source_path);
        if (!first)
            codebuf_write(buf, ", ");
        first = false;

        if (source_status == 1 && source_path != NULL) {
            codebuf_write(buf, ".%s = %s.%s",
                target_field->name, source_expr, source_path);
        } else {
            codebuf_write(buf, ".%s = 0", target_field->name);
        }
        free(source_path);
    }

    codebuf_write(buf, " }");
    result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static ASTNode *
find_ability_decl(TranspilerCtx *ctx, const char *ability_name)
{
    if (ctx == NULL || ctx->hir == NULL || ability_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->ability_count; i++) {
        ASTNode *stmt = ctx->hir->abilities[i];
        if (stmt != NULL && stmt->type == AST_ABILITY_DECL
            && stmt->data.ability_decl.name != NULL
            && strcmp(stmt->data.ability_decl.name, ability_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static ASTNode *
find_event_decl(TranspilerCtx *ctx, const char *event_name)
{
    if (ctx == NULL || ctx->hir == NULL || event_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->event_count; i++) {
        ASTNode *stmt = ctx->hir->events[i];
        if (stmt != NULL && stmt->type == AST_EVENT_DECL
            && stmt->data.event_decl.name != NULL
            && strcmp(stmt->data.event_decl.name, event_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

static bool
func_has_generic_params(ASTNode *node)
{
    return node != NULL
        && node->type == AST_FUNC_DECL
        && node->data.func_decl.generic_params != NULL
        && node->data.func_decl.generic_params->count > 0;
}

static const char *
lookup_generic_binding(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    for (int i = ctx->generic_binding_count - 1; i >= 0; i--) {
        if (strcmp(ctx->generic_bindings[i].name, name) == 0)
            return ctx->generic_bindings[i].concrete_type;
    }

    return NULL;
}

static bool
infer_generic_call_bindings(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call,
                            GenericBindingEntry *bindings, size_t *binding_count);

static char *
render_type_name_with_bindings(TranspilerCtx *ctx, ASTNode *type_node,
                               GenericBindingEntry *bindings, size_t binding_count);

static const char *
lookup_enum_variant_qualified_name(TranspilerCtx *ctx, const char *variant_name)
{
    static char qualified[128];

    if (ctx == NULL || ctx->hir == NULL || variant_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->type_count; i++) {
        ASTNode *stmt = ctx->hir->types[i];
        if (stmt == NULL || stmt->type != AST_ENUM_DECL)
            continue;
        for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
            const char *candidate = stmt->data.enum_decl.variants[j];
            if (candidate != NULL && strcmp(candidate, variant_name) == 0) {
                snprintf(qualified, sizeof(qualified), "%s_%s",
                    stmt->data.enum_decl.name, candidate);
                return qualified;
            }
        }
    }

    return NULL;
}

static const char *
operator_overload_suffix(TokenType op)
{
    switch (op) {
    case TOKEN_PLUS:          return "add";
    case TOKEN_MINUS:         return "sub";
    case TOKEN_STAR:          return "mul";
    case TOKEN_SLASH:         return "div";
    case TOKEN_PERCENT:       return "mod";
    case TOKEN_EQUAL:         return "eq";
    case TOKEN_NOT_EQUAL:     return "ne";
    case TOKEN_LESS:          return "lt";
    case TOKEN_LESS_EQUAL:    return "le";
    case TOKEN_GREATER:       return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default:                  return NULL;
    }
}

static ASTNode *
find_role_operator_method_decl(TranspilerCtx *ctx, ASTNode *role,
                               TokenType op, int depth);

static ASTNode *
find_operator_overload_decl(TranspilerCtx *ctx, const char *type_name, TokenType op)
{
    const char *suffix = operator_overload_suffix(op);
    char fn_name[256];

    if (ctx == NULL || type_name == NULL || suffix == NULL)
        return NULL;

    snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, type_name);
    ASTNode *fn = find_function_decl(ctx, fn_name);
    if (fn != NULL)
        return fn;

    if (ctx != NULL && ctx->hir != NULL && type_name != NULL) {
        for (size_t i = 0; i < ctx->hir->role_count; i++) {
            ASTNode *role = ctx->hir->roles[i];
            if (role == NULL || role->type != AST_ROLE_DECL
                || role->data.role_decl.for_type == NULL
                || role->data.role_decl.for_type->type != AST_TYPE
                || role->data.role_decl.for_type->data.type.name == NULL
                || strcmp(role->data.role_decl.for_type->data.type.name, type_name) != 0) {
                continue;
            }
            if (find_role_operator_method_decl(ctx, role, op, 0) != NULL)
                return role;
        }
    }

    return NULL;
}

static bool
operator_method_name_matches(TokenType op, const char *name)
{
    static const struct {
        TokenType op;
        const char *names[10];
    } aliases[] = {
        { TOKEN_PLUS, { "Add", "add", "OperatorAdd", "operator_add", NULL } },
        { TOKEN_MINUS, { "Sub", "sub", "Subtract", "subtract",
                         "OperatorSub", "operator_sub", NULL } },
        { TOKEN_STAR, { "Mul", "mul", "Multiply", "multiply",
                        "OperatorMul", "operator_mul", NULL } },
        { TOKEN_SLASH, { "Div", "div", "Divide", "divide",
                         "OperatorDiv", "operator_div", NULL } },
        { TOKEN_PERCENT, { "Mod", "mod", "Modulo", "modulo",
                           "OperatorMod", "operator_mod", NULL } },
        { TOKEN_EQUAL, { "Eq", "eq", "Equal", "equal", "Equals", "equals",
                         "OperatorEq", "operator_eq", NULL } },
        { TOKEN_NOT_EQUAL, { "Ne", "ne", "NotEqual", "notEqual",
                             "NotEquals", "notEquals",
                             "OperatorNe", "operator_ne", NULL } },
        { TOKEN_LESS, { "Lt", "lt", "LessThan", "lessThan",
                        "OperatorLt", "operator_lt", NULL } },
        { TOKEN_LESS_EQUAL, { "Le", "le", "LessEqual", "lessEqual",
                              "LessThanOrEqual", "lessThanOrEqual",
                              "OperatorLe", "operator_le", NULL } },
        { TOKEN_GREATER, { "Gt", "gt", "GreaterThan", "greaterThan",
                           "OperatorGt", "operator_gt", NULL } },
        { TOKEN_GREATER_EQUAL, { "Ge", "ge", "GreaterEqual", "greaterEqual",
                                 "GreaterThanOrEqual", "greaterThanOrEqual",
                                 "OperatorGe", "operator_ge", NULL } },
    };

    if (name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (aliases[i].op != op)
            continue;
        for (size_t j = 0; aliases[i].names[j] != NULL; j++) {
            if (strcmp(aliases[i].names[j], name) == 0)
                return true;
        }
        break;
    }
    return false;
}

static ASTNode *
find_role_operator_method_decl(TranspilerCtx *ctx, ASTNode *role,
                               TokenType op, int depth)
{
    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && operator_method_name_matches(op, method->data.func_decl.name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *include_stmt = role->data.role_decl.includes[i];
        ASTNode *included_role = find_role_decl(ctx,
            include_stmt->data.include_stmt.role_name);
        ASTNode *method = find_role_operator_method_decl(ctx, included_role, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

static bool
role_has_ability(ASTNode *role, const char *ability_name)
{
    if (role == NULL || role->type != AST_ROLE_DECL || ability_name == NULL)
        return false;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl->type == AST_IMPL_ABILITY
            && impl->data.impl_ability.ability_name != NULL
            && strcmp(impl->data.impl_ability.ability_name, ability_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
role_has_method(ASTNode *role, const char *method_name)
{
    if (role == NULL || role->type != AST_ROLE_DECL || method_name == NULL)
        return false;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method->type == AST_FUNC_DECL
                && method->data.func_decl.name != NULL
                && strcmp(method->data.func_decl.name, method_name) == 0) {
                return true;
            }
        }
    }

    return false;
}

/* -----------------------------------------------------------------
 * Type mapping
 * ----------------------------------------------------------------- */

const char *
pergyra_primitive_to_c(const char *name)
{
    if (strcmp(name, "Int")    == 0) return "int32_t";
    if (strcmp(name, "Long")   == 0) return "int64_t";
    if (strcmp(name, "Float")  == 0) return "float";
    if (strcmp(name, "Double") == 0) return "double";
    if (strcmp(name, "Bool")   == 0) return "bool";
    if (strcmp(name, "String") == 0) return "char*";
    if (strcmp(name, "QubitSlot") == 0) return "int32_t";
    if (strcmp(name, "Void")   == 0) return "void";
    /* Single uppercase letter = generic type parameter → void* (type erasure) */
    if (name[0] >= 'A' && name[0] <= 'Z' && name[1] == '\0')
        return "void*";
    return name; /* user-defined type — pass through */
}

/*
 * slot_inner_type_name("Slot<Int>")       → "Int"
 * slot_inner_type_name("SecureSlot<Int>") → "Int"
 */
const char *
slot_inner_type_name(const char *slot_type_name)
{
    const char *open = strchr(slot_type_name, '<');
    if (open == NULL)
        return slot_type_name;
    open++; /* skip '<' */

    /* Trim trailing '>' — return a static buffer (fine for our uses) */
    static char buf[128];
    const char *close = strrchr(slot_type_name, '>');
    if (close == NULL || close <= open)
        return "Int";

    size_t len = (size_t)(close - open);
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, open, len);
    buf[len] = '\0';
    return buf;
}

static const char *
channel_inner_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    const char *type_name = infer_expression_type_name(ctx, expr);
    if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
        return slot_inner_type_name(type_name);
    return "Int";
}

static void
sanitize_c_suffix(const char *type_name, char *buf, size_t buf_size)
{
    size_t out = 0;

    if (buf_size == 0)
        return;

    if (type_name == NULL || *type_name == '\0') {
        snprintf(buf, buf_size, "Int");
        return;
    }

    for (size_t i = 0; type_name[i] != '\0' && out + 1 < buf_size; i++) {
        unsigned char ch = (unsigned char)type_name[i];
        if ((ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')) {
            buf[out++] = (char)ch;
        } else {
            buf[out++] = '_';
        }
    }
    buf[out] = '\0';
}

static const char *
constructed_arg_name_at(const char *type_name, int arg_index)
{
    static char buf[128];
    const char *lt;
    int current = 0;
    int depth = 0;
    size_t out = 0;

    if (type_name == NULL || arg_index < 0)
        return "Int";

    lt = strchr(type_name, '<');
    if (lt == NULL)
        return "Int";
    lt++;

    for (const char *p = lt; *p != '\0'; p++) {
        char ch = *p;
        if (ch == '<') {
            if (current == arg_index && out + 1 < sizeof(buf))
                buf[out++] = ch;
            depth++;
            continue;
        }
        if (ch == '>') {
            if (depth == 0)
                break;
            if (current == arg_index && out + 1 < sizeof(buf))
                buf[out++] = ch;
            depth--;
            continue;
        }
        if (ch == ',' && depth == 0) {
            if (current == arg_index)
                break;
            current++;
            continue;
        }
        if (current == arg_index) {
            if (!(out == 0 && ch == ' ') && out + 1 < sizeof(buf))
                buf[out++] = ch;
        }
    }

    while (out > 0 && buf[out - 1] == ' ')
        out--;
    buf[out] = '\0';
    return out > 0 ? buf : "Int";
}

const char *
pergyra_type_to_c(const char *name)
{
    static char buf[128];
    char suffix[96];

    if (strcmp(name, "HashMap") == 0)
        return "PgyHashMap_Int";
    if (strcmp(name, "HashMapStr") == 0)
        return "PgyHashMap_String";
    if (strcmp(name, "List") == 0)
        return "PgyList_Int";
    if (strcmp(name, "ListStr") == 0)
        return "PgyList_String";
    if (strcmp(name, "Set") == 0)
        return "PgySet_String";
    if (strcmp(name, "Queue") == 0)
        return "PgyQueue_Int";
    if (strcmp(name, "Fsm") == 0)
        return "PgyFsm";
    if (strcmp(name, "Timer") == 0)
        return "PgyTimer";
    if (strcmp(name, "Cooldown") == 0)
        return "PgyCooldown";
    if (strcmp(name, "Allocator") == 0)
        return "PgyAllocator";
    if (strncmp(name, "List<", 5) == 0) {
        sanitize_c_suffix(slot_inner_type_name(name), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyList_%s", suffix);
        return buf;
    }
    if (strncmp(name, "HashMap<", 8) == 0) {
        sanitize_c_suffix(constructed_arg_name_at(name, 1), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyHashMap_%s", suffix);
        return buf;
    }
    if (strncmp(name, "Queue<", 6) == 0) {
        sanitize_c_suffix(slot_inner_type_name(name), suffix, sizeof(suffix));
        snprintf(buf, sizeof(buf), "PgyQueue_%s", suffix);
        return buf;
    }
    if (strncmp(name, "RemoteFuture<", 13) == 0)
        return "PgyTaskHandle";
    if (strncmp(name, "Future<", 7) == 0)
        return "PgyTaskHandle";
    if (strncmp(name, "Channel<", 8) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyChannel_%s", inner);
        return buf;
    }
    if (strncmp(name, "Weak<", 5) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyWeak_%s", inner);
        return buf;
    }
    if (strncmp(name, "Rc<", 3) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyRc_%s", inner);
        return buf;
    }
    if (strncmp(name, "Box<Array<", 10) == 0) {
        static char buf[128];
        const char *inner = name + 10;
        const char *close = strstr(inner, ">>");
        size_t len = close != NULL ? (size_t)(close - inner) : strlen(inner);
        if (len > 63) len = 63;
        char inner_buf[64];
        memcpy(inner_buf, inner, len);
        inner_buf[len] = '\0';
        snprintf(buf, sizeof(buf), "PgyBoxArray_%s", inner_buf);
        return buf;
    }
    if (strncmp(name, "Box<", 4) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyBox_%s", inner);
        return buf;
    }
    if (strncmp(name, "Slice<", 6) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlice_%s", inner);
        return buf;
    }
    if (strncmp(name, "Array<", 6) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyArray_%s", inner);
        return buf;
    }
    if (strncmp(name, "SecureSlot<", 11) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySecureSlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "ReadView<", 9) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "WriteView<", 10) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "DeviceSlot<", 11) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyDeviceSlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "Slot<", 5) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgySlot_%s", inner);
        return buf;
    }
    if (strncmp(name, "Result<", 7) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyResult_%s", inner);
        return buf;
    }
    if (strncmp(name, "Option<", 7) == 0) {
        static char buf[128];
        const char *inner = slot_inner_type_name(name);
        snprintf(buf, sizeof(buf), "PgyOption_%s", inner);
        return buf;
    }
    return pergyra_primitive_to_c(name);
}

static void
append_type_name(CodeBuf *buf, ASTNode *type_node)
{
    if (type_node == NULL
        || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        codebuf_write(buf, "Int");
        return;
    }

    {
        const char *bound = NULL;
        if (g_type_render_ctx != NULL)
            bound = lookup_generic_binding(g_type_render_ctx, type_node->data.type.name);
        codebuf_write(buf, "%s", bound != NULL ? bound : type_node->data.type.name);
    }
    if (type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0) {
        codebuf_write(buf, "<");
        for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
            GenericParam *param = type_node->data.type.generic_args->params[i];
            if (i > 0)
                codebuf_write(buf, ", ");
            if (param != NULL && param->constraint != NULL) {
                append_type_name(buf, param->constraint);
            } else if (param != NULL && param->name != NULL) {
                codebuf_write(buf, "%s", param->name);
            } else {
                codebuf_write(buf, "Int");
            }
        }
        codebuf_write(buf, ">");
    }
}

static bool resolve_slot_target(TranspilerCtx *ctx, ASTNode *slot_arg,
                                const char **inner_out, const char **slot_name_out,
                                bool *secure_out);

static char *
render_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return pergyra_strdup("Int");

    if (type_node->type == AST_CHANNEL_TYPE) {
        char *inner = render_type_name(type_node->data.channel_type.element_type);
        char *result = strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result;
    }

    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = render_type_name(type_node->data.future_type.value_type);
        char *result = strdup_fmt("Future<%s>", inner);
        free(inner);
        return result;
    }

    CodeBuf *buf = codebuf_create();
    if (buf == NULL)
        return pergyra_strdup("Int");
    append_type_name(buf, type_node);
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static char *
render_type_name_with_bindings(TranspilerCtx *ctx, ASTNode *type_node,
                               GenericBindingEntry *bindings, size_t binding_count)
{
    int saved_binding_count = ctx->generic_binding_count;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    char *result;

    for (size_t i = 0; i < binding_count && ctx->generic_binding_count < MAX_GENERIC_BINDINGS; i++)
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];

    g_type_render_ctx = ctx;
    result = render_type_name(type_node);
    g_type_render_ctx = saved_render_ctx;
    ctx->generic_binding_count = saved_binding_count;
    return result;
}

static void
ensure_collection_specialization(TranspilerCtx *ctx, const char *kind,
                                 const char *inner_type)
{
    char suffix[128];
    const char *ctype;

    if (ctx == NULL || kind == NULL || inner_type == NULL)
        return;

    if ((strcmp(kind, "List") == 0 || strcmp(kind, "Queue") == 0)
        && (strcmp(inner_type, "Int") == 0 || strcmp(inner_type, "String") == 0))
        return;
    if (strcmp(kind, "Map") == 0
        && (strcmp(inner_type, "Int") == 0 || strcmp(inner_type, "String") == 0))
        return;

    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));
    for (int i = 0; i < ctx->collection_spec_count; i++) {
        if (strcmp(ctx->collection_specs[i].kind, kind) == 0
            && strcmp(ctx->collection_specs[i].suffix, suffix) == 0) {
            return;
        }
    }

    if (ctx->collection_spec_count >= MAX_COLLECTION_SPECIALIZATIONS)
        return;

    ctype = pergyra_type_to_c(inner_type);
    if (ctype == NULL)
        return;

    snprintf(ctx->collection_specs[ctx->collection_spec_count].kind,
             sizeof(ctx->collection_specs[ctx->collection_spec_count].kind),
             "%s", kind);
    snprintf(ctx->collection_specs[ctx->collection_spec_count].suffix,
             sizeof(ctx->collection_specs[ctx->collection_spec_count].suffix),
             "%s", suffix);
    ctx->collection_spec_count++;

    if (strcmp(kind, "List") == 0) {
        codebuf_write(ctx->helpers,
            "\n/* PGY_COLLECTION_LIST_%s */\n"
            "PGY_LIST_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    } else if (strcmp(kind, "Queue") == 0) {
        codebuf_write(ctx->helpers,
            "\n/* PGY_COLLECTION_QUEUE_%s */\n"
            "PGY_QUEUE_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    } else if (strcmp(kind, "Map") == 0) {
        codebuf_write(ctx->helpers,
            "\n/* PGY_COLLECTION_MAP_%s */\n"
            "PGY_HASHMAP_DEFINE(%s, %s)\n",
            suffix, suffix, ctype);
    }
}

static const char *
collection_runtime_suffix(const char *inner_type)
{
    static char suffix[128];

    if (inner_type == NULL)
        return "int";
    if (strcmp(inner_type, "Int") == 0)
        return "int";
    if (strcmp(inner_type, "String") == 0)
        return "string";

    sanitize_c_suffix(inner_type, suffix, sizeof(suffix));
    return suffix;
}

static const char *
infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    static char inferred_call_type[128];

    if (expr == NULL)
        return "Int";

    switch (expr->type) {
    case AST_NUMBER:
        return "Int";
    case AST_STRING:
        return "String";
    case AST_BOOLEAN:
        return "Bool";
    case AST_ARRAY_LITERAL: {
        const char *inner = "Int";
        if (expr->data.array_literal.count > 0)
            inner = infer_expression_type_name(ctx, expr->data.array_literal.elements[0]);
        static char buf[128];
        snprintf(buf, sizeof(buf), "Array<%s>", inner);
        return buf;
    }
    case AST_ARRAY_ACCESS: {
        const char *array_type = infer_expression_type_name(ctx, expr->data.array_access.array);
        if (strncmp(array_type, "Array<", 6) == 0 || strncmp(array_type, "Slice<", 6) == 0)
            return slot_inner_type_name(array_type);
        return "Int";
    }
    case AST_IDENTIFIER: {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL)
            return type_name;
        type_name = current_field_type_name(ctx, expr->data.identifier.name);
        if (type_name != NULL)
            return type_name;
        {
            const char *enum_variant = lookup_enum_variant_qualified_name(ctx,
                expr->data.identifier.name);
            if (enum_variant != NULL) {
                size_t len = strcspn(enum_variant, "_");
                static char enum_name[128];
                if (len >= sizeof(enum_name))
                    len = sizeof(enum_name) - 1;
                memcpy(enum_name, enum_variant, len);
                enum_name[len] = '\0';
                return enum_name;
            }
        }
        return "Int";
    }
    case AST_CHANNEL_RECV: {
        ASTNode *channel = expr->data.channel_recv.channel;
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
            if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                return slot_inner_type_name(type_name);
        }
        return "Int";
    }
    case AST_MEMBER_ACCESS: {
        const char *resolved = resolve_nominal_host_expr_type_name(ctx, expr);
        if (resolved != NULL)
            return resolved;
        /* For member access on known-typed vars, try field lookup */
        if (expr->data.member.object != NULL
            && expr->data.member.object->type == AST_IDENTIFIER
            && expr->data.member.name != NULL) {
            const char *obj_type = lookup_typed_var(ctx,
                expr->data.member.object->data.identifier.name);
            if (obj_type != NULL) {
                ASTNode *obj_decl = find_class_decl(ctx, obj_type);
                if (obj_decl != NULL) {
                    for (size_t fi = 0; fi < obj_decl->data.class_decl.field_count; fi++) {
                        ClassField *f = obj_decl->data.class_decl.fields[fi];
                        if (f != NULL && f->name != NULL && f->type != NULL
                            && strcmp(f->name, expr->data.member.name) == 0) {
                            char *ft = render_type_name(f->type);
                            if (ft != NULL) {
                                static char mbuf[128];
                                snprintf(mbuf, sizeof(mbuf), "%s", ft);
                                free(ft);
                                return mbuf;
                            }
                        }
                    }
                }
            }
        }
        return "Int";
    }
    case AST_BINARY: {
        TokenType op = expr->data.binary.op.type;
        const char *left_type = infer_expression_type_name(ctx, expr->data.binary.left);
        const char *right_type = infer_expression_type_name(ctx, expr->data.binary.right);
        if (op == TOKEN_PLUS) {
            if ((left_type != NULL && strcmp(left_type, "String") == 0)
                || (right_type != NULL && strcmp(right_type, "String") == 0)) {
                return "String";
            }
            {
                ASTNode *cursor = expr->data.binary.left;
                while (cursor != NULL && cursor->type == AST_BINARY
                       && cursor->data.binary.op.type == TOKEN_PLUS) {
                    cursor = cursor->data.binary.left;
                }
                if (cursor != NULL) {
                    const char *leaf_type = infer_expression_type_name(ctx, cursor);
                    if (leaf_type != NULL && strcmp(leaf_type, "String") == 0)
                        return "String";
                }
            }
            return "Int";
        }
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL
            || op == TOKEN_AND || op == TOKEN_OR) {
            return "Bool";
        }
        return "Int";
    }
    case AST_CALL:
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_MEMBER_ACCESS
            && expr->data.call.callee->data.member.name != NULL) {
            ASTNode *receiver = expr->data.call.callee->data.member.object;
            const char *method_name = expr->data.call.callee->data.member.name;
            const char *receiver_type = infer_expression_type_name(ctx, receiver);
            if (receiver_type != NULL && method_name != NULL) {
                if ((strncmp(receiver_type, "Slot<", 5) == 0
                     || strncmp(receiver_type, "SecureSlot<", 11) == 0
                     || strncmp(receiver_type, "ReadView<", 9) == 0
                     || strncmp(receiver_type, "WriteView<", 10) == 0)
                    && strcmp(method_name, "Read") == 0) {
                    return slot_inner_type_name(receiver_type);
                }
                if ((strncmp(receiver_type, "DeviceSlot<", 11) == 0)
                    && strcmp(method_name, "Read") == 0) {
                    return slot_inner_type_name(receiver_type);
                }
                if (((strncmp(receiver_type, "Slot<", 5) == 0
                      || strncmp(receiver_type, "SecureSlot<", 11) == 0
                      || strncmp(receiver_type, "ReadView<", 9) == 0
                      || strncmp(receiver_type, "WriteView<", 10) == 0
                      || strncmp(receiver_type, "DeviceSlot<", 11) == 0))
                    && (strcmp(method_name, "Write") == 0
                        || strcmp(method_name, "Release") == 0)) {
                    return "Void";
                }
            }
            ASTNode *method_decl = NULL;
            receiver_type = resolve_nominal_host_expr_type_name(ctx, receiver);
            if (receiver_type != NULL) {
                method_decl = find_nominal_host_method_decl(ctx, receiver_type,
                    method_name);
            }
            if (method_decl != NULL && method_decl->type == AST_FUNC_DECL
                && method_decl->data.func_decl.return_type != NULL) {
                char *resolved = render_type_name(method_decl->data.func_decl.return_type);
                snprintf(inferred_call_type, sizeof(inferred_call_type), "%s", resolved);
                free(resolved);
                return inferred_call_type;
            }
        }
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_IDENTIFIER
            && expr->data.call.callee->data.identifier.name != NULL) {
            const char *name = expr->data.call.callee->data.identifier.name;
            if (strcmp(name, "MapNew") == 0)
                return "HashMap";
            if (strcmp(name, "MapGet") == 0 && expr->data.call.arg_count >= 1) {
                const char *map_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0)
                    return constructed_arg_name_at(map_type, 1);
                return "Int";
            }
            if (strcmp(name, "ToString") == 0)
                return "String";
            if (strcmp(name, "IntentLastTrace") == 0
                || strcmp(name, "IntentLastFailure") == 0
                || strcmp(name, "IntentLastName") == 0
                || strcmp(name, "IntentHistoryStepName") == 0
                || strcmp(name, "IntentHistoryStepZone") == 0
                || strcmp(name, "IntentHistoryStepPhase") == 0
                || strcmp(name, "IntentHistoryStepActor") == 0
                || strcmp(name, "IntentHistoryStepSlot") == 0
                || strcmp(name, "IntentHistoryStepFromZone") == 0
                || strcmp(name, "IntentHistoryStepFromSlot") == 0
                || strcmp(name, "IntentHistoryStepToZone") == 0
                || strcmp(name, "IntentHistoryStepToSlot") == 0
                || strcmp(name, "IntentActiveName") == 0
                || strcmp(name, "IntentActiveTrace") == 0)
                return "String";
            if (strcmp(name, "IntentLastHandle") == 0
                || strcmp(name, "IntentLastTraceId") == 0
                || strcmp(name, "IntentLastStepCount") == 0
                || strcmp(name, "IntentHistoryCount") == 0
                || strcmp(name, "IntentActiveCount") == 0
                || strcmp(name, "IntentActiveHandle") == 0
                || strcmp(name, "IntentActiveTraceId") == 0
                || strcmp(name, "IntentActivePriority") == 0)
                return "Int";
            if (strcmp(name, "ListNew") == 0)
                return "List";
            if (strcmp(name, "ListGet") == 0 && expr->data.call.arg_count >= 1) {
                const char *list_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                    return slot_inner_type_name(list_type);
                return "Int";
            }
            if (strcmp(name, "ListSize") == 0)
                return "Int";
            if (strcmp(name, "SetNew") == 0)
                return "Set";
            if (strcmp(name, "QueueNew") == 0)
                return "Queue";
            if (strcmp(name, "FsmNew") == 0)
                return "Fsm";
            if (strcmp(name, "TimerNew") == 0)
                return "Timer";
            if (strcmp(name, "CooldownNew") == 0)
                return "Cooldown";
            if (strcmp(name, "ClaimQubit") == 0)
                return "QubitSlot";
            if (strcmp(name, "ClaimDeviceSlot") == 0)
                return "DeviceSlot<Int>";
            if (strcmp(name, "ViewRead") == 0 && expr->data.call.arg_count >= 1) {
                static char read_view[128];
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                snprintf(read_view, sizeof(read_view), "ReadView<%s>",
                    slot_inner_type_name(slot_type));
                return read_view;
            }
            if (strcmp(name, "ViewWrite") == 0 && expr->data.call.arg_count >= 1) {
                static char write_view[128];
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                snprintf(write_view, sizeof(write_view), "WriteView<%s>",
                    slot_inner_type_name(slot_type));
                return write_view;
            }
            if (strcmp(name, "Measure") == 0 || strcmp(name, "QubitState") == 0)
                return "Int";
            if (strcmp(name, "Read") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "Slot<", 5) == 0
                    || strncmp(slot_type, "SecureSlot<", 11) == 0
                    || strncmp(slot_type, "ReadView<", 9) == 0
                    || strncmp(slot_type, "WriteView<", 10) == 0
                    || strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    return slot_inner_type_name(slot_type);
                }
            }
            if ((strcmp(name, "Write") == 0 || strcmp(name, "Release") == 0)
                && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "Slot<", 5) == 0
                    || strncmp(slot_type, "SecureSlot<", 11) == 0
                    || strncmp(slot_type, "ReadView<", 9) == 0
                    || strncmp(slot_type, "WriteView<", 10) == 0
                    || strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    return "Void";
                }
            }
            if (strcmp(name, "DeviceRead") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "DeviceSlot<", 11) == 0)
                    return slot_inner_type_name(slot_type);
            }
            if (strcmp(name, "SubmitDeviceRead") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    static char device_future[128];
                    snprintf(device_future, sizeof(device_future), "RemoteFuture<%s>",
                        slot_inner_type_name(slot_type));
                    return device_future;
                }
                return "RemoteFuture<Int>";
            }
            if (strcmp(name, "IsCollapsed") == 0
                || strcmp(name, "IntoClassical") == 0)
                return "Bool";
            if (strcmp(name, "H") == 0
                || strcmp(name, "ArrayPush") == 0
                || strcmp(name, "ArraySet") == 0
                || strcmp(name, "ArrayPop") == 0
                || strcmp(name, "ChannelClose") == 0)
                return "Void";
            if ((strcmp(name, "TryRecv") == 0
                 || strcmp(name, "RecvTimeout") == 0
                 || strcmp(name, "TrySendStatus") == 0
                 || strcmp(name, "SendTimeoutStatus") == 0)
                && expr->data.call.arg_count >= 1) {
                static char opt_buf[128];
                if (strcmp(name, "TrySendStatus") == 0
                    || strcmp(name, "SendTimeoutStatus") == 0) {
                    snprintf(opt_buf, sizeof(opt_buf), "Option<Bool>");
                } else {
                    const char *inner = channel_inner_type_name(ctx,
                        expr->data.call.arguments[0]);
                    snprintf(opt_buf, sizeof(opt_buf), "Option<%s>", inner);
                }
                return opt_buf;
            }
            if (strcmp(name, "TrySend") == 0
                || strcmp(name, "SendTimeout") == 0
                || strcmp(name, "Cancel") == 0
                || strcmp(name, "IsCancelled") == 0
                || strcmp(name, "IntentLastFailed") == 0
                || strcmp(name, "IntentHistoryStepOk") == 0
                || strcmp(name, "IntentActiveConcurrent") == 0
                || strcmp(name, "MapHas") == 0
                || strcmp(name, "HasProjection") == 0
                || strcmp(name, "HasLayer") == 0
                || strcmp(name, "HasState") == 0
                || strcmp(name, "HasZone") == 0
                || strcmp(name, "HasZoneProjection") == 0
                || strcmp(name, "HasZoneLayer") == 0
                || strcmp(name, "HasZoneState") == 0
                || strcmp(name, "ChannelFull") == 0
                || strcmp(name, "ChannelClosed") == 0
                || strcmp(name, "ChannelReady") == 0)
                return "Bool";
            if (strcmp(name, "ChannelLength") == 0
                || strcmp(name, "ChannelCapacity") == 0
                || strcmp(name, "ChannelSpace") == 0)
                return "Int";
            if (strcmp(name, "Some") == 0 && expr->data.call.arg_count == 1) {
                static char opt_buf[128];
                const char *inner = infer_expression_type_name(ctx, expr->data.call.arguments[0]);
                snprintf(opt_buf, sizeof(opt_buf), "Option<%s>", inner);
                return opt_buf;
            }
            if (strcmp(name, "None") == 0)
                return "Option<Int>";
            if ((strcmp(name, "IsSome") == 0 || strcmp(name, "IsNone") == 0)
                && expr->data.call.arg_count == 1)
                return "Bool";
            if (strcmp(name, "UnwrapOption") == 0 && expr->data.call.arg_count == 1) {
                const char *opt_type = infer_expression_type_name(ctx, expr->data.call.arguments[0]);
                if (strncmp(opt_type, "Option<", 7) == 0)
                    return slot_inner_type_name(opt_type);
            }
            if (strcmp(name, "ToDto") == 0 && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER
                && expr->data.call.arguments[0]->data.identifier.name != NULL) {
                return expr->data.call.arguments[0]->data.identifier.name;
            }
            if (strcmp(name, "ToObject") == 0 && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER
                && expr->data.call.arguments[0]->data.identifier.name != NULL) {
                return expr->data.call.arguments[0]->data.identifier.name;
            }
            if (find_class_decl(ctx, name) != NULL
                || find_actor_decl(ctx, name) != NULL
                || find_relation_decl(ctx, name) != NULL
                || find_effect_decl(ctx, name) != NULL
                || find_zone_decl(ctx, name) != NULL
                || find_world_decl(ctx, name) != NULL) {
                return name;
            }
            if (find_intent_decl(ctx, name) != NULL)
                return "Bool";

            {
                ASTNode *host_method = current_host_method_decl(ctx, name);
                if (host_method != NULL
                    && host_method->type == AST_FUNC_DECL
                    && host_method->data.func_decl.return_type != NULL) {
                    char *resolved = render_type_name(host_method->data.func_decl.return_type);
                    snprintf(inferred_call_type, sizeof(inferred_call_type), "%s", resolved);
                    free(resolved);
                    return inferred_call_type;
                }
            }

            {
                ASTNode *decl = find_function_decl(ctx, name);
                if (decl != NULL && decl->data.func_decl.return_type != NULL) {
                    char *resolved = NULL;
                    if (func_has_generic_params(decl)) {
                        GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
                        size_t binding_count = 0;
                        if (infer_generic_call_bindings(ctx, decl, expr, bindings, &binding_count))
                            resolved = render_type_name_with_bindings(ctx,
                                decl->data.func_decl.return_type, bindings, binding_count);
                    }
                    if (resolved == NULL)
                        resolved = render_type_name(decl->data.func_decl.return_type);
                    snprintf(inferred_call_type, sizeof(inferred_call_type), "%s", resolved);
                    free(resolved);
                    return inferred_call_type;
                }
            }
        }
        return "Int";
    default:
        return "Int";
    }
}

static char *
infer_spawn_return_type_name(TranspilerCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = spawn_expr != NULL ? spawn_expr->data.spawn_expr.function : NULL;
    const char *function_name = NULL;
    ASTNode *call = NULL;

    if (target == NULL)
        return pergyra_strdup("Void");

    if (target->type == AST_CALL
        && target->data.call.callee != NULL
        && target->data.call.callee->type == AST_IDENTIFIER) {
        call = target;
        function_name = target->data.call.callee->data.identifier.name;
    } else if (target->type == AST_IDENTIFIER) {
        function_name = target->data.identifier.name;
    } else if (target->type == AST_FUNC_DECL) {
        if (target->data.func_decl.return_type != NULL)
            return render_type_name(target->data.func_decl.return_type);
        return pergyra_strdup("Void");
    }

    if (function_name == NULL)
        return pergyra_strdup("Void");

    ASTNode *decl = find_function_decl(ctx, function_name);
    if (decl != NULL && decl->data.func_decl.return_type != NULL) {
        if (call != NULL && func_has_generic_params(decl)) {
            GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
            size_t binding_count = 0;
            if (infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count))
                return render_type_name_with_bindings(ctx, decl->data.func_decl.return_type,
                    bindings, binding_count);
        }
        return render_type_name(decl->data.func_decl.return_type);
    }

    return pergyra_strdup("Void");
}

static bool
is_remote_future_expr(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL) return false;
    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        return type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0;
    }
    return false;
}

static const char *
lookup_future_inner_type(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL)
        return "Void";

    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Future<", 7) == 0)
            return slot_inner_type_name(type_name);
        if (type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0)
            return slot_inner_type_name(type_name);
    }

    if (expr->type == AST_SPAWN_EXPR) {
        char *inner = infer_spawn_return_type_name(ctx, expr);
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s", inner);
        free(inner);
        return buf;
    }

    return "Void";
}

static const char *
pergyra_ast_type_to_c(ASTNode *type_node)
{
    static char mapped[128];
    if (type_node == NULL)
        return "void";

    if (type_node->type == AST_EVENT_HANDLER_TYPE)
        return "void *";

    char *type_name = render_type_name(type_node);
    snprintf(mapped, sizeof(mapped), "%s", pergyra_type_to_c(type_name));
    free(type_name);
    return mapped;
}

static char *
pergyra_ast_typed_declarator(ASTNode *type_node, const char *name)
{
    if (type_node == NULL)
        return strdup_fmt("void %s", name != NULL ? name : "value");

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        char *ret_type = pergyra_strdup("void");
        CodeBuf *params = codebuf_create();
        char *result;

        if (type_node->data.event_handler_type.return_type != NULL) {
            const char *rendered = pergyra_ast_type_to_c(
                type_node->data.event_handler_type.return_type);
            free(ret_type);
            ret_type = pergyra_strdup(rendered);
        }

        if (params == NULL) {
            result = strdup_fmt("%s (*%s)(void)", ret_type,
                name != NULL ? name : "value");
            free(ret_type);
            return result;
        }

        if (type_node->data.event_handler_type.param_count == 0) {
            codebuf_write(params, "void");
        } else {
            for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    codebuf_write(params, ", ");
                codebuf_write(params, "%s",
                    pergyra_ast_type_to_c(
                        type_node->data.event_handler_type.param_types[i]));
            }
        }

        result = strdup_fmt("%s (*%s)(%s)", ret_type,
            name != NULL ? name : "value", params->data);
        codebuf_destroy(params);
        free(ret_type);
        return result;
    }

    return strdup_fmt("%s %s", pergyra_ast_type_to_c(type_node),
        name != NULL ? name : "value");
}

static bool
transpiler_can_forward_declare_type_early(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return true;
    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return true;

    const char *name = type_node->data.type.name;
    if (strcmp(name, "Int") == 0
        || strcmp(name, "Float") == 0
        || strcmp(name, "Bool") == 0
        || strcmp(name, "String") == 0
        || strcmp(name, "Char") == 0
        || strcmp(name, "Byte") == 0
        || strcmp(name, "Void") == 0
        || strcmp(name, "Qubit") == 0)
        return true;

    if (strcmp(name, "Result") == 0
        || strcmp(name, "Option") == 0
        || strcmp(name, "Slot") == 0
        || strcmp(name, "SecureSlot") == 0
        || strcmp(name, "DeviceSlot") == 0
        || strcmp(name, "RemoteFuture") == 0
        || strcmp(name, "Array") == 0
        || strcmp(name, "Slice") == 0
        || strcmp(name, "Channel") == 0
        || strcmp(name, "Box") == 0
        || strcmp(name, "Rc") == 0
        || strcmp(name, "Weak") == 0
        || strcmp(name, "Future") == 0)
        return true;

    return find_class_decl(ctx, name) != NULL;
}

static bool
transpiler_can_forward_declare_func_early(TranspilerCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    if (func->data.func_decl.generic_params != NULL
        && func->data.func_decl.generic_params->count > 0)
        return false;
    if (!transpiler_can_forward_declare_type_early(ctx,
            func->data.func_decl.return_type))
        return false;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *p = func->data.func_decl.params[i];
        if (p == NULL || p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_early(ctx, p->type))
            return false;
    }
    return true;
}

static bool
transpiler_can_forward_declare_type_after_zones(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (transpiler_can_forward_declare_type_early(ctx, type_node))
        return true;
    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL)
        return false;

    return transpiler_has_known_nominal_type(ctx, type_node->data.type.name);
}

static bool
transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    if (func->data.func_decl.generic_params != NULL
        && func->data.func_decl.generic_params->count > 0)
        return false;
    if (!transpiler_can_forward_declare_type_after_zones(ctx,
            func->data.func_decl.return_type))
        return false;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *p = func->data.func_decl.params[i];
        if (p == NULL || p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_after_zones(ctx, p->type))
            return false;
    }
    return true;
}

static void
append_mangled_type_name(CodeBuf *buf, const char *type_name)
{
    bool wrote = false;

    for (const unsigned char *p = (const unsigned char *)type_name; *p != '\0'; p++) {
        if ((*p >= 'a' && *p <= 'z')
            || (*p >= 'A' && *p <= 'Z')
            || (*p >= '0' && *p <= '9')) {
            codebuf_write(buf, "%c", *p);
            wrote = true;
        } else if (wrote && buf->len > 0 && buf->data[buf->len - 1] != '_') {
            codebuf_write(buf, "_");
        }
    }

    if (!wrote)
        codebuf_write(buf, "Type");
}

static int
find_generic_param_index(ASTNode *decl, const char *name)
{
    if (!func_has_generic_params(decl) || name == NULL)
        return -1;

    for (size_t i = 0; i < decl->data.func_decl.generic_params->count; i++) {
        GenericParam *param = decl->data.func_decl.generic_params->params[i];
        if (param != NULL && param->name != NULL && strcmp(param->name, name) == 0)
            return (int)i;
    }

    return -1;
}

static bool
infer_generic_call_bindings(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call,
                            GenericBindingEntry *bindings, size_t *binding_count)
{
    if (!func_has_generic_params(decl)
        || call == NULL
        || call->type != AST_CALL
        || bindings == NULL
        || binding_count == NULL) {
        return false;
    }

    size_t generic_count = decl->data.func_decl.generic_params->count;
    memset(bindings, 0, sizeof(GenericBindingEntry) * generic_count);

    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *param = decl->data.func_decl.generic_params->params[i];
        if (param != NULL && param->name != NULL) {
            strncpy(bindings[i].name, param->name, sizeof(bindings[i].name) - 1);
            bindings[i].name[sizeof(bindings[i].name) - 1] = '\0';
        }
    }

    for (size_t i = 0; i < decl->data.func_decl.param_count && i < call->data.call.arg_count; i++) {
        FuncParam *param = decl->data.func_decl.params[i];
        if (param == NULL || param->type == NULL || param->type->type != AST_TYPE)
            continue;
        if (param->type->data.type.generic_args != NULL)
            continue;

        int generic_index = find_generic_param_index(decl, param->type->data.type.name);
        if (generic_index < 0)
            continue;

        const char *arg_type = infer_expression_type_name(ctx, call->data.call.arguments[i]);
        if (arg_type == NULL)
            continue;

        if (bindings[generic_index].concrete_type[0] != '\0'
            && strcmp(bindings[generic_index].concrete_type, arg_type) != 0) {
            return false;
        }

        strncpy(bindings[generic_index].concrete_type, arg_type,
            sizeof(bindings[generic_index].concrete_type) - 1);
        bindings[generic_index].concrete_type[sizeof(bindings[generic_index].concrete_type) - 1] = '\0';
    }

    for (size_t i = 0; i < generic_count; i++) {
        if (bindings[i].name[0] == '\0' || bindings[i].concrete_type[0] == '\0')
            return false;
    }

    *binding_count = generic_count;
    return true;
}

static void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx);

static void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx);

static void
emit_hosted_method_forward_decl_named(const char *host_name, ASTNode *method,
                                      bool pointer_self, CodeBuf *buf,
                                      TranspilerCtx *ctx)
{
    if (host_name == NULL || method == NULL || buf == NULL || ctx == NULL
        || method->type != AST_FUNC_DECL)
        return;

    const char *method_name = method->data.func_decl.name;
    const char *ret_type = "void";
    if (method_name == NULL)
        return;
    if (method->data.func_decl.return_type != NULL)
        ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

    codebuf_write(buf, "\n%s\n%s_%s(%s%s",
                  ret_type, host_name, method_name, host_name,
                  pointer_self ? " *self" : " self");

    for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
        FuncParam *p = method->data.func_decl.params[j];
        if (p == NULL || p->name == NULL)
            continue;
        if (strcmp(p->name, "self") == 0)
            continue;

        const char *pt = "int32_t";
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        {
            char *ptn = (p->type != NULL) ? render_type_name(p->type) : NULL;
            bool subj_param = ptn != NULL && is_pointer_self_host_type_name(ctx, ptn);
            if (subj_param)
                codebuf_write(buf, ", %s *%s", pt, p->name);
            else
                codebuf_write(buf, ", %s %s", pt, p->name);
            free(ptn);
        }
    }
    codebuf_write(buf, ");\n");
}

static const char *
ensure_generic_specialization(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call)
{
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;

    if (!infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count))
        return NULL;

    CodeBuf *name_buf = codebuf_create();
    if (name_buf == NULL)
        return NULL;

    codebuf_write(name_buf, "%s", decl->data.func_decl.name);
    for (size_t i = 0; i < binding_count; i++) {
        codebuf_write(name_buf, "_");
        append_mangled_type_name(name_buf, bindings[i].concrete_type);
    }

    for (int i = 0; i < ctx->generic_specialization_count; i++) {
        GenericSpecializationEntry *entry = &ctx->generic_specializations[i];
        if (entry->func_decl == decl
            && strcmp(entry->specialized_name, name_buf->data) == 0) {
            codebuf_destroy(name_buf);
            return entry->specialized_name;
        }
    }

    if (ctx->generic_specialization_count >= MAX_GENERIC_SPECIALIZATIONS) {
        codebuf_destroy(name_buf);
        return NULL;
    }

    GenericSpecializationEntry *entry =
        &ctx->generic_specializations[ctx->generic_specialization_count++];
    memset(entry, 0, sizeof(*entry));
    entry->func_decl = decl;
    strncpy(entry->specialized_name, name_buf->data,
        sizeof(entry->specialized_name) - 1);
    entry->specialized_name[sizeof(entry->specialized_name) - 1] = '\0';
    entry->emitting = true;
    codebuf_destroy(name_buf);

    int saved_binding_count = ctx->generic_binding_count;
    for (size_t i = 0; i < binding_count && ctx->generic_binding_count < MAX_GENERIC_BINDINGS; i++) {
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];
    }

    emit_func_forward_decl_named(decl, entry->specialized_name, ctx->decls, ctx);
    emit_func_decl_named(decl, entry->specialized_name, ctx->helpers, ctx);

    ctx->generic_binding_count = saved_binding_count;
    entry->emitting = false;
    return entry->specialized_name;
}

/* -----------------------------------------------------------------
 * Expression emitters — return heap-allocated C expression string
 * ----------------------------------------------------------------- */

static char *
escape_c_string_literal(const char *src)
{
    size_t len = 0;
    size_t i;
    size_t j = 0;
    char *out;

    if (src == NULL)
        return pergyra_strdup("");

    for (i = 0; src[i] != '\0'; i++) {
        switch (src[i]) {
        case '\n':
        case '\r':
        case '\t':
        case '\\':
        case '"':
            len += 2;
            break;
        default:
            len += 1;
            break;
        }
    }

    out = (char *)malloc(len + 1);
    if (out == NULL)
        return pergyra_strdup("");

    for (i = 0; src[i] != '\0'; i++) {
        switch (src[i]) {
        case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
        case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
        case '\t': out[j++] = '\\'; out[j++] = 't'; break;
        case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
        case '"': out[j++] = '\\'; out[j++] = '"'; break;
        default: out[j++] = src[i]; break;
        }
    }
    out[j] = '\0';
    return out;
}

static char *
strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0)
        return pergyra_strdup("");

    char *s = malloc((size_t)n + 1);
    if (s == NULL)
        return pergyra_strdup("");

    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

#include "transpiler_expr_emitters.inc"
