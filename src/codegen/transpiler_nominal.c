/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend nominal member and receiver type lookup.
 */

#include <string.h>

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
    case AST_CLASS_DECL:
        for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
            ClassField *field = decl->data.class_decl.fields[i];
            if (field != NULL && field->name != NULL
                && strcmp(field->name, field_name) == 0) {
                return render_nominal_member_type_name(ctx, field->type);
            }
        }
        break;
    case AST_ZONE_DECL: {
        ASTNode *slot = transpiler_find_zone_domain_slot(decl, field_name);
        if (slot != NULL)
            return render_nominal_member_type_name(ctx, slot->data.domain_slot.type);
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
                && strcmp(shared->data.party_shared.name, field_name) == 0) {
                return render_nominal_member_type_name(ctx, shared->data.party_shared.type);
            }
        }
        break;
    }
    case AST_RELATION_DECL:
        for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
            ASTNode *slot = decl->data.relation_decl.slots[i];
            if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
                return render_nominal_member_type_name(ctx, slot->data.domain_slot.type);
            }
        }
        break;
    case AST_EFFECT_DECL:
        for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
            ASTNode *slot = decl->data.effect_decl.slots[i];
            if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, field_name) == 0) {
                return render_nominal_member_type_name(ctx, slot->data.domain_slot.type);
            }
        }
        break;
    case AST_WORLD_DECL:
        for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
            ASTNode *slot = decl->data.world_decl.rosters[i];
            if (slot != NULL && slot->data.world_roster.slot_name != NULL
                && strcmp(slot->data.world_roster.slot_name, field_name) == 0) {
                return slot->data.world_roster.roster_type;
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
                && strcmp(shared->data.party_shared.name, field_name) == 0) {
                return render_nominal_member_type_name(ctx, shared->data.party_shared.type);
            }
        }
        break;
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
    case AST_CLASS_DECL:
        for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
            ClassField *field = decl->data.class_decl.fields[i];
            if (field != NULL && field->name != NULL
                && strcmp(field->name, member_name) == 0) {
                return render_nominal_member_type_name(ctx, field->type);
            }
        }
        break;
    case AST_ZONE_DECL: {
        ASTNode *slot = transpiler_find_zone_domain_slot(decl, member_name);
        if (slot != NULL)
            return render_nominal_member_type_name(ctx, slot->data.domain_slot.type);
        for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
            ASTNode *layer = decl->data.zone_decl.layer_slots[i];
            if (layer != NULL && layer->type == AST_ZONE_LAYER_SLOT
                && layer->data.zone_layer_slot.slot_name != NULL
                && strcmp(layer->data.zone_layer_slot.slot_name, member_name) == 0) {
                return layer->data.zone_layer_slot.layer_type;
            }
        }
        for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
            ASTNode *shared = decl->data.zone_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0) {
                return render_nominal_member_type_name(ctx, shared->data.party_shared.type);
            }
        }
        break;
    }
    case AST_RELATION_DECL:
        for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
            ASTNode *slot = decl->data.relation_decl.slots[i];
            if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, member_name) == 0) {
                return render_nominal_member_type_name(ctx, slot->data.domain_slot.type);
            }
        }
        for (size_t i = 0; i < decl->data.relation_decl.shared_count; i++) {
            ASTNode *shared = decl->data.relation_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0) {
                return render_nominal_member_type_name(ctx, shared->data.party_shared.type);
            }
        }
        break;
    case AST_EFFECT_DECL:
        for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
            ASTNode *slot = decl->data.effect_decl.slots[i];
            if (slot != NULL && slot->data.domain_slot.slot_name != NULL
                && strcmp(slot->data.domain_slot.slot_name, member_name) == 0) {
                return render_nominal_member_type_name(ctx, slot->data.domain_slot.type);
            }
        }
        for (size_t i = 0; i < decl->data.effect_decl.shared_count; i++) {
            ASTNode *shared = decl->data.effect_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0) {
                return render_nominal_member_type_name(ctx, shared->data.party_shared.type);
            }
        }
        break;
    case AST_WORLD_DECL:
        for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
            ASTNode *slot = decl->data.world_decl.rosters[i];
            if (slot != NULL && slot->data.world_roster.slot_name != NULL
                && strcmp(slot->data.world_roster.slot_name, member_name) == 0) {
                return slot->data.world_roster.roster_type;
            }
        }
        for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
            ASTNode *slot = decl->data.world_decl.zones[i];
            if (slot != NULL && slot->data.world_zone.slot_name != NULL
                && strcmp(slot->data.world_zone.slot_name, member_name) == 0) {
                return slot->data.world_zone.zone_type;
            }
        }
        for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
            ASTNode *shared = decl->data.world_decl.shared_fields[i];
            if (shared != NULL && shared->data.party_shared.name != NULL
                && strcmp(shared->data.party_shared.name, member_name) == 0) {
                return render_nominal_member_type_name(ctx, shared->data.party_shared.type);
            }
        }
        break;
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

    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        ASTNode *alias_expr = lookup_alias_expr(ctx, expr->data.identifier.name);
        if (alias_expr != NULL)
            return transpiler_resolve_nominal_host_expr_type_name(ctx, alias_expr);
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL)
            return type_name;
        return transpiler_current_field_type_name(ctx, expr->data.identifier.name);
    }

    if (expr->type == AST_MEMBER_ACCESS
        && expr->data.member.object != NULL
        && expr->data.member.name != NULL) {
        if (expr->data.member.object->type == AST_IDENTIFIER
            && strcmp(expr->data.member.object->data.identifier.name, "self") == 0) {
            return transpiler_current_field_type_name(ctx, expr->data.member.name);
        }
        {
            const char *obj_type = transpiler_resolve_nominal_host_expr_type_name(
                ctx, expr->data.member.object);
            return transpiler_lookup_nominal_host_member_type_name(
                ctx, obj_type, expr->data.member.name);
        }
    }

    return NULL;
}
