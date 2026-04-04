/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "transpiler.h"
#include "../common/string_compat.h"
#include "../semantic/type_checker.h"

static const char *infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr);

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

static ClassField *
find_class_field_decl(ASTNode *decl, const char *field_name)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL || field_name == NULL)
        return NULL;

    for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
        ClassField *field = decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return field;
        }
    }

    return NULL;
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
    if (decl != NULL && !decl->data.class_decl.is_struct)
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
    if (type_name == NULL)
        return false;
    if (is_subject_type_name(ctx, type_name))
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
        && is_subject_type_name(ctx, ctx->current_class_name);
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
domain_has_projection_slot(ASTNode **slots, size_t slot_count, const char *slot_name)
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
        return !slot->data.domain_slot.is_subject;
    }

    return false;
}

static ASTNode *
find_nth_subject_domain_slot_local(ASTNode **slots, size_t slot_count, size_t nth)
{
    size_t seen = 0;

    if (slots == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject) {
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
    ASTNode *subject_slot;

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

    subject_slot = find_nth_subject_domain_slot_local(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count, 0);
    if (subject_slot == NULL || subject_slot->data.domain_slot.slot_name == NULL)
        return;

    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name,
        subject_slot->data.domain_slot.slot_name,
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
    ASTNode *left_subject;
    ASTNode *right_subject;

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

    left_subject = find_nth_subject_domain_slot_local(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count, 0);
    right_subject = find_nth_subject_domain_slot_local(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count, 1);
    if (left_subject == NULL || right_subject == NULL
        || left_subject->data.domain_slot.slot_name == NULL
        || right_subject->data.domain_slot.slot_name == NULL) {
        return;
    }

    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name, left_subject->data.domain_slot.slot_name, left_slot_name);
    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name, right_subject->data.domain_slot.slot_name, right_slot_name);
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
        decl->data.zone_decl.slot_count, slot_name);
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
        decl->data.relation_decl.slot_count, slot_name);
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
        decl->data.effect_decl.slot_count, slot_name);
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
        ClassField *source_field;

        if (target_field == NULL || target_field->name == NULL)
            continue;

        source_field = find_class_field_decl(source_decl, target_field->name);
        if (!first)
            codebuf_write(buf, ", ");
        first = false;

        if (source_field != NULL) {
            codebuf_write(buf, ".%s = %s.%s",
                target_field->name, source_expr, target_field->name);
        } else {
            codebuf_write(buf, ".%s = 0", target_field->name);
        }
    }

    codebuf_write(buf, " }");
    free(source_expr);
    result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static char *
emit_projection_literal(ASTNode *target_decl, ASTNode *source_decl,
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
        ClassField *source_field;

        if (target_field == NULL || target_field->name == NULL)
            continue;

        source_field = find_class_field_decl(source_decl, target_field->name);
        if (!first)
            codebuf_write(buf, ", ");
        first = false;

        if (source_field != NULL) {
            codebuf_write(buf, ".%s = %s.%s",
                target_field->name, source_expr, target_field->name);
        } else {
            codebuf_write(buf, ".%s = 0", target_field->name);
        }
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
    /* String concatenation: "a" + "b" → StringConcat(a, b) */
    if (expr->data.binary.op.type == TOKEN_PLUS) {
        const char *lt = infer_expression_type_name(ctx, expr->data.binary.left);
        /* Also detect chained string concat: (str + str) + str */
        bool is_string = (lt != NULL && strcmp(lt, "String") == 0);
        if (!is_string && expr->data.binary.left->type == AST_BINARY
            && expr->data.binary.left->data.binary.op.type == TOKEN_PLUS) {
            const char *lt2 = infer_expression_type_name(ctx, expr->data.binary.left->data.binary.left);
            if (lt2 != NULL && strcmp(lt2, "String") == 0)
                is_string = true;
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

        if (obj != NULL && obj->type == AST_IDENTIFIER && method != NULL) {
            const char *var_name = obj->data.identifier.name;
            const char *type_name = lookup_typed_var(ctx, var_name);
            if (type_name != NULL && is_nominal_host_type_name(ctx, type_name)) {
                CodeBuf *args_buf = codebuf_create();
                bool use_self_cell = is_pointer_self_host_type_name(ctx, type_name);
                const char *self_expr = strcmp(var_name, "self") == 0 ? "self" : NULL;

                if (self_expr == NULL) {
                    char *obj_expr = emit_expression(obj, ctx);
                    if (use_self_cell)
                        codebuf_write(args_buf, "&%s", obj_expr);
                    else
                        codebuf_write(args_buf, "%s", obj_expr);
                    free(obj_expr);
                } else {
                    codebuf_write(args_buf, "%s", self_expr);
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
        if (!handled)
            codebuf_write(args_buf, "%s", arg);
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
        return strdup_fmt("\"%s\"", node->data.string.value);

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

/* Forward declarations for emitters defined later */
void emit_ability_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_role_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_party_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_systemic_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_relation_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_effect_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_zone_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_world_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_actor_decl(ASTNode *node, TranspilerCtx *ctx);
static bool
select_case_parts(ASTNode *case_node, ASTNode **channel_out,
                  const char **bind_name_out, ASTNode **body_out)
{
    if (case_node == NULL || case_node->type != AST_BLOCK
        || case_node->data.block.count == 0)
        return false;

    ASTNode *first = case_node->data.block.statements[0];
    ASTNode *body = case_node->data.block.count >= 2
        ? case_node->data.block.statements[1] : NULL;

    if (first->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = NULL;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    if (first->type == AST_ASSIGNMENT
        && first->data.assignment.target != NULL
        && first->data.assignment.target->type == AST_IDENTIFIER
        && first->data.assignment.value != NULL
        && first->data.assignment.value->type == AST_CHANNEL_RECV) {
        if (channel_out != NULL)
            *channel_out = first->data.assignment.value->data.channel_recv.channel;
        if (bind_name_out != NULL)
            *bind_name_out = first->data.assignment.target->data.identifier.name;
        if (body_out != NULL)
            *body_out = body;
        return true;
    }

    return false;
}

void emit_select_stmt(ASTNode *node, TranspilerCtx *ctx);

/* Forward declarations for generic class monomorphization */
static bool class_has_generic_params(ASTNode *node);
static const char *ensure_generic_class_specialization(
    TranspilerCtx *ctx, ASTNode *class_decl, ASTNode *ann);

/* -----------------------------------------------------------------
 * Let declaration emitter
 * ----------------------------------------------------------------- */

void
emit_let_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode    *init = node->data.let_decl.initializer;
    ASTNode    *ann  = node->data.let_decl.type;
    char       *ann_type_name = ann != NULL ? render_type_name(ann) : NULL;

    /* Generic class monomorphization trigger:
     * If the annotation is a user-defined generic class (e.g. Node<Int>),
     * monomorphize the class and replace ann_type_name with the
     * specialized name (e.g. Node_Int). */
    const char *generic_class_spec_name = NULL;
    if (ann != NULL && ann->type == AST_TYPE
        && ann->data.type.generic_args != NULL
        && ann->data.type.generic_args->count > 0
        && ann->data.type.name != NULL) {
        ASTNode *gc_decl = find_class_decl(ctx, ann->data.type.name);
        if (gc_decl != NULL && class_has_generic_params(gc_decl)) {
            generic_class_spec_name =
                ensure_generic_class_specialization(ctx, gc_decl, ann);
            /* Replace ann_type_name with the specialized name */
            free(ann_type_name);
            ann_type_name = pergyra_strdup(generic_class_spec_name);
        }
    }

    /* Detect ClaimSlot / ClaimSecureSlot */
    bool is_slot        = false;
    bool is_secure_slot = false;
    bool is_device_slot = false;
    const char *slot_inner = "Int"; /* default */

    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        if (strcmp(callee_name, "ClaimSlot") == 0) {
            is_slot   = true;
        } else if (strcmp(callee_name, "ClaimSecureSlot") == 0) {
            is_slot        = true;
            is_secure_slot = true;
        } else if (strcmp(callee_name, "ClaimDeviceSlot") == 0) {
            is_device_slot = true;
        }
    }

    if (is_slot) {
        /*
         * Resolve inner type from annotation.
         * If not annotated, default to Int.
         */
        if (ann != NULL) {
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                slot_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                slot_inner = slot_inner_type_name(ann->data.type.name);
            }
        }

        register_slot_var(ctx, name, slot_inner, is_secure_slot, false);

        write_indent(ctx);
        if (is_secure_slot) {
            codebuf_write(ctx->out,
                "PgyToken_%s %s_token;\n", slot_inner, name);
            write_indent(ctx);
            codebuf_write(ctx->out,
                "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
                slot_inner, name, slot_inner, name);
        } else {
            codebuf_write(ctx->out,
                "PgySlot_%s %s = pgy_claim_%s();\n",
                slot_inner, name, slot_inner);
        }
        if (ann_type_name != NULL)
            register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    if (is_device_slot) {
        if (ann != NULL) {
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                slot_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                slot_inner = slot_inner_type_name(ann->data.type.name);
            }
        }

        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyDeviceSlot_%s %s = pgy_claim_device_%s();\n",
            slot_inner, name, slot_inner);
        if (ann_type_name != NULL)
            register_typed_var(ctx, name, ann_type_name);
        else {
            char *device_type = strdup_fmt("DeviceSlot<%s>", slot_inner);
            register_typed_var(ctx, name, device_type);
            free(device_type);
        }
        free(ann_type_name);
        return;
    }

    if (ann != NULL && ann->type == AST_TYPE && ann->data.type.name != NULL) {
        const char *ann_name = ann->data.type.name;
        if (ann_name != NULL && ann_name[0] != '\0'
        && (strcmp(ann_name, "ReadView") == 0
            || strncmp(ann_name, "ReadView<", 9) == 0
            || strcmp(ann_name, "WriteView") == 0
            || strncmp(ann_name, "WriteView<", 10) == 0
            || strcmp(ann_name, "MoveToken") == 0
            || strncmp(ann_name, "MoveToken<", 10) == 0)
        && init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && init->data.call.arg_count >= 1
        && init->data.call.arguments[0] != NULL
        && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        const char *source_name = init->data.call.arguments[0]->data.identifier.name;
        bool is_view_decl = (strcmp(callee_name, "ViewRead") == 0
                          || strcmp(callee_name, "ViewWrite") == 0);
        bool is_move_decl = strcmp(callee_name, "Move") == 0;
        if (is_view_decl || is_move_decl) {
            const char *inner = "Int";
            bool source_secure = lookup_slot_is_secure(ctx, source_name);
            const char *ctype;
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                inner = ann->data.type.generic_args->params[0]->name;
            } else {
                inner = slot_inner_type_name(ann_name);
            }
            ctype = source_secure ? pergyra_type_to_c("SecureSlot<Int>") : pergyra_type_to_c("Slot<Int>");
            if (source_secure) {
                char secure_name[128];
                snprintf(secure_name, sizeof(secure_name), "SecureSlot<%s>", inner);
                ctype = pergyra_type_to_c(secure_name);
            } else {
                char slot_name_buf[128];
                snprintf(slot_name_buf, sizeof(slot_name_buf), "Slot<%s>", inner);
                ctype = pergyra_type_to_c(slot_name_buf);
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = %s;\n", ctype, name, source_name);
            if (source_secure) {
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyToken_%s %s_token = %s_token;\n",
                    inner, name, source_name);
            }
            register_view_like_var(ctx, name,
                ann_type_name != NULL ? ann_type_name : ann_name,
                source_name, source_secure, is_move_decl);
            if (is_move_decl) {
                for (int i = 0; i < ctx->slot_var_count; i++) {
                    if (strcmp(ctx->slot_vars[i].name, source_name) == 0) {
                        ctx->slot_vars[i].released = true;
                        break;
                    }
                }
            }
            free(ann_type_name);
            return;
        }
    }
    }

    /* Slot sugar: let x: Slot<Int> = 42 → auto Claim + Write */
    if (!is_slot && ann != NULL && ann->type == AST_TYPE) {
        const char *ann_name = ann->data.type.name;
        bool is_slot_sugar = false;
        bool sugar_secure = false;
        if (ann_name != NULL) {
            if (strcmp(ann_name, "Slot") == 0 || strncmp(ann_name, "Slot<", 5) == 0) {
                is_slot_sugar = true;
            } else if (strcmp(ann_name, "SecureSlot") == 0 || strncmp(ann_name, "SecureSlot<", 11) == 0) {
                is_slot_sugar = true;
                sugar_secure = true;
            }
        }
        if (is_slot_sugar) {
            const char *sugar_inner = "Int";
            if (ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                sugar_inner = ann->data.type.generic_args->params[0]->name;
            } else {
                sugar_inner = slot_inner_type_name(ann_name);
            }

            if (init != NULL && init->type == AST_IDENTIFIER) {
                TypedVarEntry *move_entry = lookup_typed_entry(ctx, init->data.identifier.name);
                if (move_entry != NULL && move_entry->is_move_token) {
                    write_indent(ctx);
                    if (sugar_secure) {
                        codebuf_write(ctx->out, "PgySecureSlot_%s %s = %s;\n",
                            sugar_inner, name, init->data.identifier.name);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "PgyToken_%s %s_token = %s_token;\n",
                            sugar_inner, name, init->data.identifier.name);
                    } else {
                        codebuf_write(ctx->out, "PgySlot_%s %s = %s;\n",
                            sugar_inner, name, init->data.identifier.name);
                    }
                    register_slot_var(ctx, name, sugar_inner, sugar_secure, false);
                    if (ann_type_name != NULL)
                        register_typed_var(ctx, name, ann_type_name);
                    free(ann_type_name);
                    return;
                }
            }

            register_slot_var(ctx, name, sugar_inner, sugar_secure, false);

            write_indent(ctx);
            if (sugar_secure) {
                codebuf_write(ctx->out,
                    "PgyToken_%s %s_token;\n", sugar_inner, name);
                write_indent(ctx);
                codebuf_write(ctx->out,
                    "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
                    sugar_inner, name, sugar_inner, name);
            } else {
                codebuf_write(ctx->out,
                    "PgySlot_%s %s = pgy_claim_%s();\n",
                    sugar_inner, name, sugar_inner);
            }

            /* Auto Write the initializer value */
            if (init != NULL) {
                char *init_expr = emit_expression(init, ctx);
                write_indent(ctx);
                if (sugar_secure) {
                    codebuf_write(ctx->out,
                        "pgy_secure_write_%s(&%s, %s, &%s_token);\n",
                        sugar_inner, name, init_expr, name);
                } else {
                    codebuf_write(ctx->out,
                        "pgy_write_%s(&%s, %s);\n",
                        sugar_inner, name, init_expr);
                }
                free(init_expr);
            }

            if (ann_type_name != NULL)
                register_typed_var(ctx, name, ann_type_name);
            free(ann_type_name);
            return;
        }
    }

    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "BoxArray") == 0) {
        const char *inner = "Int";
        if (ann_type_name != NULL && strncmp(ann_type_name, "Box<Array<", 10) == 0) {
            inner = ann_type_name + 10;
            const char *close = strstr(inner, ">>");
            static char inner_buf[64];
            size_t len = close != NULL ? (size_t)(close - inner) : strlen(inner);
            if (len >= sizeof(inner_buf))
                len = sizeof(inner_buf) - 1;
            memcpy(inner_buf, inner, len);
            inner_buf[len] = '\0';
            inner = inner_buf;
        }

        char *capacity = (init->data.call.arg_count > 0)
                         ? emit_expression(init->data.call.arguments[0], ctx)
                         : pergyra_strdup("0");
        char *allocator = (init->data.call.arg_count > 1)
                          ? emit_expression(init->data.call.arguments[1], ctx)
                          : pergyra_strdup("NULL");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyBoxArray_%s %s = pgy_box_array_new_%s(%s, %s);\n",
            inner, name, inner, capacity, allocator);
        register_typed_var(ctx, name,
            ann_type_name != NULL ? ann_type_name : "Box<Array<Int>>");
        free(capacity);
        free(allocator);
        free(ann_type_name);
        return;
    }

    /* Detect Box<T> - Type inference from Box(value) */
    bool is_box = false;
    const char *box_inner = "Int";
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name = init->data.call.callee->data.identifier.name;
        if (strcmp(callee_name, "Box") == 0) {
            is_box = true;
            /* Infer type from annotation or argument */
            if (ann != NULL && ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                box_inner = ann->data.type.generic_args->params[0]->name;
            } else if (init->data.call.arg_count > 0) {
                /* Infer from argument type */
                ASTNode *arg = init->data.call.arguments[0];
                if (arg->type == AST_NUMBER) box_inner = "Int";
                else if (arg->type == AST_STRING) box_inner = "String";
                else if (arg->type == AST_BOOLEAN) box_inner = "Bool";
            }
        }
        /* Rc<T> - Reference counted box */
        else if (strcmp(callee_name, "Rc") == 0) {
            is_box = true;
            if (ann != NULL && ann->data.type.generic_args != NULL
                && ann->data.type.generic_args->count > 0) {
                box_inner = ann->data.type.generic_args->params[0]->name;
            } else if (init->data.call.arg_count > 0) {
                ASTNode *arg = init->data.call.arguments[0];
                if (arg->type == AST_NUMBER) box_inner = "Int";
                else if (arg->type == AST_STRING) box_inner = "String";
                else if (arg->type == AST_BOOLEAN) box_inner = "Bool";
            }
        }
    }

    if (is_box) {
        write_indent(ctx);
        codebuf_write(ctx->out, "PgyBox_%s %s = pgy_box_new_%s(", 
                      box_inner, name, box_inner);
        if (init->data.call.arg_count > 0) {
            char *arg = emit_expression(init->data.call.arguments[0], ctx);
            codebuf_write(ctx->out, "%s", arg);
            free(arg);
        }
        codebuf_write(ctx->out, ");\n");
        register_typed_var(ctx, name,
            ann_type_name != NULL ? ann_type_name : "Box<Int>");
        free(ann_type_name);
        return;
    }

    if (ann_type_name != NULL && strncmp(ann_type_name, "Channel<", 8) == 0) {
        const char *inner = slot_inner_type_name(ann_type_name);
        char *capacity = pergyra_strdup("16");

        if (init != NULL && init->type == AST_CALL
            && init->data.call.arg_count > 0) {
            free(capacity);
            capacity = emit_expression(init->data.call.arguments[0], ctx);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "PgyChannel_%s %s;\n", inner, name);
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_channel_init_%s(&%s, %s);\n",
            inner, name, capacity);
        register_typed_var(ctx, name, ann_type_name);
        free(capacity);
        free(ann_type_name);
        return;
    }

    if (ann_type_name != NULL && strncmp(ann_type_name, "Option<", 7) == 0) {
        const char *inner = slot_inner_type_name(ann_type_name);
        if (init != NULL
            && init->type == AST_CALL
            && init->data.call.callee != NULL
            && init->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee_name = init->data.call.callee->data.identifier.name;
            if (strcmp(callee_name, "Some") == 0 && init->data.call.arg_count == 1) {
                char *arg = emit_expression(init->data.call.arguments[0], ctx);
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyOption_%s %s = Some_%s(%s);\n",
                    inner, name, inner, arg);
                register_typed_var(ctx, name, ann_type_name);
                free(arg);
                free(ann_type_name);
                return;
            }
            if (strcmp(callee_name, "None") == 0 && init->data.call.arg_count == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "PgyOption_%s %s = None_%s();\n",
                    inner, name, inner);
                register_typed_var(ctx, name, ann_type_name);
                free(ann_type_name);
                return;
            }
        }
    }

    /* Array literal: let arr = [1, 2, 3] → PgyArray_Int arr = ({ ... }); */
    if (init != NULL && init->type == AST_ARRAY_LITERAL) {
        const char *array_type_name = ann_type_name != NULL
            ? ann_type_name
            : infer_expression_type_name(ctx, init);
        const char *array_c_type = pergyra_type_to_c(array_type_name);
        char *init_expr = emit_expression(init, ctx);
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = %s;\n", array_c_type, name, init_expr);
        free(init_expr);
        register_typed_var(ctx, name, array_type_name);
        free(ann_type_name);
        return;
    }

    /* Normal variable with type inference */
    const char *c_type = "int32_t"; /* fallback */
    if (ann != NULL) {
        c_type = pergyra_ast_type_to_c(ann);
    } else if (init != NULL) {
        /* Type inference from initializer */
        if (init->type == AST_NUMBER)  c_type = "int32_t";
        else if (init->type == AST_STRING)  c_type = "char*";
        else if (init->type == AST_BOOLEAN) c_type = "bool";
        else if (init->type == AST_SPAWN_EXPR) c_type = "PgyTaskHandle";
        else if (init->type == AST_CHANNEL_RECV) {
            c_type = pergyra_type_to_c(infer_expression_type_name(ctx, init));
        }
        else if (init->type == AST_CALL) {
            c_type = pergyra_type_to_c(infer_expression_type_name(ctx, init));
        }
        else if (init->type == AST_ARRAY_LITERAL)
            c_type = pergyra_type_to_c(infer_expression_type_name(ctx, init));
    }

    /* Struct/class constructor: let p: Point = Point(...)
     * Lower positional constructor args into field-order initialization.
     * Missing fields stay zero-initialized.
     * For generic classes: callee is "Node" but ann_type_name is "Node_Int",
     * so also match against the original class name via generic_class_spec_name. */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER
        && ann_type_name != NULL
        && (strcmp(init->data.call.callee->data.identifier.name, ann_type_name) == 0
            || (generic_class_spec_name != NULL
                && ann->data.type.name != NULL
                && strcmp(init->data.call.callee->data.identifier.name, ann->data.type.name) == 0))
        ) {
        ASTNode *class_decl = find_class_decl(ctx, ann_type_name);
        /* For generic classes, find_class_decl won't find "Node_Int" —
         * fall back to the original generic class declaration. */
        if (class_decl == NULL && generic_class_spec_name != NULL
            && ann->data.type.name != NULL)
            class_decl = find_class_decl(ctx, ann->data.type.name);
        write_indent(ctx);
        if (class_decl != NULL
            && class_decl->type == AST_CLASS_DECL
            && class_decl->data.class_decl.field_count > 0
            && init->data.call.arg_count > 0) {
            codebuf_write(ctx->out, "%s %s = { ", ann_type_name, name);
            for (size_t i = 0; i < init->data.call.arg_count; i++) {
                ClassField *field;
                char *arg_expr;
                if (i >= class_decl->data.class_decl.field_count)
                    break;
                field = class_decl->data.class_decl.fields[i];
                if (field == NULL || field->name == NULL)
                    continue;
                arg_expr = emit_expression(init->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(ctx->out, ", ");
                codebuf_write(ctx->out, ".%s = %s", field->name, arg_expr);
                free(arg_expr);
            }
            codebuf_write(ctx->out, " };\n");
        } else {
            codebuf_write(ctx->out, "%s %s = {0};\n", ann_type_name, name);
        }
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
        return;
    }

    write_indent(ctx);
    if (init != NULL) {
        char *init_expr = emit_expression(init, ctx);
        codebuf_write(ctx->out, "%s %s = %s;\n", c_type, name, init_expr);
        free(init_expr);
    } else {
        codebuf_write(ctx->out, "%s %s = 0;\n", c_type, name);
    }

    if (ann_type_name != NULL) {
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
    } else if (init != NULL && init->type == AST_CALL) {
        register_typed_var(ctx, name, infer_expression_type_name(ctx, init));
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        char *future_type = infer_spawn_return_type_name(ctx, init);
        char *wrapped = strdup_fmt("Future<%s>", future_type);
        register_typed_var(ctx, name, wrapped);
        free(future_type);
        free(wrapped);
    } else if (init != NULL && init->type == AST_CHANNEL_RECV) {
        const char *inner = infer_expression_type_name(ctx, init);
        register_typed_var(ctx, name, inner);
    }
}

/* -----------------------------------------------------------------
 * Function declaration emitter
 * ----------------------------------------------------------------- */

static void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    const char *ret_type = "void";
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    g_type_render_ctx = ctx;
    if (node->data.func_decl.return_type != NULL)
        ret_type = pergyra_ast_type_to_c(node->data.func_decl.return_type);

    codebuf_write(buf, "%s %s(", ret_type, name);
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = "int32_t";
        char *type_name = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (i > 0)
            codebuf_write(buf, ", ");
        if (p->type != NULL)
            type_name = render_type_name(p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            const char *inner = slot_inner_type_name(type_name);
            codebuf_write(buf, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(buf, ", PgyToken_%s %s_token", inner, p->name);
        } else {
            codebuf_write(buf, "%s %s", pt, p->name);
        }
        free(type_name);
    }
    codebuf_write(buf, ");\n");
    g_type_render_ctx = saved_render_ctx;
}

static void
emit_func_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    emit_func_forward_decl_named(node, node->data.func_decl.name, buf, ctx);
}

void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;
    CodeBuf *saved_out = ctx->out;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    char saved_return_type[128];

    const char *ret_type = "void";
    ctx->out = buf;
    g_type_render_ctx = ctx;
    snprintf(saved_return_type, sizeof(saved_return_type), "%s",
        ctx->current_return_type);
    if (node->data.func_decl.return_type != NULL) {
        ret_type = pergyra_ast_type_to_c(node->data.func_decl.return_type);
        {
            char *rendered = render_type_name(node->data.func_decl.return_type);
            snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                "%s", rendered);
            free(rendered);
        }
    } else {
        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type), "Void");
    }

    codebuf_write(ctx->out, "\n%s\n%s(", ret_type, name);
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = "int32_t";
        char *type_name = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        if (i > 0) codebuf_write(ctx->out, ", ");
        if (p->type != NULL)
            type_name = render_type_name(p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            const char *inner = slot_inner_type_name(type_name);
            codebuf_write(ctx->out, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(ctx->out, ", PgyToken_%s %s_token", inner, p->name);
        } else {
            codebuf_write(ctx->out, "%s %s", pt, p->name);
        }
        free(type_name);
    }
    codebuf_write(ctx->out, ")\n");
    codebuf_write(ctx->out, "{\n");

    ctx->indent++;
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        if (p == NULL || p->name == NULL || p->type == NULL)
            continue;
        char *type_name = render_type_name(p->type);
        if (type_name != NULL) {
            bool boundary_slot = (strncmp(type_name, "Slot<", 5) == 0
                               || strncmp(type_name, "SecureSlot<", 11) == 0)
                && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
            register_typed_var(ctx, p->name, type_name);
            if (strncmp(type_name, "Slot<", 5) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), false, boundary_slot);
            else if (strncmp(type_name, "SecureSlot<", 11) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), true, boundary_slot);
            free(type_name);
        }
    }
    if (node->data.func_decl.body != NULL)
        emit_block(node->data.func_decl.body, ctx);
    ctx->indent--;
    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
        "%s", saved_return_type);

    codebuf_write(ctx->out, "}\n");
    g_type_render_ctx = saved_render_ctx;
    ctx->out = saved_out;
}

void
emit_func_decl(ASTNode *node, TranspilerCtx *ctx)
{
    emit_func_decl_named(node, node->data.func_decl.name, ctx->out, ctx);
}

void
emit_extern_block(ASTNode *node, TranspilerCtx *ctx)
{
    codebuf_write(ctx->out, "\n/* extern \"%s\" */\n",
                  node->data.extern_block.abi != NULL
                    ? node->data.extern_block.abi : "");

    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl == NULL || decl->type != AST_FUNC_DECL)
            continue;

        const char *name = decl->data.func_decl.name;
        const char *ret_type = "void";
        if (decl->data.func_decl.return_type != NULL) {
            ret_type = pergyra_ast_type_to_c(decl->data.func_decl.return_type);
        }

        codebuf_write(ctx->out, "%s %s(", ret_type, name);

        for (size_t j = 0; j < decl->data.func_decl.param_count; j++) {
            FuncParam *p = decl->data.func_decl.params[j];
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            if (j > 0) {
                codebuf_write(ctx->out, ", ");
            }
            codebuf_write(ctx->out, "%s %s", pt, p->name);
        }

        codebuf_write(ctx->out, ");\n");
    }
}

/* -----------------------------------------------------------------
 * Class declaration emitter
 * ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
 * Generic class monomorphization
 * ----------------------------------------------------------------- */

static bool
class_has_generic_params(ASTNode *node)
{
    return node != NULL
        && node->type == AST_CLASS_DECL
        && node->data.class_decl.generic_params != NULL
        && node->data.class_decl.generic_params->count > 0;
}

/* Ensure a monomorphized specialization of a generic class exists.
 * Returns the specialized name (e.g. "Node_Int") that should be used
 * as the C struct type name.  The struct + methods are emitted into
 * ctx->helpers on first invocation.
 *
 * `ann` is the AST_TYPE node for the annotation (e.g. Node<Int>).
 * We extract generic_args from it and match them to class_decl's
 * generic_params to build the bindings. */
static const char *
ensure_generic_class_specialization(TranspilerCtx *ctx,
                                     ASTNode *class_decl,
                                     ASTNode *ann)
{
    GenericParams *gp = class_decl->data.class_decl.generic_params;
    GenericParams *ga = ann->data.type.generic_args;
    if (gp == NULL || ga == NULL || gp->count != ga->count)
        return class_decl->data.class_decl.name;

    /* Build specialized name: ClassName_Arg1_Arg2 */
    CodeBuf *nbuf = codebuf_create();
    codebuf_write(nbuf, "%s", class_decl->data.class_decl.name);
    for (size_t i = 0; i < ga->count; i++) {
        codebuf_write(nbuf, "_");
        GenericParam *garg = ga->params[i];
        if (garg != NULL && garg->name != NULL)
            append_mangled_type_name(nbuf, garg->name);
        else if (garg != NULL && garg->constraint != NULL
                 && garg->constraint->type == AST_TYPE
                 && garg->constraint->data.type.name != NULL)
            append_mangled_type_name(nbuf, garg->constraint->data.type.name);
        else
            codebuf_write(nbuf, "Unknown");
    }

    /* Check if already emitted */
    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, nbuf->data) == 0) {
            const char *result = ctx->generic_class_specs[i].specialized_name;
            codebuf_destroy(nbuf);
            return result;
        }
    }

    /* Register new specialization */
    if (ctx->generic_class_spec_count >= MAX_GENERIC_CLASS_SPECIALIZATIONS) {
        codebuf_destroy(nbuf);
        return class_decl->data.class_decl.name;
    }

    GenericClassSpecEntry *entry = &ctx->generic_class_specs[ctx->generic_class_spec_count++];
    entry->class_decl = class_decl;
    snprintf(entry->specialized_name, sizeof(entry->specialized_name), "%s", nbuf->data);
    entry->emitted = true;
    const char *spec_name = entry->specialized_name;

    /* Build bindings: T -> Int, U -> String, etc. */
    int saved_binding_count = ctx->generic_binding_count;
    for (size_t i = 0; i < gp->count; i++) {
        if (ctx->generic_binding_count >= MAX_GENERIC_BINDINGS)
            break;
        GenericBindingEntry *b = &ctx->generic_bindings[ctx->generic_binding_count++];
        snprintf(b->name, sizeof(b->name), "%s",
                 gp->params[i] != NULL ? gp->params[i]->name : "T");
        /* The concrete type name comes from the annotation's generic_args.
         * ga->params[i] is a GenericParam whose 'name' field holds the
         * actual type name (e.g. "Int") when used as a type argument. */
        const char *concrete = "int32_t";
        if (ga->params[i] != NULL && ga->params[i]->name != NULL)
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s",
                     ga->params[i]->name);
        else if (ga->params[i] != NULL && ga->params[i]->constraint != NULL
                 && ga->params[i]->constraint->type == AST_TYPE)
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s",
                     ga->params[i]->constraint->data.type.name);
        else
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s", concrete);

        entry->bindings[i] = *b;
    }
    entry->binding_count = gp->count;

    /* Set g_type_render_ctx so pergyra_ast_type_to_c resolves T → Int */
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    g_type_render_ctx = ctx;

    /* Emit struct typedef into helpers buffer */
    codebuf_write(ctx->helpers, "\ntypedef struct %s\n{\n", spec_name);
    for (size_t i = 0; i < class_decl->data.class_decl.field_count; i++) {
        ClassField *f = class_decl->data.class_decl.fields[i];
        const char *ft = "int32_t";
        if (f->type != NULL)
            ft = pergyra_ast_type_to_c(f->type);
        codebuf_write(ctx->helpers, "    %s %s;\n", ft, f->name);
    }
    codebuf_write(ctx->helpers, "} %s;\n", spec_name);

    /* Container types */
    codebuf_write(ctx->helpers,
        "\nPGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n",
        spec_name, spec_name,
        spec_name, spec_name,
        spec_name, spec_name);

    /* Methods */
    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        bool use_self_cell = class_decl->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type    = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        if (use_self_cell) {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s *self",
                          ret_type, spec_name, method_name, spec_name);
        } else {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s self",
                          ret_type, spec_name, method_name, spec_name);
        }

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->helpers, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->helpers, ")\n{\n");

        {
            int saved_slot_count = ctx->slot_var_count;
            int saved_typed_count = ctx->typed_var_count;
            const char *saved_class_name = ctx->current_class_name;
            CodeBuf *saved_out = ctx->out;

            ctx->current_class_name = spec_name;
            ctx->out = ctx->helpers;
            register_typed_var(ctx, "self", spec_name);

            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                char *tn;
                if (p == NULL || p->name == NULL
                    || strcmp(p->name, "self") == 0 || p->type == NULL)
                    continue;
                tn = render_type_name(p->type);
                if (tn != NULL) {
                    register_typed_var(ctx, p->name, tn);
                    free(tn);
                }
            }

            ctx->indent++;
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            ctx->indent--;

            ctx->out = saved_out;
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            ctx->current_class_name = saved_class_name;
        }

        codebuf_write(ctx->helpers, "}\n");
    }

    /* Restore bindings and render context */
    g_type_render_ctx = saved_render_ctx;
    ctx->generic_binding_count = saved_binding_count;
    codebuf_destroy(nbuf);

    return spec_name;
}

void
emit_class_decl(ASTNode *node, TranspilerCtx *ctx)
{
    /* Generic classes are emitted lazily when first used (monomorphized). */
    if (class_has_generic_params(node))
        return;

    const char *name = node->data.class_decl.name;

    codebuf_write(ctx->out, "\ntypedef struct %s\n{\n", name);

    /* Fields */
    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        const char *ft = "int32_t";
        if (f->type != NULL)
            ft = pergyra_ast_type_to_c(f->type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, f->name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Auto-generate Slot and Result container types for this struct.
     * This allows Slot<MyStruct>, Result<MyStruct> in user code. */
    codebuf_write(ctx->out,
        "\n/* Auto-generated container types for %s */\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n",
        name,
        name, name,
        name, name,
        name, name);

    /* Methods become free functions over a subject self-cell or class value. */
    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        bool use_self_cell = node->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type    = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        if (use_self_cell) {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                          ret_type, name, method_name, name);
        } else {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                          ret_type, name, method_name, name);
        }

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");

        {
            int saved_slot_count = ctx->slot_var_count;
            int saved_typed_count = ctx->typed_var_count;
            const char *saved_class_name = ctx->current_class_name;

            ctx->current_class_name = name;
            register_typed_var(ctx, "self", name);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                char *type_name;

                if (p == NULL || p->name == NULL
                    || strcmp(p->name, "self") == 0
                    || p->type == NULL)
                    continue;

                type_name = render_type_name(p->type);
                if (type_name != NULL) {
                    register_typed_var(ctx, p->name, type_name);
                    free(type_name);
                }
            }
            ctx->indent++;
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            ctx->indent--;
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            ctx->current_class_name = saved_class_name;
        }

        codebuf_write(ctx->out, "}\n");
    }
}

/* -----------------------------------------------------------------
 * with slot<T> as s { }
 * ----------------------------------------------------------------- */

void
emit_with_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias = node->data.with_stmt.alias;
    bool is_secure    = node->data.with_stmt.is_secure;
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;

    const char *inner = "Int";
    if (node->data.with_stmt.slot_type != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    /* Register alias in slot variable table */
    register_slot_var(ctx, alias, inner, is_secure, false);

    if (is_secure) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyToken_%s %s_token;\n", inner, alias);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
            inner, alias, inner, alias);
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySlot_%s %s = pgy_claim_%s();\n",
            inner, alias, inner);
    }

    if (node->data.with_stmt.body != NULL)
        emit_block(node->data.with_stmt.body, ctx);

    /* Auto-release */
    write_indent(ctx);
    if (is_secure) {
        codebuf_write(ctx->out,
            "pgy_secure_release_%s(&%s, &%s_token);\n",
            inner, alias, alias);
    } else {
        codebuf_write(ctx->out,
            "pgy_release_%s(&%s);\n", inner, alias);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
}

/* -----------------------------------------------------------------
 * Parallel block
 * ----------------------------------------------------------------- */

void
emit_parallel_block(ASTNode *node, TranspilerCtx *ctx)
{
    size_t count = node->data.parallel.task_count;
    if (count == 0)
        return;

    unsigned int pid = ctx->parallel_id++;

    /* ---------------------------------------------------------------
     * 1) Generate a context struct that holds pointers to all local
     *    variables currently in scope (slots + non-duplicate typed vars).
     *    Wrapper functions access outer variables through this struct.
     * --------------------------------------------------------------- */
    int n_slots  = ctx->slot_var_count;
    int n_typed  = ctx->typed_var_count;

    /* Helper: check if a typed_var is already in slot_vars (avoid dupes) */
    #define IS_SLOT_DUP(name_str) ({ \
        bool _dup = false; \
        for (int _j = 0; _j < n_slots; _j++) { \
            if (strcmp(ctx->slot_vars[_j].name, (name_str)) == 0) \
                { _dup = true; break; } \
        } _dup; })

    /* Count non-duplicate typed vars */
    int n_unique_typed = 0;
    for (int i = 0; i < n_typed; i++) {
        if (!IS_SLOT_DUP(ctx->typed_vars[i].name))
            n_unique_typed++;
    }

    bool has_captures = (n_slots > 0 || n_unique_typed > 0);

    if (has_captures) {
        codebuf_write(ctx->helpers,
            "typedef struct {\n");
        for (int i = 0; i < n_slots; i++) {
            codebuf_write(ctx->helpers,
                "    PgySlot_%s *%s;\n",
                ctx->slot_vars[i].inner_type,
                ctx->slot_vars[i].name);
        }
        for (int i = 0; i < n_typed; i++) {
            if (IS_SLOT_DUP(ctx->typed_vars[i].name))
                continue;
            const char *c_type = pergyra_type_to_c(ctx->typed_vars[i].type_name);
            codebuf_write(ctx->helpers,
                "    %s *%s;\n", c_type, ctx->typed_vars[i].name);
        }
        codebuf_write(ctx->helpers,
            "} _pgy_par_ctx_%u;\n\n", pid);
    }

    /* ---------------------------------------------------------------
     * 2) Generate static wrapper functions for each task.
     *    Variable references inside the wrapper go through _pctx->.
     * --------------------------------------------------------------- */
    for (size_t i = 0; i < count; i++) {
        codebuf_write(ctx->helpers,
            "static void *_pgy_par_%zu_%u(void *_arg) {\n",
            i, pid);
        if (has_captures) {
            codebuf_write(ctx->helpers,
                "    _pgy_par_ctx_%u *_pctx = "
                "(_pgy_par_ctx_%u *)_arg;\n",
                pid, pid);
        } else {
            codebuf_write(ctx->helpers, "    (void)_arg;\n");
        }

        /* Redirect output to helpers and set parallel-capture mode */
        CodeBuf *saved = ctx->out;
        int saved_indent = ctx->indent;
        bool saved_in_pw = ctx->in_parallel_wrapper;
        int saved_slot_end  = ctx->par_capture_slot_end;
        int saved_typed_end = ctx->par_capture_typed_end;

        ctx->out = ctx->helpers;
        ctx->indent = 1;
        ctx->in_parallel_wrapper  = true;
        ctx->par_capture_slot_end  = n_slots;
        ctx->par_capture_typed_end = n_typed;

        emit_statement(node->data.parallel.tasks[i], ctx);

        ctx->out = saved;
        ctx->indent = saved_indent;
        ctx->in_parallel_wrapper  = saved_in_pw;
        ctx->par_capture_slot_end  = saved_slot_end;
        ctx->par_capture_typed_end = saved_typed_end;

        codebuf_write(ctx->helpers,
            "    return NULL;\n"
            "}\n\n");
    }

    /* ---------------------------------------------------------------
     * 3) Emit context initialization + spawn + await at call site.
     * --------------------------------------------------------------- */
    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    if (has_captures) {
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_par_ctx_%u _pctx%u = { ", pid, pid);
        bool first = true;
        for (int i = 0; i < n_slots; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", ctx->slot_vars[i].name);
            first = false;
        }
        for (int i = 0; i < n_typed; i++) {
            if (IS_SLOT_DUP(ctx->typed_vars[i].name))
                continue;
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", ctx->typed_vars[i].name);
            first = false;
        }
        codebuf_write(ctx->out, " };\n");
    }

    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        if (has_captures) {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_spawn(_pgy_par_%zu_%u, &_pctx%u);\n",
                i, i, pid, pid);
        } else {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_spawn(_pgy_par_%zu_%u, NULL);\n",
                i, i, pid);
        }
    }
    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_await(_ph_%zu);\n", i);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    #undef IS_SLOT_DUP
}

static void
emit_async_block(ASTNode *node, TranspilerCtx *ctx)
{
    unsigned int pid = ctx->parallel_id++;
    int n_slots  = ctx->slot_var_count;
    int n_typed  = ctx->typed_var_count;

    #define IS_SLOT_DUP(name_str) ({ \
        bool _dup = false; \
        for (int _j = 0; _j < n_slots; _j++) { \
            if (strcmp(ctx->slot_vars[_j].name, (name_str)) == 0) \
                { _dup = true; break; } \
        } _dup; })

    int n_unique_typed = 0;
    for (int i = 0; i < n_typed; i++) {
        if (!IS_SLOT_DUP(ctx->typed_vars[i].name))
            n_unique_typed++;
    }

    bool has_captures = (n_slots > 0 || n_unique_typed > 0);

    if (has_captures) {
        codebuf_write(ctx->helpers, "typedef struct {\n");
        for (int i = 0; i < n_slots; i++) {
            codebuf_write(ctx->helpers, "    PgySlot_%s *%s;\n",
                ctx->slot_vars[i].inner_type, ctx->slot_vars[i].name);
        }
        for (int i = 0; i < n_typed; i++) {
            if (IS_SLOT_DUP(ctx->typed_vars[i].name))
                continue;
            codebuf_write(ctx->helpers, "    %s *%s;\n",
                pergyra_type_to_c(ctx->typed_vars[i].type_name),
                ctx->typed_vars[i].name);
        }
        codebuf_write(ctx->helpers, "} _pgy_async_ctx_%u;\n\n", pid);
    }

    codebuf_write(ctx->helpers, "static void *_pgy_async_%u(void *_arg) {\n", pid);
    if (has_captures) {
        codebuf_write(ctx->helpers,
            "    _pgy_async_ctx_%u *_pctx = (_pgy_async_ctx_%u *)_arg;\n",
            pid, pid);
    } else {
        codebuf_write(ctx->helpers, "    (void)_arg;\n");
    }

    CodeBuf *saved = ctx->out;
    int saved_indent = ctx->indent;
    bool saved_in_pw = ctx->in_parallel_wrapper;
    int saved_slot_end  = ctx->par_capture_slot_end;
    int saved_typed_end = ctx->par_capture_typed_end;

    ctx->out = ctx->helpers;
    ctx->indent = 1;
    ctx->in_parallel_wrapper  = true;
    ctx->par_capture_slot_end  = n_slots;
    ctx->par_capture_typed_end = n_typed;

    for (size_t i = 0; i < node->data.async_block.statement_count; i++)
        emit_statement(node->data.async_block.statements[i], ctx);

    ctx->out = saved;
    ctx->indent = saved_indent;
    ctx->in_parallel_wrapper  = saved_in_pw;
    ctx->par_capture_slot_end  = saved_slot_end;
    ctx->par_capture_typed_end = saved_typed_end;

    codebuf_write(ctx->helpers, "    return NULL;\n}\n\n");

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (has_captures) {
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_async_ctx_%u _pctx%u = { ", pid, pid);
        bool first = true;
        for (int i = 0; i < n_slots; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", ctx->slot_vars[i].name);
            first = false;
        }
        for (int i = 0; i < n_typed; i++) {
            if (IS_SLOT_DUP(ctx->typed_vars[i].name))
                continue;
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", ctx->typed_vars[i].name);
            first = false;
        }
        codebuf_write(ctx->out, " };\n");
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "PgyTaskHandle _ah_%u = pgy_async_spawn(_pgy_async_%u, %s);\n",
        pid, pid, has_captures ? "&_pctx" : "NULL");
    write_indent(ctx);
    codebuf_write(ctx->out, "pgy_async_detach(_ah_%u);\n", pid);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    #undef IS_SLOT_DUP
}

/* -----------------------------------------------------------------
 * Control flow
 * ----------------------------------------------------------------- */

void
emit_if_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.if_stmt.condition, ctx);
    write_indent(ctx);
    codebuf_write(ctx->out, "if (%s)\n", cond);
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.if_stmt.then_branch != NULL)
        emit_block(node->data.if_stmt.then_branch, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    if (node->data.if_stmt.else_branch != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_statement(node->data.if_stmt.else_branch, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_for_loop(ASTNode *node, TranspilerCtx *ctx)
{
    const char *var = node->data.for_loop.variable;

    /* for-in collection loop: for item in array { } */
    if (node->data.for_loop.iterable != NULL) {
        char *coll = emit_expression(node->data.for_loop.iterable, ctx);
        const char *coll_type = infer_expression_type_name(ctx,
            node->data.for_loop.iterable);
        const char *elem_type = "int32_t";
        if (coll_type != NULL
            && (strncmp(coll_type, "Array<", 6) == 0
                || strncmp(coll_type, "Slice<", 6) == 0)) {
            elem_type = pergyra_type_to_c(slot_inner_type_name(coll_type));
        }

        int idx_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "for (size_t _pgy_idx_%d = 0; "
            "_pgy_idx_%d < %s.count; "
            "_pgy_idx_%d++)\n",
            idx_id, idx_id, coll, idx_id);
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = %s.data[_pgy_idx_%d];\n",
            elem_type, var, coll, idx_id);
        register_typed_var(ctx, var, slot_inner_type_name(coll_type));
        if (node->data.for_loop.body != NULL)
            emit_block(node->data.for_loop.body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        free(coll);
        return;
    }

    /* Range loop: for x in start..end { } */
    char *start = emit_expression(node->data.for_loop.range_start, ctx);
    char *end   = emit_expression(node->data.for_loop.range_end,   ctx);

    write_indent(ctx);
    codebuf_write(ctx->out,
        "for (int32_t %s = %s; %s < %s; %s++)\n",
        var, start, var, end, var);
    free(start);
    free(end);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.for_loop.body != NULL)
        emit_block(node->data.for_loop.body, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

void
emit_while_loop(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.while_loop.condition, ctx);
    write_indent(ctx);
    codebuf_write(ctx->out, "while (%s)\n", cond);
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.while_loop.body != NULL)
        emit_block(node->data.while_loop.body, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

/* Check if a match-case pattern is a destructor like Ok(x), Err(x),
 * or a tagged union variant like Circle(r), Rect(w, h) */
static bool
is_result_destructor(ASTNode *pat, const char **kind, const char **binding)
{
    if (pat == NULL || pat->type != AST_CALL)
        return false;
    if (pat->data.call.callee == NULL || pat->data.call.callee->type != AST_IDENTIFIER)
        return false;
    const char *name = pat->data.call.callee->data.identifier.name;
    if (strcmp(name, "Ok") != 0 && strcmp(name, "Err") != 0)
        return false;
    *kind = name;
    if (pat->data.call.arg_count > 0
        && pat->data.call.arguments[0] != NULL
        && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
        *binding = pat->data.call.arguments[0]->data.identifier.name;
    } else {
        *binding = NULL;
    }
    return true;
}

static bool
is_option_destructor(ASTNode *pat, const char **kind, const char **binding)
{
    *kind = NULL;
    *binding = NULL;

    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = pat->data.identifier.name;
        if (name != NULL && strcmp(name, "None") == 0) {
            *kind = "None";
            return true;
        }
        return false;
    }

    if (pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;

    if (strcmp(name, "None") == 0 && pat->data.call.arg_count == 0) {
        *kind = "None";
        return true;
    }
    if (strcmp(name, "Some") == 0 && pat->data.call.arg_count == 1) {
        *kind = "Some";
        if (pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }

    return false;
}

/* Check if pattern is a tagged union variant destructor: Circle(r), Rect(w, h), None */
static bool
is_enum_variant_destructor(ASTNode *pat, TranspilerCtx *ctx,
                           const char **variant_name_out,
                           const char **enum_name_out,
                           const char ***bindings_out,
                           size_t *binding_count_out)
{
    const char *name = NULL;
    size_t argc = 0;

    if (pat == NULL) return false;

    if (pat->type == AST_CALL
        && pat->data.call.callee != NULL
        && pat->data.call.callee->type == AST_IDENTIFIER) {
        name = pat->data.call.callee->data.identifier.name;
        argc = pat->data.call.arg_count;
    } else if (pat->type == AST_IDENTIFIER) {
        name = pat->data.identifier.name;
        argc = 0;
    } else {
        return false;
    }

    if (name == NULL) return false;

    /* Look up in HIR enum declarations */
    if (ctx->hir == NULL) return false;
    for (size_t i = 0; i < ctx->hir->type_count; i++) {
        ASTNode *stmt = ctx->hir->types[i];
        if (stmt == NULL || stmt->type != AST_ENUM_DECL)
            continue;
        /* Only tagged unions (at least one variant has data) */
        bool has_data = false;
        for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
            if (stmt->data.enum_decl.variant_param_counts != NULL
                && stmt->data.enum_decl.variant_param_counts[j] > 0) {
                has_data = true;
                break;
            }
        }
        if (!has_data) continue;

        for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
            if (strcmp(stmt->data.enum_decl.variants[j], name) == 0) {
                *variant_name_out = name;
                *enum_name_out = stmt->data.enum_decl.name;
                /* Collect bindings */
                static const char *bindings_buf[8];
                *binding_count_out = 0;
                for (size_t k = 0; k < argc && k < 8; k++) {
                    ASTNode *arg = pat->data.call.arguments[k];
                    if (arg != NULL && arg->type == AST_IDENTIFIER)
                        bindings_buf[k] = arg->data.identifier.name;
                    else
                        bindings_buf[k] = NULL;
                    (*binding_count_out)++;
                }
                *bindings_out = bindings_buf;
                return true;
            }
        }
    }
    return false;
}

void
emit_match_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *subj = emit_expression(node->data.match_stmt.subject, ctx);
    int tmp_id = ctx->tmp_counter++;
    const char *subject_type = infer_expression_type_name(ctx, node->data.match_stmt.subject);
    bool subject_is_option = subject_type != NULL && strncmp(subject_type, "Option<", 7) == 0;

    /* Detect if any case uses Ok()/Err() or tagged union destructuring */
    bool is_result_match = false;
    bool is_option_match = false;
    bool is_enum_match = false;
    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *pat = node->data.match_stmt.cases[i]->data.match_case.pattern;
        const char *k, *b;
        if (subject_is_option && is_option_destructor(pat, &k, &b)) {
            is_option_match = true;
            break;
        }
        if (is_result_destructor(pat, &k, &b)) {
            is_result_match = true;
            break;
        }
        const char *vn, *en; const char **bs; size_t bc;
        if (is_enum_variant_destructor(pat, ctx, &vn, &en, &bs, &bc)) {
            is_enum_match = true;
            break;
        }
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    if (is_result_match || is_option_match || is_enum_match) {
        /* Struct match: store subject as-is (tagged union/result) */
        write_indent(ctx);
        codebuf_write(ctx->out, "__typeof__(%s) __match_%d = %s;\n", subj, tmp_id, subj);
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out, "int32_t __match_%d = %s;\n", tmp_id, subj);
    }
    free(subj);

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        const char *kind = NULL, *binding = NULL;
        bool option_case = subject_is_option
            && is_option_destructor(mc->data.match_case.pattern, &kind, &binding);

        write_indent(ctx);

        if (option_case) {
            const char *tag_val = (strcmp(kind, "Some") == 0)
                ? "PgyOptionSome" : "PgyOptionNone";
            if (i == 0)
                codebuf_write(ctx->out, "if (__match_%d.tag == %s",
                    tmp_id, tag_val);
            else
                codebuf_write(ctx->out, "else if (__match_%d.tag == %s",
                    tmp_id, tag_val);
        } else if (is_result_destructor(mc->data.match_case.pattern, &kind, &binding)) {
            /* case Ok(x): → if (__match_N.tag == PGY_RESULT_OK) { ... } */
            const char *tag_val = (strcmp(kind, "Ok") == 0)
                ? "PgyResultOk" : "PgyResultErr";
            if (i == 0)
                codebuf_write(ctx->out, "if (__match_%d.tag == %s",
                    tmp_id, tag_val);
            else
                codebuf_write(ctx->out, "else if (__match_%d.tag == %s",
                    tmp_id, tag_val);
        } else {
            /* Check for tagged union variant pattern */
            const char *vname = NULL, *ename = NULL;
            const char **bindings = NULL;
            size_t bind_count = 0;
            if (is_enum_variant_destructor(mc->data.match_case.pattern, ctx,
                                            &vname, &ename, &bindings, &bind_count)) {
                if (i == 0)
                    codebuf_write(ctx->out, "if (__match_%d.tag == %s_TAG_%s",
                        tmp_id, ename, vname);
                else
                    codebuf_write(ctx->out, "else if (__match_%d.tag == %s_TAG_%s",
                        tmp_id, ename, vname);
                /* Mark for binding emission below */
                kind = vname;
            } else {
                char *pat = emit_expression(mc->data.match_case.pattern, ctx);
                if (i == 0)
                    codebuf_write(ctx->out, "if (__match_%d == %s", tmp_id, pat);
                else
                    codebuf_write(ctx->out, "else if (__match_%d == %s", tmp_id, pat);
                free(pat);
            }
        }

        if (mc->data.match_case.guard != NULL) {
            char *guard = emit_expression(mc->data.match_case.guard, ctx);
            codebuf_write(ctx->out, " && %s", guard);
            free(guard);
        }
        codebuf_write(ctx->out, ")\n");

        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;

        /* Emit binding variable if destructuring */
        if (binding != NULL && kind != NULL) {
            write_indent(ctx);
            if (strcmp(kind, "Some") == 0)
                codebuf_write(ctx->out, "__typeof__(__match_%d.value) %s = __match_%d.value;\n",
                    tmp_id, binding, tmp_id);
            else if (strcmp(kind, "Ok") == 0)
                codebuf_write(ctx->out, "int32_t %s = __match_%d.ok;\n",
                    binding, tmp_id);
            else if (strcmp(kind, "Err") == 0)
                codebuf_write(ctx->out, "PgyError %s = __match_%d.err;\n",
                    binding, tmp_id);
        }
        /* Emit enum variant bindings: case Circle(r) → int32_t r = __match_N.Circle._0 */
        if (kind != NULL
            && strcmp(kind, "Some") != 0
            && strcmp(kind, "None") != 0
            && strcmp(kind, "Ok") != 0
            && strcmp(kind, "Err") != 0) {
            const char *vn2 = NULL, *en2 = NULL;
            const char **bs2 = NULL;
            size_t bc2 = 0;
            if (is_enum_variant_destructor(mc->data.match_case.pattern, ctx,
                                            &vn2, &en2, &bs2, &bc2)) {
                for (size_t b = 0; b < bc2; b++) {
                    if (bs2[b] != NULL) {
                        write_indent(ctx);
                        codebuf_write(ctx->out,
                            "int32_t %s = __match_%d.%s._%zu;\n",
                            bs2[b], tmp_id, vn2, b);
                    }
                }
            }
        }

        emit_block(mc->data.match_case.body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    if (node->data.match_stmt.default_body != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_block(node->data.match_stmt.default_body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

void
emit_return_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    write_indent(ctx);
    if (node->data.return_stmt.value != NULL) {
        if (ctx->current_return_type[0] != '\0'
            && strncmp(ctx->current_return_type, "Option<", 7) == 0) {
            const char *inner = slot_inner_type_name(ctx->current_return_type);
            ASTNode *value = node->data.return_stmt.value;
            if (value->type == AST_CALL
                && value->data.call.callee != NULL
                && value->data.call.callee->type == AST_IDENTIFIER) {
                const char *callee_name = value->data.call.callee->data.identifier.name;
                if (strcmp(callee_name, "Some") == 0 && value->data.call.arg_count == 1) {
                    char *arg = emit_expression(value->data.call.arguments[0], ctx);
                    codebuf_write(ctx->out, "return Some_%s(%s);\n", inner, arg);
                    free(arg);
                    return;
                }
                if (strcmp(callee_name, "None") == 0 && value->data.call.arg_count == 0) {
                    codebuf_write(ctx->out, "return None_%s();\n", inner);
                    return;
                }
            }
        }
        char *val = emit_expression(node->data.return_stmt.value, ctx);
        codebuf_write(ctx->out, "return %s;\n", val);
        free(val);
    } else {
        codebuf_write(ctx->out, "return;\n");
    }
}

/* -----------------------------------------------------------------
 * Statement dispatcher
 * ----------------------------------------------------------------- */

void
emit_statement(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_LET_DECL:
        emit_let_decl(node, ctx);
        break;
    case AST_LET_DESTRUCTURE:
    {
        /* let (a, b, c) = expr;
         * → auto _tmp = expr;
         *   Type a = _tmp.data[0]; // Array
         *   Type b = _tmp.data[1]; */
        ASTNode *init = node->data.let_destructure.initializer;
        char *init_expr = emit_expression(init, ctx);
        const char *init_type = infer_expression_type_name(ctx, init);
        const char *c_init_type = pergyra_type_to_c(init_type);
        const char *elem_c_type = "int32_t";
        const char *inner = "Int";
        if (init_type != NULL
            && (strncmp(init_type, "Array<", 6) == 0
                || strncmp(init_type, "Slice<", 6) == 0)) {
            inner = slot_inner_type_name(init_type);
            elem_c_type = pergyra_type_to_c(inner);
        }
        int tmp_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s _pgy_destr_%d = %s;\n",
            c_init_type, tmp_id, init_expr);
        for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = _pgy_destr_%d.data[%zu];\n",
                elem_c_type, node->data.let_destructure.names[i], tmp_id, i);
            register_typed_var(ctx, node->data.let_destructure.names[i], inner);
        }
        free(init_expr);
        break;
    }
    case AST_FUNC_DECL:
        emit_func_decl(node, ctx);
        break;
    case AST_CLASS_DECL:
        emit_class_decl(node, ctx);
        break;
    case AST_EXTERN_BLOCK:
        emit_extern_block(node, ctx);
        break;
    case AST_IMPORT_DECL:
        /* Import is resolved at driver level (AST merging).
         * Nothing to emit — the imported declarations are
         * already present in the merged AST. */
        break;
    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } → emit body directly (no safety wrappers) */
        write_indent(ctx);
        codebuf_write(ctx->out, "/* unsafe */\n");
        if (node->data.unsafe_block.body != NULL)
            emit_block(node->data.unsafe_block.body, ctx);
        break;
    case AST_DEFER_STMT: {
        /* defer { body } → GCC __attribute__((cleanup)) pattern.
         * 1. Emit a static cleanup function into the helpers buffer.
         * 2. Declare a sentinel variable with cleanup attribute.
         * When the sentinel goes out of scope, GCC calls the cleanup. */
        int defer_id = ctx->defer_counter++;

        /* Generate cleanup function in helpers buffer */
        codebuf_write(ctx->helpers,
            "\nstatic void _pgy_defer_%d(int *_pgy_unused) {\n"
            "    (void)_pgy_unused;\n", defer_id);

        /* Save current output, redirect to helpers for the body */
        CodeBuf *saved_out = ctx->out;
        int saved_indent = ctx->indent;
        ctx->out = ctx->helpers;
        ctx->indent = 1;
        if (node->data.defer_stmt.body != NULL)
            emit_block(node->data.defer_stmt.body, ctx);
        ctx->out = saved_out;
        ctx->indent = saved_indent;

        codebuf_write(ctx->helpers, "}\n");

        /* Emit sentinel with cleanup attribute */
        write_indent(ctx);
        codebuf_write(ctx->out,
            "int _pgy_defer_sentinel_%d "
            "__attribute__((cleanup(_pgy_defer_%d))) = 0;\n",
            defer_id, defer_id);
        break;
    }
    case AST_BIND_STMT: {
        /* bind party.slot = Role;
         * → lookup party's typed_var to get PartyType,
         *   then emit PartyType_bind_slot(&party, NULL, &Role_Ability_vtable_instance)
         * For now: use the typed_var mapping to find the party type. */
        const char *pvar = node->data.bind_stmt.party_var;
        const char *slot = node->data.bind_stmt.slot_name;
        const char *role = node->data.bind_stmt.role_name;
        const char *party_type = NULL;
        for (int ti = 0; ti < ctx->typed_var_count; ti++) {
            if (strcmp(ctx->typed_vars[ti].name, pvar) == 0) {
                party_type = ctx->typed_vars[ti].type_name;
                break;
            }
        }
        if (party_type == NULL) party_type = "UnknownParty";

        /* Find the ability name by scanning the HIR for the party declaration.
         * The dyn role slot records the required ability. */
        const char *ability_name = slot; /* fallback: use slot name */
        if (ctx->hir != NULL) {
            for (size_t hi = 0; hi < ctx->hir->item_count; hi++) {
                ASTNode *it = ctx->hir->items[hi].ast;
                if (it == NULL || it->type != AST_PARTY_DECL)
                    continue;
                if (strcmp(it->data.party_decl.name, party_type) != 0)
                    continue;
                for (size_t ri = 0; ri < it->data.party_decl.role_count; ri++) {
                    ASTNode *rs = it->data.party_decl.role_slots[ri];
                    if (strcmp(rs->data.role_slot.slot_name, slot) == 0
                        && rs->data.role_slot.ability_count > 0
                        && rs->data.role_slot.required_abilities[0] != NULL) {
                        ability_name = rs->data.role_slot.required_abilities[0]->data.type.name;
                    }
                }
            }
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "%s_bind_%s(&%s, NULL, &%s_%s_vtable_instance);\n",
            party_type, slot, pvar,
            role, ability_name);
        break;
    }
    case AST_ABILITY_DECL:
        emit_ability_decl(node, ctx);
        break;
    case AST_ROLE_DECL:
        emit_role_decl(node, ctx);
        break;
    case AST_PARTY_DECL:
        emit_party_decl(node, ctx);
        break;
    case AST_SYSTEMIC_DECL:
        emit_systemic_decl(node, ctx);
        break;
    case AST_WORLD_DECL:
        emit_world_decl(node, ctx);
        break;
    case AST_RELATION_DECL:
        emit_relation_decl(node, ctx);
        break;
    case AST_EFFECT_DECL:
        emit_effect_decl(node, ctx);
        break;
    case AST_ZONE_DECL:
        emit_zone_decl(node, ctx);
        break;
    case AST_EVENT_DECL:
        emit_event_decl(node, ctx);
        break;
    case AST_EVENT_SUBSCRIBE:
        emit_event_subscribe(node, ctx);
        break;
    case AST_EVENT_UNSUBSCRIBE:
        emit_event_unsubscribe(node, ctx);
        break;
    case AST_IF_STMT:
        emit_if_stmt(node, ctx);
        break;
    case AST_FOR_LOOP:
        emit_for_loop(node, ctx);
        break;
    case AST_WHILE_LOOP:
        emit_while_loop(node, ctx);
        break;
    case AST_MATCH_STMT:
        emit_match_stmt(node, ctx);
        break;
    case AST_RETURN:
        emit_return_stmt(node, ctx);
        break;
    case AST_BREAK:
        write_indent(ctx);
        codebuf_write(ctx->out, "break;\n");
        break;
    case AST_ENUM_DECL: {
        const char *ename = node->data.enum_decl.name;

        /* Check if any variant has data → tagged union */
        bool has_data = false;
        for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
            if (node->data.enum_decl.variant_param_counts != NULL
                && node->data.enum_decl.variant_param_counts[i] > 0) {
                has_data = true;
                break;
            }
        }

        if (!has_data) {
            /* Simple enum: typedef enum { Color_Red=0, ... } Color; */
            codebuf_write(ctx->out, "typedef enum {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                codebuf_write(ctx->out, "    %s_%s = %zu",
                    ename, node->data.enum_decl.variants[i], i);
                if (i + 1 < node->data.enum_decl.variant_count)
                    codebuf_write(ctx->out, ",");
                codebuf_write(ctx->out, "\n");
            }
            codebuf_write(ctx->out, "} %s;\n\n", ename);
        } else {
            /* Tagged union:
             * typedef enum { Shape_TAG_Circle, Shape_TAG_Rect, Shape_TAG_None } Shape_Tag;
             * typedef struct {
             *     Shape_Tag tag;
             *     union {
             *         struct { int32_t _0; } Circle;
             *         struct { int32_t _0; int32_t _1; } Rect;
             *     };
             * } Shape; */

            /* Tag enum */
            codebuf_write(ctx->out, "typedef enum {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                codebuf_write(ctx->out, "    %s_TAG_%s = %zu",
                    ename, node->data.enum_decl.variants[i], i);
                if (i + 1 < node->data.enum_decl.variant_count)
                    codebuf_write(ctx->out, ",");
                codebuf_write(ctx->out, "\n");
            }
            codebuf_write(ctx->out, "} %s_Tag;\n\n", ename);

            /* Tagged union struct */
            codebuf_write(ctx->out, "typedef struct {\n");
            codebuf_write(ctx->out, "    %s_Tag tag;\n", ename);
            codebuf_write(ctx->out, "    union {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                size_t pc = (node->data.enum_decl.variant_param_counts != NULL)
                    ? node->data.enum_decl.variant_param_counts[i] : 0;
                if (pc == 0) continue;
                codebuf_write(ctx->out, "        struct { ");
                for (size_t p = 0; p < pc; p++) {
                    ASTNode *pt = node->data.enum_decl.variant_params[i][p];
                    const char *ctype = "int32_t";
                    if (pt != NULL && pt->type == AST_TYPE
                        && pt->data.type.name != NULL) {
                        ctype = pergyra_ast_type_to_c(pt);
                    }
                    codebuf_write(ctx->out, "%s _%zu; ", ctype, p);
                }
                codebuf_write(ctx->out, "} %s;\n",
                    node->data.enum_decl.variants[i]);
            }
            codebuf_write(ctx->out, "    };\n");
            codebuf_write(ctx->out, "} %s;\n\n", ename);

            /* Constructor functions:
             * static inline Shape Shape_Circle(int32_t _0) {
             *     Shape v; v.tag = Shape_TAG_Circle; v.Circle._0 = _0; return v;
             * } */
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                size_t pc = (node->data.enum_decl.variant_param_counts != NULL)
                    ? node->data.enum_decl.variant_param_counts[i] : 0;
                const char *vname = node->data.enum_decl.variants[i];
                if (pc == 0) {
                    /* No-data variant: macro constant */
                    codebuf_write(ctx->out,
                        "#define %s_%s() ((%s){ .tag = %s_TAG_%s })\n",
                        ename, vname, ename, ename, vname);
                } else {
                    /* Data variant: constructor function */
                    codebuf_write(ctx->out,
                        "static inline %s %s_%s(", ename, ename, vname);
                    for (size_t p = 0; p < pc; p++) {
                        ASTNode *pt = node->data.enum_decl.variant_params[i][p];
                        const char *ctype = "int32_t";
                        if (pt != NULL && pt->type == AST_TYPE
                            && pt->data.type.name != NULL)
                            ctype = pergyra_ast_type_to_c(pt);
                        if (p > 0) codebuf_write(ctx->out, ", ");
                        codebuf_write(ctx->out, "%s _%zu", ctype, p);
                    }
                    codebuf_write(ctx->out, ") {\n");
                    codebuf_write(ctx->out,
                        "    %s _v; _v.tag = %s_TAG_%s;\n", ename, ename, vname);
                    for (size_t p = 0; p < pc; p++)
                        codebuf_write(ctx->out,
                            "    _v.%s._%zu = _%zu;\n", vname, p, p);
                    codebuf_write(ctx->out, "    return _v;\n}\n");
                }
            }
            codebuf_write(ctx->out, "\n");
        }
        break;
    }
    case AST_CONTINUE:
        write_indent(ctx);
        codebuf_write(ctx->out, "continue;\n");
        break;
    case AST_WITH_STMT:
        emit_with_stmt(node, ctx);
        break;
    case AST_PARALLEL_BLOCK:
        emit_parallel_block(node, ctx);
        break;
    case AST_BLOCK:
        emit_block(node, ctx);
        break;
    case AST_LAMBDA_EXPR:
        {
            char *expr = emit_expression(node, ctx);
            if (expr != NULL && expr[0] != '\0') {
                write_indent(ctx);
                codebuf_write(ctx->out, "%s;\n", expr);
            }
            free(expr);
            break;
        }
    case AST_ACTOR_DECL:
        emit_actor_decl(node, ctx);
        break;
    case AST_SELECT_STMT:
        emit_select_stmt(node, ctx);
        break;
    case AST_ASYNC_BLOCK:
        emit_async_block(node, ctx);
        break;
    default: {
        /* Expression statement (including event invoke) */
        char *expr = emit_expression(node, ctx);
        if (expr != NULL && expr[0] != '\0') {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s;\n", expr);
        }
        free(expr);
        break;
    }
    }
}

void
emit_block(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    if (node->type == AST_BLOCK) {
        int saved_slot_count = ctx->slot_var_count;
        int saved_typed_count = ctx->typed_var_count;
        for (size_t i = 0; i < node->data.block.count; i++)
            emit_statement(node->data.block.statements[i], ctx);

        /* Slot sugar: auto-release slot vars declared in this scope (LIFO).
         * Skip slots already explicitly released by the user. */
        for (int i = ctx->slot_var_count - 1; i >= saved_slot_count; i--) {
            SlotVarEntry *e = &ctx->slot_vars[i];
            if (e->released) continue;
            write_indent(ctx);
            if (e->is_secure) {
                codebuf_write(ctx->out,
                    "pgy_secure_release_%s(&%s, &%s_token);\n",
                    e->inner_type, e->name, e->name);
            } else {
                codebuf_write(ctx->out,
                    "pgy_release_%s(&%s);\n",
                    e->inner_type, e->name);
            }
        }

        ctx->slot_var_count = saved_slot_count;
        ctx->typed_var_count = saved_typed_count;
    } else {
        emit_statement(node, ctx);
    }
}

/* -----------------------------------------------------------------
 * Program emitter
 * ----------------------------------------------------------------- */

void
emit_program(const HIRProgram *hir, TranspilerCtx *ctx)
{
    if (hir == NULL)
        return;

    ctx->hir = hir;

    /* File header */
    codebuf_write(ctx->out,
        "/*\n"
        " * Generated by the Pergyra compiler C backend\n"
        " * Do not edit manually.\n"
        " */\n"
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include \"pgy_runtime.h\"\n"
        "#include \"pgy_parallel.h\"\n"
        "#include \"pgy_channel.h\"\n"
        "#ifndef PGY_EVENT_MAX_HANDLERS\n"
        "#define PGY_EVENT_MAX_HANDLERS 16\n"
        "#endif\n\n");

    /*
     * Multi-pass strategy for valid C output:
     *   Pass 1 — ability declarations (vtable typedefs)
     *   Pass 2 — class declarations (type completeness)
     *   Pass 3 — role declarations (vtable instances + methods)
     *   Pass 4 — function declarations (file scope)
     *   Pass 5 — remaining top-level statements → wrapped in main()
     */

    /* Pass 1: abilities (vtable typedefs) */
    for (size_t i = 0; i < hir->ability_count; i++)
        emit_ability_decl(hir->abilities[i], ctx);

    /* Pass 1.5: enums */
    for (size_t i = 0; i < hir->type_count; i++) {
        if (hir->types[i] != NULL && hir->types[i]->type == AST_ENUM_DECL)
            emit_statement(hir->types[i], ctx);
    }

    /* Pass 2: classes */
    for (size_t i = 0; i < hir->type_count; i++) {
        if (hir->types[i] != NULL && hir->types[i]->type != AST_ENUM_DECL)
            emit_class_decl(hir->types[i], ctx);
    }

    /* Pass 2.5: extern declarations */
    for (size_t i = 0; i < hir->extern_count; i++)
        emit_extern_block(hir->externs[i], ctx);

    /* Pass 3: roles (vtable instances + free functions) */
    for (size_t i = 0; i < hir->role_count; i++)
        emit_role_decl(hir->roles[i], ctx);

    /* Pass 3.5: parties (struct + methods) */
    for (size_t i = 0; i < hir->party_count; i++)
        emit_party_decl(hir->parties[i], ctx);

    /* Pass 3.7: systemics (struct + methods) */
    for (size_t i = 0; i < hir->systemic_count; i++)
        emit_systemic_decl(hir->systemics[i], ctx);

    /* Pass 3.8: zones (struct + methods + sync helpers) */
    for (size_t i = 0; i < hir->zone_count; i++)
        emit_zone_decl(hir->zones[i], ctx);

    /* Pass 3.9: worlds (struct + methods) */
    for (size_t i = 0; i < hir->world_count; i++)
        emit_world_decl(hir->worlds[i], ctx);

    /* Pass 3.95: actors (struct + methods) */
    for (size_t i = 0; i < hir->actor_count; i++)
        emit_actor_decl(hir->actors[i], ctx);

    for (size_t i = 0; i < hir->event_count; i++)
        emit_event_decl(hir->events[i], ctx);

    for (size_t i = 0; i < hir->function_count; i++)
        emit_func_forward_decl(hir->functions[i], ctx->decls, ctx);

    /* Pass 4: functions — emit in two sub-passes so that helpers
     * (parallel context structs, wrapper functions) generated during
     * function emission are available.  First pass: emit all functions
     * into a temporary buffer. */
    {
        CodeBuf *func_buf = codebuf_create();
        CodeBuf *saved_out = ctx->out;
        ctx->out = func_buf;
        for (size_t i = 0; i < hir->function_count; i++)
            emit_func_decl(hir->functions[i], ctx);
        ctx->out = saved_out;

        /* Emit forward declarations after function emission so late-added
         * helper declarations, including generic specializations, are visible. */
        if (ctx->decls->len > 0) {
            codebuf_write(ctx->out, "\n");
            codebuf_write_raw(ctx->out, ctx->decls->data, ctx->decls->len);
        }

        /* Emit helpers (parallel context structs + wrappers) first */
        if (ctx->helpers->len > 0) {
            codebuf_write(ctx->out, "\n");
            codebuf_write_raw(ctx->out, ctx->helpers->data, ctx->helpers->len);
        }

        /* Then emit the function bodies */
        if (func_buf->len > 0) {
            codebuf_write_raw(ctx->out, func_buf->data, func_buf->len);
        }
        codebuf_destroy(func_buf);
    }

    /* Check if a Main() function exists */
    /* Generate int main(void) { ... } */
    if (hir->executable_count > 0 || hir->has_main_function) {
        codebuf_write(ctx->out, "\nint\nmain(void)\n{\n");
        ctx->indent++;

        /* Initialize runtime */
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_pool_init(0);\n\n");

        /* Emit top-level statements inside main() */
        for (size_t i = 0; i < hir->executable_count; i++)
            emit_statement(hir->executables[i], ctx);

        /* If Main() exists and no top-level statements, call it */
        if (hir->has_main_function) {
            write_indent(ctx);
            codebuf_write(ctx->out, "Main();\n");
        }

        /* Shutdown runtime */
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_pool_shutdown();\n");

        write_indent(ctx);
        codebuf_write(ctx->out, "return 0;\n");
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

/* -----------------------------------------------------------------
 * Main entry point
 * ----------------------------------------------------------------- */

TranspileResult *
transpile(const HIRProgram *hir, const char *output_path)
{
    TranspileResult *result = calloc(1, sizeof(TranspileResult));
    if (result == NULL)
        return NULL;

    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        result->success       = false;
        result->error_message = pergyra_strdup("Out of memory");
        return result;
    }

    emit_program(hir, ctx);

    if (output_path != NULL) {
        if (!codebuf_dump_file(ctx->out, output_path)) {
            result->success       = false;
            result->error_message = strdup_fmt(
                "Cannot write output file: %s", output_path);
            transpiler_ctx_destroy(ctx);
            return result;
        }
    }

    result->success = true;
    transpiler_ctx_destroy(ctx);
    return result;
}

void
transpile_result_destroy(TranspileResult *res)
{
    if (res == NULL)
        return;
    free(res->error_message);
    free(res);
}

/* =================================================================
 * Role/Ability system emitters
 * ================================================================= */

static void
emit_role_method_impl(const char *role_name, ASTNode *method, TranspilerCtx *ctx)
{
    const char *method_name = method->data.func_decl.name;
    const char *ret_type = "void";
    if (method->data.func_decl.return_type != NULL)
        ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

    codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *self",
                  ret_type, role_name, method_name);

    for (size_t k = 0; k < method->data.func_decl.param_count; k++) {
        FuncParam *p = method->data.func_decl.params[k];
        if (strcmp(p->name, "self") == 0 && p->type == NULL)
            continue;
        const char *pt = "int32_t";
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        codebuf_write(ctx->out, ", %s %s", pt, p->name);
    }
    codebuf_write(ctx->out, ")\n{\n");

    ctx->indent++;
    if (method->data.func_decl.body != NULL)
        emit_block(method->data.func_decl.body, ctx);
    ctx->indent--;

    codebuf_write(ctx->out, "}\n");
}

static void
emit_role_vtable_instance(const char *role_name, ASTNode *impl, TranspilerCtx *ctx)
{
    const char *ability_name = impl->data.impl_ability.ability_name;
    if (ability_name == NULL || impl->data.impl_ability.method_count == 0)
        return;

    codebuf_write(ctx->out,
        "\nstatic const %s_vtable %s_%s_vtable_instance = {\n",
        ability_name, role_name, ability_name);

    for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
        ASTNode *method = impl->data.impl_ability.methods[j];
        if (method->type != AST_FUNC_DECL)
            continue;
        codebuf_write(ctx->out, "    .%s = %s_%s,\n",
                      method->data.func_decl.name,
                      role_name, method->data.func_decl.name);
    }

    codebuf_write(ctx->out, "};\n");
}

static void
emit_role_operator_aliases(ASTNode *role, TranspilerCtx *ctx)
{
    const char *role_name;
    const char *for_type;
    TokenType ops[] = {
        TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
        TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
        TOKEN_GREATER, TOKEN_GREATER_EQUAL
    };

    if (role == NULL || role->type != AST_ROLE_DECL
        || role->data.role_decl.name == NULL
        || role->data.role_decl.for_type == NULL
        || role->data.role_decl.for_type->type != AST_TYPE
        || role->data.role_decl.for_type->data.type.name == NULL) {
        return;
    }

    role_name = role->data.role_decl.name;
    for_type = role->data.role_decl.for_type->data.type.name;

    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        TokenType op = ops[i];
        const char *suffix = operator_overload_suffix(op);
        ASTNode *method = find_role_operator_method_decl(ctx, role, op, 0);
        char fn_name[256];
        if (suffix == NULL || method == NULL
            ) {
            continue;
        }
        snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, for_type);
        if (find_function_decl(ctx, fn_name) != NULL)
            continue;

        FuncParam *rhs_param = NULL;
        size_t rhs_param_count = 0;
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
                rhs_param = p;
                rhs_param_count++;
            }
        }
        if (rhs_param_count != 1)
            continue;

        const char *ret_type = "void";
        const char *lhs_type = pergyra_ast_type_to_c(role->data.role_decl.for_type);
        const char *rhs_type = "int32_t";
        const char *rhs_name = (rhs_param != NULL && rhs_param->name != NULL)
            ? rhs_param->name : "rhs";

        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);
        if (rhs_param != NULL && rhs_param->type != NULL)
            rhs_type = pergyra_ast_type_to_c(rhs_param->type);

        codebuf_write(ctx->out,
            "\nstatic %s\noperator_%s_%s(%s lhs, %s %s)\n{\n",
            ret_type, suffix, for_type, lhs_type, rhs_type, rhs_name);
        codebuf_write(ctx->out, "    %s lhs_copy = lhs;\n", lhs_type);
        if (strcmp(ret_type, "void") == 0) {
            codebuf_write(ctx->out, "    %s_%s(&lhs_copy, %s);\n",
                role_name, method->data.func_decl.name, rhs_name);
        } else {
            codebuf_write(ctx->out, "    return %s_%s(&lhs_copy, %s);\n",
                role_name, method->data.func_decl.name, rhs_name);
        }
        codebuf_write(ctx->out, "}\n");
    }
}

static void
emit_included_role_impls(ASTNode *role, TranspilerCtx *ctx)
{
    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *include_stmt = role->data.role_decl.includes[i];
        ASTNode *included_role = find_role_decl(ctx, include_stmt->data.include_stmt.role_name);

        if (included_role == NULL) {
            codebuf_write(ctx->out, "/* unresolved include role: %s */\n",
                          include_stmt->data.include_stmt.role_name);
            continue;
        }

        for (size_t j = 0; j < included_role->data.role_decl.impl_count; j++) {
            ASTNode *impl = included_role->data.role_decl.impl_abilities[j];
            if (impl->type != AST_IMPL_ABILITY)
                continue;

            if (role_has_ability(role, impl->data.impl_ability.ability_name))
                continue;

            for (size_t k = 0; k < impl->data.impl_ability.method_count; k++) {
                ASTNode *method = impl->data.impl_ability.methods[k];
                if (method->type != AST_FUNC_DECL)
                    continue;
                if (role_has_method(role, method->data.func_decl.name))
                    continue;
                emit_role_method_impl(role->data.role_decl.name, method, ctx);
            }

            emit_role_vtable_instance(role->data.role_decl.name, impl, ctx);
        }
    }
}

/*
 * Ability → vtable struct typedef
 *
 *   typedef struct {
 *       RetType (*MethodName)(void* self, ParamType p1, ...);
 *       ...
 *   } AbilityName_vtable;
 */
void
emit_ability_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.ability_decl.name;

    codebuf_write(ctx->out, "\n/* Ability: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct\n{\n");

    for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
        ASTNode *method = node->data.ability_decl.methods[i];
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "    %s (*%s)(void *self", ret_type, method_name);

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (strcmp(p->name, "self") == 0 && p->type == NULL)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ");\n");
    }

    codebuf_write(ctx->out, "} %s_vtable;\n", name);
}

/*
 * Role → vtable instance + free functions
 *
 *   static RetType RoleName_MethodName(void* self, ...) { body }
 *   static const AbilityName_vtable RoleName_AbilityName_vtable_instance = {
 *       .MethodName = RoleName_MethodName,
 *   };
 */
void
emit_role_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.role_decl.name;

    codebuf_write(ctx->out, "\n/* Role: %s */\n", name);
    emit_included_role_impls(node, ctx);

    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];

        if (impl->type == AST_IMPL_ABILITY) {
            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method->type != AST_FUNC_DECL)
                    continue;
                emit_role_method_impl(name, method, ctx);
            }

            emit_role_vtable_instance(name, impl, ctx);

        } else if (impl->type == AST_OVERRIDE_FUNC) {
            ASTNode *func = impl->data.override_func.func_decl;
            if (func == NULL || func->type != AST_FUNC_DECL)
                continue;

            const char *method_name = func->data.func_decl.name;
            const char *ret_type = "void";
            if (func->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(func->data.func_decl.return_type);

            codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *self",
                          ret_type, name, method_name);

            for (size_t k = 0; k < func->data.func_decl.param_count; k++) {
                FuncParam *p = func->data.func_decl.params[k];
                if (strcmp(p->name, "self") == 0 && p->type == NULL)
                    continue;
                const char *pt = "int32_t";
                if (p->type != NULL)
                    pt = pergyra_ast_type_to_c(p->type);
                codebuf_write(ctx->out, ", %s %s", pt, p->name);
            }
            codebuf_write(ctx->out, ")\n{\n");

            ctx->indent++;
            if (func->data.func_decl.body != NULL)
                emit_block(func->data.func_decl.body, ctx);
            ctx->indent--;

            codebuf_write(ctx->out, "}\n");
        }
    }

    emit_role_operator_aliases(node, ctx);
}

/* =================================================================
 * Party system emitters
 * ================================================================= */

/*
 * Party → C struct with role slot pointers + shared fields
 * Party methods → free functions
 */
void
emit_party_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.party_decl.name;

    codebuf_write(ctx->out, "\n/* Party: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Role slots as void* + vtable pointer */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];
        const char *slot_name = rs->data.role_slot.slot_name;
        bool is_dyn = rs->data.role_slot.is_dynamic;
        codebuf_write(ctx->out, "    void *%s;\n", slot_name);
        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab = rs->data.role_slot.required_abilities[j];
            if (ab != NULL && ab->data.type.name != NULL) {
                if (is_dyn) {
                    /* dyn: mutable vtable pointer — swappable at runtime */
                    codebuf_write(ctx->out,
                        "    const %s_vtable *%s_%s_vt; /* dyn */\n",
                        ab->data.type.name, slot_name, ab->data.type.name);
                } else {
                    codebuf_write(ctx->out,
                        "    const %s_vtable *%s_%s_vt;\n",
                        ab->data.type.name, slot_name, ab->data.type.name);
                }
            }
        }
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
        ASTNode *shared = node->data.party_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods as free functions */
    for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
        ASTNode *method = node->data.party_decl.methods[i];
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");

        ctx->indent++;
        if (method->data.func_decl.body != NULL)
            emit_block(method->data.func_decl.body, ctx);
        ctx->indent--;

        codebuf_write(ctx->out, "}\n");
    }

    /* Emit bind helpers for dyn role slots */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];
        if (!rs->data.role_slot.is_dynamic)
            continue;
        const char *slot_name = rs->data.role_slot.slot_name;
        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab = rs->data.role_slot.required_abilities[j];
            if (ab == NULL || ab->data.type.name == NULL)
                continue;
            const char *ab_name = ab->data.type.name;
            codebuf_write(ctx->out,
                "\nstatic inline void\n"
                "%s_bind_%s(%s *self, void *impl, const %s_vtable *vt)\n"
                "{\n"
                "    self->%s = impl;\n"
                "    self->%s_%s_vt = vt;\n"
                "}\n",
                name, slot_name, name, ab_name,
                slot_name,
                slot_name, ab_name);
        }
    }
}

/* =================================================================
 * Systemic/World system emitters
 * ================================================================= */

void
emit_systemic_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.systemic_decl.name;

    codebuf_write(ctx->out, "\n/* Systemic: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Party slots */
    for (size_t i = 0; i < node->data.systemic_decl.party_count; i++) {
        ASTNode *ps = node->data.systemic_decl.party_slots[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            ps->data.systemic_slot.party_type,
            ps->data.systemic_slot.slot_name);
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.systemic_decl.shared_count; i++) {
        ASTNode *shared = node->data.systemic_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods */
    for (size_t i = 0; i < node->data.systemic_decl.method_count; i++) {
        ASTNode *method = node->data.systemic_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        if (method->data.func_decl.body != NULL)
            emit_block(method->data.func_decl.body, ctx);
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_relation_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.relation_decl.name;

    codebuf_write(ctx->out, "\n/* Relation: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.relation_decl.slot_count; i++) {
        ASTNode *slot = node->data.relation_decl.slots[i];
        const char *ft = "int32_t";
        if (slot->data.domain_slot.type != NULL)
            ft = pergyra_ast_type_to_c(slot->data.domain_slot.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, slot->data.domain_slot.slot_name);
        if (!slot->data.domain_slot.is_subject) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                slot->data.domain_slot.slot_name);
        }
    }

    for (size_t i = 0; i < node->data.relation_decl.shared_count; i++) {
        ASTNode *shared = node->data.relation_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    for (size_t i = 0; i < node->data.relation_decl.slot_count; i++) {
        ASTNode *slot = node->data.relation_decl.slots[i];
        if (!slot->data.domain_slot.is_subject) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
                slot->data.domain_slot.slot_name);
        }
    }
    for (size_t i = 0; i < node->data.relation_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.relation_decl.refreshes[i];
        ASTNode *target_slot = NULL;
        ASTNode *source_slot = NULL;
        ASTNode *target_decl = NULL;
        ASTNode *source_decl = NULL;
        const char *target_type_name = NULL;
        const char *source_type_name = NULL;
        char *literal;
        for (size_t j = 0; j < node->data.relation_decl.slot_count; j++) {
            ASTNode *slot = node->data.relation_decl.slots[j];
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.object_slot_name) == 0)
                target_slot = slot;
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.source_slot_name) == 0)
                source_slot = slot;
        }
        if (target_slot == NULL || source_slot == NULL
            || target_slot->data.domain_slot.type == NULL
            || source_slot->data.domain_slot.type == NULL)
            continue;
        target_type_name = target_slot->data.domain_slot.type->data.type.name;
        source_type_name = source_slot->data.domain_slot.type->data.type.name;
        target_decl = find_class_decl(ctx, target_type_name);
        source_decl = find_class_decl(ctx, source_type_name);
        {
            char *source_expr = strdup_fmt("self->%s",
                refresh->data.zone_refresh.source_slot_name);
            literal = emit_projection_literal(target_decl, source_decl,
                target_type_name, source_expr);
            free(source_expr);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->%s = %s;\n",
            refresh->data.zone_refresh.object_slot_name, literal);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = true;\n",
            refresh->data.zone_refresh.object_slot_name);
        free(literal);
    }
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < node->data.relation_decl.method_count; i++) {
        ASTNode *method = node->data.relation_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        {
            const char *saved_relation_name = ctx->current_relation_name;
            ctx->current_relation_name = name;
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            ctx->current_relation_name = saved_relation_name;
        }
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_effect_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.effect_decl.name;

    codebuf_write(ctx->out, "\n/* Effect: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.effect_decl.slot_count; i++) {
        ASTNode *slot = node->data.effect_decl.slots[i];
        const char *ft = "int32_t";
        if (slot->data.domain_slot.type != NULL)
            ft = pergyra_ast_type_to_c(slot->data.domain_slot.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, slot->data.domain_slot.slot_name);
        if (!slot->data.domain_slot.is_subject) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                slot->data.domain_slot.slot_name);
        }
    }

    for (size_t i = 0; i < node->data.effect_decl.shared_count; i++) {
        ASTNode *shared = node->data.effect_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    for (size_t i = 0; i < node->data.effect_decl.slot_count; i++) {
        ASTNode *slot = node->data.effect_decl.slots[i];
        if (!slot->data.domain_slot.is_subject) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
                slot->data.domain_slot.slot_name);
        }
    }
    for (size_t i = 0; i < node->data.effect_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.effect_decl.refreshes[i];
        ASTNode *target_slot = NULL;
        ASTNode *source_slot = NULL;
        ASTNode *target_decl = NULL;
        ASTNode *source_decl = NULL;
        const char *target_type_name = NULL;
        const char *source_type_name = NULL;
        char *literal;
        for (size_t j = 0; j < node->data.effect_decl.slot_count; j++) {
            ASTNode *slot = node->data.effect_decl.slots[j];
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.object_slot_name) == 0)
                target_slot = slot;
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.source_slot_name) == 0)
                source_slot = slot;
        }
        if (target_slot == NULL || source_slot == NULL
            || target_slot->data.domain_slot.type == NULL
            || source_slot->data.domain_slot.type == NULL)
            continue;
        target_type_name = target_slot->data.domain_slot.type->data.type.name;
        source_type_name = source_slot->data.domain_slot.type->data.type.name;
        target_decl = find_class_decl(ctx, target_type_name);
        source_decl = find_class_decl(ctx, source_type_name);
        {
            char *source_expr = strdup_fmt("self->%s",
                refresh->data.zone_refresh.source_slot_name);
            literal = emit_projection_literal(target_decl, source_decl,
                target_type_name, source_expr);
            free(source_expr);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->%s = %s;\n",
            refresh->data.zone_refresh.object_slot_name, literal);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = true;\n",
            refresh->data.zone_refresh.object_slot_name);
        free(literal);
    }
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < node->data.effect_decl.method_count; i++) {
        ASTNode *method = node->data.effect_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        {
            const char *saved_effect_name = ctx->current_effect_name;
            ctx->current_effect_name = name;
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            ctx->current_effect_name = saved_effect_name;
        }
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_zone_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.zone_decl.name;

    codebuf_write(ctx->out, "\n/* Zone: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.slots[i];
        const char *ft = "int32_t";
        if (slot->data.domain_slot.type != NULL)
            ft = pergyra_ast_type_to_c(slot->data.domain_slot.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, slot->data.domain_slot.slot_name);
        if (!slot->data.domain_slot.is_subject) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                slot->data.domain_slot.slot_name);
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            slot->data.zone_layer_slot.layer_type,
            slot->data.zone_layer_slot.slot_name);
        codebuf_write(ctx->out, "    bool __layer_active_%s;\n",
            slot->data.zone_layer_slot.slot_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.shared_count; i++) {
        ASTNode *shared = node->data.zone_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        codebuf_write(ctx->out, "    bool __state_%s;\n",
            state->data.zone_state.state_name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__state_%s = false;\n",
            state->data.zone_state.state_name);
    }
    for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.slots[i];
        if (!slot->data.domain_slot.is_subject) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
                slot->data.domain_slot.slot_name);
        }
    }
    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            slot->data.zone_layer_slot.slot_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.zone_decl.refreshes[i];
        ASTNode *target_slot = NULL;
        ASTNode *source_slot = NULL;
        ASTNode *target_decl = NULL;
        ASTNode *source_decl = NULL;
        const char *target_type_name = NULL;
        const char *source_type_name = NULL;
        char *literal;
        for (size_t j = 0; j < node->data.zone_decl.slot_count; j++) {
            ASTNode *slot = node->data.zone_decl.slots[j];
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.object_slot_name) == 0)
                target_slot = slot;
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.source_slot_name) == 0)
                source_slot = slot;
        }
        if (target_slot == NULL || source_slot == NULL
            || target_slot->data.domain_slot.type == NULL
            || source_slot->data.domain_slot.type == NULL)
            continue;
        target_type_name = target_slot->data.domain_slot.type->data.type.name;
        source_type_name = source_slot->data.domain_slot.type->data.type.name;
        target_decl = find_class_decl(ctx, target_type_name);
        source_decl = find_class_decl(ctx, source_type_name);
        {
            char *source_expr = strdup_fmt("self->%s",
                refresh->data.zone_refresh.source_slot_name);
            literal = emit_projection_literal(target_decl, source_decl,
                target_type_name, source_expr);
            free(source_expr);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->%s = %s;\n",
            refresh->data.zone_refresh.object_slot_name, literal);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = true;\n",
            refresh->data.zone_refresh.object_slot_name);
        free(literal);
    }

    for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
        ASTNode *apply = node->data.zone_decl.applies[i];
        if (apply->data.zone_apply.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                apply->data.zone_apply.state_name);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (!state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              apply->data.zone_apply.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                        state->data.zone_state.layer_slot_name);
                    emit_zone_bind_effect_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name, ctx);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            apply->data.zone_apply.effect_slot_name);
        emit_zone_bind_effect_layer(ctx->out, node,
            apply->data.zone_apply.effect_slot_name,
            apply->data.zone_apply.target_slot_name, ctx);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (!state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          apply->data.zone_apply.effect_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          apply->data.zone_apply.target_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_effects[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            maintain->data.zone_maintain_effect.effect_slot_name);
        emit_zone_bind_effect_layer(ctx->out, node,
            maintain->data.zone_maintain_effect.effect_slot_name,
            maintain->data.zone_maintain_effect.target_slot_name, ctx);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (!state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          maintain->data.zone_maintain_effect.effect_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          maintain->data.zone_maintain_effect.target_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__state_%s = true;\n",
            maintain->data.zone_maintain_state.state_name);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (strcmp(state->data.zone_state.state_name,
                       maintain->data.zone_maintain_state.state_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                    state->data.zone_state.layer_slot_name);
                if (!state->data.zone_state.is_relation) {
                    emit_zone_bind_effect_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name, ctx);
                } else {
                    emit_zone_bind_relation_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name,
                        state->data.zone_state.right_slot_name, ctx);
                }
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
        ASTNode *detach = node->data.zone_decl.detaches[i];
        if (detach->data.zone_detach.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = false;\n",
                detach->data.zone_detach.state_name);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (!state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              detach->data.zone_detach.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
                        state->data.zone_state.layer_slot_name);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            detach->data.zone_detach.effect_slot_name);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (!state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          detach->data.zone_detach.effect_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          detach->data.zone_detach.target_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = false;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
        ASTNode *link = node->data.zone_decl.links[i];
        if (link->data.zone_link.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                link->data.zone_link.state_name);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              link->data.zone_link.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                        state->data.zone_state.layer_slot_name);
                    emit_zone_bind_relation_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name,
                        state->data.zone_state.right_slot_name, ctx);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            link->data.zone_link.relation_slot_name);
        emit_zone_bind_relation_layer(ctx->out, node,
            link->data.zone_link.relation_slot_name,
            link->data.zone_link.left_slot_name,
            link->data.zone_link.right_slot_name, ctx);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          link->data.zone_link.relation_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          link->data.zone_link.left_slot_name) == 0
                && strcmp(state->data.zone_state.right_slot_name,
                          link->data.zone_link.right_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_relations[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            maintain->data.zone_maintain_relation.relation_slot_name);
        emit_zone_bind_relation_layer(ctx->out, node,
            maintain->data.zone_maintain_relation.relation_slot_name,
            maintain->data.zone_maintain_relation.left_slot_name,
            maintain->data.zone_maintain_relation.right_slot_name, ctx);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          maintain->data.zone_maintain_relation.relation_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          maintain->data.zone_maintain_relation.left_slot_name) == 0
                && strcmp(state->data.zone_state.right_slot_name,
                          maintain->data.zone_maintain_relation.right_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = node->data.zone_decl.unlinks[i];
        if (unlink->data.zone_unlink.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = false;\n",
                unlink->data.zone_unlink.state_name);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              unlink->data.zone_unlink.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
                        state->data.zone_state.layer_slot_name);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            unlink->data.zone_unlink.relation_slot_name);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          unlink->data.zone_unlink.relation_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          unlink->data.zone_unlink.left_slot_name) == 0
                && strcmp(state->data.zone_state.right_slot_name,
                          unlink->data.zone_unlink.right_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = false;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < node->data.zone_decl.method_count; i++) {
        ASTNode *method = node->data.zone_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        {
            const char *saved_zone_name = ctx->current_zone_name;
            ctx->current_zone_name = name;
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            ctx->current_zone_name = saved_zone_name;
        }
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_world_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.world_decl.name;

    codebuf_write(ctx->out, "\n/* World: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Systemic instances */
    for (size_t i = 0; i < node->data.world_decl.systemic_count; i++) {
        ASTNode *ws = node->data.world_decl.systemics[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            ws->data.world_systemic.systemic_type,
            ws->data.world_systemic.slot_name);
    }

    /* Zone instances */
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            wz->data.world_zone.zone_type,
            wz->data.world_zone.slot_name);
        codebuf_write(ctx->out, "    bool __zone_active_%s;\n",
            wz->data.world_zone.slot_name);
        codebuf_write(ctx->out, "    bool __zone_dirty_%s;\n",
            wz->data.world_zone.slot_name);
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
        ASTNode *shared = node->data.world_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
        ASTNode *state = node->data.world_decl.states[i];
        codebuf_write(ctx->out, "    bool __zone_state_%s;\n",
            state->data.world_state.state_name);
    }
    codebuf_write(ctx->out, "    bool __world_derived_dirty;\n");

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world command pass: reset */\n");
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "bool _pgy_prev_active_%s = self->__zone_active_%s;\n",
            wz->data.world_zone.slot_name, wz->data.world_zone.slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_active_%s = false;\n",
            wz->data.world_zone.slot_name);
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_world_needs_derived = self->__world_derived_dirty;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world command pass: directives */\n");
    for (size_t i = 0; i < node->data.world_decl.activate_count; i++) {
        ASTNode *act = node->data.world_decl.activations[i];
        const char *slot_name = act->data.world_activate.zone_slot_name;
        if (slot_name == NULL && act->data.world_activate.state_name != NULL) {
            ASTNode *state = find_world_state_decl(node, act->data.world_activate.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
            else if (world_has_zone_slot(node, act->data.world_activate.state_name))
                slot_name = act->data.world_activate.state_name;
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = true;\n", slot_name);
        }
    }
    for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++) {
        ASTNode *mnt = node->data.world_decl.maintained_zones[i];
        const char *slot_name = mnt->data.world_maintain.zone_slot_name;
        if (slot_name == NULL && mnt->data.world_maintain.state_name != NULL) {
            ASTNode *state = find_world_state_decl(node, mnt->data.world_maintain.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
            else if (world_has_zone_slot(node, mnt->data.world_maintain.state_name))
                slot_name = mnt->data.world_maintain.state_name;
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = true;\n", slot_name);
        }
    }
    for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++) {
        ASTNode *act = node->data.world_decl.deactivations[i];
        const char *slot_name = act->data.world_deactivate.zone_slot_name;
        if (slot_name == NULL && act->data.world_deactivate.state_name != NULL) {
            ASTNode *state = find_world_state_decl(node, act->data.world_deactivate.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
            else if (world_has_zone_slot(node, act->data.world_deactivate.state_name))
                slot_name = act->data.world_deactivate.state_name;
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = false;\n", slot_name);
        }
    }
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        const char *slot_name = wz->data.world_zone.slot_name;
        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__zone_active_%s != _pgy_prev_active_%s) {\n",
            slot_name, slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_dirty_%s = true;\n", slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_world_needs_derived = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world zone sync pass */\n");
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        const char *slot_name = wz->data.world_zone.slot_name;
        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__zone_dirty_%s) {\n", slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s_sync(&self->%s);\n",
            wz->data.world_zone.zone_type, slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_dirty_%s = false;\n", slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_world_needs_derived = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world derived pass */\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "if (_pgy_world_needs_derived) {\n");
    ctx->indent++;
    for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
        ASTNode *state = node->data.world_decl.states[i];
        const char *expr_fmt = "self->__zone_active_%s";
        const char *detail_name = state->data.world_state.detail_name;
        write_indent(ctx);
        switch (state->data.world_state.source_kind) {
        case WORLD_STATE_SOURCE_ALL:
        case WORLD_STATE_SOURCE_ANY: {
            bool first = true;
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                state->data.world_state.state_name);
            codebuf_write(ctx->out, "(");
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                if (input_name == NULL)
                    continue;
                if (!first) {
                    codebuf_write(ctx->out,
                        state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                            ? " && " : " || ");
                }
                if (world_has_zone_slot(node, input_name))
                    codebuf_write(ctx->out, "self->__zone_active_%s", input_name);
                else
                    codebuf_write(ctx->out, "self->__zone_state_%s", input_name);
                first = false;
            }
            if (first)
                codebuf_write(ctx->out,
                    state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                        ? "true" : "false");
            codebuf_write(ctx->out, ");\n");
            break;
        }
        case WORLD_STATE_SOURCE_PROJECTION:
            expr_fmt = "(self->__zone_active_%s && self->%s.__projection_ready_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ", state->data.world_state.state_name);
            codebuf_write(ctx->out, expr_fmt,
                state->data.world_state.zone_slot_name,
                state->data.world_state.zone_slot_name,
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_LAYER:
            expr_fmt = "(self->__zone_active_%s && self->%s.__layer_active_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ", state->data.world_state.state_name);
            codebuf_write(ctx->out, expr_fmt,
                state->data.world_state.zone_slot_name,
                state->data.world_state.zone_slot_name,
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_STATE:
            expr_fmt = "(self->__zone_active_%s && self->%s.__state_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ", state->data.world_state.state_name);
            codebuf_write(ctx->out, expr_fmt,
                state->data.world_state.zone_slot_name,
                state->data.world_state.zone_slot_name,
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_ZONE:
        default:
            codebuf_write(ctx->out, "self->__zone_state_%s = self->__zone_active_%s;\n",
                state->data.world_state.state_name,
                state->data.world_state.zone_slot_name);
            break;
        }
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "self->__world_derived_dirty = false;\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    /* Methods */
    for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
        ASTNode *method = node->data.world_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        {
            const char *saved_world_name = ctx->current_world_name;
            ctx->current_world_name = name;
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            for (size_t j = 0; j < node->data.world_decl.zone_count; j++) {
                ASTNode *zone = node->data.world_decl.zones[j];
                const char *slot_name = zone != NULL
                    ? zone->data.world_zone.slot_name
                    : NULL;
                if (slot_name == NULL)
                    continue;
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__zone_dirty_%s = true;\n", slot_name);
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__world_derived_dirty = true;\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            ctx->current_world_name = saved_world_name;
        }
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

/* =================================================================
 * Async system emitters
 * ================================================================= */

void
emit_actor_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.actor_decl.name;

    codebuf_write(ctx->out, "\n/* Actor: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Fields */
    for (size_t i = 0; i < node->data.actor_decl.field_count; i++) {
        ClassField *f = node->data.actor_decl.fields[i];
        const char *ft = "int32_t";
        if (f->type != NULL)
            ft = pergyra_ast_type_to_c(f->type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, f->name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out,
        "\n/* Auto-generated container types for actor host %s */\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n",
        name,
        name, name,
        name, name,
        name, name);

    /* Methods (actor methods are emitted as free functions) */
    for (size_t i = 0; i < node->data.actor_decl.method_count; i++) {
        ASTNode *method = node->data.actor_decl.methods[i];
        if (method == NULL) continue;

        const char *method_name = method->data.async_func_decl.name;
        const char *ret_type = "void";
        if (method->data.async_func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.async_func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.async_func_decl.param_count; j++) {
            FuncParam *p = method->data.async_func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        if (method->data.async_func_decl.body != NULL)
            emit_block(method->data.async_func_decl.body, ctx);
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_select_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    size_t case_count = node->data.select_stmt.case_count;

    codebuf_write(ctx->out, "\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "/* select */\n");

    if (case_count == 0) {
        if (node->data.select_stmt.default_case != NULL)
            emit_statement(node->data.select_stmt.default_case, ctx);
        return;
    }

    for (size_t i = 0; i < case_count; i++) {
        ASTNode *c = node->data.select_stmt.cases[i];
        ASTNode *channel = NULL;
        ASTNode *body = NULL;
        const char *bind_name = NULL;
        bool valid_case = select_case_parts(c, &channel, &bind_name, &body);
        const char *inner = "Int";

        if (!valid_case || bind_name == NULL || channel == NULL
            || channel->type != AST_IDENTIFIER)
            continue;

        {
            const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
            if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                inner = slot_inner_type_name(type_name);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "%s _sel_recv_%zu;\n",
                      pergyra_type_to_c(inner), i);
    }

    {
        int select_id = ctx->tmp_counter++;
        write_indent(ctx);
        codebuf_write(ctx->out, "static unsigned int _sel_rr_%d = 0;\n", select_id);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "unsigned int _sel_start_%d = _sel_rr_%d++ %% %zu;\n",
            select_id, select_id, case_count);
        write_indent(ctx);
        codebuf_write(ctx->out, "switch (_sel_start_%d) {\n", select_id);
        ctx->indent++;

        for (size_t start = 0; start < case_count; start++) {
            write_indent(ctx);
            codebuf_write(ctx->out, "case %zu:\n", start);
            ctx->indent++;

            for (size_t offset = 0; offset < case_count; offset++) {
                size_t i = (start + offset) % case_count;
                ASTNode *c = node->data.select_stmt.cases[i];
                ASTNode *channel = NULL;
                ASTNode *body = NULL;
                const char *bind_name = NULL;
                bool valid_case = select_case_parts(c, &channel, &bind_name, &body);
                const char *inner = "Int";

                if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                    const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
                    if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                        inner = slot_inner_type_name(type_name);
                }

                write_indent(ctx);
                if (offset == 0) {
                    if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                        if (bind_name != NULL)
                            codebuf_write(ctx->out,
                                "if (pgy_channel_try_recv_%s(&%s, &_sel_recv_%zu)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i, i);
                        else
                            codebuf_write(ctx->out,
                                "if (pgy_channel_ready_%s(&%s)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i);
                    } else {
                        codebuf_write(ctx->out, "if (1) { /* select case %zu */\n", i);
                    }
                } else {
                    if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                        if (bind_name != NULL)
                            codebuf_write(ctx->out,
                                "} else if (pgy_channel_try_recv_%s(&%s, &_sel_recv_%zu)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i, i);
                        else
                            codebuf_write(ctx->out,
                                "} else if (pgy_channel_ready_%s(&%s)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i);
                    } else {
                        codebuf_write(ctx->out, "} else if (1) { /* select case %zu */\n", i);
                    }
                }
                ctx->indent++;
                if (valid_case) {
                    if (bind_name != NULL) {
                        write_indent(ctx);
                        codebuf_write(ctx->out, "%s %s = _sel_recv_%zu;\n",
                                      pergyra_type_to_c(inner), bind_name, i);
                    } else if (channel != NULL) {
                        char *recv = emit_channel_recv(ast_create_channel_recv(channel), ctx);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "(void)%s;\n", recv);
                        free(recv);
                    }
                    if (body != NULL)
                        emit_statement(body, ctx);
                } else if (c != NULL) {
                    emit_statement(c, ctx);
                }
                ctx->indent--;
            }

            if (node->data.select_stmt.default_case != NULL) {
                write_indent(ctx);
                codebuf_write(ctx->out, "} else { /* default */\n");
                ctx->indent++;
                emit_statement(node->data.select_stmt.default_case, ctx);
                ctx->indent--;
            }

            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "break;\n");
            ctx->indent--;
        }

        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

/* =================================================================
 * Event system emitters
 * ================================================================= */

void
emit_event_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.event_decl.name;
    char *event_type = strdup_fmt("%s_Event", name);
    
    /* Generate event handler type typedef */
    codebuf_write(ctx->out, "\n/* Event: %s */\n", name);
    
    /* Build parameter types string */
    codebuf_write(ctx->out, "typedef void (*%s_Handler)(", name);
    
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        const char *pt = "void*";
        if (param->data.let_decl.type != NULL) {
            pt = pergyra_ast_type_to_c(param->data.let_decl.type);
        }
        if (i > 0) codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s %s", pt, param->data.let_decl.name);
    }
    
    codebuf_write(ctx->out, ");\n");
    
    /* Generate event struct with handler array */
    codebuf_write(ctx->out, "typedef struct {\n");
    codebuf_write(ctx->out, "    %s_Handler handlers[PGY_EVENT_MAX_HANDLERS];\n", name);
    codebuf_write(ctx->out, "    void* contexts[PGY_EVENT_MAX_HANDLERS];\n");
    codebuf_write(ctx->out, "    size_t count;\n");
    codebuf_write(ctx->out, "    bool is_invoking;\n");
    codebuf_write(ctx->out, "    bool pending_changes;\n");
    codebuf_write(ctx->out, "} %s;\n", event_type);
    codebuf_write(ctx->out, "static %s %s;\n", event_type, name);
    
    /* Generate inline init function */
    codebuf_write(ctx->out, "static inline void %s_INIT(%s* e) {\n", name, event_type);
    codebuf_write(ctx->out, "    memset(e, 0, sizeof(*e));\n");
    codebuf_write(ctx->out, "}\n");
    
    /* Generate subscribe function */
    codebuf_write(ctx->out, "static inline void %s_SUBSCRIBE(%s* e, %s_Handler h) {\n", name, event_type, name);
    codebuf_write(ctx->out, "    if (e->count < PGY_EVENT_MAX_HANDLERS) {\n");
    codebuf_write(ctx->out, "        e->handlers[e->count++] = h;\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "}\n");
    
    /* Generate unsubscribe function */
    codebuf_write(ctx->out, "static inline void %s_UNSUBSCRIBE(%s* e, %s_Handler h) {\n", name, event_type, name);
    codebuf_write(ctx->out, "    for (size_t i = 0; i < e->count; i++) {\n");
    codebuf_write(ctx->out, "        if (e->handlers[i] == h) {\n");
    codebuf_write(ctx->out, "            for (size_t j = i; j < e->count - 1; j++) {\n");
    codebuf_write(ctx->out, "                e->handlers[j] = e->handlers[j + 1];\n");
    codebuf_write(ctx->out, "            }\n");
    codebuf_write(ctx->out, "            e->count--;\n");
    codebuf_write(ctx->out, "            break;\n");
    codebuf_write(ctx->out, "        }\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "}\n");
    
    /* Generate invoke function */
    codebuf_write(ctx->out, "static inline void %s_INVOKE(%s* e", name, event_type);
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        const char *pt = "void*";
        if (param->data.let_decl.type != NULL) {
            pt = pergyra_ast_type_to_c(param->data.let_decl.type);
        }
        codebuf_write(ctx->out, ", %s %s", pt, param->data.let_decl.name);
    }
    codebuf_write(ctx->out, ") {\n");
    codebuf_write(ctx->out, "    e->is_invoking = true;\n");
    codebuf_write(ctx->out, "    for (size_t i = 0; i < e->count; i++) {\n");
    codebuf_write(ctx->out, "        e->handlers[i](");
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        if (i > 0) codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s", node->data.event_decl.params[i]->data.let_decl.name);
    }
    codebuf_write(ctx->out, ");\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "    e->is_invoking = false;\n");
    codebuf_write(ctx->out, "}\n");
    free(event_type);
}

void
emit_event_subscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = emit_expression(node->data.event_op.event, ctx);
    char *handler_expr = emit_expression(node->data.event_op.handler, ctx);
    
    write_indent(ctx);
    codebuf_write(ctx->out, "%s_SUBSCRIBE(&%s, %s);\n", 
                  event_expr, event_expr, handler_expr);
    
    free(event_expr);
    free(handler_expr);
}

void
emit_event_unsubscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = emit_expression(node->data.event_op.event, ctx);
    char *handler_expr = emit_expression(node->data.event_op.handler, ctx);
    
    write_indent(ctx);
    codebuf_write(ctx->out, "%s_UNSUBSCRIBE(&%s, %s);\n", 
                  event_expr, event_expr, handler_expr);
    
    free(event_expr);
    free(handler_expr);
}

void
emit_event_invoke(ASTNode *node, TranspilerCtx *ctx)
{
    /* Event invoke is handled in emit_call for function-style invocation */
    (void)node;
    (void)ctx;
}

char *
emit_lambda_expr(ASTNode *node, TranspilerCtx *ctx)
{
    int lambda_id = ++ctx->tmp_counter;
    const char *return_type = "int32_t";

    if (node->data.lambda_expr.return_type != NULL) {
        return_type = pergyra_ast_type_to_c(node->data.lambda_expr.return_type);
    } else if (node->data.lambda_expr.body != NULL
               && node->data.lambda_expr.body->type == AST_BLOCK) {
        return_type = "void";
    }

    char *lambda_name = strdup_fmt("pgy_lambda_%d", lambda_id);

    codebuf_write(ctx->decls, "\nstatic %s %s(",
                  return_type, lambda_name);
    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        if (i > 0)
            codebuf_write(ctx->decls, ", ");
        codebuf_write(ctx->decls, "int32_t %s", param->data.identifier.name);
    }
    codebuf_write(ctx->decls, ");\n");

    codebuf_write(ctx->helpers, "\nstatic %s %s(",
                  return_type, lambda_name);
    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        if (i > 0)
            codebuf_write(ctx->helpers, ", ");
        codebuf_write(ctx->helpers, "int32_t %s", param->data.identifier.name);
    }
    codebuf_write(ctx->helpers, ")\n{\n");

    if (node->data.lambda_expr.body != NULL
        && node->data.lambda_expr.body->type == AST_BLOCK) {
        CodeBuf *saved_out = ctx->out;
        int saved_indent = ctx->indent;
        ctx->out = ctx->helpers;
        ctx->indent = 1;
        emit_block(node->data.lambda_expr.body, ctx);
        ctx->indent = saved_indent;
        ctx->out = saved_out;
    } else if (node->data.lambda_expr.body != NULL) {
        char *expr = emit_expression(node->data.lambda_expr.body, ctx);
        write_indent_to(ctx->helpers, 1);
        codebuf_write(ctx->helpers, "return %s;\n", expr);
        free(expr);
    }

    codebuf_write(ctx->helpers, "}\n");
    return lambda_name;
}

void
emit_include_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *included_role = node->data.include_stmt.role_name;

    codebuf_write(ctx->out, "/* include %s */\n", included_role);
    if (find_role_decl(ctx, included_role) == NULL) {
        codebuf_write(ctx->out, "/* unresolved include %s */\n", included_role);
    }
}

void
emit_impl_ability(ASTNode *node, TranspilerCtx *ctx)
{
    const char *ability_name = node->data.impl_ability.ability_name;
    
    codebuf_write(ctx->out, "/* Impl ability: %s */\n", ability_name);
    
    /* This is handled within emit_role_decl */
    (void)ctx;
}
