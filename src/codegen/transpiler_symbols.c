/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend local symbol, slot, and alias tracking.
 */

#include <string.h>

#include "transpiler_symbols.h"
#include "transpiler_context.h"
#include "codegen_slot_type_policy.h"
#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"

void
register_slot_var(TranspilerCtx *ctx, const char *name,
                  const char *inner_type, bool is_secure, bool is_indirect)
{
    SlotVarEntry *e;

    if (ctx == NULL || name == NULL || inner_type == NULL
        || ctx->slot_var_count >= MAX_SLOT_VARS)
        return;

    e = &ctx->slot_vars[ctx->slot_var_count++];
    ctx->last_slot_var_index = ctx->slot_var_count - 1;
    pergyra_str_copy(e->name, sizeof(e->name), name);
    pergyra_str_copy(e->inner_type, sizeof(e->inner_type), inner_type);
    if (is_secure) {
        snprintf(e->token_name, sizeof(e->token_name), "%s_token", name);
    } else {
        e->token_name[0] = '\0';
    }
    e->is_secure = is_secure;
    e->is_indirect = is_indirect;
}

void
set_slot_token_name(TranspilerCtx *ctx, const char *slot_name,
                    const char *token_name)
{
    if (ctx == NULL || slot_name == NULL || token_name == NULL)
        return;
    if (ctx->last_slot_var_index >= 0
        && ctx->last_slot_var_index < ctx->slot_var_count
        && strcmp(ctx->slot_vars[ctx->last_slot_var_index].name, slot_name) == 0) {
        pergyra_str_copy(ctx->slot_vars[ctx->last_slot_var_index].token_name,
            sizeof(ctx->slot_vars[ctx->last_slot_var_index].token_name),
            token_name);
        return;
    }
    for (int i = 0; i < ctx->slot_var_count; i++) {
        if (strcmp(ctx->slot_vars[i].name, slot_name) == 0) {
            ctx->last_slot_var_index = i;
            pergyra_str_copy(ctx->slot_vars[i].token_name,
                sizeof(ctx->slot_vars[i].token_name), token_name);
            return;
        }
    }
}

const char *
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
        if (pgy_codegen_type_name_is_slot_family(typed_name)) {
            return slot_inner_type_name(typed_name);
        }
        return typed_name;
    }
    if (ctx->active_type_hint != NULL) {
        if (pgy_codegen_type_name_is_slot_family(ctx->active_type_hint)) {
            return slot_inner_type_name(ctx->active_type_hint);
        }
        return ctx->active_type_hint;
    }
    return NULL;
}

bool
lookup_slot_is_secure(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx == NULL || var_name == NULL)
        return false;
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

const char *
lookup_slot_token_name(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx == NULL || var_name == NULL)
        return NULL;
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

const char *
require_slot_token_name(TranspilerCtx *ctx,
                        const char *var_name,
                        const char *operation)
{
    const char *token_name = lookup_slot_token_name(ctx, var_name);

    if (token_name != NULL && token_name[0] != '\0')
        return token_name;
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT,
        "C backend: %s requires paired secure-slot token for '%s'; token name synthesis is disabled",
        operation != NULL ? operation : "SecureSlot operation",
        var_name != NULL ? var_name : "<slot>");
    return NULL;
}

bool
lookup_slot_is_indirect(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx == NULL || var_name == NULL)
        return false;
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

char *
slot_ref_expr(TranspilerCtx *ctx, const char *slot_name, const char *slot_expr)
{
    if (slot_expr == NULL)
        return pergyra_strdup("");
    if (slot_name != NULL && lookup_slot_is_indirect(ctx, slot_name))
        return pergyra_strdup(slot_expr);
    return pergyra_strdup_printf("&%s", slot_expr);
}

bool
is_slot_var(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx == NULL || var_name == NULL)
        return false;
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

void
register_typed_var(TranspilerCtx *ctx, const char *name, const char *type_name)
{
    TypedVarEntry *e;

    if (ctx == NULL || name == NULL || type_name == NULL
        || ctx->typed_var_count >= MAX_SLOT_VARS)
        return;

    if (ctx->last_typed_var_index >= 0
        && ctx->last_typed_var_index < ctx->typed_var_count
        && strcmp(ctx->typed_vars[ctx->last_typed_var_index].name, name) == 0
        && (ctx->typed_vars[ctx->last_typed_var_index].is_view
            || ctx->typed_vars[ctx->last_typed_var_index].is_move_token)
        && (pgy_codegen_type_name_is_view(type_name)
            || strcmp(type_name, "MoveToken") == 0
            || strncmp(type_name, "MoveToken<", 10) == 0)) {
        e = &ctx->typed_vars[ctx->last_typed_var_index];
        pergyra_str_copy(e->type_name, sizeof(e->type_name), type_name);
        return;
    }

    e = &ctx->typed_vars[ctx->typed_var_count++];
    ctx->last_typed_var_index = ctx->typed_var_count - 1;
    memset(e, 0, sizeof(*e));
    pergyra_str_copy(e->name, sizeof(e->name), name);
    pergyra_str_copy(e->type_name, sizeof(e->type_name), type_name);
}

void
register_alias_var(TranspilerCtx *ctx, const char *name, ASTNode *target_expr)
{
    AliasVarEntry *e;

    if (ctx == NULL || name == NULL || target_expr == NULL
        || ctx->alias_var_count >= MAX_ALIAS_VARS)
        return;

    e = &ctx->alias_vars[ctx->alias_var_count++];
    ctx->last_alias_var_index = ctx->alias_var_count - 1;
    memset(e, 0, sizeof(*e));
    pergyra_str_copy(e->name, sizeof(e->name), name);
    e->target_expr = target_expr;
}

ASTNode *
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

TypedVarEntry *
lookup_typed_entry(TranspilerCtx *ctx, const char *var_name)
{
    if (ctx == NULL || var_name == NULL)
        return NULL;
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

const char *
lookup_typed_var(TranspilerCtx *ctx, const char *var_name)
{
    TypedVarEntry *entry = lookup_typed_entry(ctx, var_name);
    return entry != NULL ? entry->type_name : NULL;
}

void
register_view_like_var(TranspilerCtx *ctx, const char *name, const char *type_name,
                       const char *source_slot, bool source_secure,
                       bool is_move_token)
{
    TypedVarEntry *e;

    if (ctx == NULL || name == NULL || type_name == NULL
        || ctx->typed_var_count >= MAX_SLOT_VARS)
        return;

    e = &ctx->typed_vars[ctx->typed_var_count++];
    memset(e, 0, sizeof(*e));
    pergyra_str_copy(e->name, sizeof(e->name), name);
    pergyra_str_copy(e->type_name, sizeof(e->type_name), type_name);
    if (source_slot != NULL) {
        pergyra_str_copy(e->source_slot, sizeof(e->source_slot), source_slot);
    }
    e->is_view = !is_move_token;
    e->is_move_token = is_move_token;
    e->source_secure = source_secure;
}

void
register_projection_borrow_var(TranspilerCtx *ctx, const char *name,
                               const char *type_name,
                               const char *source_name)
{
    TypedVarEntry *e;

    if (ctx == NULL || name == NULL || type_name == NULL || source_name == NULL
        || ctx->typed_var_count >= MAX_SLOT_VARS)
        return;

    e = &ctx->typed_vars[ctx->typed_var_count++];
    memset(e, 0, sizeof(*e));
    pergyra_str_copy(e->name, sizeof(e->name), name);
    pergyra_str_copy(e->type_name, sizeof(e->type_name), type_name);
    pergyra_str_copy(e->source_slot, sizeof(e->source_slot), source_name);
    e->is_projection_borrow = true;
}
