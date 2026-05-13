/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR SSA name rendering helpers.
 */

#include "transpiler_mir_ssa_names.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_mir_local_binding.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_overlay_host_fields.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"

static char *
transpiler_ssa_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int n;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        va_end(ap2);
        return NULL;
    }
    buf = malloc((size_t)n + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

const char *
transpiler_resolve_active_ssa_name(const TranspilerCtx *ctx,
                                   const char *base_name)
{
    const char *resolved;
    if (ctx == NULL || ctx->active_ssa_map == NULL || base_name == NULL)
        return NULL;
    if (transpiler_is_implicit_field((TranspilerCtx *)ctx, base_name))
        return NULL;
    if (is_slot_var((TranspilerCtx *)ctx, base_name))
        return NULL;
    resolved = transpiler_resolve_ssa_name(
        (const TranspilerSSANameMap *)ctx->active_ssa_map,
        base_name);
    if (resolved != NULL)
        return resolved;
    if (transpiler_name_is_token_local(base_name))
        return base_name;
    return NULL;
}

char *
transpiler_make_c_ssa_name(TranspilerCtx *ctx, const char *versioned_name)
{
    char base[128];
    size_t version = 0;

    if (versioned_name == NULL)
        return NULL;
    if (!transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version))
        return pergyra_strdup(versioned_name);
    if (ctx != NULL && transpiler_is_implicit_field(ctx, base)) {
        if (current_class_has_field(ctx, base)) {
            return transpiler_ssa_strdup_fmt(current_class_uses_self_cell(ctx)
                ? "self->%s"
                : "self.%s", base);
        }
        return transpiler_ssa_strdup_fmt("self->%s", base);
    }
    return transpiler_ssa_strdup_fmt("_pgy_ssa_%s_%zu", base, version);
}

bool
transpiler_is_implicit_field(TranspilerCtx *ctx, const char *base_name)
{
    ASTNode *host_decl = NULL;
    bool in_zone_context = false;
    const char *host_name = NULL;

    if (ctx == NULL || base_name == NULL)
        return false;
    if (strcmp(base_name, "self") == 0)
        return false;
    if (ctx->current_func_decl != NULL
        && transpiler_has_explicit_local_binding(ctx->current_func_decl, base_name))
        return false;
    host_decl = transpiler_current_host_decl_local(ctx);
    in_zone_context = (host_decl != NULL && host_decl->type == AST_ZONE_DECL);
    host_name = transpiler_decl_name_local(host_decl);
    if (current_class_has_field(ctx, base_name))
        return true;
    if (current_relation_has_field(ctx, base_name))
        return true;
    if (current_effect_has_field(ctx, base_name))
        return true;
    if (in_zone_context) {
        if (current_zone_has_field(ctx, base_name))
            return true;
        if (lookup_typed_var(ctx, base_name) == NULL && !is_slot_var(ctx, base_name))
            return true;
    }
    if (transpiler_current_world_has_field(ctx, base_name))
        return true;
    if (!in_zone_context && host_name != NULL) {
        ASTNode *zone_decl = find_zone_decl(ctx, host_name);
        if (zone_decl != NULL) {
            size_t slot_count = 0;
            ASTNode **slots = ast_zone_slots(zone_decl, &slot_count);
            for (size_t i = 0; i < slot_count; i++) {
                ASTNode *slot = slots[i];
                if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                    && strcmp(slot->data.domain_slot.slot_name, base_name) == 0) {
                    return true;
                }
            }
            size_t layer_slot_count = 0;
            ASTNode **layer_slots = ast_zone_layer_slots(zone_decl,
                &layer_slot_count);
            for (size_t i = 0; i < layer_slot_count; i++) {
                ASTNode *slot = layer_slots[i];
                if (slot != NULL && slot->data.zone_layer_slot.slot_name != NULL
                    && strcmp(slot->data.zone_layer_slot.slot_name, base_name) == 0) {
                    return true;
                }
            }
            size_t shared_count = 0;
            ASTNode **shared_fields = ast_zone_shared_fields(zone_decl,
                &shared_count);
            for (size_t i = 0; i < shared_count; i++) {
                ASTNode *shared = shared_fields[i];
                if (shared != NULL && shared->data.party_shared.name != NULL
                    && strcmp(shared->data.party_shared.name, base_name) == 0) {
                    return true;
                }
            }
        }
        if (host_name != NULL) {
            size_t name_len = strlen(host_name);
            if (name_len > 4
                && strcmp(host_name + name_len - 4, "Zone") == 0
                && lookup_typed_var(ctx, base_name) == NULL
                && !is_slot_var(ctx, base_name)) {
                return true;
            }
        }
    }
    return false;
}

char *
transpiler_render_ssa_name(TranspilerCtx *ctx, const char *versioned_name)
{
    char base[128];
    size_t version = 0;

    if (ctx != NULL
        && versioned_name != NULL
        && transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version)
        && transpiler_is_implicit_field(ctx, base)) {
        if (current_class_has_field(ctx, base)) {
            return transpiler_ssa_strdup_fmt(current_class_uses_self_cell(ctx)
                ? "self->%s"
                : "self.%s", base);
        }
        return transpiler_ssa_strdup_fmt("self->%s", base);
    }
    return transpiler_make_c_ssa_name(ctx, versioned_name);
}
