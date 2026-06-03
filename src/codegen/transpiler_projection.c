/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend projection provenance and nominal type predicates.
 */

#include <string.h>

#include "../common/string_compat.h"
#include "parser/ast_api.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "transpiler_context.h"
#include "transpiler_projection.h"

static const char *
transpiler_domain_slot_type_name_in_decl(TranspilerCtx *ctx,
                                         ASTNode *decl,
                                         const char *slot_name,
                                         const char *context)
{
    const char *decl_name;
    TranspilerHostedDomainSlotView slot_view;

    if (ctx == NULL || decl == NULL || slot_name == NULL)
        return NULL;

    decl_name = transpiler_decl_name_local(decl);
    slot_view = transpiler_hosted_domain_slot_view_from_decl(ctx, decl_name,
        decl);
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing %s domain-slot type metadata for '%s'",
            context != NULL ? context : "domain",
            decl_name != NULL ? decl_name : "(anonymous-domain)");
        return NULL;
    }

    for (size_t i = 0; i < slot_view.count; i++) {
        const char *candidate =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        if (candidate != NULL && strcmp(candidate, slot_name) == 0)
            return transpiler_hosted_domain_slot_view_type_name(&slot_view, i);
    }
    return NULL;
}

static bool
transpiler_domain_slot_is_projection_target(const char *slot_name,
                                            ASTNode **refreshes,
                                            size_t refresh_count)
{
    if (slot_name == NULL)
        return false;

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH
            || ast_zone_refresh_object_slot_name(refresh) == NULL) {
            continue;
        }
        if (strcmp(slot_name, ast_zone_refresh_object_slot_name(refresh)) == 0) {
            return true;
        }
    }

    return false;
}

bool
transpiler_domain_slot_view_is_projection_slot(
    const TranspilerHostedDomainSlotView *slot_view,
    size_t index,
    ASTNode **refreshes,
    size_t refresh_count)
{
    const char *slot_name;

    if (slot_view == NULL || index >= slot_view->count)
        return false;

    slot_name = transpiler_hosted_domain_slot_view_name(slot_view, index);
    if (slot_name == NULL)
        return false;

    return transpiler_hosted_domain_slot_view_is_tobject_like(slot_view, index)
        || transpiler_domain_slot_is_projection_target(slot_name, refreshes,
            refresh_count);
}

const char *
transpiler_domain_slot_view_bindable_name(
    const TranspilerHostedDomainSlotView *slot_view,
    size_t nth)
{
    size_t seen = 0;

    if (slot_view == NULL)
        return NULL;

    for (size_t i = 0; i < slot_view->count; i++) {
        if (!transpiler_hosted_domain_slot_view_is_binding_like(slot_view, i))
            continue;
        if (seen == nth)
            return transpiler_hosted_domain_slot_view_name(slot_view, i);
        seen++;
    }

    return NULL;
}

const char *
transpiler_current_overlay_domain_slot_type_name(TranspilerCtx *ctx,
                                                 const char *slot_name)
{
    ASTNode *decl;

    if (ctx == NULL || slot_name == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type == AST_RELATION_DECL)
        return transpiler_domain_slot_type_name_in_decl(ctx, decl,
            slot_name, "relation");
    if (decl != NULL && decl->type == AST_EFFECT_DECL)
        return transpiler_domain_slot_type_name_in_decl(ctx, decl,
            slot_name, "effect");
    if (decl != NULL && decl->type == AST_ZONE_DECL)
        return transpiler_domain_slot_type_name_in_decl(ctx, decl,
            slot_name, "zone");

    return NULL;
}

static bool
transpiler_domain_slot_in_decl_is_projection(TranspilerCtx *ctx,
                                             ASTNode *decl,
                                             const char *slot_name,
                                             const char *context)
{
    const char *decl_name;
    TranspilerHostedDomainSlotView slot_view;

    if (ctx == NULL || decl == NULL || slot_name == NULL)
        return false;

    decl_name = transpiler_decl_name_local(decl);
    slot_view = transpiler_hosted_domain_slot_view_from_decl(ctx, decl_name,
        decl);
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing %s projection-slot metadata for '%s'",
            context != NULL ? context : "domain",
            decl_name != NULL ? decl_name : "(anonymous-domain)");
        return false;
    }

    for (size_t i = 0; i < slot_view.count; i++) {
        const char *candidate =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        if (candidate == NULL || strcmp(candidate, slot_name) != 0)
            continue;
        return !transpiler_hosted_domain_slot_view_is_subject_like(
            &slot_view, i);
    }
    return false;
}

bool
transpiler_current_overlay_domain_slot_is_projection(
    TranspilerCtx *ctx,
    const char *slot_name)
{
    ASTNode *decl;

    if (ctx == NULL || slot_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type == AST_RELATION_DECL) {
        return transpiler_domain_slot_in_decl_is_projection(ctx, decl,
            slot_name, "relation");
    }
    if (decl != NULL && decl->type == AST_EFFECT_DECL) {
        return transpiler_domain_slot_in_decl_is_projection(ctx, decl,
            slot_name, "effect");
    }
    if (decl != NULL && decl->type == AST_ZONE_DECL) {
        return transpiler_domain_slot_in_decl_is_projection(ctx, decl,
            slot_name, "zone");
    }
    return false;
}

bool
transpiler_zone_domain_slot_is_projection(TranspilerCtx *ctx,
                                          ASTNode *zone_decl,
                                          const char *slot_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return false;
    return transpiler_domain_slot_in_decl_is_projection(ctx, zone_decl,
        slot_name, "zone");
}

bool
transpiler_current_world_has_field(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;
    const char *decl_name;
    TranspilerHostedSharedFieldView shared_view;

    if (ctx == NULL || field_name == NULL)
        return false;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type != AST_WORLD_DECL)
        decl = NULL;
    if (decl == NULL)
        return false;

    decl_name = transpiler_decl_name_local(decl);
    TranspilerHostedWorldRosterSlotView roster_view =
        transpiler_hosted_world_roster_slot_view_from_decl(
            ctx, decl_name, decl);
    if (transpiler_hosted_world_roster_slot_view_missing_mir_metadata(
            &roster_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing world roster-slot metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-world)");
        return false;
    }
    for (size_t i = 0; i < roster_view.count; i++) {
        const char *slot_name =
            transpiler_hosted_world_roster_slot_view_name(&roster_view, i);
        if (slot_name != NULL && strcmp(slot_name, field_name) == 0) {
            return true;
        }
    }
    TranspilerHostedWorldZoneSlotView zone_view =
        transpiler_hosted_world_zone_slot_view_from_decl(ctx, decl_name, decl);
    if (transpiler_hosted_world_zone_slot_view_missing_mir_metadata(
            &zone_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing world zone-slot metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-world)");
        return false;
    }
    for (size_t i = 0; i < zone_view.count; i++) {
        const char *slot_name =
            transpiler_hosted_world_zone_slot_view_name(&zone_view, i);
        if (slot_name != NULL && strcmp(slot_name, field_name) == 0) {
            return true;
        }
    }
    shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, decl_name, decl);
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(
            &shared_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing world shared-field metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-world)");
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

ASTNode *
transpiler_find_zone_state_decl(ASTNode *zone_decl, const char *state_name)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    size_t state_count = 0;
    ASTNode **states = ast_zone_states(zone_decl, &state_count);
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && ast_zone_state_name(state) != NULL
            && strcmp(ast_zone_state_name(state), state_name) == 0) {
            return state;
        }
    }
    return NULL;
}

ASTNode *
transpiler_find_world_state_decl(ASTNode *world_decl, const char *state_name)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    size_t state_count = 0;
    ASTNode **states = ast_world_states(world_decl, &state_count);
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && ast_world_state_name(state) != NULL
            && strcmp(ast_world_state_name(state), state_name) == 0) {
            return state;
        }
    }
    return NULL;
}

static const char *transpiler_world_zone_slot_type_name(TranspilerCtx *ctx,
                                                       ASTNode *world_decl,
                                                       const char *slot_name);

bool
transpiler_zone_has_layer_slot(TranspilerCtx *ctx,
                               ASTNode *zone_decl,
                               const char *slot_name)
{
    const char *zone_name;
    TranspilerHostedZoneLayerSlotView layer_view;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL) {
        return false;
    }

    zone_name = transpiler_decl_name_local(zone_decl);
    layer_view = transpiler_hosted_zone_layer_slot_view_from_decl(
        ctx, zone_name, zone_decl);
    if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone layer lookup metadata for '%s'",
            zone_name != NULL ? zone_name : "(anonymous-zone)");
        return false;
    }

    for (size_t i = 0; i < layer_view.count; i++) {
        const char *candidate_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (candidate_name != NULL && strcmp(candidate_name, slot_name) == 0) {
            return true;
        }
    }
    return false;
}

bool
transpiler_world_has_zone_slot(TranspilerCtx *ctx,
                               ASTNode *world_decl,
                               const char *slot_name)
{
    return transpiler_world_zone_slot_type_name(ctx, world_decl, slot_name)
        != NULL;
}

static const char *
transpiler_world_zone_slot_type_name(TranspilerCtx *ctx,
                                     ASTNode *world_decl,
                                     const char *slot_name)
{
    const char *world_name;
    TranspilerHostedWorldZoneSlotView zone_view;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || slot_name == NULL)
        return NULL;

    world_name = transpiler_decl_name_local(world_decl);
    zone_view = transpiler_hosted_world_zone_slot_view_from_decl(
        ctx, world_name, world_decl);
    if (transpiler_hosted_world_zone_slot_view_missing_mir_metadata(
            &zone_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing world zone-slot lookup metadata for '%s'",
            world_name != NULL ? world_name : "(anonymous-world)");
        return NULL;
    }
    for (size_t i = 0; i < zone_view.count; i++) {
        const char *candidate_name =
            transpiler_hosted_world_zone_slot_view_name(&zone_view, i);
        if (candidate_name != NULL && strcmp(candidate_name, slot_name) == 0) {
            return transpiler_hosted_world_zone_slot_view_type_name(
                &zone_view, i);
        }
    }
    return NULL;
}

ASTNode *
transpiler_resolve_world_zone_decl(TranspilerCtx *ctx, ASTNode *world_decl,
                                   const char *slot_name)
{
    const char *zone_type = transpiler_world_zone_slot_type_name(ctx,
        world_decl, slot_name);
    if (ctx == NULL || zone_type == NULL)
        return NULL;
    return transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL,
                                                   zone_type);
}

static bool
projection_class_field_view(TranspilerCtx *ctx,
                            ASTNode *decl,
                            TranspilerHostedFieldView *view)
{
    const char *decl_name;

    if (view == NULL)
        return false;
    view->decl_header = NULL;
    view->ast_compat_fields = NULL;
    view->ast_compat_count = 0;
    view->count = 0;
    view->uses_mir_metadata = false;
    view->requires_mir_metadata = false;

    if (ctx == NULL || decl == NULL || decl->type != AST_CLASS_DECL)
        return false;

    decl_name = transpiler_decl_name_local(decl);
    *view = transpiler_hosted_class_field_view_from_decl(ctx, decl_name, decl);
    if (transpiler_hosted_field_view_missing_mir_metadata(view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing projection class-field metadata for '%s'",
            decl_name != NULL ? decl_name : "(anonymous-class)");
        return false;
    }
    return true;
}

static size_t
projection_class_field_count(TranspilerCtx *ctx, ASTNode *decl)
{
    TranspilerHostedFieldView view;

    return projection_class_field_view(ctx, decl, &view) ? view.count : 0;
}

static const char *
projection_class_field_name(TranspilerCtx *ctx, ASTNode *decl, size_t index)
{
    TranspilerHostedFieldView view;

    if (!projection_class_field_view(ctx, decl, &view))
        return NULL;
    return transpiler_hosted_field_view_name(&view, index);
}

static const char *
projection_class_field_type_name(TranspilerCtx *ctx, ASTNode *decl,
                                 size_t index)
{
    TranspilerHostedFieldView view;
    const MIRDeclField *field;
    ASTNode *type_node;

    if (!projection_class_field_view(ctx, decl, &view))
        return NULL;
    field = transpiler_hosted_field_view_metadata(&view, index);
    if (field != NULL) {
        const char *type_name = transpiler_mir_decl_field_type_name(field);
        if (type_name != NULL)
            return type_name;
        type_node = transpiler_mir_decl_field_type(field);
    } else {
        type_node = transpiler_hosted_field_view_type(&view, index);
    }

    if (type_node != NULL && type_node->type == AST_TYPE)
        return ast_type_name(type_node);
    return NULL;
}

int
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

    field_count = projection_class_field_count(ctx, source_decl);
    for (size_t i = 0; i < field_count; i++) {
        const char *candidate_name =
            projection_class_field_name(ctx, source_decl, i);
        if (candidate_name != NULL
            && strcmp(candidate_name, field_name) == 0) {
            if (path_out != NULL)
                *path_out = transpiler_scratch_strdup(ctx, field_name);
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ASTNode *vessel_decl;
        const char *candidate_name;
        const char *type_name;
        char *nested_path = NULL;
        char *prefixed_path;
        int nested_status;

        candidate_name = projection_class_field_name(ctx, source_decl, i);
        type_name = projection_class_field_type_name(ctx, source_decl, i);
        if (candidate_name == NULL || type_name == NULL) {
            continue;
        }

        vessel_decl = transpiler_find_projection_nominal_decl_local(
            ctx, type_name);
        if (vessel_decl == NULL
            || vessel_decl->type != AST_CLASS_DECL
            || ast_class_nominal_kind(vessel_decl) != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested_status = resolve_projection_source_path_rec(
            ctx, vessel_decl, field_name, depth + 1, &nested_path);
        if (nested_status != 1) {
            if (nested_status == 2)
                match_count = 2;
            continue;
        }

        prefixed_path = transpiler_scratch_fmt(ctx,
                                               "%s.%s",
                                               candidate_name,
                                               nested_path);
        if (prefixed_path == NULL)
            continue;

        match_count++;
        if (match_count == 1) {
            resolved_path = prefixed_path;
        } else {
            resolved_path = NULL;
        }
    }

    if (match_count == 1) {
        if (path_out != NULL)
            *path_out = resolved_path;
        return 1;
    }

    return match_count > 1 ? 2 : 0;
}

char *
emit_projection_literal(TranspilerCtx *ctx, ASTNode *target_decl, ASTNode *source_decl,
                        ASTNode *refresh, const char *target_type_name,
                        const char *source_expr)
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

    size_t target_field_count = projection_class_field_count(ctx, target_decl);
    for (size_t i = 0; i < target_field_count; i++) {
        const char *target_field_name =
            projection_class_field_name(ctx, target_decl, i);
        const char *source_field_name = NULL;
        char *source_path = NULL;
        int source_status;

        if (target_field_name == NULL)
            continue;

        source_field_name = target_field_name;
        if (refresh != NULL && refresh->type == AST_ZONE_REFRESH) {
            for (size_t j = 0; j < ast_zone_refresh_field_map_count(refresh); j++) {
                const char *mapped_target =
                    ast_zone_refresh_mapped_target_field(refresh, j);
                const char *mapped_source =
                    ast_zone_refresh_mapped_source_field(refresh, j);
                if (mapped_target != NULL && mapped_source != NULL
                    && strcmp(mapped_target, target_field_name) == 0) {
                    source_field_name = mapped_source;
                    break;
                }
            }
        }

        source_status = resolve_projection_source_path_rec(
            ctx, source_decl, source_field_name, 0, &source_path);
        if (!first)
            codebuf_write(buf, ", ");
        first = false;

        if (source_status == 1 && source_path != NULL) {
            codebuf_write(buf, ".%s = %s.%s",
                target_field_name, source_expr, source_path);
        } else {
            codebuf_write(buf, ".%s = 0", target_field_name);
        }
    }

    codebuf_write(buf, " }");
    result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

bool
is_subject_type_name(TranspilerCtx *ctx, const char *type_name)
{
    ASTNode *decl = find_subject_host_decl(ctx, type_name);
    if (decl != NULL && !ast_class_is_struct(decl))
        return ast_class_nominal_kind(decl) == NOMINAL_DECL_SUBJECT;
    for (int i = 0; ctx != NULL && i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, type_name) == 0) {
            const ASTNode *orig = ctx->generic_class_specs[i].class_decl;
            return orig != NULL && !ast_class_is_struct(orig)
                && ast_class_nominal_kind(orig) == NOMINAL_DECL_SUBJECT;
        }
    }
    return false;
}

bool
is_nominal_host_type_name(TranspilerCtx *ctx, const char *type_name)
{
    ASTNode *decl;

    if (ctx == NULL || type_name == NULL)
        return false;

    decl = transpiler_find_nominal_host_decl_local(ctx, type_name);
    if (decl != NULL && decl->type == AST_CLASS_DECL) {
        return !ast_class_is_struct(decl)
            || ast_class_nominal_kind(decl) == NOMINAL_DECL_VESSEL
            || ast_class_nominal_kind(decl) == NOMINAL_DECL_OBJECT;
    }
    if (decl != NULL)
        return true;
    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, type_name) == 0) {
            const ASTNode *orig = ctx->generic_class_specs[i].class_decl;
            return orig != NULL && !ast_class_is_struct(orig);
        }
    }
    return false;
}
