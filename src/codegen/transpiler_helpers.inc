static const char *infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr);
static char *escape_c_string_literal(const char *src);
static char *render_type_name(ASTNode *type_node);

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
        || decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL))
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
        /* obj.field — resolve obj type, then look up field type in that class */
        const char *obj_type = NULL;
        if (expr->data.member.object->type == AST_IDENTIFIER)
            obj_type = lookup_typed_var(ctx, expr->data.member.object->data.identifier.name);
        if (obj_type != NULL) {
            ASTNode *obj_decl = find_class_decl(ctx, obj_type);
            if (obj_decl != NULL) {
                for (size_t i = 0; i < obj_decl->data.class_decl.field_count; i++) {
                    ClassField *f = obj_decl->data.class_decl.fields[i];
                    if (f != NULL && f->name != NULL && f->type != NULL
                        && strcmp(f->name, expr->data.member.name) == 0) {
                        char *ft = render_type_name(f->type);
                        if (ft != NULL) {
                            /* Copy to static buffer to match return convention */
                            static char field_type_buf[128];
                            snprintf(field_type_buf, sizeof(field_type_buf), "%s", ft);
                            free(ft);
                            return field_type_buf;
                        }
                    }
                }
            }
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

const char *
pergyra_type_to_c(const char *name)
{
    if (strcmp(name, "Allocator") == 0)
        return "PgyAllocator";
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
    case AST_CALL:
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_IDENTIFIER
            && expr->data.call.callee->data.identifier.name != NULL) {
            const char *name = expr->data.call.callee->data.identifier.name;
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
                || strcmp(name, "HasProjection") == 0
                || strcmp(name, "HasLayer") == 0
                || strcmp(name, "HasState") == 0
                || strcmp(name, "HasZone") == 0
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

    char *type_name = render_type_name(type_node);
    snprintf(mapped, sizeof(mapped), "%s", pergyra_type_to_c(type_name));
    free(type_name);
    return mapped;
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

/* -----------------------------------------------------------------
 * Built-in call emitters
 * ----------------------------------------------------------------- */

char *
emit_builtin_claim_slot(ASTNode *call, TranspilerCtx *ctx)
{
    /*
     * ClaimSlot<T>() is handled in let_decl where the type is known.
     * If encountered standalone, emit a comment.
     */
    (void)call;
    (void)ctx;
    return pergyra_strdup("/* ClaimSlot */");
}

static char *
emit_builtin_claim_device_slot(ASTNode *call, TranspilerCtx *ctx)
{
    (void)call;
    (void)ctx;
    return pergyra_strdup("pgy_claim_device_Int()");
}

char *
emit_builtin_write(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 2)
        return pergyra_strdup("/* Write: missing args */");

    /* Resolve slot inner type from tracking table */
    const char *inner = "Int";
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = call->data.call.arguments[0];
    (void)resolve_slot_target(ctx, slot_arg, &inner, &slot_name, &secure);

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    char *value_expr = emit_expression(call->data.call.arguments[1], ctx);

    char *result;
    if (call->data.call.arg_count >= 3) {
        /* SecureSlot: Write(slot, value, token) */
        char *token_expr = emit_expression(call->data.call.arguments[2], ctx);
        result = strdup_fmt(
            "pgy_secure_write_%s(%s, %s, &%s)",
            inner, slot_ref, value_expr, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        result = strdup_fmt(
            "pgy_secure_write_%s(%s, %s, &%s_token)",
            inner, slot_ref, value_expr, slot_name);
    } else {
        /* Plain slot: Write(slot, value) */
        result = strdup_fmt(
            "pgy_write_%s(%s, %s)",
            inner, slot_ref, value_expr);
    }

    free(slot_ref);
    free(slot_expr);
    free(value_expr);
    return result;
}

static char *
emit_builtin_view(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 1)
        return pergyra_strdup("/* View: missing args */");

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(call->data.call.arguments[0], ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    return slot_expr;
}

static bool
resolve_slot_target(TranspilerCtx *ctx, ASTNode *slot_arg,
                    const char **inner_out, const char **slot_name_out,
                    bool *secure_out)
{
    const char *inner = "Int";
    const char *slot_name = NULL;
    bool secure = false;

    if (slot_arg == NULL)
        return false;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *id = slot_arg->data.identifier.name;
        TypedVarEntry *entry = lookup_typed_entry(ctx, id);
        if (entry != NULL && (entry->is_view || entry->is_move_token)
            && entry->source_slot[0] != '\0') {
            slot_name = entry->source_slot;
            secure = entry->source_secure;
            inner = slot_inner_type_name(entry->type_name);
        } else {
            slot_name = id;
            inner = lookup_slot_type(ctx, id);
            secure = lookup_slot_is_secure(ctx, id);
        }
    } else if (slot_arg->type == AST_CALL
               && slot_arg->data.call.callee != NULL
               && slot_arg->data.call.callee->type == AST_IDENTIFIER
               && slot_arg->data.call.arg_count >= 1
               && slot_arg->data.call.arguments[0] != NULL
               && slot_arg->data.call.arguments[0]->type == AST_IDENTIFIER) {
        const char *callee = slot_arg->data.call.callee->data.identifier.name;
        const char *src = slot_arg->data.call.arguments[0]->data.identifier.name;
        if (callee != NULL
            && (strcmp(callee, "ViewRead") == 0
                || strcmp(callee, "ViewWrite") == 0
                || strcmp(callee, "Move") == 0)) {
            slot_name = src;
            inner = lookup_slot_type(ctx, src);
            secure = lookup_slot_is_secure(ctx, src);
        }
    }

    if (inner == NULL)
        inner = "Int";
    if (inner_out != NULL)
        *inner_out = inner;
    if (slot_name_out != NULL)
        *slot_name_out = slot_name;
    if (secure_out != NULL)
        *secure_out = secure;
    return slot_name != NULL;
}

char *
emit_builtin_read(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 1)
        return pergyra_strdup("/* Read: missing args */");

    /* Resolve slot inner type from tracking table */
    const char *inner = "Int";
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = call->data.call.arguments[0];
    (void)resolve_slot_target(ctx, slot_arg, &inner, &slot_name, &secure);

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    char *result;

    if (call->data.call.arg_count >= 2) {
        char *token_expr = emit_expression(call->data.call.arguments[1], ctx);
        result = strdup_fmt(
            "pgy_secure_read_%s(%s, &%s)",
            inner, slot_ref, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        result = strdup_fmt("pgy_secure_read_%s(%s, &%s_token)",
            inner, slot_ref, slot_name);
    } else {
        result = strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
    }

    free(slot_ref);
    free(slot_expr);
    return result;
}

char *
emit_builtin_release(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 1)
        return pergyra_strdup("/* Release: missing args */");

    /* Resolve slot inner type from tracking table */
    const char *inner = "Int";
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = call->data.call.arguments[0];
    (void)resolve_slot_target(ctx, slot_arg, &inner, &slot_name, &secure);

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    char *result;

    if (call->data.call.arg_count >= 2) {
        char *token_expr = emit_expression(call->data.call.arguments[1], ctx);
        result = strdup_fmt(
            "pgy_secure_release_%s(%s, &%s)",
            inner, slot_ref, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        result = strdup_fmt("pgy_secure_release_%s(%s, &%s_token)",
            inner, slot_ref, slot_name);
    } else {
        result = strdup_fmt("pgy_release_%s(%s)", inner, slot_ref);
    }

    /* Mark slot as explicitly released — prevents auto-release at scope exit */
    if (slot_arg->type == AST_IDENTIFIER) {
        const char *sname = slot_name != NULL ? slot_name : slot_arg->data.identifier.name;
        for (int i = 0; i < ctx->slot_var_count; i++) {
            if (strcmp(ctx->slot_vars[i].name, sname) == 0) {
                ctx->slot_vars[i].released = true;
                break;
            }
        }
    }

    free(slot_ref);
    free(slot_expr);
    return result;
}

static char *
emit_builtin_device_write(ASTNode *call, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *value_expr = emit_expression(call->data.call.arguments[1], ctx);
    char *result;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, slot_arg->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "DeviceSlot<", 11) == 0)
            inner = slot_inner_type_name(type_name);
    }

    result = strdup_fmt("pgy_device_write_%s(&%s, %s)", inner, slot_expr, value_expr);
    free(slot_expr);
    free(value_expr);
    return result;
}

static char *
emit_builtin_device_read(ASTNode *call, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *result;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, slot_arg->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "DeviceSlot<", 11) == 0)
            inner = slot_inner_type_name(type_name);
    }

    result = strdup_fmt("pgy_device_read_%s(&%s)", inner, slot_expr);
    free(slot_expr);
    return result;
}

static char *
emit_builtin_release_device_slot(ASTNode *call, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *result;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, slot_arg->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "DeviceSlot<", 11) == 0)
            inner = slot_inner_type_name(type_name);
    }

    result = strdup_fmt("pgy_release_device_%s(&%s)", inner, slot_expr);
    free(slot_expr);
    return result;
}

static char *
emit_builtin_submit_device_read(ASTNode *call, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *slot_arg = call->data.call.arguments[0];
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *result;

    if (slot_arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, slot_arg->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "DeviceSlot<", 11) == 0)
            inner = slot_inner_type_name(type_name);
    }

    result = strdup_fmt("pgy_submit_device_read_%s(&%s)", inner, slot_expr);
    free(slot_expr);
    return result;
}

char *
emit_builtin_log(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count == 0)
        return pergyra_strdup("printf(\"\\n\")");

    if (call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_log(%s)", arg);
        free(arg);
        return result;
    }

    /* Multi-arg Log: emit each argument with pgy_log() */
    CodeBuf *buf = codebuf_create();
    codebuf_write(buf, "do { ");
    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        char *arg = emit_expression(call->data.call.arguments[i], ctx);
        codebuf_write(buf, "pgy_log(%s); ", arg);
        free(arg);
    }
    codebuf_write(buf, "} while(0)");
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static const char *
lookup_wrapped_inner_type(TranspilerCtx *ctx, ASTNode *arg, const char *wrapper)
{
    if (arg != NULL && arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, arg->data.identifier.name);
        size_t wrapper_len = strlen(wrapper);
        if (type_name != NULL && strncmp(type_name, wrapper, wrapper_len) == 0
            && type_name[wrapper_len] == '<') {
            return slot_inner_type_name(type_name);
        }
    }
    return "Int";
}

char *
emit_builtin_rc(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *arg = call->data.call.arg_count > 0 ? call->data.call.arguments[0] : NULL;

    switch (kind) {
    case BUILTIN_RC_NEW:
        if (call->data.call.arg_count != 1)
            return pergyra_strdup("/* RcNew: invalid args */");
        if (arg->type == AST_NUMBER) inner = "Int";
        else if (arg->type == AST_STRING) inner = "String";
        else if (arg->type == AST_BOOLEAN) inner = "Bool";
        else if (arg->type == AST_IDENTIFIER) {
            const char *arg_type = lookup_typed_var(ctx, arg->data.identifier.name);
            if (arg_type != NULL)
                inner = arg_type;
        }
        break;
    case BUILTIN_RC_CLONE:
    case BUILTIN_RC_DROP:
    case BUILTIN_RC_GET:
    case BUILTIN_RC_DOWNGRADE:
        inner = lookup_wrapped_inner_type(ctx, arg, "Rc");
        break;
    case BUILTIN_WEAK_UPGRADE:
    case BUILTIN_WEAK_DROP:
        inner = lookup_wrapped_inner_type(ctx, arg, "Weak");
        break;
    default:
        break;
    }

    if (kind == BUILTIN_RC_NEW) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_new_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_CLONE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_clone_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_GET) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("(*pgy_rc_get_%s(&%s))", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_DOWNGRADE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_downgrade_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_WEAK_UPGRADE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_weak_upgrade_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_WEAK_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_weak_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    return pergyra_strdup("/* unsupported rc builtin */");
}

char *
emit_builtin_box(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    const char *inner = "Int";
    ASTNode *arg = call->data.call.arg_count > 0 ? call->data.call.arguments[0] : NULL;

    switch (kind) {
    case BUILTIN_BOX:
        if (call->data.call.arg_count != 1)
            return pergyra_strdup("/* Box: invalid args */");
        if (arg->type == AST_NUMBER) inner = "Int";
        else if (arg->type == AST_STRING) inner = "String";
        else if (arg->type == AST_BOOLEAN) inner = "Bool";
        else if (arg->type == AST_CALL
                 && arg->data.call.callee != NULL
                 && arg->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee_name = arg->data.call.callee->data.identifier.name;
            ASTNode *class_decl = find_class_decl(ctx, callee_name);
            if (class_decl != NULL && class_decl->type == AST_CLASS_DECL)
                inner = callee_name;
            else {
                const char *arg_type = infer_expression_type_name(ctx, arg);
                if (arg_type != NULL)
                    inner = arg_type;
            }
        }
        else if (arg->type == AST_IDENTIFIER) {
            const char *arg_type = lookup_typed_var(ctx, arg->data.identifier.name);
            if (arg_type != NULL)
                inner = arg_type;
        } else {
            const char *arg_type = infer_expression_type_name(ctx, arg);
            if (arg_type != NULL)
                inner = arg_type;
        }
        break;
    case BUILTIN_BOX_GET:
    case BUILTIN_BOX_SET:
    case BUILTIN_BOX_DROP:
    case BUILTIN_BOX_IS_VALID:
        inner = lookup_wrapped_inner_type(ctx, arg, "Box");
        break;
    default:
        break;
    }

    if (kind == BUILTIN_BOX) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_new_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_GET) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_get_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_SET) {
        char *box_expr = emit_expression(arg, ctx);
        char *value = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_box_set_%s(&%s, %s)", inner, box_expr, value);
        free(box_expr);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_IS_VALID) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_is_valid_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    return pergyra_strdup("/* unsupported box builtin */");
}

char *
emit_builtin_allocator(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    switch (kind) {
    case BUILTIN_ALLOCATOR_SYSTEM:
        return pergyra_strdup("pgy_allocator_system()");
    case BUILTIN_ALLOCATOR_TRACING:
        return pergyra_strdup("pgy_allocator_tracing()");
    case BUILTIN_ALLOCATOR_DEBUG:
        return pergyra_strdup("pgy_allocator_debug()");
    case BUILTIN_ALLOCATOR_POOL:
        if (call->data.call.arg_count != 1)
            return pergyra_strdup("/* AllocatorPool: invalid args */");
        {
            char *cap = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("pgy_allocator_pool(%s)", cap);
            free(cap);
            return result;
        }
    default:
        return pergyra_strdup("/* unsupported allocator builtin */");
    }
}

/* -----------------------------------------------------------------
 * Token operator helpers
 * ----------------------------------------------------------------- */

static const char *
binary_op_to_c(TokenType op)
{
    switch (op) {
    case TOKEN_PLUS:          return "+";
    case TOKEN_MINUS:         return "-";
    case TOKEN_STAR:          return "*";
    case TOKEN_SLASH:         return "/";
    case TOKEN_PERCENT:       return "%";
    case TOKEN_EQUAL:         return "==";
    case TOKEN_NOT_EQUAL:     return "!=";
    case TOKEN_LESS:          return "<";
    case TOKEN_LESS_EQUAL:    return "<=";
    case TOKEN_GREATER:       return ">";
    case TOKEN_GREATER_EQUAL: return ">=";
    case TOKEN_AND:           return "&&";
    case TOKEN_OR:            return "||";
    default:                  return "?";
    }
}

/* -----------------------------------------------------------------
 * Expression emitters
 * ----------------------------------------------------------------- */

char *
emit_binary(ASTNode *expr, TranspilerCtx *ctx)
{
    /* String concatenation: "a" + "b" → StringConcat(a, b)
     * Walk the left spine of chained + to find the leftmost leaf;
     * if that leaf is String, the whole chain is string concat. */
    if (expr->data.binary.op.type == TOKEN_PLUS) {
        const char *lt = infer_expression_type_name(ctx, expr->data.binary.left);
        bool is_string = (lt != NULL && strcmp(lt, "String") == 0);
        if (!is_string) {
            /* Walk left spine of + chain to find leftmost operand */
            ASTNode *cursor = expr->data.binary.left;
            while (cursor != NULL && cursor->type == AST_BINARY
                   && cursor->data.binary.op.type == TOKEN_PLUS) {
                cursor = cursor->data.binary.left;
            }
            if (cursor != NULL) {
                const char *leaf_t = infer_expression_type_name(ctx, cursor);
                if (leaf_t != NULL && strcmp(leaf_t, "String") == 0)
                    is_string = true;
            }
        }
        if (is_string) {
            char *left  = emit_expression(expr->data.binary.left,  ctx);
            char *right = emit_expression(expr->data.binary.right, ctx);
            char *result = strdup_fmt("StringConcat(%s, %s)", left, right);
            free(left);
            free(right);
            return result;
        }
    }

    {
        const char *lt = infer_expression_type_name(ctx, expr->data.binary.left);
        ASTNode *overload = find_operator_overload_decl(ctx, lt, expr->data.binary.op.type);
        if (overload != NULL) {
            char *left  = emit_expression(expr->data.binary.left, ctx);
            char *right = emit_expression(expr->data.binary.right, ctx);
            const char *suffix = operator_overload_suffix(expr->data.binary.op.type);
            char *result = strdup_fmt("operator_%s_%s(%s, %s)", suffix, lt, left, right);
            free(left);
            free(right);
            return result;
        }
    }

    char *left  = emit_expression(expr->data.binary.left,  ctx);
    char *right = emit_expression(expr->data.binary.right, ctx);
    const char *op = binary_op_to_c(expr->data.binary.op.type);
    char *result = strdup_fmt("(%s %s %s)", left, op, right);
    free(left);
    free(right);
    return result;
}

char *
emit_unary(ASTNode *expr, TranspilerCtx *ctx)
{
    /* Postfix ? — try/propagate: expr? → early return on error */
    if (expr->data.unary.op.type == TOKEN_QUESTION) {
        char *operand = emit_expression(expr->data.unary.operand, ctx);
        int try_id = ctx->tmp_counter++;
        char *result = strdup_fmt(
            "({ __typeof__(%s) __try_%d = %s; "
            "if (__try_%d.tag != PgyResultOk) return __try_%d; "
            "__try_%d.ok; })",
            operand, try_id, operand,
            try_id, try_id,
            try_id);
        free(operand);
        return result;
    }

    char *operand = emit_expression(expr->data.unary.operand, ctx);
    const char *op = (expr->data.unary.op.type == TOKEN_NOT) ? "!" : "-";
    char *result = strdup_fmt("(%s%s)", op, operand);
    free(operand);
    return result;
}

char *
emit_call(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode    *callee = call->data.call.callee;
    BuiltinKind bk     = BUILTIN_NOT_BUILTIN;

    if (callee->type == AST_IDENTIFIER) {
        bk = builtin_resolve(callee->data.identifier.name);
    }

    switch (bk) {
    case BUILTIN_CLAIM_SLOT:
    case BUILTIN_CLAIM_SECURE_SLOT:
        return emit_builtin_claim_slot(call, ctx);
    case BUILTIN_CLAIM_DEVICE_SLOT:
        return emit_builtin_claim_device_slot(call, ctx);
    case BUILTIN_VIEW_READ:
    case BUILTIN_VIEW_WRITE:
    case BUILTIN_MOVE:
        return emit_builtin_view(call, ctx);
    case BUILTIN_WRITE:
        return emit_builtin_write(call, ctx);
    case BUILTIN_READ:
        return emit_builtin_read(call, ctx);
    case BUILTIN_RELEASE:
        return emit_builtin_release(call, ctx);
    case BUILTIN_DEVICE_WRITE:
        return emit_builtin_device_write(call, ctx);
    case BUILTIN_DEVICE_READ:
        return emit_builtin_device_read(call, ctx);
    case BUILTIN_RELEASE_DEVICE_SLOT:
        return emit_builtin_release_device_slot(call, ctx);
    case BUILTIN_SUBMIT_DEVICE_READ:
        return emit_builtin_submit_device_read(call, ctx);
    case BUILTIN_LOG:
        return emit_builtin_log(call, ctx);
    case BUILTIN_RC_NEW:
    case BUILTIN_RC_CLONE:
    case BUILTIN_RC_DROP:
    case BUILTIN_RC_DOWNGRADE:
    case BUILTIN_RC_GET:
    case BUILTIN_WEAK_UPGRADE:
    case BUILTIN_WEAK_DROP:
        return emit_builtin_rc(call, bk, ctx);
    case BUILTIN_BOX:
    case BUILTIN_BOX_GET:
    case BUILTIN_BOX_SET:
    case BUILTIN_BOX_DROP:
    case BUILTIN_BOX_IS_VALID:
        return emit_builtin_box(call, bk, ctx);
    case BUILTIN_TO_OBJECT:
        return emit_builtin_to_dto(call, ctx);
    case BUILTIN_TO_DTO:
        return emit_builtin_to_dto(call, ctx);
    case BUILTIN_HAS_PROJECTION:
        if (call->data.call.arg_count == 1 && call->data.call.arguments[0] != NULL) {
            const char *slot_name = NULL;
            if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                slot_name = call->data.call.arguments[0]->data.identifier.name;
            else if (call->data.call.arguments[0]->type == AST_STRING)
                slot_name = call->data.call.arguments[0]->data.string.value;
            if (slot_name != NULL
                && (current_relation_has_projection_slot(ctx, slot_name)
                    || current_effect_has_projection_slot(ctx, slot_name)
                    || current_zone_has_projection_slot(ctx, slot_name))) {
                return strdup_fmt("self->__projection_ready_%s", slot_name);
            }
        }
        return pergyra_strdup("false /* HasProjection: domain-semantic query only */");
    case BUILTIN_HAS_LAYER:
        if (ctx->current_zone_name != NULL
            && call->data.call.arg_count == 1
            && call->data.call.arguments[0] != NULL) {
            const char *layer_name = NULL;
            if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                layer_name = call->data.call.arguments[0]->data.identifier.name;
            else if (call->data.call.arguments[0]->type == AST_STRING)
                layer_name = call->data.call.arguments[0]->data.string.value;
            if (layer_name != NULL)
                return strdup_fmt("self->__layer_active_%s", layer_name);
        }
        return pergyra_strdup("false /* HasLayer: zone-semantic query only */");
    case BUILTIN_HAS_STATE:
        if (ctx->current_zone_name != NULL
            && call->data.call.arg_count >= 1
            && call->data.call.arguments[0] != NULL) {
            ASTNode *zone_decl = find_zone_decl(ctx, ctx->current_zone_name);
            const char *state_name = NULL;
            ASTNode *state_decl;
            if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                state_name = call->data.call.arguments[0]->data.identifier.name;
            else if (call->data.call.arguments[0]->type == AST_STRING)
                state_name = call->data.call.arguments[0]->data.string.value;
            state_decl = find_zone_state_decl(zone_decl, state_name);
            if (state_decl != NULL && state_name != NULL)
                return strdup_fmt("self->__state_%s", state_name);
        }
        return pergyra_strdup("false /* HasState: zone-semantic query only */");
    case BUILTIN_HAS_ZONE:
        if (ctx->current_world_name != NULL
            && call->data.call.arg_count >= 1
            && call->data.call.arguments[0] != NULL) {
            ASTNode *world_decl = find_world_decl(ctx, ctx->current_world_name);
            const char *name = NULL;
            ASTNode *state_decl;
            if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                name = call->data.call.arguments[0]->data.identifier.name;
            else if (call->data.call.arguments[0]->type == AST_STRING)
                name = call->data.call.arguments[0]->data.string.value;
            state_decl = find_world_state_decl(world_decl, name);
            if (state_decl != NULL && name != NULL)
                return strdup_fmt("self->__zone_state_%s", name);
            if (world_has_zone_slot(world_decl, name))
                return strdup_fmt("self->__zone_active_%s", name);
        }
        return pergyra_strdup("false /* HasZone: world-semantic query only */");
    case BUILTIN_HAS_ZONE_PROJECTION:
        if (ctx->current_world_name != NULL
            && call->data.call.arg_count == 2
            && call->data.call.arguments[0] != NULL
            && call->data.call.arguments[1] != NULL) {
            ASTNode *world_decl = find_world_decl(ctx, ctx->current_world_name);
            const char *zone_name = NULL;
            const char *slot_name = NULL;
            ASTNode *zone_decl;
            if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                zone_name = call->data.call.arguments[0]->data.identifier.name;
            else if (call->data.call.arguments[0]->type == AST_STRING)
                zone_name = call->data.call.arguments[0]->data.string.value;
            if (call->data.call.arguments[1]->type == AST_IDENTIFIER)
                slot_name = call->data.call.arguments[1]->data.identifier.name;
            else if (call->data.call.arguments[1]->type == AST_STRING)
                slot_name = call->data.call.arguments[1]->data.string.value;
            zone_decl = resolve_world_zone_decl(ctx, world_decl, zone_name);
            if (zone_decl != NULL && slot_name != NULL
                && find_zone_domain_slot(zone_decl, slot_name) != NULL
                && !find_zone_domain_slot(zone_decl, slot_name)->data.domain_slot.is_subject) {
                return strdup_fmt("self->%s.__projection_ready_%s", zone_name, slot_name);
            }
        }
        return pergyra_strdup("false /* HasZoneProjection: world-semantic query only */");
    case BUILTIN_HAS_ZONE_LAYER:
        if (ctx->current_world_name != NULL
            && call->data.call.arg_count == 2
            && call->data.call.arguments[0] != NULL
            && call->data.call.arguments[1] != NULL) {
            ASTNode *world_decl = find_world_decl(ctx, ctx->current_world_name);
            const char *zone_name = NULL;
            const char *layer_name = NULL;
            ASTNode *zone_decl;
            if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                zone_name = call->data.call.arguments[0]->data.identifier.name;
            else if (call->data.call.arguments[0]->type == AST_STRING)
                zone_name = call->data.call.arguments[0]->data.string.value;
            if (call->data.call.arguments[1]->type == AST_IDENTIFIER)
                layer_name = call->data.call.arguments[1]->data.identifier.name;
            else if (call->data.call.arguments[1]->type == AST_STRING)
                layer_name = call->data.call.arguments[1]->data.string.value;
            zone_decl = resolve_world_zone_decl(ctx, world_decl, zone_name);
            if (zone_decl != NULL && layer_name != NULL
                && find_zone_layer_slot(zone_decl, layer_name) != NULL) {
                return strdup_fmt("self->%s.__layer_active_%s", zone_name, layer_name);
            }
        }
        return pergyra_strdup("false /* HasZoneLayer: world-semantic query only */");
    case BUILTIN_HAS_ZONE_STATE:
        if (ctx->current_world_name != NULL
            && call->data.call.arg_count == 2
            && call->data.call.arguments[0] != NULL
            && call->data.call.arguments[1] != NULL) {
            ASTNode *world_decl = find_world_decl(ctx, ctx->current_world_name);
            const char *zone_name = NULL;
            const char *state_name = NULL;
            ASTNode *zone_decl;
            if (call->data.call.arguments[0]->type == AST_IDENTIFIER)
                zone_name = call->data.call.arguments[0]->data.identifier.name;
            else if (call->data.call.arguments[0]->type == AST_STRING)
                zone_name = call->data.call.arguments[0]->data.string.value;
            if (call->data.call.arguments[1]->type == AST_IDENTIFIER)
                state_name = call->data.call.arguments[1]->data.identifier.name;
            else if (call->data.call.arguments[1]->type == AST_STRING)
                state_name = call->data.call.arguments[1]->data.string.value;
            zone_decl = resolve_world_zone_decl(ctx, world_decl, zone_name);
            if (zone_decl != NULL && state_name != NULL
                && find_zone_state_decl(zone_decl, state_name) != NULL) {
                return strdup_fmt("self->%s.__state_%s", zone_name, state_name);
            }
        }
        return pergyra_strdup("false /* HasZoneState: world-semantic query only */");
    case BUILTIN_ALLOCATOR_SYSTEM:
    case BUILTIN_ALLOCATOR_TRACING:
    case BUILTIN_ALLOCATOR_DEBUG:
    case BUILTIN_ALLOCATOR_POOL:
        return emit_builtin_allocator(call, bk, ctx);
    /* I/O built-ins */
    case BUILTIN_FILE_OPEN: {
        /* FileOpen(path, mode) → pgy_file_open(path, mode) */
        if (call->data.call.arg_count < 2)
            return pergyra_strdup("/* FileOpen: need path, mode */");
        char *path = emit_expression(call->data.call.arguments[0], ctx);
        char *mode = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_file_open(%s, %s)", path, mode);
        free(path); free(mode);
        return result;
    }
    case BUILTIN_FILE_READ: {
        /* FileRead(fd) → pgy_file_read(fd) */
        if (call->data.call.arg_count < 1)
            return pergyra_strdup("/* FileRead: need fd */");
        char *fd = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_file_read(%s)", fd);
        free(fd);
        return result;
    }
    case BUILTIN_FILE_WRITE: {
        /* FileWrite(fd, data) → pgy_file_write(fd, data) */
        if (call->data.call.arg_count < 2)
            return pergyra_strdup("/* FileWrite: need fd, data */");
        char *fd   = emit_expression(call->data.call.arguments[0], ctx);
        char *data = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_file_write(%s, %s)", fd, data);
        free(fd); free(data);
        return result;
    }
    case BUILTIN_FILE_CLOSE: {
        /* FileClose(fd) → pgy_file_close(fd) */
        if (call->data.call.arg_count < 1)
            return pergyra_strdup("/* FileClose: need fd */");
        char *fd = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_file_close(%s)", fd);
        free(fd);
        return result;
    }
    case BUILTIN_READ_FILE: {
        /* ReadFile(path) → pgy_read_file(path) */
        if (call->data.call.arg_count < 1)
            return pergyra_strdup("/* ReadFile: need path */");
        char *path = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_read_file(%s)", path);
        free(path);
        return result;
    }
    case BUILTIN_WRITE_FILE: {
        /* WriteFile(path, data) → pgy_write_file(path, data) */
        if (call->data.call.arg_count < 2)
            return pergyra_strdup("/* WriteFile: need path, data */");
        char *path = emit_expression(call->data.call.arguments[0], ctx);
        char *data = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_write_file(%s, %s)", path, data);
        free(path); free(data);
        return result;
    }
    case BUILTIN_INPUT: {
        /* Input(prompt) → pgy_input(prompt) */
        char *prompt = (call->data.call.arg_count >= 1)
            ? emit_expression(call->data.call.arguments[0], ctx)
            : pergyra_strdup("\"\"");
        char *result = strdup_fmt("pgy_input(%s)", prompt);
        free(prompt);
        return result;
    }
    case BUILTIN_PRINT: {
        /* Print(msg) → pgy_print(msg) */
        if (call->data.call.arg_count < 1)
            return pergyra_strdup("/* Print: need msg */");
        char *msg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_print(%s)", msg);
        free(msg);
        return result;
    }
    default:
        break;
    }

    /* Tagged union variant constructors: Circle(42) → Shape_Circle(42) */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;
        ASTNode *class_decl = find_class_decl(ctx, fn);
        ASTNode *actor_decl = find_actor_decl(ctx, fn);
        if (class_decl != NULL && class_decl->type == AST_CLASS_DECL) {
            size_t argc = call->data.call.arg_count;
            CodeBuf *fields = codebuf_create();
            for (size_t i = 0; i < argc && i < class_decl->data.class_decl.field_count; i++) {
                ClassField *field = class_decl->data.class_decl.fields[i];
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(fields, ", ");
                codebuf_write(fields, ".%s = %s",
                    field != NULL && field->name != NULL ? field->name : "field",
                    arg != NULL ? arg : "0");
                free(arg);
            }
            char *result;
            if (fields->len > 0)
                result = strdup_fmt("(%s){ %s }", fn, fields->data);
            else
                result = strdup_fmt("(%s){0}", fn);
            codebuf_destroy(fields);
            return result;
        }
        if (actor_decl != NULL && actor_decl->type == AST_ACTOR_DECL) {
            size_t argc = call->data.call.arg_count;
            CodeBuf *fields = codebuf_create();
            for (size_t i = 0; i < argc && i < actor_decl->data.actor_decl.field_count; i++) {
                ClassField *field = actor_decl->data.actor_decl.fields[i];
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(fields, ", ");
                codebuf_write(fields, ".%s = %s",
                    field != NULL && field->name != NULL ? field->name : "field",
                    arg != NULL ? arg : "0");
                free(arg);
            }
            char *result;
            if (fields->len > 0)
                result = strdup_fmt("(%s){ %s }", fn, fields->data);
            else
                result = strdup_fmt("(%s){0}", fn);
            codebuf_destroy(fields);
            return result;
        }
        {
            ASTNode *relation_decl = find_relation_decl(ctx, fn);
            ASTNode *effect_decl = find_effect_decl(ctx, fn);
            ASTNode *overlay_decl = relation_decl != NULL ? relation_decl : effect_decl;
            if (overlay_decl != NULL) {
                size_t argc = call->data.call.arg_count;
                size_t slot_count = overlay_decl->type == AST_RELATION_DECL
                    ? overlay_decl->data.relation_decl.slot_count
                    : overlay_decl->data.effect_decl.slot_count;
                size_t shared_count = overlay_decl->type == AST_RELATION_DECL
                    ? overlay_decl->data.relation_decl.shared_count
                    : overlay_decl->data.effect_decl.shared_count;
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc && i < slot_count + shared_count; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < slot_count) {
                        ASTNode *slot = overlay_decl->type == AST_RELATION_DECL
                            ? overlay_decl->data.relation_decl.slots[i]
                            : overlay_decl->data.effect_decl.slots[i];
                        field_name = slot != NULL ? slot->data.domain_slot.slot_name : "field";
                    } else {
                        ASTNode *shared = overlay_decl->type == AST_RELATION_DECL
                            ? overlay_decl->data.relation_decl.shared_fields[i - slot_count]
                            : overlay_decl->data.effect_decl.shared_fields[i - slot_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *zone_decl = find_zone_decl(ctx, fn);
            if (zone_decl != NULL && zone_decl->type == AST_ZONE_DECL) {
                size_t argc = call->data.call.arg_count;
                CodeBuf *fields = codebuf_create();
                for (size_t i = 0; i < argc
                        && i < zone_decl->data.zone_decl.slot_count
                               + zone_decl->data.zone_decl.shared_count; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < zone_decl->data.zone_decl.slot_count) {
                        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
                        field_name = slot != NULL ? slot->data.domain_slot.slot_name : "field";
                    } else {
                        ASTNode *shared = zone_decl->data.zone_decl.shared_fields[
                            i - zone_decl->data.zone_decl.slot_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }
        {
            ASTNode *world_decl = find_world_decl(ctx, fn);
            if (world_decl != NULL && world_decl->type == AST_WORLD_DECL) {
                size_t argc = call->data.call.arg_count;
                CodeBuf *fields = codebuf_create();
                size_t exposed = world_decl->data.world_decl.systemic_count
                    + world_decl->data.world_decl.zone_count
                    + world_decl->data.world_decl.shared_count;
                for (size_t i = 0; i < argc && i < exposed; i++) {
                    const char *field_name = NULL;
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    if (i < world_decl->data.world_decl.systemic_count) {
                        ASTNode *slot = world_decl->data.world_decl.systemics[i];
                        field_name = slot != NULL ? slot->data.world_systemic.slot_name : "field";
                    } else if (i < world_decl->data.world_decl.systemic_count
                                   + world_decl->data.world_decl.zone_count) {
                        ASTNode *slot = world_decl->data.world_decl.zones[
                            i - world_decl->data.world_decl.systemic_count];
                        field_name = slot != NULL ? slot->data.world_zone.slot_name : "field";
                    } else {
                        ASTNode *shared = world_decl->data.world_decl.shared_fields[
                            i - world_decl->data.world_decl.systemic_count
                              - world_decl->data.world_decl.zone_count];
                        field_name = shared != NULL ? shared->data.party_shared.name : "field";
                    }
                    if (i > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".%s = %s",
                        field_name != NULL ? field_name : "field",
                        arg != NULL ? arg : "0");
                    free(arg);
                }
                for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
                    ASTNode *zone = world_decl->data.world_decl.zones[i];
                    const char *slot_name = zone != NULL
                        ? zone->data.world_zone.slot_name
                        : NULL;
                    if (slot_name == NULL)
                        continue;
                    if (fields->len > 0)
                        codebuf_write(fields, ", ");
                    codebuf_write(fields, ".__zone_dirty_%s = true", slot_name);
                }
                if (fields->len > 0)
                    codebuf_write(fields, ", ");
                codebuf_write(fields, ".__world_derived_dirty = true");
                {
                    char *result = fields->len > 0
                        ? strdup_fmt("(%s){ %s }", fn, fields->data)
                        : strdup_fmt("(%s){0}", fn);
                    codebuf_destroy(fields);
                    return result;
                }
            }
        }

        const char *qualified = lookup_enum_variant_qualified_name(ctx, fn);
        if (qualified != NULL) {
            /* Emit: EnumName_VariantName(args...) */
            size_t argc = call->data.call.arg_count;
            char **arg_strs = calloc(argc > 0 ? argc : 1, sizeof(char *));
            for (size_t i = 0; i < argc; i++)
                arg_strs[i] = emit_expression(call->data.call.arguments[i], ctx);
            /* Build argument list string */
            size_t buf_len = strlen(qualified) + 3;
            for (size_t i = 0; i < argc; i++)
                buf_len += strlen(arg_strs[i]) + 2;
            char *result = malloc(buf_len);
            strcpy(result, qualified);
            strcat(result, "(");
            for (size_t i = 0; i < argc; i++) {
                if (i > 0) strcat(result, ", ");
                strcat(result, arg_strs[i]);
                free(arg_strs[i]);
            }
            strcat(result, ")");
            free(arg_strs);
            return result;
        }
    }

    /* Result<T> built-in functions: Ok, Err, IsOk, IsErr, Unwrap, UnwrapOr */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;
        if (strcmp(fn, "Ok") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Ok_Int(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Err") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Err_Int(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsOk") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("IsOk_Int(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsErr") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("IsErr_Int(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Unwrap") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Unwrap_Int(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "UnwrapOr") == 0 && call->data.call.arg_count == 2) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *fallback = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("UnwrapOr_Int(%s, %s)", arg, fallback);
            free(arg);
            free(fallback);
            return result;
        }
        if (strcmp(fn, "Some") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char *result = strdup_fmt("Some_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "None") == 0 && call->data.call.arg_count == 0) {
            return strdup_fmt("None_Int()");
        }
        if (strcmp(fn, "IsSome") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = strncmp(opt_type, "Option<", 7) == 0
                ? slot_inner_type_name(opt_type) : "Int";
            char *result = strdup_fmt("IsSome_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsNone") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = strncmp(opt_type, "Option<", 7) == 0
                ? slot_inner_type_name(opt_type) : "Int";
            char *result = strdup_fmt("IsNone_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "UnwrapOption") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *opt_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = strncmp(opt_type, "Option<", 7) == 0
                ? slot_inner_type_name(opt_type) : "Int";
            char *result = strdup_fmt("UnwrapOption_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
    }

    /* Standard library built-in functions */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;

        /* Math functions */
        if (strcmp(fn, "Abs") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((%s) < 0 ? -(%s) : (%s))", arg, arg, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Min") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("((%s) < (%s) ? (%s) : (%s))", a, b, a, b);
            free(a); free(b);
            return result;
        }
        if (strcmp(fn, "Max") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("((%s) > (%s) ? (%s) : (%s))", a, b, a, b);
            free(a); free(b);
            return result;
        }
        /* String functions */
        if (strcmp(fn, "StringLength") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((int32_t)strlen(%s))", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Contains") == 0 || strcmp(fn, "StringContains") == 0)
            && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringContains(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if ((strcmp(fn, "Replace") == 0 || strcmp(fn, "StringReplace") == 0)
            && call->data.call.arg_count == 3) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *old_s = emit_expression(call->data.call.arguments[1], ctx);
            char *new_s = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("StringReplace(%s, %s, %s)", s, old_s, new_s);
            free(s); free(old_s); free(new_s);
            return result;
        }
        if (strcmp(fn, "Substring") == 0 && call->data.call.arg_count == 3) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *start = emit_expression(call->data.call.arguments[1], ctx);
            char *len = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("Substring(%s, %s, %s)", s, start, len);
            free(s); free(start); free(len);
            return result;
        }
        if ((strcmp(fn, "Trim") == 0 || strcmp(fn, "StringTrim") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("StringTrim(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Upper") == 0 || strcmp(fn, "ToUpper") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToUpper(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Lower") == 0 || strcmp(fn, "ToLower") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToLower(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Concat") == 0 || strcmp(fn, "StringConcat") == 0)
            && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringConcat(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        /* StringSplit / StringJoin / ToInt / ToFloat / Math */
        if ((strcmp(fn, "StringSplit") == 0 || strcmp(fn, "Split") == 0)
            && call->data.call.arg_count == 2) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *d = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringSplit(%s, %s)", s, d);
            free(s); free(d);
            return result;
        }
        if ((strcmp(fn, "StringJoin") == 0 || strcmp(fn, "Join") == 0)
            && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *sep = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringJoin(&%s, %s)", arr, sep);
            free(arr); free(sep);
            return result;
        }
        if (strcmp(fn, "ToInt") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToInt(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ToFloat") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToFloat(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Sqrt") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Sqrt(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Pow") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("Pow(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if ((strcmp(fn, "Floor") == 0 || strcmp(fn, "Ceil") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("%s(%s)", fn, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Random") == 0) {
            if (call->data.call.arg_count >= 1) {
                char *arg = emit_expression(call->data.call.arguments[0], ctx);
                char *result = strdup_fmt("Random(%s)", arg);
                free(arg);
                return result;
            }
            return pergyra_strdup("Random(100)");
        }
        if (strcmp(fn, "ArrayLength") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((int32_t)(%s.length))", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ArrayPush") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *suffix = infer_expression_type_name(
                ctx, call->data.call.arguments[1]);
            char *result = strdup_fmt(
                "pgy_array_push_%s(&%s, %s)", suffix, arr, val);
            free(arr); free(val);
            return result;
        }
        if (strcmp(fn, "ArraySet") == 0 && call->data.call.arg_count == 3) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *idx = emit_expression(call->data.call.arguments[1], ctx);
            char *val = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt(
                "((%s).data[%s] = %s)", arr, idx, val);
            free(arr); free(idx); free(val);
            return result;
        }
        if (strcmp(fn, "ArrayPop") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((%s).length > 0 ? (%s).length-- : 0)",
                arr, arr);
            free(arr);
            return result;
        }
        /* ArraySort — in-place qsort on array */
        if (strcmp(fn, "ArraySort") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            char *result = strdup_fmt(
                "({ qsort((%s).data, (%s).length, sizeof(*(%s).data), "
                "pgy_cmp_%s); %s; })",
                arr, arr, arr, inner, arr);
            free(arr);
            return result;
        }
        /* ArrayMap — apply function to each element, return new array */
        if (strcmp(fn, "ArrayMap") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *fn_arg = emit_expression(call->data.call.arguments[1], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            int tmp_id = ++ctx->tmp_counter;
            char *result = strdup_fmt(
                "({ PgyArray_%s _pgy_map_%d = pgy_array_new_%s((%s).length); "
                "for (size_t _mi = 0; _mi < (%s).length; _mi++) "
                "pgy_array_push_%s(&_pgy_map_%d, %s((%s).data[_mi])); "
                "_pgy_map_%d; })",
                inner, tmp_id, inner, arr,
                arr,
                inner, tmp_id, fn_arg, arr,
                tmp_id);
            free(arr); free(fn_arg);
            return result;
        }
        /* ArrayFilter — keep elements where predicate returns true */
        if (strcmp(fn, "ArrayFilter") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *fn_arg = emit_expression(call->data.call.arguments[1], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            int tmp_id = ++ctx->tmp_counter;
            char *result = strdup_fmt(
                "({ PgyArray_%s _pgy_filt_%d = pgy_array_new_%s((%s).length); "
                "for (size_t _fi = 0; _fi < (%s).length; _fi++) "
                "if (%s((%s).data[_fi])) "
                "pgy_array_push_%s(&_pgy_filt_%d, (%s).data[_fi]); "
                "_pgy_filt_%d; })",
                inner, tmp_id, inner, arr,
                arr,
                fn_arg, arr,
                inner, tmp_id, arr,
                tmp_id);
            free(arr); free(fn_arg);
            return result;
        }
        /* ArrayReverse — in-place reverse */
        if (strcmp(fn, "ArrayReverse") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            const char *c_type = "int32_t";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0) {
                inner = slot_inner_type_name(arr_type);
                c_type = pergyra_type_to_c(inner);
            }
            char *result = strdup_fmt(
                "({ for (size_t _ri = 0; _ri < (%s).length / 2; _ri++) { "
                "%s _tmp = (%s).data[_ri]; "
                "(%s).data[_ri] = (%s).data[(%s).length - 1 - _ri]; "
                "(%s).data[(%s).length - 1 - _ri] = _tmp; } %s; })",
                arr, c_type, arr, arr, arr, arr, arr, arr, arr);
            free(arr);
            return result;
        }
        /* Channel builtins */
        if (strcmp(fn, "TryRecv") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            const char *c_inner = pergyra_type_to_c(inner);
            char *result = strdup_fmt(
                "({ %s _pgy_recv_tmp; "
                "pgy_channel_try_recv_%s(&%s, &_pgy_recv_tmp) "
                "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
                c_inner, inner, ch, inner, inner);
            free(ch);
            return result;
        }
        if (strcmp(fn, "RecvTimeout") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *timeout = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            const char *c_inner = pergyra_type_to_c(inner);
            char *result = strdup_fmt(
                "({ %s _pgy_recv_tmp; "
                "pgy_channel_recv_timeout_%s(&%s, &_pgy_recv_tmp, (uint64_t)(%s)) "
                "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
                c_inner, inner, ch, timeout, inner, inner);
            free(ch);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "TrySend") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_try_send_%s(&%s, %s)", inner, ch, val);
            free(ch);
            free(val);
            return result;
        }
        if (strcmp(fn, "TrySendStatus") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_try_send_status_%s(&%s, %s)", inner, ch, val);
            free(ch);
            free(val);
            return result;
        }
        if (strcmp(fn, "SendTimeout") == 0 && call->data.call.arg_count == 3) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            char *timeout = emit_expression(call->data.call.arguments[2], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_send_timeout_%s(&%s, %s, (uint64_t)(%s))",
                inner, ch, val, timeout);
            free(ch);
            free(val);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "SendTimeoutStatus") == 0 && call->data.call.arg_count == 3) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            char *timeout = emit_expression(call->data.call.arguments[2], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_send_timeout_status_%s(&%s, %s, (uint64_t)(%s))",
                inner, ch, val, timeout);
            free(ch);
            free(val);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "Cancel") == 0 && call->data.call.arg_count == 1) {
            char *task = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("pgy_task_cancel(%s)", task);
            free(task);
            return result;
        }
        if (strcmp(fn, "IsCancelled") == 0 && call->data.call.arg_count == 0) {
            return strdup_fmt("pgy_task_is_cancelled()");
        }
        if (strcmp(fn, "ChannelClose") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_close_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelReady") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_ready_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelLength") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_length_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelCapacity") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_capacity_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelSpace") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_space_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelFull") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_full_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelClosed") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_closed_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        /* Clone: explicit copy of Slot */
        if (strcmp(fn, "Clone") == 0 && call->data.call.arg_count == 1) {
            char *src = emit_expression(call->data.call.arguments[0], ctx);
            const char *tn = infer_expression_type_name(
                ctx, call->data.call.arguments[0]);
            if (tn != NULL && strncmp(tn, "Slot<", 5) == 0) {
                const char *inner = slot_inner_type_name(tn);
                char *result = strdup_fmt(
                    "({ PgySlot_%s _c = pgy_claim_%s(); "
                    "pgy_write_%s(&_c, pgy_read_%s(&%s)); _c; })",
                    inner, inner, inner, inner, src);
                free(src);
                return result;
            }
            return src;
        }
        /* Print (no newline) vs Log (with newline) */
        if (strcmp(fn, "Print") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("printf(\"%%s\", %s)", arg);
            free(arg);
            return result;
        }
        /* ToString */
        if (strcmp(fn, "ToString") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("pgy_int_to_string(%s)", arg);
            free(arg);
            return result;
        }
    }

    if (callee->type == AST_IDENTIFIER) {
        const char *name = callee->data.identifier.name;
        if (find_event_decl(ctx, name) != NULL) {
            CodeBuf *args_buf = codebuf_create();
            for (size_t i = 0; i < call->data.call.arg_count; i++) {
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(args_buf, ", ");
                codebuf_write(args_buf, "%s", arg);
                free(arg);
            }

            char *result = strdup_fmt("%s_INVOKE(&%s%s%s)",
                                      name, name,
                                      args_buf->len > 0 ? ", " : "",
                                      args_buf->data);
            codebuf_destroy(args_buf);
            return result;
        }
    }

    /* Method-call style slot operations: slot.Write(val), slot.Read(), slot.Release() */
    if (callee->type == AST_MEMBER_ACCESS) {
        const char *method = callee->data.member.name;
        ASTNode *obj = callee->data.member.object;

        if (obj != NULL && obj->type == AST_MEMBER_ACCESS && method != NULL) {
            ASTNode *party_node = obj->data.member.object;
            const char *slot_name = obj->data.member.name;

            if (party_node != NULL && party_node->type == AST_IDENTIFIER
                && slot_name != NULL) {
                const char *party_var = party_node->data.identifier.name;
                const char *party_type = lookup_typed_var(ctx, party_var);
                ASTNode *party_decl = find_party_decl(ctx, party_type);
                const char *ability_name = NULL;

                if (party_decl != NULL) {
                    for (size_t i = 0; i < party_decl->data.party_decl.role_count; i++) {
                        ASTNode *rs = party_decl->data.party_decl.role_slots[i];
                        if (rs == NULL || strcmp(rs->data.role_slot.slot_name, slot_name) != 0)
                            continue;
                        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
                            ASTNode *ab = rs->data.role_slot.required_abilities[j];
                            ASTNode *ability_decl;
                            bool has_method = false;
                            if (ab == NULL || ab->data.type.name == NULL)
                                continue;
                            ability_decl = find_ability_decl(ctx, ab->data.type.name);
                            if (ability_decl != NULL) {
                                for (size_t mi = 0; mi < ability_decl->data.ability_decl.method_count; mi++) {
                                    ASTNode *m = ability_decl->data.ability_decl.methods[mi];
                                    if (m != NULL && m->type == AST_FUNC_DECL
                                        && m->data.func_decl.name != NULL
                                        && strcmp(m->data.func_decl.name, method) == 0) {
                                        has_method = true;
                                        break;
                                    }
                                }
                            }
                            if (has_method || ability_name == NULL)
                                ability_name = ab->data.type.name;
                            if (has_method)
                                break;
                        }
                        break;
                    }
                }

                if (ability_name != NULL) {
                    CodeBuf *args_buf = codebuf_create();
                    codebuf_write(args_buf, "%s.%s", party_var, slot_name);
                    for (size_t i = 0; i < call->data.call.arg_count; i++) {
                        char *arg = emit_expression(call->data.call.arguments[i], ctx);
                        codebuf_write(args_buf, ", %s", arg);
                        free(arg);
                    }

                    char *result = strdup_fmt("%s.%s_%s_vt->%s(%s)",
                                              party_var, slot_name, ability_name,
                                              method, args_buf->data);
                    codebuf_destroy(args_buf);
                    return result;
                }
            }
        }

        bool is_slot_method = (strcmp(method, "Write") == 0
                            || strcmp(method, "Read") == 0
                            || strcmp(method, "Release") == 0);

        if (obj != NULL && method != NULL) {
            const char *type_name = resolve_nominal_host_expr_type_name(ctx, obj);
            if (type_name != NULL && is_nominal_host_type_name(ctx, type_name)) {
                CodeBuf *args_buf = codebuf_create();
                bool use_self_cell = is_pointer_self_host_type_name(ctx, type_name);
                bool is_self_ident = (obj->type == AST_IDENTIFIER
                    && obj->data.identifier.name != NULL
                    && strcmp(obj->data.identifier.name, "self") == 0);

                if (is_self_ident && use_self_cell) {
                    codebuf_write(args_buf, "self");
                } else {
                    char *obj_expr = emit_expression(obj, ctx);
                    /* Check if receiver is already a pointer (subject-ref param) */
                    bool already_pointer = false;
                    if (obj->type == AST_IDENTIFIER) {
                        TypedVarEntry *entry = lookup_typed_entry(ctx,
                            obj->data.identifier.name);
                        if (entry != NULL && entry->is_subject_ref)
                            already_pointer = true;
                    }
                    if (use_self_cell && !already_pointer)
                        codebuf_write(args_buf, "&%s", obj_expr);
                    else
                        codebuf_write(args_buf, "%s", obj_expr);
                    free(obj_expr);
                }

                for (size_t i = 0; i < call->data.call.arg_count; i++) {
                    char *arg = emit_expression(call->data.call.arguments[i], ctx);
                    codebuf_write(args_buf, ", %s", arg);
                    free(arg);
                }

                {
                    char *result = strdup_fmt("%s_%s(%s)",
                        type_name, method, args_buf->data);
                    codebuf_destroy(args_buf);
                    return result;
                }
            }
        }

        if (is_slot_method && obj->type == AST_IDENTIFIER) {
            const char *inner = lookup_slot_type(ctx, obj->data.identifier.name);
            bool is_secure = lookup_slot_is_secure(ctx, obj->data.identifier.name);
            bool saved_suppress = ctx->suppress_slot_auto_read;
            ctx->suppress_slot_auto_read = true;
            char *obj_expr = emit_expression(obj, ctx);
            ctx->suppress_slot_auto_read = saved_suppress;
            char *slot_ref = slot_ref_expr(ctx, obj->data.identifier.name, obj_expr);

            if (strcmp(method, "Write") == 0 && call->data.call.arg_count >= 1) {
                char *val_expr = emit_expression(call->data.call.arguments[0], ctx);
                char *result;
                if (is_secure && call->data.call.arg_count >= 2) {
                    char *tok = emit_expression(call->data.call.arguments[1], ctx);
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s)",
                                        inner, slot_ref, val_expr, tok);
                    free(tok);
                } else if (is_secure) {
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s_token)",
                                        inner, slot_ref, val_expr,
                                        obj->data.identifier.name);
                } else {
                    result = strdup_fmt("pgy_write_%s(%s, %s)",
                                        inner, slot_ref, val_expr);
                }
                free(val_expr);
                free(slot_ref);
                free(obj_expr);
                return result;
            } else if (strcmp(method, "Read") == 0) {
                char *result;
                if (is_secure) {
                    result = strdup_fmt("pgy_secure_read_%s(%s, &%s_token)",
                                        inner, slot_ref, obj->data.identifier.name);
                } else {
                    result = strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
                }
                free(slot_ref);
                free(obj_expr);
                return result;
            } else if (strcmp(method, "Release") == 0) {
                char *result;
                if (is_secure) {
                    result = strdup_fmt("pgy_secure_release_%s(%s, &%s_token)",
                                        inner, slot_ref, obj->data.identifier.name);
                } else {
                    result = strdup_fmt("pgy_release_%s(%s)", inner, slot_ref);
                }
                /* Mark as released */
                for (int ri = 0; ri < ctx->slot_var_count; ri++) {
                    if (strcmp(ctx->slot_vars[ri].name,
                              obj->data.identifier.name) == 0) {
                        ctx->slot_vars[ri].released = true;
                        break;
                    }
                }
                free(slot_ref);
                free(obj_expr);
                return result;
            }
            free(slot_ref);
            free(obj_expr);
        }
    }

    /* User function call */
    if (callee->type == AST_IDENTIFIER) {
        ASTNode *host_method = current_host_method_decl(ctx, callee->data.identifier.name);
        const char *host_name = current_host_decl_name(ctx);
        if (host_method != NULL && host_name != NULL) {
            CodeBuf *args_buf = codebuf_create();
            codebuf_write(args_buf, "self");
            for (size_t i = 0; i < call->data.call.arg_count; i++) {
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                codebuf_write(args_buf, ", %s", arg);
                free(arg);
            }
            {
                char *result = strdup_fmt("%s_%s(%s)",
                    host_name, callee->data.identifier.name, args_buf->data);
                codebuf_destroy(args_buf);
                return result;
            }
        }
    }

    char *callee_str = NULL;
    if (callee->type == AST_IDENTIFIER) {
        ASTNode *decl = find_function_decl(ctx, callee->data.identifier.name);
        if (func_has_generic_params(decl)) {
            const char *specialized_name = ensure_generic_specialization(ctx, decl, call);
            if (specialized_name != NULL)
                callee_str = pergyra_strdup(specialized_name);
        }
    }
    if (callee_str == NULL)
        callee_str = emit_expression(callee, ctx);

    /* Build argument list */
    CodeBuf *args_buf = codebuf_create();
    ASTNode *decl = (callee->type == AST_IDENTIFIER)
        ? find_function_decl(ctx, callee->data.identifier.name) : NULL;
    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        FuncParam *param = (decl != NULL && i < decl->data.func_decl.param_count)
            ? decl->data.func_decl.params[i] : NULL;
        bool handled = false;
        char *arg = NULL;

        if (param != NULL && param->type != NULL
            && (param->mode == PARAM_MODE_OWN || param->mode == PARAM_MODE_REF)) {
            char *param_type = render_type_name(param->type);
            bool slot_param = param_type != NULL
                && (strncmp(param_type, "Slot<", 5) == 0
                    || strncmp(param_type, "SecureSlot<", 11) == 0);
            bool secure_param = param_type != NULL
                && strncmp(param_type, "SecureSlot<", 11) == 0;
            if (slot_param) {
                const char *inner = NULL;
                const char *slot_name = NULL;
                bool secure = false;
                bool saved_suppress = ctx->suppress_slot_auto_read;
                ctx->suppress_slot_auto_read = true;
                arg = emit_expression(call->data.call.arguments[i], ctx);
                ctx->suppress_slot_auto_read = saved_suppress;
                resolve_slot_target(ctx, call->data.call.arguments[i],
                    &inner, &slot_name, &secure);
                if (i > 0)
                    codebuf_write(args_buf, ", ");
                if (slot_name != NULL) {
                    char *slot_ref = slot_ref_expr(ctx, slot_name, arg);
                    codebuf_write(args_buf, "%s", slot_ref);
                    free(slot_ref);
                    if (secure_param)
                        codebuf_write(args_buf, ", %s_token", slot_name);
                    handled = true;
                }
            }
            free(param_type);
        }

        if (!handled)
            arg = emit_expression(call->data.call.arguments[i], ctx);
        if (!handled && i > 0)
            codebuf_write(args_buf, ", ");
        if (!handled) {
            /* Subject arguments: pass by pointer (reference semantics) */
            bool is_subject_arg = false;
            if (param != NULL && param->type != NULL
                && param->name != NULL && strcmp(param->name, "self") != 0) {
                char *ptn = render_type_name(param->type);
                if (ptn != NULL && is_pointer_self_host_type_name(ctx, ptn))
                    is_subject_arg = true;
                free(ptn);
            }
            if (is_subject_arg) {
                /* Don't add & if already a pointer (subject-ref param) */
                bool already_ptr = false;
                ASTNode *arg_node = call->data.call.arguments[i];
                if (arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
                    TypedVarEntry *entry = lookup_typed_entry(ctx,
                        arg_node->data.identifier.name);
                    if (entry != NULL && entry->is_subject_ref)
                        already_ptr = true;
                }
                if (already_ptr)
                    codebuf_write(args_buf, "%s", arg);
                else
                    codebuf_write(args_buf, "&%s", arg);
            } else {
                codebuf_write(args_buf, "%s", arg);
            }
        }
        free(arg);
    }

    char *result = strdup_fmt("%s(%s)", callee_str, args_buf->data);
    free(callee_str);
    codebuf_destroy(args_buf);
    return result;
}

char *
emit_spawn_expr(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *target = node->data.spawn_expr.function;
    ASTNode *call = NULL;
    ASTNode *callee = NULL;
    const char *function_name = NULL;
    const char *emitted_function_name = NULL;
    ASTNode *decl = NULL;
    size_t arg_count = 0;
    int wrapper_id = ++ctx->tmp_counter;
    char *wrapper_name = strdup_fmt("pgy_spawn_wrapper_%d", wrapper_id);
    char *args_type_name = NULL;
    char *return_type_name = infer_spawn_return_type_name(ctx, node);
    char *return_c_type = pergyra_strdup(pergyra_type_to_c(return_type_name));
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;

    if (target == NULL) {
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
    }

    if (target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
        arg_count = target->data.call.arg_count;
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        function_name = callee->data.identifier.name;
    if (function_name == NULL) {
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("/* unsupported spawn target */");
    }

    decl = find_function_decl(ctx, function_name);
    emitted_function_name = function_name;
    if (call != NULL && func_has_generic_params(decl)
        && infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count)) {
        const char *specialized = ensure_generic_specialization(ctx, decl, call);
        if (specialized != NULL)
            emitted_function_name = specialized;
    }
    if (arg_count > 0)
        args_type_name = strdup_fmt("PgySpawnArgs_%d", wrapper_id);

    if (args_type_name != NULL) {
        codebuf_write(ctx->decls, "\ntypedef struct {\n");
        for (size_t i = 0; i < arg_count; i++) {
            const char *arg_type = "int32_t";
            if (decl != NULL && i < decl->data.func_decl.param_count
                && decl->data.func_decl.params[i] != NULL
                && decl->data.func_decl.params[i]->type != NULL) {
                if (binding_count > 0) {
                    char *bound_type = render_type_name_with_bindings(ctx,
                        decl->data.func_decl.params[i]->type, bindings, binding_count);
                    arg_type = pergyra_type_to_c(bound_type);
                    codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
                    free(bound_type);
                    continue;
                }
                arg_type = pergyra_ast_type_to_c(decl->data.func_decl.params[i]->type);
            } else if (call != NULL) {
                arg_type = pergyra_type_to_c(infer_expression_type_name(
                    ctx, call->data.call.arguments[i]));
            }
            codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
        }
        codebuf_write(ctx->decls, "} %s;\n", args_type_name);
    }

    codebuf_write(ctx->decls, "static void *%s(void *raw);\n", wrapper_name);
    codebuf_write(ctx->helpers, "\nstatic void *%s(void *raw)\n{\n", wrapper_name);
    if (args_type_name != NULL) {
        codebuf_write(ctx->helpers, "    %s *args = (%s *)raw;\n",
            args_type_name, args_type_name);
    } else {
        codebuf_write(ctx->helpers, "    (void)raw;\n");
    }

    if (strcmp(return_type_name, "Void") == 0) {
        codebuf_write(ctx->helpers, "    %s(", emitted_function_name);
    } else {
        codebuf_write(ctx->helpers,
            "    %s *result = (%s *)malloc(sizeof(%s));\n",
            return_c_type, return_c_type, return_c_type);
        codebuf_write(ctx->helpers,
            "    if (result == NULL) {\n"
            "        PGY_PANIC(\"spawn result allocation failed\");\n"
            "    }\n"
            "    *result = %s(",
            emitted_function_name);
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (i > 0)
            codebuf_write(ctx->helpers, ", ");
        codebuf_write(ctx->helpers, "args->arg%zu", i);
    }
    codebuf_write(ctx->helpers, ");\n");

    if (args_type_name != NULL)
        codebuf_write(ctx->helpers, "    free(args);\n");

    if (strcmp(return_type_name, "Void") == 0)
        codebuf_write(ctx->helpers, "    return NULL;\n");
    else
        codebuf_write(ctx->helpers, "    return result;\n");
    codebuf_write(ctx->helpers, "}\n");

    CodeBuf *expr = codebuf_create();
    if (expr == NULL) {
        free(wrapper_name);
        free(args_type_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("/* spawn alloc failed */");
    }

    {
        /* spawn blocking → offload to dedicated blocking pool */
        const char *spawn_fn = node->data.spawn_expr.is_blocking
            ? "pgy_spawn_blocking" : "pgy_async_spawn";
        if (args_type_name == NULL) {
            codebuf_write(expr, "%s(%s, NULL)", spawn_fn, wrapper_name);
        } else {
            codebuf_write(expr,
                "({ %s *_pgy_args = (%s *)malloc(sizeof(%s)); "
                "if (_pgy_args == NULL) { PGY_PANIC(\"spawn arg allocation failed\"); } ",
                args_type_name, args_type_name, args_type_name);
            for (size_t i = 0; i < arg_count; i++) {
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                codebuf_write(expr, "_pgy_args->arg%zu = %s; ", i, arg);
                free(arg);
            }
            codebuf_write(expr, "%s(%s, _pgy_args); })", spawn_fn, wrapper_name);
        }
    }

    char *result = pergyra_strdup(expr->data);
    codebuf_destroy(expr);
    free(wrapper_name);
    free(args_type_name);
    free(return_type_name);
    free(return_c_type);
    return result;
}

char *
emit_channel_send(ASTNode *node, TranspilerCtx *ctx)
{
    char *ch  = emit_expression(node->data.channel_send.channel, ctx);
    char *val = emit_expression(node->data.channel_send.value, ctx);
    const char *inner = "Int";

    if (node->data.channel_send.channel != NULL
        && node->data.channel_send.channel->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            node->data.channel_send.channel->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
            inner = slot_inner_type_name(type_name);
    }

    char *result = strdup_fmt("pgy_channel_send_%s(&%s, %s)", inner, ch, val);
    free(ch);
    free(val);
    return result;
}

char *
emit_channel_recv(ASTNode *node, TranspilerCtx *ctx)
{
    char *ch = emit_expression(node->data.channel_recv.channel, ctx);
    const char *inner = "Int";

    if (node->data.channel_recv.channel != NULL
        && node->data.channel_recv.channel->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            node->data.channel_recv.channel->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
            inner = slot_inner_type_name(type_name);
    }

    char *result = strdup_fmt("pgy_channel_recv_val_%s(&%s)", inner, ch);
    free(ch);
    return result;
}

char *
emit_expression(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return pergyra_strdup("0");

    switch (node->type) {
    case AST_NUMBER:
        if (node->data.number.value == (int64_t)node->data.number.value)
            return strdup_fmt("%lld", (long long)(int64_t)node->data.number.value);
        return strdup_fmt("%g", node->data.number.value);

    case AST_STRING:
    {
        char *escaped = escape_c_string_literal(node->data.string.value);
        char *result = strdup_fmt("\"%s\"", escaped);
        free(escaped);
        return result;
    }

    case AST_BOOLEAN:
        return pergyra_strdup(node->data.boolean.value ? "true" : "false");

    case AST_IDENTIFIER: {
        const char *id_name = node->data.identifier.name;
        /* Inside parallel wrapper: captured outer variables are accessed
         * through the context struct pointer.  (*_pctx->x) yields the
         * value, and &(*_pctx->x) collapses to _pctx->x (a pointer). */
        if (ctx->in_parallel_wrapper) {
            for (int i = 0; i < ctx->par_capture_slot_end; i++) {
                if (strcmp(ctx->slot_vars[i].name, id_name) == 0)
                    return strdup_fmt("(*_pctx->%s)", id_name);
            }
            for (int i = 0; i < ctx->par_capture_typed_end; i++) {
                if (strcmp(ctx->typed_vars[i].name, id_name) == 0)
                    return strdup_fmt("(*_pctx->%s)", id_name);
            }
        }
        if (ctx->current_class_name != NULL
            && strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_class_has_field(ctx, id_name)) {
            return strdup_fmt(current_class_uses_self_cell(ctx)
                ? "self->%s"
                : "self.%s", id_name);
        }
        if (ctx->current_relation_name != NULL
            && strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_relation_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (ctx->current_effect_name != NULL
            && strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_effect_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (ctx->current_zone_name != NULL
            && strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_zone_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (ctx->current_world_name != NULL
            && strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_world_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        /* Slot sugar: auto-Read — emit pgy_read_T(&x) instead of x */
        if (!ctx->suppress_slot_auto_read && is_slot_var(ctx, id_name)) {
            const char *inner = lookup_slot_type(ctx, id_name);
            bool secure = lookup_slot_is_secure(ctx, id_name);
            char *slot_ref = slot_ref_expr(ctx, id_name, id_name);
            char *result = secure
                ? strdup_fmt("pgy_secure_read_%s(%s, &%s_token)",
                              inner, slot_ref, id_name)
                : strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
            free(slot_ref);
            return result;
        }
        {
            const char *enum_variant = lookup_enum_variant_qualified_name(ctx, id_name);
            if (enum_variant != NULL)
                return pergyra_strdup(enum_variant);
        }
        return pergyra_strdup(id_name);
    }

    case AST_BINARY:
        return emit_binary(node, ctx);

    case AST_UNARY:
        return emit_unary(node, ctx);

    case AST_CALL:
        return emit_call(node, ctx);

    case AST_MEMBER_ACCESS: {
        char *obj = emit_expression(node->data.member.object, ctx);
        /* Enum variant access: Color.Red → Color_Red */
        if (node->data.member.object->type == AST_IDENTIFIER
            && node->data.member.object->data.identifier.name[0] >= 'A'
            && node->data.member.object->data.identifier.name[0] <= 'Z') {
            char *result = strdup_fmt("%s_%s", obj, node->data.member.name);
            free(obj);
            return result;
        }
        if (node->data.member.object->type == AST_IDENTIFIER
            && strcmp(node->data.member.object->data.identifier.name, "self") == 0) {
            bool self_is_pointer = current_class_uses_self_cell(ctx)
                || ctx->current_relation_name != NULL
                || ctx->current_effect_name != NULL
                || ctx->current_zone_name != NULL
                || ctx->current_world_name != NULL;
            char *result = strdup_fmt(self_is_pointer
                ? "%s->%s"
                : "%s.%s", obj, node->data.member.name);
            free(obj);
            return result;
        }
        /* Subject-ref parameter: use -> for member access */
        if (node->data.member.object->type == AST_IDENTIFIER) {
            TypedVarEntry *entry = lookup_typed_entry(ctx,
                node->data.member.object->data.identifier.name);
            if (entry != NULL && entry->is_subject_ref) {
                char *result = strdup_fmt("%s->%s", obj, node->data.member.name);
                free(obj);
                return result;
            }
        }
        char *result = strdup_fmt("%s.%s", obj, node->data.member.name);
        free(obj);
        return result;
    }

    case AST_ARRAY_ACCESS: {
        char *array = emit_expression(node->data.array_access.array, ctx);
        char *index = emit_expression(node->data.array_access.index, ctx);
        const char *array_type = infer_expression_type_name(ctx, node->data.array_access.array);
        char *result;
        if (strncmp(array_type, "Array<", 6) == 0 || strncmp(array_type, "Slice<", 6) == 0)
            result = strdup_fmt("%s.data[%s]", array, index);
        else
            result = strdup_fmt("%s[%s]", array, index);
        free(array);
        free(index);
        return result;
    }

    case AST_ARRAY_LITERAL: {
        const char *array_type = infer_expression_type_name(ctx, node);
        const char *inner = slot_inner_type_name(array_type);
        int tmp_id = ++ctx->tmp_counter;
        CodeBuf *buf = codebuf_create();
        codebuf_write(buf, "({ PgyArray_%s _pgy_arr_%d = pgy_array_new_%s(%zu); ",
            inner, tmp_id, inner, node->data.array_literal.count);
        for (size_t i = 0; i < node->data.array_literal.count; i++) {
            char *elem = emit_expression(node->data.array_literal.elements[i], ctx);
            codebuf_write(buf, "pgy_array_push_%s(&_pgy_arr_%d, %s); ",
                inner, tmp_id, elem);
            free(elem);
        }
        codebuf_write(buf, "_pgy_arr_%d; })", tmp_id);
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }

    case AST_ASSIGNMENT: {
        /* Slot sugar: x = 5 → pgy_write_T(&x, 5) */
        if (node->data.assignment.target->type == AST_IDENTIFIER) {
            const char *tgt_name = node->data.assignment.target->data.identifier.name;
            if (is_slot_var(ctx, tgt_name)) {
                const char *inner = lookup_slot_type(ctx, tgt_name);
                bool secure = lookup_slot_is_secure(ctx, tgt_name);
                char *value = emit_expression(node->data.assignment.value, ctx);
                char *slot_ref = slot_ref_expr(ctx, tgt_name, tgt_name);
                char *result;
                if (secure) {
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s_token)",
                        inner, slot_ref, value, tgt_name);
                } else {
                    result = strdup_fmt("pgy_write_%s(%s, %s)",
                        inner, slot_ref, value);
                }
                free(slot_ref);
                free(value);
                return result;
            }
        }
        char *target = emit_expression(node->data.assignment.target, ctx);
        char *value  = emit_expression(node->data.assignment.value,  ctx);
        char *result = strdup_fmt("%s = %s", target, value);
        free(target);
        free(value);
        return result;
    }

    case AST_AWAIT_EXPR:
        {
            char *expr = emit_expression(node->data.await_expr.expression, ctx);
            const char *inner = lookup_future_inner_type(ctx,
                node->data.await_expr.expression);
            bool is_remote = is_remote_future_expr(ctx,
                node->data.await_expr.expression);
            char *result;
            if (strcmp(inner, "Void") == 0) {
                result = strdup_fmt("pgy_await_void(%s)", expr);
            } else if (is_remote) {
                /* RemoteFuture<T> → Result<T>: wrap in PgyResult */
                result = strdup_fmt("pgy_await_result_take(%s, %s, %s)",
                    expr, inner, pergyra_type_to_c(inner));
            } else {
                result = strdup_fmt("pgy_await_take(%s, %s)",
                    expr, pergyra_type_to_c(inner));
            }
            free(expr);
            return result;
        }

    case AST_SPAWN_EXPR:
        return emit_spawn_expr(node, ctx);

    case AST_CHANNEL_SEND:
        return emit_channel_send(node, ctx);

    case AST_CHANNEL_RECV:
        return emit_channel_recv(node, ctx);

    case AST_EVENT_INVOKE: {
        char *event = emit_expression(node->data.event_invoke.event, ctx);
        CodeBuf *args = codebuf_create();
        for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
            char *arg = emit_expression(node->data.event_invoke.arguments[i], ctx);
            if (i > 0)
                codebuf_write(args, ", ");
            codebuf_write(args, "%s", arg);
            free(arg);
        }
        char *result = strdup_fmt("%s_INVOKE(&%s%s%s)",
                                  event,
                                  event,
                                  args->len > 0 ? ", " : "",
                                  args->data);
        free(event);
        codebuf_destroy(args);
        return result;
    }

    case AST_CONTEXT_ACCESS:
        if (node->data.context_access.role_slot_name != NULL) {
            return strdup_fmt("self->%s", node->data.context_access.role_slot_name);
        }
        return pergyra_strdup("self");

    case AST_PARTY_INSTANCE: {
        CodeBuf *assignments = codebuf_create();
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            char *value = emit_expression(node->data.party_instance.assignments[i].value, ctx);
            if (i > 0)
                codebuf_write(assignments, ", ");
            codebuf_write(assignments, ".%s = %s",
                          node->data.party_instance.assignments[i].slot_name,
                          value);
            free(value);
        }
        char *result = strdup_fmt("(%s){%s}",
                                  node->data.party_instance.party_type,
                                  assignments->data);
        codebuf_destroy(assignments);
        return result;
    }

    case AST_LAMBDA_EXPR:
        return emit_lambda_expr(node, ctx);

    default:
        return pergyra_strdup("/* unsupported expr */");
    }
}
