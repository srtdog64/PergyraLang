/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker built-in dispatch and stdlib helpers
 */

#include <string.h>
#include "type_checker_internal.h"

static bool
check_call_arity(ASTNode *expr, size_t expected, const char *name,
                 SemanticContext *ctx)
{
    if (expr->data.call.arg_count != expected) {
        semantic_error(ctx, expr,
            "'%s' expects %zu argument(s), got %zu",
            name, expected, expr->data.call.arg_count);
        return false;
    }
    return true;
}

static Type *
channel_builtin_element_type(ASTNode *expr, size_t channel_arg_index,
                             const char *name, SemanticContext *ctx)
{
    Type *ch_type = type_check_expression(
        expr->data.call.arguments[channel_arg_index], ctx);
    if (!type_is_constructed_named(ch_type, "Channel")) {
        semantic_error(ctx, expr->data.call.arguments[channel_arg_index],
            "%s requires Channel<T>, got '%s'", name,
            ch_type != NULL ? ch_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return type_get_constructed_arg(ch_type, 0);
}

static Type *
channel_builtin_recv_result(Type *element_type, const char *name,
                            ASTNode *site, SemanticContext *ctx)
{
    if (type_is_anchored_resource_handle(element_type)) {
        semantic_error(ctx, site,
            "%s cannot yield anchored resource handles (Slot/SecureSlot/DeviceSlot) yet; receive a plain value instead",
            name);
        return TYPE_UNKNOWN;
    }
    if (type_is_movable_resource_handle(element_type)) {
        semantic_error(ctx, site,
            "%s does not support movable resource channels yet; use blocking '<-' receive and bind the result first",
            name);
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_OPTION, element_type);
}

static ASTNode *
find_zone_domain_slot_local(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.slots[i];
        if (slot != NULL
            && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

static ASTNode *
find_zone_layer_slot_local(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

static ASTNode *
find_domain_projection_slot_local(ASTNode **slots, size_t slot_count,
                                  ASTNode **refreshes, size_t refresh_count,
                                  const char *slot_name);

static ASTNode *
find_world_zone_slot_local_builtin(ASTNode *world, const char *slot_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.zone_count; i++) {
        ASTNode *zone = world->data.world_decl.zones[i];
        if (zone != NULL
            && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

static ASTNode *
find_program_domain_decl_local(ASTNode *program, ASTNodeType decl_type, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != decl_type)
            continue;
        switch (decl_type) {
        case AST_ZONE_DECL:
            if (stmt->data.zone_decl.name != NULL
                && strcmp(stmt->data.zone_decl.name, name) == 0)
                return stmt;
            break;
        case AST_RELATION_DECL:
            if (stmt->data.relation_decl.name != NULL
                && strcmp(stmt->data.relation_decl.name, name) == 0)
                return stmt;
            break;
        case AST_EFFECT_DECL:
            if (stmt->data.effect_decl.name != NULL
                && strcmp(stmt->data.effect_decl.name, name) == 0)
                return stmt;
            break;
        case AST_WORLD_DECL:
            if (stmt->data.world_decl.name != NULL
                && strcmp(stmt->data.world_decl.name, name) == 0)
                return stmt;
            break;
        default:
            break;
        }
    }

    return NULL;
}

static ASTNode *
resolve_world_zone_decl_local(SemanticContext *ctx, ASTNode *world, const char *slot_name)
{
    ASTNode *zone_slot;

    if (ctx == NULL || world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    zone_slot = find_world_zone_slot_local_builtin(world, slot_name);
    if (zone_slot == NULL || zone_slot->data.world_zone.zone_type == NULL)
        return NULL;

    return find_program_domain_decl_local(ctx->program_root, AST_ZONE_DECL,
        zone_slot->data.world_zone.zone_type);
}

static ASTNode *
find_zone_projection_slot_local(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL)
        return NULL;
    return find_domain_projection_slot_local(zone->data.zone_decl.slots,
        zone->data.zone_decl.slot_count,
        zone->data.zone_decl.refreshes,
        zone->data.zone_decl.refresh_count,
        slot_name);
}

static ASTNode *
find_zone_state_decl_local_builtin(ASTNode *zone, const char *state_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        ASTNode *state = zone->data.zone_decl.states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
find_domain_projection_slot_local(ASTNode **slots, size_t slot_count,
                                  ASTNode **refreshes, size_t refresh_count,
                                  const char *slot_name)
{
    if (slots == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.slot_name == NULL
            || strcmp(slot->data.domain_slot.slot_name, slot_name) != 0) {
            continue;
        }
        if (!slot->data.domain_slot.is_subject) {
            for (size_t j = 0; j < refresh_count; j++) {
                ASTNode *refresh = refreshes[j];
                if (refresh != NULL && refresh->type == AST_ZONE_REFRESH
                    && refresh->data.zone_refresh.object_slot_name != NULL
                    && strcmp(refresh->data.zone_refresh.object_slot_name,
                              slot_name) == 0) {
                    return slot;
                }
            }
            if (slot->data.domain_slot.is_dto)
                return slot;
        }
        return NULL;
    }

    return NULL;
}

static ASTNode *
current_projection_host_decl(SemanticContext *ctx, const char **label_out,
                             ASTNode ***slots_out, size_t *slot_count_out)
{
    if (label_out != NULL)
        *label_out = NULL;
    if (slots_out != NULL)
        *slots_out = NULL;
    if (slot_count_out != NULL)
        *slot_count_out = 0;
    if (ctx == NULL)
        return NULL;

    if (ctx->current_relation != NULL
        && ctx->current_relation->type == AST_RELATION_DECL) {
        if (label_out != NULL)
            *label_out = "relation";
        if (slots_out != NULL)
            *slots_out = ctx->current_relation->data.relation_decl.slots;
        if (slot_count_out != NULL)
            *slot_count_out = ctx->current_relation->data.relation_decl.slot_count;
        return ctx->current_relation;
    }

    if (ctx->current_effect != NULL
        && ctx->current_effect->type == AST_EFFECT_DECL) {
        if (label_out != NULL)
            *label_out = "effect";
        if (slots_out != NULL)
            *slots_out = ctx->current_effect->data.effect_decl.slots;
        if (slot_count_out != NULL)
            *slot_count_out = ctx->current_effect->data.effect_decl.slot_count;
        return ctx->current_effect;
    }

    if (ctx->current_zone != NULL && ctx->current_zone->type == AST_ZONE_DECL) {
        if (label_out != NULL)
            *label_out = "zone";
        if (slots_out != NULL)
            *slots_out = ctx->current_zone->data.zone_decl.slots;
        if (slot_count_out != NULL)
            *slot_count_out = ctx->current_zone->data.zone_decl.slot_count;
        return ctx->current_zone;
    }
    return NULL;
}

static bool
type_is_future_like(Type *type)
{
    return type_is_constructed_named(type, "Future")
        || type_is_constructed_named(type, "RemoteFuture");
}

static ASTNode *
find_named_class_decl(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_CLASS_DECL
            || stmt->data.class_decl.name == NULL) {
            continue;
        }
        if (strcmp(stmt->data.class_decl.name, name) == 0)
            return stmt;
    }

    return NULL;
}

static bool
decl_is_subject_nominal(ASTNode *decl)
{
    return (decl != NULL
            && decl->type == AST_CLASS_DECL
            && !decl->data.class_decl.is_struct
            && decl->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT);
}

static size_t
projection_source_field_count_local(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.field_count;
    return 0;
}

static ClassField *
projection_source_field_at_local(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        if (index < decl->data.class_decl.field_count)
            return decl->data.class_decl.fields[index];
        return NULL;
    }
    return NULL;
}

static int
resolve_projection_source_field_type_rec(ASTNode *program,
                                         ASTNode *source_decl,
                                         const char *field_name,
                                         unsigned depth,
                                         SemanticContext *ctx,
                                         Type **field_type_out)
{
    size_t field_count;
    int match_count = 0;
    Type *resolved_type = NULL;

    if (field_type_out != NULL)
        *field_type_out = NULL;
    if (program == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = projection_source_field_count_local(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at_local(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (field_type_out != NULL)
                *field_type_out = field->type != NULL
                    ? resolve_type_node(field->type, ctx)
                    : TYPE_UNKNOWN;
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at_local(source_decl, i);
        ASTNode *vessel_decl;
        Type *nested_type = NULL;
        int nested_status;

        if (field == NULL || !field->is_vessel_field
            || field->type == NULL || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = find_named_class_decl(program, field->type->data.type.name);
        if (vessel_decl == NULL || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL)
            continue;

        nested_status = resolve_projection_source_field_type_rec(
            program, vessel_decl, field_name, depth + 1, ctx, &nested_type);
        if (nested_status == 1) {
            match_count++;
            if (match_count == 1)
                resolved_type = nested_type;
            else
                resolved_type = NULL;
        } else if (nested_status == 2) {
            match_count = 2;
            resolved_type = NULL;
        }
    }

    if (match_count == 1) {
        if (field_type_out != NULL)
            *field_type_out = resolved_type;
        return 1;
    }
    return match_count > 1 ? 2 : 0;
}

/* subject_host_field_count / subject_host_field_at:
 * defined in type_checker_helpers.inc — removed duplicate here */

static Type *
type_check_channel_send_builtin(ASTNode *expr, const char *name,
                                bool has_timeout, bool detailed_status,
                                SemanticContext *ctx)
{
    if (!check_call_arity(expr, has_timeout ? 3 : 2, name, ctx))
        return TYPE_UNKNOWN;

    semantic_record_effect(ctx, EFFECT_REMOTE);
    Type *element_type = channel_builtin_element_type(expr, 0, name, ctx);
    Type *value_type = type_check_expression(expr->data.call.arguments[1], ctx);

    if (has_timeout) {
        require_assignable(type_check_expression(expr->data.call.arguments[2], ctx),
            TYPE_INT, expr->data.call.arguments[2], ctx);
    }

    if (element_type == TYPE_UNKNOWN) {
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (type_is_anchored_resource_handle(element_type)
        || type_is_anchored_resource_handle(value_type)) {
        semantic_error(ctx, expr->data.call.arguments[1],
            "%s cannot transport anchored resource handles (Slot/SecureSlot/DeviceSlot) yet; send the inner value or keep the handle local",
            name);
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (type_is_movable_resource_handle(element_type)
        || type_is_movable_resource_handle(value_type)) {
        semantic_error(ctx, expr->data.call.arguments[1],
            "%s does not support movable resource sends yet; use blocking 'ch <- value' so ownership transfer stays explicit",
            name);
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    require_assignable(value_type, element_type, expr->data.call.arguments[1], ctx);
    return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                           : TYPE_BOOL;
}

static Type *
type_check_channel_recv_builtin(ASTNode *expr, const char *name,
                                bool has_timeout, SemanticContext *ctx)
{
    if (!check_call_arity(expr, has_timeout ? 2 : 1, name, ctx))
        return TYPE_UNKNOWN;

    semantic_record_effect(ctx, EFFECT_REMOTE);
    Type *element_type = channel_builtin_element_type(expr, 0, name, ctx);
    if (has_timeout) {
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
    }
    return channel_builtin_recv_result(
        element_type, name, expr->data.call.arguments[0], ctx);
}

static Type *
type_check_has_projection(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *host;
    ASTNode *arg;
    ASTNode **slots = NULL;
    ASTNode **refreshes = NULL;
    size_t slot_count = 0;
    size_t refresh_count = 0;
    const char *label = NULL;
    const char *slot_name = NULL;
    ASTNode *slot;

    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call,
            "'HasProjection' expects exactly 1 argument, got %zu",
            call->data.call.arg_count);
        return TYPE_BOOL;
    }

    host = current_projection_host_decl(ctx, &label, &slots, &slot_count);
    if (host == NULL) {
        semantic_error(ctx, call,
            "HasProjection(...) is only available inside relation/effect/zone declarations and methods");
        return TYPE_BOOL;
    }
    if (host->type == AST_RELATION_DECL) {
        refreshes = host->data.relation_decl.refreshes;
        refresh_count = host->data.relation_decl.refresh_count;
    } else if (host->type == AST_EFFECT_DECL) {
        refreshes = host->data.effect_decl.refreshes;
        refresh_count = host->data.effect_decl.refresh_count;
    } else if (host->type == AST_ZONE_DECL) {
        refreshes = host->data.zone_decl.refreshes;
        refresh_count = host->data.zone_decl.refresh_count;
    }

    arg = call->data.call.arguments[0];
    if (arg == NULL) {
        semantic_error(ctx, call,
            "HasProjection(...) requires an object/tobject slot name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        slot_name = arg->data.identifier.name;
    } else if (arg->type == AST_STRING) {
        slot_name = arg->data.string.value;
    } else {
        semantic_error(ctx, arg,
            "HasProjection(...) expects an object/tobject slot identifier or string literal");
        return TYPE_BOOL;
    }

    if (slot_name == NULL) {
        semantic_error(ctx, arg,
            "HasProjection(...) requires a valid object/tobject slot name");
        return TYPE_BOOL;
    }

    slot = find_domain_projection_slot_local(slots, slot_count,
        refreshes, refresh_count, slot_name);
    if (slot == NULL) {
        semantic_error(ctx, arg,
            "Unknown %s projection slot '%s' in HasProjection(...)",
            label != NULL ? label : "domain", slot_name);
        return TYPE_BOOL;
    }

    return TYPE_BOOL;
}

static Type *
type_check_has_layer(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *zone;
    ASTNode *arg;
    const char *slot_name = NULL;
    ASTNode *layer_slot;

    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call,
            "'HasLayer' expects exactly 1 argument, got %zu",
            call->data.call.arg_count);
        return TYPE_BOOL;
    }

    zone = ctx->current_zone;
    if (zone == NULL || zone->type != AST_ZONE_DECL) {
        semantic_error(ctx, call,
            "HasLayer(...) is only available inside zone declarations and zone methods");
        return TYPE_BOOL;
    }

    arg = call->data.call.arguments[0];
    if (arg == NULL) {
        semantic_error(ctx, call,
            "HasLayer(...) requires a relation/effect slot name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        slot_name = arg->data.identifier.name;
    } else if (arg->type == AST_STRING) {
        slot_name = arg->data.string.value;
    } else {
        semantic_error(ctx, arg,
            "HasLayer(...) expects a layer-slot identifier or string literal");
        return TYPE_BOOL;
    }

    if (slot_name == NULL) {
        semantic_error(ctx, arg,
            "HasLayer(...) requires a valid relation/effect slot name");
        return TYPE_BOOL;
    }

    layer_slot = find_zone_layer_slot_local(zone, slot_name);
    if (layer_slot == NULL) {
        semantic_error(ctx, arg,
            "Unknown zone layer slot '%s' in HasLayer(...)",
            slot_name);
        return TYPE_BOOL;
    }

    return TYPE_BOOL;
}

static Type *
type_check_has_state(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *zone;
    ASTNode *arg;
    ASTNode *state = NULL;
    const char *state_name = NULL;
    const char *slot_name = NULL;
    const char *left_slot_name = NULL;
    const char *right_slot_name = NULL;

    if (call->data.call.arg_count < 1 || call->data.call.arg_count > 3) {
        semantic_error(ctx, call,
            "'HasState' expects 1 to 3 argument(s), got %zu",
            call->data.call.arg_count);
        return TYPE_BOOL;
    }

    zone = ctx->current_zone;
    if (zone == NULL || zone->type != AST_ZONE_DECL) {
        semantic_error(ctx, call,
            "HasState(...) is only available inside zone declarations and zone methods");
        return TYPE_BOOL;
    }

    arg = call->data.call.arguments[0];
    if (arg == NULL) {
        semantic_error(ctx, call,
            "HasState(...) requires a zone state name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        state_name = arg->data.identifier.name;
    } else if (arg->type == AST_STRING) {
        state_name = arg->data.string.value;
    } else {
        semantic_error(ctx, arg,
            "HasState(...) expects a state identifier or string literal");
        return TYPE_BOOL;
    }

    if (state_name == NULL) {
        semantic_error(ctx, arg,
            "HasState(...) requires a valid zone state name");
        return TYPE_BOOL;
    }

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        state = zone->data.zone_decl.states[i];
        if (state != NULL
            && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            break;
        }
        state = NULL;
    }

    if (state == NULL) {
        semantic_error(ctx, arg,
            "Unknown zone state '%s' in HasState(...)",
            state_name);
        return TYPE_BOOL;
    }

    if (call->data.call.arg_count == 1)
        return TYPE_BOOL;

    if (call->data.call.arguments[1] == NULL
        || call->data.call.arguments[1]->type != AST_IDENTIFIER
        || call->data.call.arguments[1]->data.identifier.name == NULL) {
        semantic_error(ctx, call->data.call.arguments[1],
            "HasState(...) slot arguments must be zone slot identifiers");
        return TYPE_BOOL;
    }
    if (find_zone_domain_slot_local(zone,
            call->data.call.arguments[1]->data.identifier.name) == NULL) {
        semantic_error(ctx, call->data.call.arguments[1],
            "Unknown zone slot '%s' in HasState(...)",
            call->data.call.arguments[1]->data.identifier.name);
        return TYPE_BOOL;
    }

    if (!state->data.zone_state.is_relation) {
        slot_name = call->data.call.arguments[1]->data.identifier.name;
        if (call->data.call.arg_count != 2) {
            semantic_error(ctx, call,
                "Effect state '%s' in HasState(...) accepts at most one zone slot target",
                state_name);
            return TYPE_BOOL;
        }
        if (strcmp(slot_name, state->data.zone_state.left_or_target_slot_name) != 0) {
            semantic_error(ctx, call->data.call.arguments[1],
                "State '%s' is declared on slot '%s', not '%s'",
                state_name,
                state->data.zone_state.left_or_target_slot_name,
                slot_name);
        }
        return TYPE_BOOL;
    }

    if (call->data.call.arg_count != 3) {
        semantic_error(ctx, call,
            "Relation state '%s' in HasState(...) requires exactly two endpoint slots",
            state_name);
        return TYPE_BOOL;
    }

    if (call->data.call.arguments[2] == NULL
        || call->data.call.arguments[2]->type != AST_IDENTIFIER
        || call->data.call.arguments[2]->data.identifier.name == NULL) {
        semantic_error(ctx, call->data.call.arguments[2],
            "HasState(...) relation endpoint arguments must be zone slot identifiers");
        return TYPE_BOOL;
    }
    if (find_zone_domain_slot_local(zone,
            call->data.call.arguments[2]->data.identifier.name) == NULL) {
        semantic_error(ctx, call->data.call.arguments[2],
            "Unknown zone slot '%s' in HasState(...)",
            call->data.call.arguments[2]->data.identifier.name);
        return TYPE_BOOL;
    }

    left_slot_name = call->data.call.arguments[1]->data.identifier.name;
    right_slot_name = call->data.call.arguments[2]->data.identifier.name;
    if (strcmp(left_slot_name, state->data.zone_state.left_or_target_slot_name) != 0
        || strcmp(right_slot_name, state->data.zone_state.right_slot_name) != 0) {
        semantic_error(ctx, call,
            "State '%s' is declared between '%s' and '%s', not '%s' and '%s'",
            state_name,
            state->data.zone_state.left_or_target_slot_name,
            state->data.zone_state.right_slot_name,
            left_slot_name,
            right_slot_name);
    }
    return TYPE_BOOL;
}

static Type *
type_check_has_zone(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *world;
    ASTNode *arg;
    const char *name = NULL;

    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call,
            "'HasZone' expects exactly 1 argument, got %zu",
            call->data.call.arg_count);
        return TYPE_BOOL;
    }

    world = ctx->current_world;
    if (world == NULL || world->type != AST_WORLD_DECL) {
        semantic_error(ctx, call,
            "HasZone(...) is only available inside world declarations and world methods");
        return TYPE_BOOL;
    }

    arg = call->data.call.arguments[0];
    if (arg == NULL) {
        semantic_error(ctx, call, "HasZone(...) requires a zone slot or world state name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        name = arg->data.identifier.name;
    } else if (arg->type == AST_STRING) {
        name = arg->data.string.value;
    } else {
        semantic_error(ctx, arg,
            "HasZone(...) expects a zone/state identifier or string literal");
        return TYPE_BOOL;
    }

    if (name == NULL) {
        semantic_error(ctx, arg,
            "HasZone(...) requires a valid zone/state name");
        return TYPE_BOOL;
    }

    for (size_t i = 0; i < world->data.world_decl.zone_count; i++) {
        ASTNode *zone = world->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, name) == 0) {
            return TYPE_BOOL;
        }
    }

    for (size_t i = 0; i < world->data.world_decl.state_count; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, name) == 0) {
            return TYPE_BOOL;
        }
    }

    semantic_error(ctx, arg,
        "Unknown world zone/state '%s' in HasZone(...)",
        name);
    return TYPE_BOOL;
}

static Type *
type_check_has_world_zone_detail(ASTNode *call, SemanticContext *ctx,
                                 const char *builtin_name,
                                 ASTNode *(*resolver)(ASTNode *, const char *),
                                 const char *detail_label)
{
    ASTNode *world;
    ASTNode *zone_decl;
    ASTNode *zone_arg;
    ASTNode *detail_arg;
    const char *zone_slot_name = NULL;
    const char *detail_name = NULL;
    ASTNode *detail_decl;

    if (call->data.call.arg_count != 2) {
        semantic_error(ctx, call,
            "'%s' expects exactly 2 argument(s), got %zu",
            builtin_name, call->data.call.arg_count);
        return TYPE_BOOL;
    }

    world = ctx->current_world;
    if (world == NULL || world->type != AST_WORLD_DECL) {
        semantic_error(ctx, call,
            "%s(...) is only available inside world declarations and world methods",
            builtin_name);
        return TYPE_BOOL;
    }

    zone_arg = call->data.call.arguments[0];
    detail_arg = call->data.call.arguments[1];
    if (zone_arg == NULL || detail_arg == NULL) {
        semantic_error(ctx, call,
            "%s(...) requires a world zone slot and zone %s name",
            builtin_name, detail_label);
        return TYPE_BOOL;
    }

    if (zone_arg->type == AST_IDENTIFIER)
        zone_slot_name = zone_arg->data.identifier.name;
    else if (zone_arg->type == AST_STRING)
        zone_slot_name = zone_arg->data.string.value;
    else {
        semantic_error(ctx, zone_arg,
            "%s(...) expects a world zone-slot identifier or string literal as the first argument",
            builtin_name);
        return TYPE_BOOL;
    }

    if (detail_arg->type == AST_IDENTIFIER)
        detail_name = detail_arg->data.identifier.name;
    else if (detail_arg->type == AST_STRING)
        detail_name = detail_arg->data.string.value;
    else {
        semantic_error(ctx, detail_arg,
            "%s(...) expects a zone %s identifier or string literal as the second argument",
            builtin_name, detail_label);
        return TYPE_BOOL;
    }

    if (zone_slot_name == NULL || detail_name == NULL) {
        semantic_error(ctx, call,
            "%s(...) requires valid zone-slot and %s names",
            builtin_name, detail_label);
        return TYPE_BOOL;
    }

    zone_decl = resolve_world_zone_decl_local(ctx, world, zone_slot_name);
    if (zone_decl == NULL) {
        semantic_error(ctx, zone_arg,
            "Unknown world zone slot '%s' in %s(...)",
            zone_slot_name, builtin_name);
        return TYPE_BOOL;
    }

    detail_decl = resolver(zone_decl, detail_name);
    if (detail_decl == NULL) {
        semantic_error(ctx, detail_arg,
            "Unknown zone %s '%s' in %s(%s, ...)",
            detail_label, detail_name, builtin_name, zone_slot_name);
        return TYPE_BOOL;
    }

    return TYPE_BOOL;
}

BuiltinKind
builtin_resolve(const char *name)
{
    if (strcmp(name, "ClaimSlot")       == 0) return BUILTIN_CLAIM_SLOT;
    if (strcmp(name, "ClaimSecureSlot") == 0) return BUILTIN_CLAIM_SECURE_SLOT;
    if (strcmp(name, "ClaimDeviceSlot") == 0) return BUILTIN_CLAIM_DEVICE_SLOT;
    if (strcmp(name, "ViewRead")        == 0) return BUILTIN_VIEW_READ;
    if (strcmp(name, "ViewWrite")       == 0) return BUILTIN_VIEW_WRITE;
    if (strcmp(name, "Move")            == 0) return BUILTIN_MOVE;
    if (strcmp(name, "Write")           == 0) return BUILTIN_WRITE;
    if (strcmp(name, "Read")            == 0) return BUILTIN_READ;
    if (strcmp(name, "Release")         == 0) return BUILTIN_RELEASE;
    if (strcmp(name, "DeviceWrite")     == 0) return BUILTIN_DEVICE_WRITE;
    if (strcmp(name, "DeviceRead")      == 0) return BUILTIN_DEVICE_READ;
    if (strcmp(name, "ReleaseDeviceSlot") == 0) return BUILTIN_RELEASE_DEVICE_SLOT;
    if (strcmp(name, "SubmitDeviceRead") == 0) return BUILTIN_SUBMIT_DEVICE_READ;
    if (strcmp(name, "Log")             == 0) return BUILTIN_LOG;
    if (strcmp(name, "LogBanner")       == 0) return BUILTIN_LOG_BANNER;
    if (strcmp(name, "LogBlock")        == 0) return BUILTIN_LOG_BLOCK;
    if (strcmp(name, "LogRaw")          == 0) return BUILTIN_LOG_RAW;
    if (strcmp(name, "RcNew")           == 0) return BUILTIN_RC_NEW;
    if (strcmp(name, "RcClone")         == 0) return BUILTIN_RC_CLONE;
    if (strcmp(name, "RcDrop")          == 0) return BUILTIN_RC_DROP;
    if (strcmp(name, "RcDowngrade")     == 0) return BUILTIN_RC_DOWNGRADE;
    if (strcmp(name, "RcGet")           == 0) return BUILTIN_RC_GET;
    if (strcmp(name, "WeakUpgrade")     == 0) return BUILTIN_WEAK_UPGRADE;
    if (strcmp(name, "WeakDrop")        == 0) return BUILTIN_WEAK_DROP;
    if (strcmp(name, "AllocatorSystem") == 0) return BUILTIN_ALLOCATOR_SYSTEM;
    if (strcmp(name, "AllocatorTracing")== 0) return BUILTIN_ALLOCATOR_TRACING;
    if (strcmp(name, "AllocatorDebug")  == 0) return BUILTIN_ALLOCATOR_DEBUG;
    if (strcmp(name, "AllocatorPool")   == 0) return BUILTIN_ALLOCATOR_POOL;
    if (strcmp(name, "Box")             == 0) return BUILTIN_BOX;
    if (strcmp(name, "BoxGet")          == 0) return BUILTIN_BOX_GET;
    if (strcmp(name, "BoxSet")          == 0) return BUILTIN_BOX_SET;
    if (strcmp(name, "BoxDrop")         == 0) return BUILTIN_BOX_DROP;
    if (strcmp(name, "BoxIsValid")      == 0) return BUILTIN_BOX_IS_VALID;
    if (strcmp(name, "BoxArray")        == 0) return BUILTIN_BOX_ARRAY;
    if (strcmp(name, "ToObject")        == 0) return BUILTIN_TO_OBJECT;
    if (strcmp(name, "ToTObject")       == 0) return BUILTIN_TO_DTO;
    if (strcmp(name, "HasProjection")   == 0) return BUILTIN_HAS_PROJECTION;
    if (strcmp(name, "HasLayer")        == 0) return BUILTIN_HAS_LAYER;
    if (strcmp(name, "HasState")        == 0) return BUILTIN_HAS_STATE;
    if (strcmp(name, "HasZone")         == 0) return BUILTIN_HAS_ZONE;
    if (strcmp(name, "HasZoneProjection") == 0) return BUILTIN_HAS_ZONE_PROJECTION;
    if (strcmp(name, "HasZoneLayer")    == 0) return BUILTIN_HAS_ZONE_LAYER;
    if (strcmp(name, "HasZoneState")    == 0) return BUILTIN_HAS_ZONE_STATE;
    if (strcmp(name, "StringSplit")     == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringJoin")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringContains")  == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringReplace")   == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "Substring")       == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringTrim")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ToUpper")         == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ToLower")         == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "StringConcat")    == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ClaimQubit")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "Measure")         == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "Entangle")        == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "QubitState")      == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "IsCollapsed")     == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "ReleaseQubit")    == 0) return BUILTIN_NOT_BUILTIN;
    if (strcmp(name, "FileOpen")        == 0) return BUILTIN_FILE_OPEN;
    if (strcmp(name, "FileRead")        == 0) return BUILTIN_FILE_READ;
    if (strcmp(name, "FileWrite")       == 0) return BUILTIN_FILE_WRITE;
    if (strcmp(name, "FileClose")       == 0) return BUILTIN_FILE_CLOSE;
    if (strcmp(name, "ReadFile")        == 0) return BUILTIN_READ_FILE;
    if (strcmp(name, "WriteFile")       == 0) return BUILTIN_WRITE_FILE;
    if (strcmp(name, "Input")           == 0) return BUILTIN_INPUT;
    if (strcmp(name, "Print")           == 0) return BUILTIN_PRINT;
    if (strcmp(name, "ReadLine")        == 0) return BUILTIN_READ_LINE;
    if (strcmp(name, "Now")             == 0) return BUILTIN_NOW;
    if (strcmp(name, "Sleep")           == 0) return BUILTIN_SLEEP;
    if (strcmp(name, "IntentLastTrace") == 0) return BUILTIN_INTENT_LAST_TRACE;
    if (strcmp(name, "IntentLastFailure") == 0) return BUILTIN_INTENT_LAST_FAILURE;
    if (strcmp(name, "IntentLastName")  == 0) return BUILTIN_INTENT_LAST_NAME;
    if (strcmp(name, "IntentLastHandle") == 0) return BUILTIN_INTENT_LAST_HANDLE;
    if (strcmp(name, "IntentLastTraceId") == 0) return BUILTIN_INTENT_LAST_TRACE_ID;
    if (strcmp(name, "IntentLastStepCount") == 0) return BUILTIN_INTENT_LAST_STEP_COUNT;
    if (strcmp(name, "IntentLastFailed") == 0) return BUILTIN_INTENT_LAST_FAILED;
    if (strcmp(name, "IntentHistoryCount") == 0) return BUILTIN_INTENT_HISTORY_COUNT;
    if (strcmp(name, "IntentHistoryStepName") == 0) return BUILTIN_INTENT_HISTORY_STEP_NAME;
    if (strcmp(name, "IntentHistoryStepZone") == 0) return BUILTIN_INTENT_HISTORY_STEP_ZONE;
    if (strcmp(name, "IntentHistoryStepPhase") == 0) return BUILTIN_INTENT_HISTORY_STEP_PHASE;
    if (strcmp(name, "IntentHistoryStepParticipant") == 0) return BUILTIN_INTENT_HISTORY_STEP_PARTICIPANT;
    if (strcmp(name, "IntentHistoryStepSlot") == 0) return BUILTIN_INTENT_HISTORY_STEP_SLOT;
    if (strcmp(name, "IntentHistoryStepFromZone") == 0) return BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE;
    if (strcmp(name, "IntentHistoryStepFromSlot") == 0) return BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT;
    if (strcmp(name, "IntentHistoryStepToZone") == 0) return BUILTIN_INTENT_HISTORY_STEP_TO_ZONE;
    if (strcmp(name, "IntentHistoryStepToSlot") == 0) return BUILTIN_INTENT_HISTORY_STEP_TO_SLOT;
    if (strcmp(name, "IntentHistoryStepOk") == 0) return BUILTIN_INTENT_HISTORY_STEP_OK;
    if (strcmp(name, "IntentHistoryStepFailure") == 0) return BUILTIN_INTENT_HISTORY_STEP_FAILURE;
    if (strcmp(name, "IntentActiveCount") == 0) return BUILTIN_INTENT_ACTIVE_COUNT;
    if (strcmp(name, "IntentActiveName") == 0) return BUILTIN_INTENT_ACTIVE_NAME;
    if (strcmp(name, "IntentActiveHandle") == 0) return BUILTIN_INTENT_ACTIVE_HANDLE;
    if (strcmp(name, "IntentActiveTraceId") == 0) return BUILTIN_INTENT_ACTIVE_TRACE_ID;
    if (strcmp(name, "IntentActivePriority") == 0) return BUILTIN_INTENT_ACTIVE_PRIORITY;
    if (strcmp(name, "IntentActiveConcurrent") == 0) return BUILTIN_INTENT_ACTIVE_CONCURRENT;
    if (strcmp(name, "IntentActiveTrace") == 0) return BUILTIN_INTENT_ACTIVE_TRACE;
    return BUILTIN_NOT_BUILTIN;
}

Type *
type_check_claim_slot(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 0) {
        semantic_error(ctx, call, "ClaimSlot takes no arguments");
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}

static Type *
type_check_claim_device_slot(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 0, "ClaimDeviceSlot", ctx))
        return TYPE_UNKNOWN;
    semantic_record_effect(ctx, EFFECT_REMOTE);
    return wrap_constructed(TYPE_DEVICE_SLOT, TYPE_INT);
}

static Type *
type_check_view_read(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 1, "ViewRead", ctx))
        return TYPE_UNKNOWN;

    Type *slot_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_owned_slot_handle(slot_type)) {
        semantic_error(ctx, call->data.call.arguments[0],
            "ViewRead requires owning Slot<T>, got '%s'",
            slot_type != NULL ? slot_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return type_create_read_view(slot_type->data.slot.inner_type);
}

static Type *
type_check_view_write(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 1, "ViewWrite", ctx))
        return TYPE_UNKNOWN;

    Type *slot_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_owned_slot_handle(slot_type)) {
        semantic_error(ctx, call->data.call.arguments[0],
            "ViewWrite requires owning Slot<T>, got '%s'",
            slot_type != NULL ? slot_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return type_create_write_view(slot_type->data.slot.inner_type);
}

static Type *
type_check_move_token(ASTNode *call, SemanticContext *ctx)
{
    if (!check_call_arity(call, 1, "Move", ctx))
        return TYPE_UNKNOWN;

    ASTNode *slot_arg = call->data.call.arguments[0];
    if (slot_arg == NULL || slot_arg->type != AST_IDENTIFIER) {
        semantic_error(ctx, call,
            "Move requires a named owning Slot<T>/SecureSlot<T> binding");
        return TYPE_UNKNOWN;
    }

    Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
    Type *slot_type = type_check_expression(slot_arg, ctx);
    if (!type_is_owned_slot_handle(slot_type)) {
        semantic_error(ctx, slot_arg,
            "Move requires owning Slot<T>/SecureSlot<T>, got '%s'",
            slot_type != NULL ? slot_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    if (sym == NULL || sym->kind != SYMBOL_SLOT) {
        semantic_error(ctx, slot_arg,
            "Move requires an owning slot binding");
        return TYPE_UNKNOWN;
    }
    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
        semantic_error(ctx, slot_arg,
            "Cannot move released slot '%s'",
            sym->name);
        return TYPE_UNKNOWN;
    }

    scope_release_slot(ctx->scope, sym->name);
    return type_create_slot_access(slot_type->data.slot.inner_type,
        slot_type->data.slot.is_secure, SLOT_ACCESS_MOVE_TOKEN);
}

bool
type_check_write_slot(ASTNode *call, SemanticContext *ctx)
{
    size_t arg_count = call->data.call.arg_count;

    if (arg_count < 2) {
        semantic_error(ctx, call,
            "Write requires at least 2 arguments: Write(slot, value)");
        return false;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];
    Type *slot_type = type_check_expression(slot_arg, ctx);

    if (type_is_constructed_named(slot_type, "RemoteFuture")) {
        semantic_error(ctx, slot_arg,
            "RemoteFuture does not support Write(); remote resources are "
            "read-only via 'await'. Use Channel to send data to remote World");
        return false;
    }
    if (slot_type->kind != TYPE_KIND_SLOT) {
        semantic_error(ctx, slot_arg,
            "First argument to Write must be a Slot, got '%s'",
            slot_type->name);
        return false;
    }
    if (type_is_read_view(slot_type)) {
        semantic_error(ctx, slot_arg,
            "Cannot write through ReadView<T>; create a WriteView(slot) or keep the owning Slot<T>");
        return false;
    }
    if (type_is_move_token(slot_type)) {
        semantic_error(ctx, slot_arg,
            "Cannot write through MoveToken<T>");
        return false;
    }

    if (slot_type->data.slot.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
                    "Cannot write to released slot '%s'",
                    sym->name);
                return false;
            }

            if (sym->slot_info.is_secure) {
                if (arg_count < 3) {
                    semantic_error(ctx, call,
                        "Write to SecureSlot '%s' requires a token argument",
                        sym->name);
                    return false;
                }

                ASTNode *token_arg = call->data.call.arguments[2];
                if (token_arg->type == AST_IDENTIFIER) {
                    const char *token_name = token_arg->data.identifier.name;
                    if (sym->slot_info.paired_token_name == NULL
                        || strcmp(sym->slot_info.paired_token_name,
                                  token_name) != 0) {
                        semantic_error(ctx, token_arg,
                            "Token '%s' is not paired with slot '%s'",
                            token_name, sym->name);
                        return false;
                    }
                }
            } else if (arg_count > 2) {
                semantic_warning(ctx, call,
                    "Write to plain Slot '%s' ignores extra token argument",
                    sym->name);
            }
        } else if (sym != NULL && type_is_write_view(sym->type)
                   && sym->slot_info.paired_slot_name != NULL) {
            Symbol *owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
            if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
                    "Cannot write through WriteView '%s' because source slot '%s' was released",
                    sym->name, owner->name);
                return false;
            }
            if (owner != NULL && owner->slot_info.is_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);
        }
    }

    ASTNode *value_arg = call->data.call.arguments[1];
    Type *value_type = type_check_expression(value_arg, ctx);
    Type *inner_type = slot_type->data.slot.inner_type;

    if (!type_is_assignable(value_type, inner_type)) {
        semantic_error(ctx, value_arg,
            "Cannot write '%s' to %s (expected '%s')",
            value_type->name, slot_type->name, inner_type->name);
        return false;
    }

    return true;
}

Type *
type_check_read_slot(ASTNode *call, SemanticContext *ctx)
{
    size_t arg_count = call->data.call.arg_count;

    if (arg_count < 1) {
        semantic_error(ctx, call,
            "Read requires at least 1 argument: Read(slot)");
        return TYPE_UNKNOWN;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];
    Type *slot_type = type_check_expression(slot_arg, ctx);

    if (type_is_constructed_named(slot_type, "RemoteFuture")) {
        semantic_error(ctx, slot_arg,
            "RemoteFuture does not support Read(); use 'await' to obtain "
            "Result<T>, then Unwrap() or '?' to extract the value");
        return TYPE_UNKNOWN;
    }
    if (slot_type->kind != TYPE_KIND_SLOT) {
        semantic_error(ctx, slot_arg,
            "First argument to Read must be a Slot, got '%s'",
            slot_type->name);
        return TYPE_UNKNOWN;
    }
    if (type_is_write_view(slot_type)) {
        semantic_error(ctx, slot_arg,
            "Cannot read through WriteView<T>; create a ReadView(slot) or keep the owning Slot<T>");
        return TYPE_UNKNOWN;
    }
    if (type_is_move_token(slot_type)) {
        semantic_error(ctx, slot_arg,
            "Cannot read through MoveToken<T>");
        return TYPE_UNKNOWN;
    }

    if (slot_type->data.slot.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
                    "Cannot read from released slot '%s'",
                    sym->name);
                return TYPE_UNKNOWN;
            }

            if (sym->slot_info.is_secure) {
                if (arg_count < 2) {
                    semantic_error(ctx, call,
                        "Read from SecureSlot '%s' requires a token argument",
                        sym->name);
                    return TYPE_UNKNOWN;
                }
                ASTNode *token_arg = call->data.call.arguments[1];
                if (token_arg->type == AST_IDENTIFIER) {
                    const char *token_name = token_arg->data.identifier.name;
                    if (sym->slot_info.paired_token_name == NULL
                        || strcmp(sym->slot_info.paired_token_name,
                                  token_name) != 0) {
                        semantic_error(ctx, token_arg,
                            "Token '%s' is not paired with slot '%s'",
                            token_name, sym->name);
                        return TYPE_UNKNOWN;
                    }
                }
            }
        } else if (sym != NULL && type_is_read_view(sym->type)
                   && sym->slot_info.paired_slot_name != NULL) {
            Symbol *owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
            if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
                    "Cannot read through ReadView '%s' because source slot '%s' was released",
                    sym->name, owner->name);
                return TYPE_UNKNOWN;
            }
            if (owner != NULL && owner->slot_info.is_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);
        }
    }

    return slot_type->data.slot.inner_type;
}

bool
type_check_release_slot(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count < 1) {
        semantic_error(ctx, call,
            "Release requires at least 1 argument: Release(slot)");
        return false;
    }

    ASTNode *slot_arg = call->data.call.arguments[0];

    /* RemoteFuture has no Release — it is consumed by await */
    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *rsym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (rsym != NULL && rsym->type != NULL
            && type_is_constructed_named(rsym->type, "RemoteFuture")) {
            semantic_error(ctx, slot_arg,
                "RemoteFuture does not support Release(); it is consumed by "
                "'await' and returns Result<T>");
            return false;
        }
    }

    if (slot_arg->type != AST_IDENTIFIER) {
        semantic_error(ctx, slot_arg,
            "Argument to Release must be a slot identifier");
        return false;
    }

    const char *slot_name = slot_arg->data.identifier.name;
    Symbol *sym = scope_lookup(ctx->scope, slot_name);

    if (sym == NULL || sym->kind != SYMBOL_SLOT) {
        if (sym != NULL && sym->type != NULL && sym->type->kind == TYPE_KIND_SLOT) {
            semantic_error(ctx, slot_arg,
                "Only owning Slot<T>/SecureSlot<T> values can be released; views are non-owning and move tokens are transfer-only");
        } else {
            semantic_error(ctx, slot_arg,
                "'%s' is not a slot", slot_name);
        }
        return false;
    }

    if (sym->slot_info.is_secure)
        semantic_record_effect(ctx, EFFECT_SECURE);

    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
        semantic_error(ctx, slot_arg,
            "Slot '%s' has already been released", slot_name);
        return false;
    }

    if (sym->slot_info.is_secure && call->data.call.arg_count < 2) {
        semantic_error(ctx, call,
            "Release of SecureSlot '%s' requires a token argument",
            slot_name);
        return false;
    }

    scope_release_slot(ctx->scope, slot_name);
    return true;
}

static Type *
type_check_device_handle_arg(ASTNode *expr, SemanticContext *ctx,
                             const char *builtin_name,
                             bool allow_released)
{
    Type *slot_type;
    Symbol *sym = NULL;

    if (expr == NULL)
        return TYPE_UNKNOWN;

    slot_type = type_check_expression(expr, ctx);
    if (!type_is_constructed_named(slot_type, "DeviceSlot")) {
        semantic_error(ctx, expr,
            "%s requires DeviceSlot<T>, got '%s'",
            builtin_name, slot_type->name);
        return TYPE_UNKNOWN;
    }

    if (expr->type == AST_IDENTIFIER) {
        sym = scope_lookup(ctx->scope, expr->data.identifier.name);
        if (!allow_released
            && sym != NULL
            && sym->slot_info.state == SLOT_STATE_RELEASED) {
            semantic_error(ctx, expr,
                "Cannot use released DeviceSlot '%s' in %s",
                expr->data.identifier.name, builtin_name);
            return TYPE_UNKNOWN;
        }
    }

    semantic_record_effect(ctx, EFFECT_REMOTE);
    return slot_type;
}

Type *
type_check_stdlib_call(ASTNode *expr, const char *name, SemanticContext *ctx)
{
    if (strcmp(name, "Abs") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return type_check_expression(expr->data.call.arguments[0], ctx);
    }
    if (strcmp(name, "Min") == 0 || strcmp(name, "Max") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *a = type_check_expression(expr->data.call.arguments[0], ctx);
        Type *b = type_check_expression(expr->data.call.arguments[1], ctx);
        require_assignable(b, a, expr->data.call.arguments[1], ctx);
        return a;
    }
    if (strcmp(name, "StringLength") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "Contains") == 0 || strcmp(name, "StringContains") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "Replace") == 0 || strcmp(name, "StringReplace") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < 3; i++) {
            require_assignable(type_check_expression(expr->data.call.arguments[i], ctx),
                TYPE_STRING, expr->data.call.arguments[i], ctx);
        }
        return TYPE_STRING;
    }
    if (strcmp(name, "Substring") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[2], ctx),
            TYPE_INT, expr->data.call.arguments[2], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Trim") == 0 || strcmp(name, "StringTrim") == 0
        || strcmp(name, "Upper") == 0 || strcmp(name, "ToUpper") == 0
        || strcmp(name, "Lower") == 0 || strcmp(name, "ToLower") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Concat") == 0 || strcmp(name, "StringConcat") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "StringSplit") == 0 || strcmp(name, "Split") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        Type *args[1] = { TYPE_STRING };
        return type_create_constructed(TYPE_ARRAY, args, 1);
    }
    if (strcmp(name, "StringJoin") == 0 || strcmp(name, "Join") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr_type, "Array")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "StringJoin requires Array<String> as first argument");
        }
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "ToInt") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "ToFloat") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_FLOAT;
    }
    /* Math builtins */
    if (strcmp(name, "Sqrt") == 0 || strcmp(name, "Floor") == 0
        || strcmp(name, "Ceil") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Pow") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Random") == 0) {
        if (expr->data.call.arg_count > 0)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "SeedRandom") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_INT, expr->data.call.arguments[0], ctx);
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_VOID;
    }
    /* HashMap builtins */
    if (strcmp(name, "MapNew") == 0) {
        return TYPE_UNKNOWN; /* type resolved from let annotation */
    }
    if (strcmp(name, "MapSet") == 0) {
        if (expr->data.call.arg_count >= 3) {
            Type *map_type = type_check_expression(expr->data.call.arguments[0], ctx);
            Type *key_type = type_check_expression(expr->data.call.arguments[1], ctx);
            Type *value_type = type_check_expression(expr->data.call.arguments[2], ctx);
            if (type_is_constructed_named(map_type, "HashMap")
                && map_type->data.constructed.arg_count == 2) {
                require_assignable(key_type,
                    map_type->data.constructed.args[0],
                    expr->data.call.arguments[1], ctx);
                require_assignable(value_type,
                    map_type->data.constructed.args[1],
                    expr->data.call.arguments[2], ctx);
            }
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "MapGet") == 0) {
        if (expr->data.call.arg_count >= 2) {
            Type *map_type = type_check_expression(expr->data.call.arguments[0], ctx);
            Type *key_type = type_check_expression(expr->data.call.arguments[1], ctx);
            if (type_is_constructed_named(map_type, "HashMap")
                && map_type->data.constructed.arg_count == 2) {
                require_assignable(key_type,
                    map_type->data.constructed.args[0],
                    expr->data.call.arguments[1], ctx);
                return map_type->data.constructed.args[1];
            }
        }
        return TYPE_UNKNOWN; /* resolved from context */
    }
    if (strcmp(name, "MapHas") == 0) {
        if (expr->data.call.arg_count >= 2) {
            Type *map_type = type_check_expression(expr->data.call.arguments[0], ctx);
            Type *key_type = type_check_expression(expr->data.call.arguments[1], ctx);
            if (type_is_constructed_named(map_type, "HashMap")
                && map_type->data.constructed.arg_count == 2) {
                require_assignable(key_type,
                    map_type->data.constructed.args[0],
                    expr->data.call.arguments[1], ctx);
            }
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "MapRemove") == 0) {
        if (expr->data.call.arg_count >= 2) {
            Type *map_type = type_check_expression(expr->data.call.arguments[0], ctx);
            Type *key_type = type_check_expression(expr->data.call.arguments[1], ctx);
            if (type_is_constructed_named(map_type, "HashMap")
                && map_type->data.constructed.arg_count == 2) {
                require_assignable(key_type,
                    map_type->data.constructed.args[0],
                    expr->data.call.arguments[1], ctx);
            }
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "MapSize") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    /* List builtins */
    if (strcmp(name, "ListNew") == 0) { return TYPE_UNKNOWN; }
    if (strcmp(name, "ListPush") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ListGet") == 0) {
        if (expr->data.call.arg_count >= 2) {
            Type *list_type = type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
            if (type_is_constructed_named(list_type, "List")
                && list_type->data.constructed.arg_count == 1) {
                return list_type->data.constructed.args[0];
            }
        }
        return TYPE_INT;
    }
    if (strcmp(name, "ListSet") == 0) {
        if (expr->data.call.arg_count >= 3) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
            type_check_expression(expr->data.call.arguments[2], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ListSize") == 0 || strcmp(name, "ListRemove") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    /* Set builtins */
    if (strcmp(name, "SetNew") == 0) { return TYPE_UNKNOWN; }
    if (strcmp(name, "SetAdd") == 0 || strcmp(name, "SetRemove") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "SetHas") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "SetSize") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    /* Queue builtins */
    if (strcmp(name, "QueueNew") == 0) { return TYPE_UNKNOWN; }
    if (strcmp(name, "QueuePush") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "QueuePop") == 0) {
        if (expr->data.call.arg_count >= 1) {
            Type *queue_type = type_check_expression(expr->data.call.arguments[0], ctx);
            if (type_is_constructed_named(queue_type, "Queue")
                && queue_type->data.constructed.arg_count == 1) {
                return queue_type->data.constructed.args[0];
            }
        }
        return TYPE_INT;
    }
    if (strcmp(name, "QueueSize") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "QueueEmpty") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    /* FSM builtins */
    if (strcmp(name, "FsmNew") == 0) { return TYPE_UNKNOWN; }
    if (strcmp(name, "FsmAddState") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "FsmTransition") == 0) {
        if (expr->data.call.arg_count >= 4) {
            for (size_t ai = 0; ai < 4; ai++)
                type_check_expression(expr->data.call.arguments[ai], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "FsmStep") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "FsmCurrent") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "FsmCurrentName") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    /* Timer builtins */
    if (strcmp(name, "TimerNew") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "TimerTick") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "TimerRemaining") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "TimerDone") == 0 || strcmp(name, "CooldownReady") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "TimerReset") == 0 || strcmp(name, "CooldownTrigger") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "CooldownNew") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "CooldownTick") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ArrayLength") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arg = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arg, "Array")
            && !type_is_constructed_named(arg, "Slice")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArrayLength requires Array<T> or Slice<T>, got '%s'",
                arg->name);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "ArrayPush") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        Type *val = type_check_expression(expr->data.call.arguments[1], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArrayPush requires Array<T>, got '%s'", arr->name);
        else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL)
                require_assignable(val, inner, expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ArraySet") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        require_assignable(
            type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
        Type *val = type_check_expression(expr->data.call.arguments[2], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArraySet requires Array<T>, got '%s'", arr->name);
        else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL)
                require_assignable(val, inner, expr->data.call.arguments[2], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ArrayPop") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArrayPop requires Array<T>, got '%s'", arr->name);
        return TYPE_VOID;
    }
    if (strcmp(name, "ArraySort") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArraySort requires Array<T>, got '%s'", arr->name);
        return arr;
    }
    if (strcmp(name, "ArrayMap") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArrayMap requires Array<T> as first argument, got '%s'", arr->name);
        /* Second arg is a function — type-check it but allow any callable */
        type_check_expression(expr->data.call.arguments[1], ctx);
        /* Return type: same Array<T> (element type preserved for now) */
        return arr;
    }
    if (strcmp(name, "ArrayFilter") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArrayFilter requires Array<T> as first argument, got '%s'", arr->name);
        /* Second arg is a predicate function */
        type_check_expression(expr->data.call.arguments[1], ctx);
        return arr;
    }
    if (strcmp(name, "ArrayReverse") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error(ctx, expr->data.call.arguments[0],
                "ArrayReverse requires Array<T>, got '%s'", arr->name);
        return arr;
    }
    if (strcmp(name, "ToString") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Print") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "ReadLine") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_STRING;
    }
    if (strcmp(name, "Now") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_INT;
    }
    if (strcmp(name, "Sleep") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_INT, expr->data.call.arguments[0], ctx);
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_VOID;
    }

    if (strcmp(name, "ClaimQubit") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        /* Qubit starts in SUPERPOSITION state (uncollapsed). */
        return TYPE_QUBIT;
    }
    if (strcmp(name, "ClaimDeviceSlot") == 0) {
        return type_check_claim_device_slot(expr, ctx);
    }
    if (strcmp(name, "DeviceRead") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return type_get_constructed_arg(
            type_check_device_handle_arg(expr->data.call.arguments[0], ctx, name, false), 0);
    }
    if (strcmp(name, "DeviceWrite") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *slot_type = type_check_device_handle_arg(
            expr->data.call.arguments[0], ctx, name, false);
        Type *inner = type_get_constructed_arg(slot_type, 0);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            inner, expr->data.call.arguments[1], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "ReleaseDeviceSlot") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ASTNode *slot_arg = expr->data.call.arguments[0];
        Type *slot_type = type_check_device_handle_arg(slot_arg, ctx, name, true);
        if (slot_type == TYPE_UNKNOWN)
            return TYPE_UNKNOWN;
        if (slot_arg->type != AST_IDENTIFIER) {
            semantic_error(ctx, slot_arg,
                "ReleaseDeviceSlot requires a DeviceSlot identifier");
            return TYPE_UNKNOWN;
        }
        {
            Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
            if (sym != NULL && sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, slot_arg,
                    "DeviceSlot '%s' has already been released",
                    slot_arg->data.identifier.name);
                return TYPE_UNKNOWN;
            }
        }
        scope_release_slot(ctx->scope, slot_arg->data.identifier.name);
        return TYPE_VOID;
    }
    if (strcmp(name, "SubmitDeviceRead") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *slot_type = type_check_device_handle_arg(
            expr->data.call.arguments[0], ctx, name, false);
        return wrap_constructed(TYPE_REMOTE_FUTURE,
            type_get_constructed_arg(slot_type, 0));
    }

    /* ---- Clone: explicit copy of Slot ---- */
    if (strcmp(name, "Clone") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arg_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (arg_type == NULL)
            return TYPE_UNKNOWN;
        /* Clone returns the same type — a fresh independent copy */
        return arg_type;
    }

    /* ---- Result builtins ---- */
    if (strcmp(name, "IsOk") == 0 || strcmp(name, "IsErr") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "Some") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return wrap_constructed(TYPE_OPTION,
            type_check_expression(expr->data.call.arguments[0], ctx));
    }
    if (strcmp(name, "None") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return wrap_constructed(TYPE_OPTION, TYPE_UNKNOWN);
    }
    if (strcmp(name, "IsSome") == 0 || strcmp(name, "IsNone") == 0) {
        Type *ot;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ot = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ot, "Option")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "%s requires Option<T>, got '%s'", name,
                ot != NULL ? ot->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "UnwrapOption") == 0) {
        Type *ot;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ot = type_check_expression(expr->data.call.arguments[0], ctx);
        if (type_is_constructed_named(ot, "Option"))
            return type_get_constructed_arg(ot, 0);
        semantic_error(ctx, expr->data.call.arguments[0],
            "UnwrapOption requires Option<T>, got '%s'",
            ot != NULL ? ot->name : "<null>");
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "Unwrap") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *rt = type_check_expression(expr->data.call.arguments[0], ctx);
        if (type_is_constructed_named(rt, "Result"))
            return type_get_constructed_arg(rt, 0);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "UnwrapOr") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *rt = type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(rt, "Result"))
            return type_get_constructed_arg(rt, 0);
        return TYPE_UNKNOWN;
    }

    /* ---- Channel builtins ---- */
    if (strcmp(name, "TryRecv") == 0) {
        return type_check_channel_recv_builtin(expr, name, false, ctx);
    }
    if (strcmp(name, "RecvTimeout") == 0) {
        return type_check_channel_recv_builtin(expr, name, true, ctx);
    }
    if (strcmp(name, "TrySend") == 0) {
        return type_check_channel_send_builtin(expr, name, false, false, ctx);
    }
    if (strcmp(name, "SendTimeout") == 0) {
        return type_check_channel_send_builtin(expr, name, true, false, ctx);
    }
    if (strcmp(name, "TrySendStatus") == 0) {
        return type_check_channel_send_builtin(expr, name, false, true, ctx);
    }
    if (strcmp(name, "SendTimeoutStatus") == 0) {
        return type_check_channel_send_builtin(expr, name, true, true, ctx);
    }
    if (strcmp(name, "ChannelLength") == 0
        || strcmp(name, "ChannelCapacity") == 0
        || strcmp(name, "ChannelSpace") == 0
        || strcmp(name, "ChannelFull") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "%s requires Channel<T>, got '%s'",
                name,
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return strcmp(name, "ChannelFull") == 0 ? TYPE_BOOL : TYPE_INT;
    }
    if (strcmp(name, "ChannelClosed") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ChannelClosed requires Channel<T>, got '%s'",
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "ChannelClose") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ChannelClose requires Channel<T>, got '%s'",
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ChannelReady") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ChannelReady requires Channel<T>, got '%s'",
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "Cancel") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *task_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_future_like(task_type)) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "Cancel requires Future<T> or RemoteFuture<T>, got '%s'",
                task_type != NULL ? task_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "IsCancelled") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        return TYPE_BOOL;
    }

    if (strcmp(name, "Measure") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC | EFFECT_COLLAPSE);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: CLASSICAL qubits cannot be measured */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs == QUBIT_STATE_CLASSICAL)
                semantic_error(ctx, expr,
                    "Cannot Measure() a qubit in CLASSICAL state "
                    "(already converted via IntoClassical)");
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_COLLAPSED);
        /* Propagate collapse to all qubits in the same entanglement pool */
        {
            int32_t pool = get_qubit_entangle_pool(
                expr->data.call.arguments[0], ctx);
            if (pool >= 0)
                propagate_collapse_to_pool(ctx, pool);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "Entangle") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[1], ctx),
            TYPE_QUBIT, expr->data.call.arguments[1], ctx);
        /* State validation: only SUPERPOSITION/NONE qubits can be entangled */
        {
            QubitSemanticState sa = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            QubitSemanticState sb = get_qubit_semantic_state(
                expr->data.call.arguments[1], ctx);
            if (sa == QUBIT_STATE_COLLAPSED || sa == QUBIT_STATE_CLASSICAL)
                semantic_error(ctx, expr,
                    "Cannot Entangle() a qubit in %s state",
                    qubit_state_name(sa));
            if (sb == QUBIT_STATE_COLLAPSED || sb == QUBIT_STATE_CLASSICAL)
                semantic_error(ctx, expr,
                    "Cannot Entangle() a qubit in %s state",
                    qubit_state_name(sb));
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_ENTANGLED);
        set_qubit_semantic_state(expr->data.call.arguments[1], ctx,
                                 QUBIT_STATE_ENTANGLED);
        /* Compile-time entanglement pool: allocate / merge */
        {
            int32_t pa = get_qubit_entangle_pool(
                expr->data.call.arguments[0], ctx);
            int32_t pb = get_qubit_entangle_pool(
                expr->data.call.arguments[1], ctx);
            if (pa >= 0 && pb >= 0) {
                if (pa != pb)
                    merge_entangle_pools(ctx, pa, pb);
            } else if (pa >= 0) {
                set_qubit_entangle_pool(expr->data.call.arguments[1], ctx, pa);
            } else if (pb >= 0) {
                set_qubit_entangle_pool(expr->data.call.arguments[0], ctx, pb);
            } else {
                int32_t new_pool = alloc_entangle_pool(ctx);
                set_qubit_entangle_pool(expr->data.call.arguments[0], ctx,
                                        new_pool);
                set_qubit_entangle_pool(expr->data.call.arguments[1], ctx,
                                        new_pool);
            }
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "QubitState") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "IsCollapsed") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "ReleaseQubit") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        consume_qubit_value(expr->data.call.arguments[0], ctx, "released");
        return TYPE_VOID;
    }
    if (strcmp(name, "H") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: CLASSICAL qubits cannot receive gate operations */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs == QUBIT_STATE_CLASSICAL)
                semantic_error(ctx, expr,
                    "Cannot apply H() to a qubit in CLASSICAL state "
                    "(already converted via IntoClassical)");
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_SUPERPOSITION);
        return TYPE_VOID;
    }
    if (strcmp(name, "IntoClassical") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: only COLLAPSED qubits can be converted.
         * Unmeasured/unknown states (NONE) must be rejected. */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs != QUBIT_STATE_COLLAPSED)
                semantic_error(ctx, expr,
                    "IntoClassical() requires a COLLAPSED qubit (after Measure) "
                    "got %s", qubit_state_name(qs));
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_CLASSICAL);
        consume_qubit_value(expr->data.call.arguments[0], ctx,
                            "converted to classical");
        return TYPE_BOOL;
    }

    return NULL;
}

static Type *
type_check_rc_new(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcNew requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_RC,
        type_check_expression(call->data.call.arguments[0], ctx));
}

static Type *
type_check_rc_clone(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcClone requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error(ctx, call, "RcClone requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    return rc_type;
}

static Type *
type_check_rc_get(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcGet requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error(ctx, call, "RcGet requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    return type_get_constructed_arg(rc_type, 0);
}

static Type *
type_check_rc_downgrade(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "RcDowngrade requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *rc_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(rc_type, "Rc")) {
        semantic_error(ctx, call, "RcDowngrade requires Rc<T>, got '%s'", rc_type->name);
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_WEAK, type_get_constructed_arg(rc_type, 0));
}

static Type *
type_check_weak_upgrade(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "WeakUpgrade requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *weak_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(weak_type, "Weak")) {
        semantic_error(ctx, call, "WeakUpgrade requires Weak<T>, got '%s'",
            weak_type->name);
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_RC, type_get_constructed_arg(weak_type, 0));
}

static Type *
type_check_weak_drop(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "WeakDrop requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    Type *weak_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(weak_type, "Weak")) {
        semantic_error(ctx, call, "WeakDrop requires Weak<T>, got '%s'", weak_type->name);
        return TYPE_UNKNOWN;
    }
    return TYPE_VOID;
}

static Type *
type_check_allocator_builtin(ASTNode *call, SemanticContext *ctx,
                             bool requires_capacity)
{
    if ((!requires_capacity && call->data.call.arg_count != 0)
        || (requires_capacity && call->data.call.arg_count != 1)) {
        semantic_error(ctx, call,
            requires_capacity
                ? "AllocatorPool requires exactly 1 capacity argument"
                : "Allocator constructor takes no arguments");
        return TYPE_UNKNOWN;
    }

    if (requires_capacity) {
        Type *cap_type = type_check_expression(call->data.call.arguments[0], ctx);
        if (!type_equals(cap_type, TYPE_INT) && !type_equals(cap_type, TYPE_LONG)) {
            semantic_error(ctx, call->data.call.arguments[0],
                "AllocatorPool capacity must be Int or Long, got '%s'",
                cap_type->name);
            return TYPE_UNKNOWN;
        }
    }

    return TYPE_ALLOCATOR;
}

static Type *
type_check_box_builtin(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "Box requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_BOX,
        type_check_expression(call->data.call.arguments[0], ctx));
}

static Type *
type_check_box_get(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "BoxGet requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }

    Type *box_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error(ctx, call, "BoxGet requires Box<T>, got '%s'",
            box_type != NULL ? box_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return type_get_constructed_arg(box_type, 0);
}

static Type *
type_check_box_set(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 2) {
        semantic_error(ctx, call, "BoxSet requires exactly 2 arguments");
        return TYPE_UNKNOWN;
    }

    Type *box_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error(ctx, call, "BoxSet requires Box<T>, got '%s'",
            box_type != NULL ? box_type->name : "<null>");
        return TYPE_UNKNOWN;
    }

    Type *inner = type_get_constructed_arg(box_type, 0);
    Type *value_type = type_check_expression(call->data.call.arguments[1], ctx);
    require_assignable(value_type, inner, call->data.call.arguments[1], ctx);
    return TYPE_VOID;
}

static Type *
type_check_box_drop(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "BoxDrop requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }

    Type *box_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error(ctx, call, "BoxDrop requires Box<T>, got '%s'",
            box_type != NULL ? box_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return TYPE_VOID;
}

static Type *
type_check_box_is_valid(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count != 1) {
        semantic_error(ctx, call, "BoxIsValid requires exactly 1 argument");
        return TYPE_UNKNOWN;
    }

    Type *box_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_is_constructed_named(box_type, "Box")) {
        semantic_error(ctx, call, "BoxIsValid requires Box<T>, got '%s'",
            box_type != NULL ? box_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return TYPE_BOOL;
}

static Type *
type_check_box_array_builtin(ASTNode *call, SemanticContext *ctx)
{
    if (call->data.call.arg_count < 1 || call->data.call.arg_count > 2) {
        semantic_error(ctx, call,
            "BoxArray requires capacity and optional allocator");
        return TYPE_UNKNOWN;
    }

    Type *cap_type = type_check_expression(call->data.call.arguments[0], ctx);
    if (!type_equals(cap_type, TYPE_INT) && !type_equals(cap_type, TYPE_LONG)) {
        semantic_error(ctx, call->data.call.arguments[0],
            "BoxArray capacity must be Int or Long, got '%s'", cap_type->name);
        return TYPE_UNKNOWN;
    }

    if (call->data.call.arg_count == 2) {
        Type *alloc_type = type_check_expression(call->data.call.arguments[1], ctx);
        if (!type_equals(alloc_type, TYPE_ALLOCATOR)) {
            semantic_error(ctx, call->data.call.arguments[1],
                "BoxArray allocator must be Allocator, got '%s'", alloc_type->name);
            return TYPE_UNKNOWN;
        }
    }

    return TYPE_UNKNOWN;
}

static Type *
type_check_projection_call(ASTNode *call,
                           SemanticContext *ctx,
                           const char *builtin_name,
                           NominalDeclKind expected_kind,
                           const char *expected_label)
{
    ASTNode *target_arg;
    ASTNode *source_arg;
    ASTNode *target_decl;
    ASTNode *source_decl;
    Symbol *target_sym;
    Type *source_type;
    bool in_projection_context;

    if (!check_call_arity(call, 2, builtin_name, ctx))
        return TYPE_UNKNOWN;

    in_projection_context =
        ctx != NULL
        && (ctx->current_relation != NULL
            || ctx->current_effect != NULL
            || ctx->current_zone != NULL
            || ctx->current_world != NULL);

    target_arg = call->data.call.arguments[0];
    source_arg = call->data.call.arguments[1];

    if (target_arg == NULL || target_arg->type != AST_IDENTIFIER
        || target_arg->data.identifier.name == NULL) {
        semantic_error(ctx, call,
            "%s requires the first argument to be a %s type name",
            builtin_name, expected_label);
        return TYPE_UNKNOWN;
    }

    if (source_arg == NULL || source_arg->type != AST_IDENTIFIER
        || source_arg->data.identifier.name == NULL) {
        semantic_error(ctx, call,
            "%s currently requires a named subject binding as the source",
            builtin_name);
        return TYPE_UNKNOWN;
    }

    target_decl = find_named_class_decl(ctx->program_root,
        target_arg->data.identifier.name);
    if (target_decl == NULL
        || !target_decl->data.class_decl.is_struct
        || target_decl->data.class_decl.nominal_kind != expected_kind) {
        semantic_error(ctx, target_arg,
            "%s target '%s' must be a %s declaration",
            builtin_name,
            target_arg->data.identifier.name,
            expected_label);
        return TYPE_UNKNOWN;
    }

    target_sym = scope_lookup(ctx->scope, target_arg->data.identifier.name);
    if (target_sym == NULL || target_sym->type == NULL) {
        semantic_error(ctx, target_arg,
            "Unknown %s type '%s'",
            expected_label,
            target_arg->data.identifier.name);
        return TYPE_UNKNOWN;
    }

    source_type = type_check_expression(source_arg, ctx);
    if (source_type == NULL || source_type == TYPE_UNKNOWN)
        return TYPE_UNKNOWN;

    if (source_type->kind != TYPE_KIND_CLASS || source_type->name == NULL) {
        semantic_error(ctx, source_arg,
            "%s source must be a subject binding, got '%s'",
            builtin_name,
            source_type->name != NULL ? source_type->name : "<unknown>");
        return TYPE_UNKNOWN;
    }

        source_decl = find_named_class_decl(ctx->program_root, source_type->name);
    if (!decl_is_subject_nominal(source_decl)) {
        semantic_error(ctx, source_arg,
            "%s source '%s' must be a subject declaration",
            builtin_name,
            source_type->name != NULL ? source_type->name : "<unknown>");
        return TYPE_UNKNOWN;
    }

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        Type *target_field_type;
        Type *source_field_type;
        int source_status;

        if (target_field == NULL || target_field->name == NULL
            || target_field->type == NULL) {
            continue;
        }

        source_status = resolve_projection_source_field_type_rec(
            ctx->program_root, source_decl, target_field->name, 0, ctx, &source_field_type);
        if (source_status == 0 || source_field_type == NULL) {
            semantic_error(ctx, call,
                "%s target field '%s' is missing from source subject '%s'",
                builtin_name,
                target_field->name,
                source_type->name != NULL ? source_type->name : "<unknown>");
            continue;
        }
        if (source_status == 2) {
            semantic_error(ctx, call,
                "%s target field '%s' is ambiguous in source subject '%s'; rename the field or expose it directly on the host",
                builtin_name,
                target_field->name,
                source_type->name != NULL ? source_type->name : "<unknown>");
            continue;
        }

        target_field_type = resolve_type_node(target_field->type, ctx);
        require_assignable(source_field_type, target_field_type, call, ctx);
    }

    if (!in_projection_context) {
        if (expected_kind == NOMINAL_DECL_DTO) {
            semantic_warning(ctx, call,
                "%s is being used as a direct boundary projection outside relation/effect/zone/world context; tobject is a transfer contract, so prefer tobject slots plus publish/transport flow",
                builtin_name);
        } else {
            semantic_warning(ctx, call,
                "%s is being used as a direct internal projection outside relation/effect/zone/world context; object is a local projection contract, so prefer object slots plus refresh flow",
                builtin_name);
        }
    }

    return target_sym->type;
}

static Type *
type_check_to_tobject(ASTNode *call, SemanticContext *ctx)
{
    return type_check_projection_call(call, ctx, "ToTObject",
        NOMINAL_DECL_DTO, "tobject");
}

static Type *
type_check_to_object(ASTNode *call, SemanticContext *ctx)
{
    return type_check_projection_call(call, ctx, "ToObject",
        NOMINAL_DECL_OBJECT, "object");
}

Type *
type_check_builtin_call(ASTNode *call, BuiltinKind kind, SemanticContext *ctx)
{
    switch (kind) {
    case BUILTIN_CLAIM_SLOT:
        return type_check_claim_slot(call, ctx);
    case BUILTIN_CLAIM_SECURE_SLOT:
        semantic_record_effect(ctx, EFFECT_SECURE);
        return type_check_claim_slot(call, ctx);
    case BUILTIN_CLAIM_DEVICE_SLOT:
        return type_check_claim_device_slot(call, ctx);
    case BUILTIN_VIEW_READ:
        return type_check_view_read(call, ctx);
    case BUILTIN_VIEW_WRITE:
        return type_check_view_write(call, ctx);
    case BUILTIN_MOVE:
        return type_check_move_token(call, ctx);
    case BUILTIN_WRITE:
        type_check_write_slot(call, ctx);
        return TYPE_VOID;
    case BUILTIN_READ:
        return type_check_read_slot(call, ctx);
    case BUILTIN_RELEASE:
        type_check_release_slot(call, ctx);
        return TYPE_VOID;
    case BUILTIN_DEVICE_WRITE:
    case BUILTIN_DEVICE_READ:
    case BUILTIN_RELEASE_DEVICE_SLOT:
    case BUILTIN_SUBMIT_DEVICE_READ:
        return type_check_stdlib_call(call, call->data.call.callee->data.identifier.name, ctx);
    case BUILTIN_LOG:
        for (size_t i = 0; i < call->data.call.arg_count; i++)
            type_check_expression(call->data.call.arguments[i], ctx);
        return TYPE_VOID;
    case BUILTIN_LOG_RAW:
        if (!check_call_arity(call, 1, "LogRaw", ctx))
            return TYPE_VOID;
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                          TYPE_STRING, call->data.call.arguments[0], ctx);
        return TYPE_VOID;
    case BUILTIN_LOG_BANNER:
        if (!check_call_arity(call, 1, "LogBanner", ctx))
            return TYPE_VOID;
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                          TYPE_STRING, call->data.call.arguments[0], ctx);
        return TYPE_VOID;
    case BUILTIN_LOG_BLOCK:
        if (!check_call_arity(call, 1, "LogBlock", ctx))
            return TYPE_VOID;
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                          TYPE_STRING, call->data.call.arguments[0], ctx);
        return TYPE_VOID;
    case BUILTIN_RC_NEW:
        return type_check_rc_new(call, ctx);
    case BUILTIN_RC_CLONE:
        return type_check_rc_clone(call, ctx);
    case BUILTIN_RC_DROP:
        (void)type_check_rc_clone(call, ctx);
        return TYPE_VOID;
    case BUILTIN_RC_DOWNGRADE:
        return type_check_rc_downgrade(call, ctx);
    case BUILTIN_RC_GET:
        return type_check_rc_get(call, ctx);
    case BUILTIN_WEAK_UPGRADE:
        return type_check_weak_upgrade(call, ctx);
    case BUILTIN_WEAK_DROP:
        return type_check_weak_drop(call, ctx);
    case BUILTIN_ALLOCATOR_SYSTEM:
    case BUILTIN_ALLOCATOR_TRACING:
    case BUILTIN_ALLOCATOR_DEBUG:
        return type_check_allocator_builtin(call, ctx, false);
    case BUILTIN_ALLOCATOR_POOL:
        return type_check_allocator_builtin(call, ctx, true);
    case BUILTIN_BOX:
        return type_check_box_builtin(call, ctx);
    case BUILTIN_BOX_GET:
        return type_check_box_get(call, ctx);
    case BUILTIN_BOX_SET:
        return type_check_box_set(call, ctx);
    case BUILTIN_BOX_DROP:
        return type_check_box_drop(call, ctx);
    case BUILTIN_BOX_IS_VALID:
        return type_check_box_is_valid(call, ctx);
    case BUILTIN_BOX_ARRAY:
        return type_check_box_array_builtin(call, ctx);
    case BUILTIN_TO_OBJECT:
        return type_check_to_object(call, ctx);
        case BUILTIN_TO_DTO:
            return type_check_to_tobject(call, ctx);
    case BUILTIN_HAS_PROJECTION:
        return type_check_has_projection(call, ctx);
    case BUILTIN_HAS_LAYER:
        return type_check_has_layer(call, ctx);
    case BUILTIN_HAS_STATE:
        return type_check_has_state(call, ctx);
    case BUILTIN_HAS_ZONE:
        return type_check_has_zone(call, ctx);
    case BUILTIN_HAS_ZONE_PROJECTION:
        return type_check_has_world_zone_detail(call, ctx, "HasZoneProjection",
            find_zone_projection_slot_local, "projection slot");
    case BUILTIN_HAS_ZONE_LAYER:
        return type_check_has_world_zone_detail(call, ctx, "HasZoneLayer",
            find_zone_layer_slot_local, "layer slot");
    case BUILTIN_HAS_ZONE_STATE:
        return type_check_has_world_zone_detail(call, ctx, "HasZoneState",
            find_zone_state_decl_local_builtin, "state");
    case BUILTIN_PARALLEL:
        return TYPE_VOID;
    case BUILTIN_FILE_OPEN:
        if (check_call_arity(call, 2, "FileOpen", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
            require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
                TYPE_STRING, call->data.call.arguments[1], ctx);
        }
        return TYPE_INT;
    case BUILTIN_FILE_READ:
        if (check_call_arity(call, 1, "FileRead", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_INT, call->data.call.arguments[0], ctx);
        }
        return TYPE_STRING;
    case BUILTIN_FILE_WRITE:
        if (check_call_arity(call, 2, "FileWrite", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_INT, call->data.call.arguments[0], ctx);
            require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
                TYPE_STRING, call->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    case BUILTIN_FILE_CLOSE:
        if (check_call_arity(call, 1, "FileClose", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_INT, call->data.call.arguments[0], ctx);
        }
        return TYPE_VOID;
    case BUILTIN_READ_FILE:
        if (check_call_arity(call, 1, "ReadFile", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
        }
        return TYPE_STRING;
    case BUILTIN_WRITE_FILE:
        if (check_call_arity(call, 2, "WriteFile", ctx)) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
            require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
                TYPE_STRING, call->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    case BUILTIN_INPUT:
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        if (call->data.call.arg_count > 1) {
            semantic_error(ctx, call,
                "'Input' expects at most 1 argument, got %zu",
                call->data.call.arg_count);
            return TYPE_STRING;
        }
        if (call->data.call.arg_count == 1) {
            require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
                TYPE_STRING, call->data.call.arguments[0], ctx);
        }
        return TYPE_STRING;
    case BUILTIN_PRINT:
        return type_check_stdlib_call(call, "Print", ctx);
    case BUILTIN_READ_LINE:
        return type_check_stdlib_call(call, "ReadLine", ctx);
    case BUILTIN_NOW:
        return type_check_stdlib_call(call, "Now", ctx);
    case BUILTIN_SLEEP:
        return type_check_stdlib_call(call, "Sleep", ctx);
    case BUILTIN_INTENT_LAST_TRACE:
        check_call_arity(call, 0, "IntentLastTrace", ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_LAST_FAILURE:
        check_call_arity(call, 0, "IntentLastFailure", ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_LAST_NAME:
        check_call_arity(call, 0, "IntentLastName", ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_LAST_HANDLE:
        check_call_arity(call, 0, "IntentLastHandle", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_LAST_TRACE_ID:
        check_call_arity(call, 0, "IntentLastTraceId", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_LAST_STEP_COUNT:
        check_call_arity(call, 0, "IntentLastStepCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_LAST_FAILED:
        check_call_arity(call, 0, "IntentLastFailed", ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_HISTORY_COUNT:
        check_call_arity(call, 0, "IntentHistoryCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_HISTORY_STEP_NAME:
        check_call_arity(call, 1, "IntentHistoryStepName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_ZONE:
        check_call_arity(call, 1, "IntentHistoryStepZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_PHASE:
        check_call_arity(call, 1, "IntentHistoryStepPhase", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_PARTICIPANT:
        check_call_arity(call, 1, "IntentHistoryStepParticipant", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_SLOT:
        check_call_arity(call, 1, "IntentHistoryStepSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE:
        check_call_arity(call, 1, "IntentHistoryStepFromZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT:
        check_call_arity(call, 1, "IntentHistoryStepFromSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_TO_ZONE:
        check_call_arity(call, 1, "IntentHistoryStepToZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_TO_SLOT:
        check_call_arity(call, 1, "IntentHistoryStepToSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_OK:
        check_call_arity(call, 1, "IntentHistoryStepOk", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_HISTORY_STEP_FAILURE:
        check_call_arity(call, 1, "IntentHistoryStepFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_COUNT:
        check_call_arity(call, 0, "IntentActiveCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_NAME:
        check_call_arity(call, 1, "IntentActiveName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_HANDLE:
        check_call_arity(call, 1, "IntentActiveHandle", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_TRACE_ID:
        check_call_arity(call, 1, "IntentActiveTraceId", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_PRIORITY:
        check_call_arity(call, 1, "IntentActivePriority", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_CONCURRENT:
        check_call_arity(call, 1, "IntentActiveConcurrent", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_ACTIVE_TRACE:
        check_call_arity(call, 1, "IntentActiveTrace", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    default:
        return TYPE_UNKNOWN;
    }
}
