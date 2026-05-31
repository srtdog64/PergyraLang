/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend nominal member and receiver type lookup.
 */

#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_nominal.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"

static const char *
render_nominal_member_type_name(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE)
        return NULL;
    return transpiler_render_type_name_local(ctx, type_node);
}

static const char *
render_mir_decl_field_type_name(TranspilerCtx *ctx,
                                const MIRDeclField *field)
{
    const char *type_name;

    if (ctx == NULL || field == NULL)
        return NULL;
    type_name = transpiler_mir_decl_field_type_name(field);
    if (type_name != NULL)
        return type_name;
    return render_nominal_member_type_name(
        ctx, transpiler_mir_decl_field_type(field));
}

static const char *
transpiler_class_member_type_name(TranspilerCtx *ctx,
                                  ASTNode *decl,
                                  const char *field_name)
{
    const char *host_name;
    TranspilerHostedFieldView field_view;
    size_t field_index = 0;
    const MIRDeclField *field;
    const char *type_name;

    if (ctx == NULL || decl == NULL || decl->type != AST_CLASS_DECL
        || field_name == NULL)
        return NULL;

    host_name = transpiler_decl_name_local(decl);
    field_view = transpiler_hosted_class_field_view_from_decl(
        ctx, host_name, decl);
    if (transpiler_hosted_field_view_missing_mir_metadata(&field_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing class-field member metadata for '%s'",
            host_name != NULL ? host_name : "(anonymous-class)");
        return NULL;
    }

    if (!transpiler_hosted_field_view_find_index(
            &field_view, field_name, &field_index)) {
        return NULL;
    }

    field = transpiler_hosted_field_view_metadata(&field_view, field_index);
    type_name = render_mir_decl_field_type_name(ctx, field);
    if (type_name != NULL)
        return type_name;
    return render_nominal_member_type_name(
        ctx, transpiler_hosted_field_view_type(&field_view, field_index));
}

static const char *
transpiler_domain_slot_member_type_name(TranspilerCtx *ctx,
                                        ASTNode **slots,
                                        size_t slot_count,
                                        const char *field_name)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *slot_name = ast_domain_slot_name(slot);
        if (slot_name != NULL && strcmp(slot_name, field_name) == 0) {
            return render_nominal_member_type_name(ctx, ast_domain_slot_type(slot));
        }
    }
    return NULL;
}

static const char *
transpiler_host_shared_member_type_name(TranspilerCtx *ctx,
                                        ASTNode *decl,
                                        const char *field_name)
{
    const char *host_name;
    TranspilerHostedSharedFieldView shared_view;

    if (ctx == NULL || decl == NULL || field_name == NULL)
        return NULL;

    host_name = transpiler_decl_name_local(decl);
    shared_view = transpiler_hosted_shared_field_view_from_decl(
        ctx, host_name, decl);
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(
            &shared_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing shared-field member metadata for '%s'",
            host_name != NULL ? host_name : "(anonymous-domain)");
        return NULL;
    }

    for (size_t i = 0; i < shared_view.count; i++) {
        const char *shared_name =
            transpiler_hosted_shared_field_view_name(&shared_view, i);
        if (shared_name == NULL || strcmp(shared_name, field_name) != 0)
            continue;
        {
            const MIRDeclField *field =
                transpiler_hosted_shared_field_view_metadata(&shared_view, i);
            const char *type_name = render_mir_decl_field_type_name(ctx, field);
            if (type_name != NULL)
                return type_name;
        }
        return render_nominal_member_type_name(ctx,
            transpiler_hosted_shared_field_view_type(&shared_view, i));
    }
    return NULL;
}

static const char *
transpiler_zone_member_type_name(TranspilerCtx *ctx,
                                 ASTNode *decl,
                                 const char *field_name)
{
    const char *zone_name;
    TranspilerHostedZoneLayerSlotView layer_view;
    size_t slot_count = 0;
    ASTNode **slots = ast_zone_slots(decl, &slot_count);
    const char *slot_type = transpiler_domain_slot_member_type_name(
        ctx, slots, slot_count, field_name);
    if (slot_type != NULL)
        return slot_type;

    zone_name = transpiler_decl_name_local(decl);
    layer_view = transpiler_hosted_zone_layer_slot_view_from_decl(
        ctx, zone_name, decl);
    if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone layer-slot member metadata for '%s'",
            zone_name != NULL ? zone_name : "(anonymous-zone)");
        return NULL;
    }
    for (size_t i = 0; i < layer_view.count; i++) {
        const char *layer_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (layer_name != NULL && strcmp(layer_name, field_name) == 0) {
            return transpiler_hosted_zone_layer_slot_view_type_name(
                &layer_view, i);
        }
    }

    return transpiler_host_shared_member_type_name(ctx, decl, field_name);
}

static const char *
transpiler_world_member_type_name(TranspilerCtx *ctx,
                                  ASTNode *decl,
                                  const char *field_name)
{
    size_t roster_count = 0;
    ASTNode **rosters = ast_world_rosters(decl, &roster_count);
    for (size_t i = 0; i < roster_count; i++) {
        ASTNode *slot = rosters[i];
        const char *slot_name = ast_world_roster_slot_name(slot);
        if (slot_name != NULL && strcmp(slot_name, field_name) == 0) {
            return ast_world_roster_type_name(slot);
        }
    }

    const char *world_name = transpiler_decl_name_local(decl);
    TranspilerHostedWorldZoneSlotView zone_view =
        transpiler_hosted_world_zone_slot_view_from_decl(ctx, world_name, decl);
    if (transpiler_hosted_world_zone_slot_view_missing_mir_metadata(
            &zone_view)) {
        return NULL;
    }
    for (size_t i = 0; i < zone_view.count; i++) {
        const char *slot_name =
            transpiler_hosted_world_zone_slot_view_name(&zone_view, i);
        if (slot_name != NULL && strcmp(slot_name, field_name) == 0) {
            return transpiler_hosted_world_zone_slot_view_type_name(
                &zone_view, i);
        }
    }

    return transpiler_host_shared_member_type_name(ctx, decl, field_name);
}

static const char *
transpiler_domain_host_member_type_name(TranspilerCtx *ctx,
                                        ASTNode *decl,
                                        const char *field_name,
                                        bool include_overlay_shared)
{
    size_t slot_count = 0;
    ASTNode **slots = NULL;
    const char *slot_type = NULL;

    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_ZONE_DECL:
        return transpiler_zone_member_type_name(ctx, decl, field_name);
    case AST_WORLD_DECL:
        return transpiler_world_member_type_name(ctx, decl, field_name);
    case AST_RELATION_DECL:
        slots = ast_relation_slots(decl, &slot_count);
        slot_type = transpiler_domain_slot_member_type_name(
            ctx, slots, slot_count, field_name);
        if (slot_type != NULL || !include_overlay_shared)
            return slot_type;
        return transpiler_host_shared_member_type_name(
            ctx, decl, field_name);
    case AST_EFFECT_DECL:
        slots = ast_effect_slots(decl, &slot_count);
        slot_type = transpiler_domain_slot_member_type_name(
            ctx, slots, slot_count, field_name);
        if (slot_type != NULL || !include_overlay_shared)
            return slot_type;
        return transpiler_host_shared_member_type_name(
            ctx, decl, field_name);
    default:
        return NULL;
    }
}

const char *
transpiler_current_field_type_name(TranspilerCtx *ctx, const char *field_name)
{
    ASTNode *decl;
    const MIRDeclField *field;
    const char *host_name;
    const char *mir_type_name;

    if (ctx == NULL || field_name == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl == NULL)
        return NULL;
    host_name = transpiler_decl_name_local(decl);
    field = transpiler_find_decl_field_metadata(ctx, host_name, field_name);
    if ((decl->type != AST_RELATION_DECL
         && decl->type != AST_EFFECT_DECL)
        || transpiler_mir_decl_field_kind_or(field, MIR_DECL_FIELD_UNKNOWN)
            != MIR_DECL_FIELD_SHARED) {
        mir_type_name = render_mir_decl_field_type_name(ctx, field);
        if (mir_type_name != NULL)
            return mir_type_name;
    }

    switch (decl->type) {
    case AST_CLASS_DECL:
        return transpiler_class_member_type_name(ctx, decl, field_name);
    case AST_ZONE_DECL: {
        return transpiler_domain_host_member_type_name(ctx, decl, field_name, false);
    }
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_WORLD_DECL:
        return transpiler_domain_host_member_type_name(ctx, decl, field_name, false);
    default:
        break;
    }

    return NULL;
}

const char *
transpiler_lookup_nominal_host_member_type_name(TranspilerCtx *ctx,
                                                const char *host_type_name,
                                                const char *member_name)
{
    ASTNode *decl;
    const MIRDeclField *field;
    const char *mir_type_name;

    if (ctx == NULL || host_type_name == NULL || member_name == NULL)
        return NULL;

    field = transpiler_find_decl_field_metadata(ctx, host_type_name, member_name);
    mir_type_name = render_mir_decl_field_type_name(ctx, field);
    if (mir_type_name != NULL)
        return mir_type_name;

    decl = transpiler_find_nominal_host_decl_local(ctx, host_type_name);
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        return transpiler_class_member_type_name(ctx, decl, member_name);
    case AST_ZONE_DECL: {
        return transpiler_domain_host_member_type_name(ctx, decl, member_name, true);
    }
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_WORLD_DECL:
        return transpiler_domain_host_member_type_name(ctx, decl, member_name, true);
    default:
        break;
    }

    return NULL;
}

const char *
transpiler_resolve_nominal_host_expr_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    if (ctx == NULL || expr == NULL)
        return NULL;

    if (expr->type == AST_IDENTIFIER && ast_identifier_name(expr) != NULL) {
        const char *name = ast_identifier_name(expr);
        ASTNode *alias_expr = lookup_alias_expr(ctx, name);
        if (alias_expr != NULL)
            return transpiler_resolve_nominal_host_expr_type_name(ctx, alias_expr);
        const char *type_name = lookup_typed_var(ctx, name);
        if (type_name != NULL)
            return type_name;
        return transpiler_current_field_type_name(ctx, name);
    }

    if (expr->type == AST_MEMBER_ACCESS
        && ast_member_object(expr) != NULL
        && ast_member_name(expr) != NULL) {
        if (ast_member_object(expr)->type == AST_IDENTIFIER
            && strcmp(ast_identifier_name(ast_member_object(expr)), "self") == 0) {
            return transpiler_current_field_type_name(ctx, ast_member_name(expr));
        }
        {
            const char *obj_type = transpiler_resolve_nominal_host_expr_type_name(
                ctx, ast_member_object(expr));
            return transpiler_lookup_nominal_host_member_type_name(
                ctx, obj_type, ast_member_name(expr));
        }
    }

    return NULL;
}
