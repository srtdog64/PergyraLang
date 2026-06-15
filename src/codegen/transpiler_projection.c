/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend projection provenance and nominal type predicates.
 */

#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_decl_headers.h"
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
    return transpiler_find_named_decl_local(ctx, AST_ZONE_DECL, zone_type);
}

bool
is_subject_type_name(TranspilerCtx *ctx, const char *type_name)
{
    const MIRDeclHeader *header;
    ASTNode *decl;

    header = transpiler_active_decl_header_of_type(
        ctx, AST_CLASS_DECL, type_name);
    if (header != NULL) {
        return mir_decl_header_nominal_kind_or(
            header, NOMINAL_DECL_CLASS) == NOMINAL_DECL_SUBJECT;
    }
    if (transpiler_active_has_mir(ctx))
        return false;
    decl = find_subject_host_decl(ctx, type_name);
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
    const MIRDeclHeader *header;
    ASTNode *decl;

    if (ctx == NULL || type_name == NULL)
        return false;

    header = transpiler_active_host_decl_header(ctx, type_name);
    if (header != NULL) {
        if (mir_decl_header_ast_type_or(header, AST_PROGRAM)
            != AST_CLASS_DECL) {
            return true;
        }
        switch (mir_decl_header_nominal_kind_or(
            header, NOMINAL_DECL_CLASS)) {
        case NOMINAL_DECL_STRUCT:
            return false;
        default:
            return true;
        }
    }
    if (transpiler_active_has_mir(ctx))
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
