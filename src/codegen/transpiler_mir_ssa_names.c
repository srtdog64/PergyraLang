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
#include "../parser/ast_api.h"
#include "transpiler_context.h"
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

static bool
transpiler_zone_shared_view_has_field(TranspilerCtx *ctx,
                                      ASTNode *zone_decl,
                                      const char *zone_name,
                                      const char *field_name)
{
    TranspilerHostedSharedFieldView shared_view;

    if (ctx == NULL || zone_decl == NULL || field_name == NULL)
        return false;

    shared_view = transpiler_hosted_shared_field_view_from_decl(
        ctx, zone_name, zone_decl);
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(
            &shared_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing shared-field metadata for zone '%s'",
            zone_name != NULL ? zone_name : "(anonymous-zone)");
        return false;
    }

    for (size_t i = 0; i < shared_view.count; i++) {
        const char *shared_name =
            transpiler_hosted_shared_field_view_name(&shared_view, i);
        if (shared_name != NULL && strcmp(shared_name, field_name) == 0)
            return true;
    }
    return false;
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
    if (current_party_has_field(ctx, base_name))
        return true;
    if (current_roster_has_field(ctx, base_name))
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
        ASTNode *zone_decl =
            transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL,
                                                    host_name);
        if (zone_decl != NULL) {
            size_t slot_count = 0;
            ASTNode **slots = ast_zone_slots(zone_decl, &slot_count);
            for (size_t i = 0; i < slot_count; i++) {
                ASTNode *slot = slots[i];
                const char *slot_name = ast_domain_slot_name(slot);
                if (slot != NULL && slot_name != NULL
                    && strcmp(slot_name, base_name) == 0) {
                    return true;
                }
            }
            TranspilerHostedZoneLayerSlotView layer_view =
                transpiler_hosted_zone_layer_slot_view_from_decl(
                    ctx, host_name, zone_decl);
            if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
                    &layer_view)) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR-only C path missing zone layer-slot SSA metadata for '%s'",
                    host_name != NULL ? host_name : "(anonymous-zone)");
                return false;
            }
            for (size_t i = 0; i < layer_view.count; i++) {
                const char *layer_name =
                    transpiler_hosted_zone_layer_slot_view_name(
                        &layer_view, i);
                if (layer_name != NULL && strcmp(layer_name, base_name) == 0) {
                    return true;
                }
            }
            if (transpiler_zone_shared_view_has_field(
                    ctx, zone_decl, host_name, base_name)) {
                return true;
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
