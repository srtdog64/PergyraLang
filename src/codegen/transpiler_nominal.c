/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend nominal member and receiver type lookup.
 */

#include <string.h>

#include "../parser/ast_api.h"
#include "host_decl_compat.h"
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
transpiler_domain_slot_member_type_name(TranspilerCtx *ctx,
                                        ASTNode **slots,
                                        size_t slot_count,
                                        const char *field_name)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *slot_name = ast_domain_slot_name(slot);
        if (slot != NULL && slot_name != NULL
            && strcmp(slot_name, field_name) == 0) {
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
    ASTNode *shared =
        pgy_host_shared_field_compat_find(decl, field_name);
    return shared != NULL
        ? render_nominal_member_type_name(ctx, ast_party_shared_type(shared))
        : NULL;
}

static const char *
transpiler_zone_member_type_name(TranspilerCtx *ctx,
                                 ASTNode *decl,
                                 const char *field_name)
{
    size_t slot_count = 0;
    ASTNode **slots = ast_zone_slots(decl, &slot_count);
    const char *slot_type = transpiler_domain_slot_member_type_name(
        ctx, slots, slot_count, field_name);
    if (slot_type != NULL)
        return slot_type;

    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(decl, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *layer = layer_slots[i];
        if (layer != NULL && layer->type == AST_ZONE_LAYER_SLOT
            && ast_zone_layer_slot_name(layer) != NULL
            && strcmp(ast_zone_layer_slot_name(layer), field_name) == 0) {
            return ast_zone_layer_slot_layer_type(layer);
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
        if (slot != NULL && slot_name != NULL
            && strcmp(slot_name, field_name) == 0) {
            return ast_world_roster_type_name(slot);
        }
    }

    size_t zone_count = 0;
    ASTNode **zones = ast_world_zones(decl, &zone_count);
    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *slot = zones[i];
        const char *slot_name = ast_world_zone_slot_name(slot);
        if (slot != NULL && slot_name != NULL
            && strcmp(slot_name, field_name) == 0) {
            return ast_world_zone_type_name(slot);
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

    if (ctx == NULL || field_name == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL: {
        ClassField *field =
            pgy_host_class_field_compat_find(decl, field_name);
        if (field != NULL)
            return render_nominal_member_type_name(ctx, field->type);
        break;
    }
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

    if (ctx == NULL || host_type_name == NULL || member_name == NULL)
        return NULL;

    decl = transpiler_find_nominal_host_decl_local(ctx, host_type_name);
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL: {
        ClassField *field =
            pgy_host_class_field_compat_find(decl, member_name);
        if (field != NULL)
            return render_nominal_member_type_name(ctx, field->type);
        break;
    }
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
