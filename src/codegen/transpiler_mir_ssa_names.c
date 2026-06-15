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
#include "transpiler_inventory_view.h"
#include "transpiler_mir_inventory_intent_collect.h"
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
transpiler_mir_routine_has_local_name(const MIRRoutine *routine,
                                      const char *base_name)
{
    if (routine == NULL || base_name == NULL)
        return false;
    if (mir_routine_has_signature(routine)) {
        for (size_t i = 0; i < mir_routine_param_count(routine); i++) {
            FuncParam *param = mir_routine_param(routine, i);
            if (param != NULL
                && param->name != NULL
                && strcmp(param->name, base_name) == 0) {
                return true;
            }
        }
    }
    return false;
}

static const MIRRoutine *
transpiler_find_current_mir_routine(const TranspilerCtx *ctx)
{
    TranspilerMIRRoutineInventory inventory;
    const char *target_name;
    const char *host_name;

    if (ctx == NULL || ctx->current_func_decl == NULL)
        return NULL;

    transpiler_active_routine_inventory(ctx, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, i);
        if (routine != NULL && routine->ast == ctx->current_func_decl)
            return routine;
    }
    target_name = ast_declaration_name(ctx->current_func_decl);
    host_name = transpiler_decl_name_local(ctx->current_host_decl);
    if (target_name != NULL && host_name != NULL) {
        for (size_t i = 0; i < inventory.count; i++) {
            const MIRRoutine *routine =
                transpiler_routine_inventory_get(&inventory, i);
            const char *routine_name = transpiler_mir_routine_name(routine);
            const char *owner_name =
                transpiler_mir_routine_owner_name(routine);
            if (routine != NULL
                && transpiler_mir_routine_kind(routine) == MIR_SCOPE_METHOD
                && routine_name != NULL
                && owner_name != NULL
                && strcmp(routine_name, target_name) == 0
                && strcmp(owner_name, host_name) == 0) {
                return routine;
            }
        }
    }

    return transpiler_find_mir_function(ctx, ctx->current_func_decl);
}

static bool
transpiler_current_function_has_local_binding(TranspilerCtx *ctx,
                                              const char *base_name)
{
    const MIRRoutine *routine;

    if (ctx == NULL || ctx->current_func_decl == NULL || base_name == NULL)
        return false;
    routine = transpiler_find_current_mir_routine(ctx);
    if (routine != NULL
        && transpiler_mir_routine_has_local_name(routine, base_name)) {
        return true;
    }
    return transpiler_has_explicit_local_binding(ctx->current_func_decl,
        base_name);
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
    /* Slot variables (Slot<T> / SecureSlot<T> / DeviceSlot<T>) carry
     * their own backing alloca and skip SSA renaming entirely -- the
     * runtime reads/writes through `pgy_read_<T>(&base)` etc. and we
     * must not rewrite them to `_pgy_ssa_base_N`. Keep the slot-var
     * short-circuit first.
     *
     * For non-slot names we now check the SSA map BEFORE the
     * implicit-field short-circuit: when a `let heated = ...` inside a
     * zone method shares its name with a zone field, the SSA local
     * must win so the right-hand `heated` resolves to
     * `_pgy_ssa_heated_N` rather than `self->heated`. */
    if (is_slot_var((TranspilerCtx *)ctx, base_name))
        return NULL;
    resolved = transpiler_resolve_ssa_name(
        (const TranspilerSSANameMap *)ctx->active_ssa_map,
        base_name);
    if (resolved != NULL)
        return resolved;
    if (transpiler_is_implicit_field((TranspilerCtx *)ctx, base_name))
        return NULL;
    if (ctx->match_binding_alias_map != NULL) {
        resolved = transpiler_resolve_ssa_name(
            (const TranspilerSSANameMap *)ctx->match_binding_alias_map,
            base_name);
        if (resolved != NULL)
            return resolved;
    }
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
    /* SSA-versioned name (base.N with N > 0) always denotes a local SSA
     * value introduced by a let-decl or assignment inside the function
     * body. It must NOT be rewritten to `self->base` even when the host
     * class happens to declare a field of the same name -- doing so
     * silently shadows the local with the field and breaks code like:
     *
     *     let heated = furnace.Heat(...);     // local `heated`
     *     self.heated = self.heated + heated; // zone field also named
     *                                         // `heated`; right-hand
     *                                         // `heated` must read the
     *                                         // local, not the field.
     *
     * `transpiler_is_implicit_field` is only the right answer when the
     * source identifier appears WITHOUT a version, since unversioned
     * references inside a host method body do legitimately resolve to
     * `self.field`. */
    if (version > 0)
        return transpiler_ssa_strdup_fmt("_pgy_ssa_%s_%zu", base, version);
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
    if (transpiler_current_function_has_local_binding(ctx, base_name))
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
        ASTNode *zone_decl = NULL;
        bool has_zone_decl = false;
        if (transpiler_active_has_mir(ctx)) {
            has_zone_decl = transpiler_active_decl_header_of_type(
                ctx, AST_ZONE_DECL, host_name) != NULL;
        } else {
            zone_decl = transpiler_find_named_decl_local(
                ctx, AST_ZONE_DECL, host_name);
            has_zone_decl = zone_decl != NULL;
        }
        if (has_zone_decl) {
            TranspilerHostedDomainSlotView slot_view =
                transpiler_hosted_domain_slot_view_from_decl(ctx, host_name,
                    zone_decl);
            if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
                    &slot_view)) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR-only C path missing zone domain-slot SSA metadata for '%s'",
                    host_name != NULL ? host_name : "(anonymous-zone)");
                return false;
            }
            for (size_t i = 0; i < slot_view.count; i++) {
                const char *slot_name =
                    transpiler_hosted_domain_slot_view_name(&slot_view, i);
                if (slot_name != NULL && strcmp(slot_name, base_name) == 0) {
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

    /* Same invariant as transpiler_make_c_ssa_name: a name with a
     * non-zero SSA version is a locally-introduced SSA value (let-decl
     * LHS or assignment LHS inside the function body). Rewriting it to
     * `self->base` because the host class declares a field of the same
     * name silently turns `let heated = ...` into a field assignment
     * and leaves the matching `_pgy_ssa_heated_N` local undeclared --
     * the C compiler then fails on every right-hand reference to
     * `heated`. Only treat unversioned references as candidates for
     * implicit-field promotion. */
    if (ctx != NULL
        && versioned_name != NULL
        && transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version)
        && version == 0
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
