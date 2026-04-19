/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker built-in dispatch and stdlib helpers
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../common/string_compat.h"
#include "type_checker_internal.h"

static const char *
builtin_type_name_or_unknown(const Type *type)
{
    return (type != NULL && type->name != NULL) ? type->name : "<unknown>";
}

static bool
builtin_is_subject_boundary_type(const Type *type, SemanticContext *ctx)
{
    if (type == NULL || ctx == NULL)
        return false;
    if (type->kind != TYPE_KIND_CLASS)
        return false;
    return !type_requires_boundary_borrow_tracking(type, ctx);
}

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

static bool
validate_secure_token_arg(ASTNode *token_arg, Symbol *slot_sym, Type *slot_type,
                          SemanticContext *ctx)
{
    Symbol *token_sym;
    Type *expected_token_type;
    Type *token_args[1];
    const char *token_name;

    if (token_arg == NULL || slot_sym == NULL || slot_type == NULL
        || slot_type->kind != TYPE_KIND_SLOT || !slot_type->data.slot.is_secure) {
        return false;
    }

    if (token_arg->type != AST_IDENTIFIER || token_arg->data.identifier.name == NULL) {
        semantic_error(ctx, token_arg,
            "SecureSlot '%s' requires a named paired token identifier",
            slot_sym->name != NULL ? slot_sym->name : "<slot>");
        return false;
    }

    token_name = token_arg->data.identifier.name;
    token_sym = scope_lookup(ctx->scope, token_name);
    if (token_sym == NULL || token_sym->kind != SYMBOL_TOKEN) {
        semantic_error(ctx, token_arg,
            "'%s' is not a capability token for slot '%s'",
            token_name, slot_sym->name != NULL ? slot_sym->name : "<slot>");
        return false;
    }

    if (slot_sym->slot_info.paired_token_name == NULL
        || strcmp(slot_sym->slot_info.paired_token_name, token_name) != 0) {
        semantic_error(ctx, token_arg,
            "Token '%s' is not paired with slot '%s'",
            token_name, slot_sym->name);
        return false;
    }

    token_args[0] = slot_type->data.slot.inner_type;
    expected_token_type = type_create_constructed(TYPE_TOKEN, token_args, 1);
    if (token_sym->type != NULL && expected_token_type != NULL
        && !type_equals(token_sym->type, expected_token_type)) {
        semantic_error(ctx, token_arg,
            "Token '%s' does not match SecureSlot '%s' capability type",
            token_name, slot_sym->name);
        return false;
    }

    return true;
}

static const char *
builtin_borrowed_boundary_root_name(ASTNode *value_expr,
                                    SemanticContext *ctx)
{
    ASTNode *cursor = value_expr;

    while (cursor != NULL) {
        if (cursor->type == AST_IDENTIFIER
            && cursor->data.identifier.name != NULL
            && identifier_is_borrowed_boundary_param(cursor, ctx)) {
            return cursor->data.identifier.name;
        }
        if (cursor->type == AST_MEMBER_ACCESS) {
            cursor = cursor->data.member.object;
            continue;
        }
        if (cursor->type == AST_ARRAY_ACCESS) {
            cursor = cursor->data.array_access.array;
            continue;
        }
        break;
    }

    return NULL;
}

static char *
builtin_expr_source_path(ASTNode *value_expr)
{
    char path_buf[512];

    if (value_expr == NULL) {
        return NULL;
    }

    if (value_expr->type == AST_IDENTIFIER
        && value_expr->data.identifier.name != NULL) {
        return pergyra_strdup(value_expr->data.identifier.name);
    }

    if (value_expr->type == AST_MEMBER_ACCESS
        && value_expr->data.member.name != NULL) {
        ASTNode *obj = value_expr->data.member.object;
        if (obj != NULL
            && obj->type == AST_IDENTIFIER
            && obj->data.identifier.name != NULL) {
            snprintf(path_buf, sizeof(path_buf), "%s.%s",
                     obj->data.identifier.name,
                     value_expr->data.member.name);
            return pergyra_strdup(path_buf);
        }
    }

    if (value_expr->type == AST_ARRAY_ACCESS) {
        ASTNode *arr = value_expr->data.array_access.array;
        ASTNode *idx = value_expr->data.array_access.index;
        if (arr != NULL
            && arr->type == AST_IDENTIFIER
            && arr->data.identifier.name != NULL) {
            if (idx != NULL && idx->type == AST_NUMBER) {
                snprintf(path_buf, sizeof(path_buf), "%s[%g]",
                         arr->data.identifier.name,
                         idx->data.number.value);
                return pergyra_strdup(path_buf);
            }
            if (idx != NULL
                && idx->type == AST_IDENTIFIER
                && idx->data.identifier.name != NULL) {
                snprintf(path_buf, sizeof(path_buf), "%s[%s]",
                         arr->data.identifier.name,
                         idx->data.identifier.name);
                return pergyra_strdup(path_buf);
            }
        }
    }

    return NULL;
}

static void
reject_borrowed_boundary_container_store(ASTNode *value_expr,
                                         const char *container_kind,
                                         const char *container_name,
                                         SemanticContext *ctx)
{
    const char *value_name;
    Symbol *value_sym;
    const Type *value_type;
    const char *value_label;
    const char *snapshot_label;
    const char *transfer_label;
    char *source_path = NULL;
    const char *borrowed_root_name =
        builtin_borrowed_boundary_root_name(value_expr, ctx);

    if (value_expr == NULL || ctx == NULL
        || borrowed_root_name == NULL) {
        return;
    }

    source_path = builtin_expr_source_path(value_expr);

    value_name = borrowed_root_name;
    value_sym = scope_lookup(ctx->scope, value_name);
    value_type = value_sym != NULL ? value_sym->type : NULL;

    if (value_type != NULL
        && !type_is_movable_resource_handle(value_type)
        && !builtin_is_subject_boundary_type(value_type, ctx)
        && !type_requires_boundary_borrow_tracking(value_type, ctx)) {
        return;
    }

    value_label = "boundary value";
    snapshot_label = "copied/projection/value snapshot";
    transfer_label = "transfer into";

    if (type_is_movable_resource_handle(value_type)) {
        value_label = "movable resource";
        snapshot_label = "copied/value/projection form";
        transfer_label = "transfer";
    } else if (builtin_is_subject_boundary_type(value_type, ctx)) {
        value_label = "subject";
        snapshot_label = "projection/object/tobject/value snapshot";
        transfer_label = "transfer";
    }

    semantic_error(ctx, value_expr,
        "Borrowed ref %s '%s' cannot escape through %s store%s%s%s.\n"
        "Reason:\n"
        "- consumer path is function '%s'\n"
        "- '%s' entered this function as a borrowed 'ref' %s\n"
        "- '%s' is derived from that borrowed provenance\n"
        "- %s inserts values into %s state that may outlive the current call boundary\n"
        "- storing the borrow would create a longer-lived ownership alias the compiler cannot keep boundary-safe\n"
        "Fix:\n"
        "- store a %s instead\n"
        "- or change the current parameter to 'own' if %s %s is intended",
        value_label,
        value_name,
        container_kind != NULL ? container_kind : "container",
        source_path != NULL ? " from '" : "",
        source_path != NULL ? source_path : "",
        source_path != NULL ? "'" : "",
        ctx->current_function_decl != NULL
            && ctx->current_function_decl->data.func_decl.name != NULL
                ? ctx->current_function_decl->data.func_decl.name
                : "<anonymous>",
        value_name,
        value_label,
        source_path != NULL ? source_path : value_name,
        container_kind != NULL ? container_kind : "container",
        container_name != NULL ? container_name : "<container store>",
        snapshot_label,
        transfer_label,
        container_kind != NULL ? container_kind : "container");
    free(source_path);
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
            if (slot->data.domain_slot.is_tobject)
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

    if (type_is_capability_bearing(element_type)
        || type_is_capability_bearing(value_type)) {
        semantic_error(ctx, expr->data.call.arguments[1],
            "%s cannot transport capability-bearing values (SecureSlot/Token) yet; keep capability-bearing state local to the authorized flow",
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

    if (builtin_is_subject_boundary_type(element_type, ctx)
        || builtin_is_subject_boundary_type(value_type, ctx)) {
        char *source_path = builtin_expr_source_path(expr->data.call.arguments[1]);
        const char *borrowed_root_name =
            builtin_borrowed_boundary_root_name(expr->data.call.arguments[1], ctx);
        if (!builtin_is_subject_boundary_type(element_type, ctx)
            || !builtin_is_subject_boundary_type(value_type, ctx)) {
            semantic_error(ctx, expr->data.call.arguments[1],
                "%s subject mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the same subject boundary contract\n"
                "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                name,
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type));
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        if (expr->data.call.arguments[1] == NULL
            || expr->data.call.arguments[1]->type != AST_IDENTIFIER) {
            char *reason_line = NULL;
            if (borrowed_root_name != NULL) {
                const char *rendered_path =
                    source_path != NULL ? source_path : "<expression>";
                size_t needed = snprintf(NULL, 0,
                    "- '%s' is derived from borrowed source '%s'\n",
                    rendered_path, borrowed_root_name);
                reason_line = malloc(needed + 1);
                if (reason_line != NULL) {
                    snprintf(reason_line, needed + 1,
                        "- '%s' is derived from borrowed source '%s'\n",
                        rendered_path, borrowed_root_name);
                }
            }
            semantic_error(ctx, expr->data.call.arguments[1],
                "%s subject sends must transfer from a named variable instead of '%s'.\n"
                "Reason:\n"
                "- ownership transfer at a channel boundary must point to one concrete source binding\n"
                "- unnamed expressions make moved-here provenance ambiguous\n"
                "%s"
                "Fix:\n"
                "- bind the subject first in a local variable\n"
                "- then send that named variable",
                name,
                source_path != NULL ? source_path : "<expression>",
                reason_line != NULL ? reason_line : "");
            free(reason_line);
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        if (identifier_is_borrowed_boundary_param(expr->data.call.arguments[1], ctx)) {
            semantic_error(ctx, expr->data.call.arguments[1],
                "Borrowed ref subject '%s' cannot escape through channel send.\n"
                "Reason:\n"
                "- consumer path is function '%s'\n"
                "- '%s' entered this function as a borrowed 'ref' subject\n"
                "- %s would transfer that borrow beyond the current call boundary\n"
                "Fix:\n"
                "- send a projection/object/tobject/value snapshot instead\n"
                "- or change the parameter to 'own' before transfer",
                expr->data.call.arguments[1]->data.identifier.name,
                ctx->current_function_decl != NULL
                    && ctx->current_function_decl->data.func_decl.name != NULL
                        ? ctx->current_function_decl->data.func_decl.name
                        : "<anonymous>",
                expr->data.call.arguments[1]->data.identifier.name,
                name);
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        free(source_path);
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (type_requires_boundary_borrow_tracking(element_type, ctx)
        || type_requires_boundary_borrow_tracking(value_type, ctx)) {
        char *source_path = builtin_expr_source_path(expr->data.call.arguments[1]);
        const char *borrowed_root_name =
            builtin_borrowed_boundary_root_name(expr->data.call.arguments[1], ctx);
        if (!type_requires_boundary_borrow_tracking(element_type, ctx)
            || !type_requires_boundary_borrow_tracking(value_type, ctx)
            || builtin_is_subject_boundary_type(element_type, ctx)
            || builtin_is_subject_boundary_type(value_type, ctx)
            || type_is_movable_resource_handle(element_type)
            || type_is_movable_resource_handle(value_type)) {
            semantic_error(ctx, expr->data.call.arguments[1],
                "%s boundary value mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the same boundary value contract\n"
                "- ownership provenance cannot be preserved when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                name,
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type));
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        if (expr->data.call.arguments[1] == NULL
            || expr->data.call.arguments[1]->type != AST_IDENTIFIER) {
            char *reason_line = NULL;
            if (borrowed_root_name != NULL) {
                const char *rendered_path =
                    source_path != NULL ? source_path : "<expression>";
                size_t needed = snprintf(NULL, 0,
                    "- '%s' is derived from borrowed source '%s'\n",
                    rendered_path, borrowed_root_name);
                reason_line = malloc(needed + 1);
                if (reason_line != NULL) {
                    snprintf(reason_line, needed + 1,
                        "- '%s' is derived from borrowed source '%s'\n",
                        rendered_path, borrowed_root_name);
                }
            }
            semantic_error(ctx, expr->data.call.arguments[1],
                "%s boundary value channel sends must transfer from a named variable instead of '%s'.\n"
                "Reason:\n"
                "- ownership transfer at a channel boundary must point to one concrete source binding\n"
                "- unnamed expressions make moved-here provenance ambiguous\n"
                "%s"
                "Fix:\n"
                "- bind the boundary value first in a local variable\n"
                "- then send that named variable",
                name,
                source_path != NULL ? source_path : "<expression>",
                reason_line != NULL ? reason_line : "");
            free(reason_line);
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        if (expr->data.call.arguments[1] != NULL
            && expr->data.call.arguments[1]->type == AST_IDENTIFIER
            && identifier_is_borrowed_boundary_param(expr->data.call.arguments[1], ctx)) {
            semantic_error(ctx, expr->data.call.arguments[1],
                "Borrowed ref boundary value '%s' cannot escape through channel send.\n"
                "Reason:\n"
                "- consumer path is function '%s'\n"
                "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                "- %s would transfer that borrow beyond the current call boundary\n"
                "Fix:\n"
                "- send a copied/value/projection snapshot instead\n"
                "- or change the parameter to 'own' before transfer",
                expr->data.call.arguments[1]->data.identifier.name,
                ctx->current_function_decl != NULL
                    && ctx->current_function_decl->data.func_decl.name != NULL
                        ? ctx->current_function_decl->data.func_decl.name
                        : "<anonymous>",
                expr->data.call.arguments[1]->data.identifier.name,
                name);
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        free(source_path);
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
    if (type_is_capability_bearing(element_type)) {
        semantic_error(ctx, expr->data.call.arguments[0],
            "%s cannot yield capability-bearing values (SecureSlot/Token) yet; receive a plain value instead",
            name);
        return TYPE_UNKNOWN;
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
            "HasProjection(...) is only available inside relation/effect/zone declarations and methods.\n"
            "Reason:\n"
            "- projection slots only exist on relation/effect/zone surfaces\n"
            "- the current function is outside domain projection context\n"
            "Fix:\n"
            "- call HasProjection(...) inside a relation/effect/zone body\n"
            "- or pass projection state into this function explicitly");
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
            "Unknown %s projection slot '%s' in HasProjection(...).\n"
            "Reason:\n"
            "- HasProjection(...) only accepts object/tobject slots declared on the current %s\n"
            "- '%s' is either missing or not a projection slot\n"
            "Fix:\n"
            "- declare an object/tobject slot named '%s'\n"
            "- or call HasProjection(...) with an existing projection slot name",
            label != NULL ? label : "domain",
            slot_name,
            label != NULL ? label : "domain",
            slot_name,
            slot_name);
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
            "HasLayer(...) is only available inside zone declarations and zone methods.\n"
            "Reason:\n"
            "- relation/effect layer slots are zone-local\n"
            "- the current function is outside zone context\n"
            "Fix:\n"
            "- call HasLayer(...) inside a zone body\n"
            "- or pass the needed layer state into this function explicitly");
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
            "Unknown zone layer slot '%s' in HasLayer(...).\n"
            "Reason:\n"
            "- '%s' is not a declared relation/effect slot in the current zone\n"
            "Fix:\n"
            "- declare a relation/effect slot named '%s'\n"
            "- or call HasLayer(...) with an existing zone layer slot name",
            slot_name, slot_name, slot_name);
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
            "HasState(...) is only available inside zone declarations and zone methods.\n"
            "Reason:\n"
            "- zone state contracts are scoped to a zone body\n"
            "- the current function is outside zone context\n"
            "Fix:\n"
            "- call HasState(...) inside a zone body\n"
            "- or pass the relevant state into this function explicitly");
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
            "Unknown zone state '%s' in HasState(...).\n"
            "Reason:\n"
            "- '%s' is not a declared state in the current zone\n"
            "Fix:\n"
            "- declare a zone state named '%s'\n"
            "- or call HasState(...) with an existing state name",
            state_name, state_name, state_name);
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
            "'HasZone' expects exactly 1 argument, got %zu.\n"
            "Reason:\n"
            "- world zone/state observability requires a single zone-slot or world-state name\n"
            "Fix:\n"
            "- call HasZone(zoneOrState)\n"
            "- or remove the extra argument(s)",
            call->data.call.arg_count);
        return TYPE_BOOL;
    }

    world = ctx->current_world;
    if (world == NULL || world->type != AST_WORLD_DECL) {
        semantic_error(ctx, call,
            "HasZone(...) is only available inside world declarations and world methods.\n"
            "Reason:\n"
            "- zone/state observability is anchored to the current world context\n"
            "- there is no active world declaration or world method here\n"
            "Fix:\n"
            "- move this query into a world declaration or world method\n"
            "- or pass the relevant state through another contract surface");
        return TYPE_BOOL;
    }

    arg = call->data.call.arguments[0];
    if (arg == NULL) {
        semantic_error(ctx, call,
            "HasZone(...) requires a zone slot or world state name.\n"
            "Reason:\n"
            "- the query cannot resolve an empty zone/state reference\n"
            "Fix:\n"
            "- pass a world zone slot name\n"
            "- or pass a declared world state name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        name = arg->data.identifier.name;
    } else if (arg->type == AST_STRING) {
        name = arg->data.string.value;
    } else {
        semantic_error(ctx, arg,
            "HasZone(...) expects a zone/state identifier or string literal.\n"
            "Reason:\n"
            "- world observability queries resolve names, not arbitrary expressions\n"
            "Fix:\n"
            "- pass a zone/state identifier\n"
            "- or pass a string literal with the declared name");
        return TYPE_BOOL;
    }

    if (name == NULL) {
        semantic_error(ctx, arg,
            "HasZone(...) requires a valid zone/state name.\n"
            "Reason:\n"
            "- the provided identifier/string did not contain a usable name\n"
            "Fix:\n"
            "- provide a non-empty world zone slot name\n"
            "- or provide a non-empty world state name");
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
        "Unknown world zone/state '%s' in HasZone(...).\n"
        "Reason:\n"
        "- current world '%s' does not declare a zone slot or state named '%s'\n"
        "Fix:\n"
        "- use a declared world zone slot/state name\n"
        "- or declare '%s' on the current world",
        name,
        world->data.world_decl.name != NULL ? world->data.world_decl.name : "<world>",
        name,
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
            "'%s' expects exactly 2 argument(s), got %zu.\n"
            "Reason:\n"
            "- %s needs a world zone slot and a zone %s name\n"
            "Fix:\n"
            "- call %s(zoneSlot, %sName)\n"
            "- or remove the extra argument(s)",
            builtin_name, call->data.call.arg_count,
            builtin_name, detail_label,
            builtin_name, detail_label);
        return TYPE_BOOL;
    }

    world = ctx->current_world;
    if (world == NULL || world->type != AST_WORLD_DECL) {
        semantic_error(ctx, call,
            "%s(...) is only available inside world declarations and world methods.\n"
            "Reason:\n"
            "- world zone detail queries are anchored to the active world context\n"
            "- there is no active world declaration or world method here\n"
            "Fix:\n"
            "- move this query into a world declaration or world method\n"
            "- or surface the needed detail through another contract",
            builtin_name);
        return TYPE_BOOL;
    }

    zone_arg = call->data.call.arguments[0];
    detail_arg = call->data.call.arguments[1];
    if (zone_arg == NULL || detail_arg == NULL) {
        semantic_error(ctx, call,
            "%s(...) requires a world zone slot and zone %s name.\n"
            "Reason:\n"
            "- the query cannot resolve a missing zone/detail reference\n"
            "Fix:\n"
            "- pass a world zone slot as the first argument\n"
            "- and pass a zone %s name as the second argument",
            builtin_name, detail_label, detail_label);
        return TYPE_BOOL;
    }

    if (zone_arg->type == AST_IDENTIFIER)
        zone_slot_name = zone_arg->data.identifier.name;
    else if (zone_arg->type == AST_STRING)
        zone_slot_name = zone_arg->data.string.value;
    else {
        semantic_error(ctx, zone_arg,
            "%s(...) expects a world zone-slot identifier or string literal as the first argument.\n"
            "Reason:\n"
            "- the first argument names the embedded zone slot inside the current world\n"
            "Fix:\n"
            "- pass a world zone-slot identifier\n"
            "- or pass a string literal with that slot name",
            builtin_name);
        return TYPE_BOOL;
    }

    if (detail_arg->type == AST_IDENTIFIER)
        detail_name = detail_arg->data.identifier.name;
    else if (detail_arg->type == AST_STRING)
        detail_name = detail_arg->data.string.value;
    else {
        semantic_error(ctx, detail_arg,
            "%s(...) expects a zone %s identifier or string literal as the second argument.\n"
            "Reason:\n"
            "- the second argument names the zone-local %s being queried\n"
            "Fix:\n"
            "- pass a zone %s identifier\n"
            "- or pass a string literal with that declared name",
            builtin_name, detail_label, detail_label, detail_label);
        return TYPE_BOOL;
    }

    if (zone_slot_name == NULL || detail_name == NULL) {
        semantic_error(ctx, call,
            "%s(...) requires valid zone-slot and %s names.\n"
            "Reason:\n"
            "- one or both query names resolved to an unusable value\n"
            "Fix:\n"
            "- provide a non-empty world zone-slot name\n"
            "- and provide a non-empty zone %s name",
            builtin_name, detail_label, detail_label);
        return TYPE_BOOL;
    }

    zone_decl = resolve_world_zone_decl_local(ctx, world, zone_slot_name);
    if (zone_decl == NULL) {
        semantic_error(ctx, zone_arg,
            "Unknown world zone slot '%s' in %s(...).\n"
            "Reason:\n"
            "- current world '%s' does not declare a zone slot named '%s'\n"
            "Fix:\n"
            "- use a declared world zone slot name\n"
            "- or declare zone slot '%s' on the current world",
            zone_slot_name, builtin_name,
            world->data.world_decl.name != NULL ? world->data.world_decl.name : "<world>",
            zone_slot_name,
            zone_slot_name);
        return TYPE_BOOL;
    }

    detail_decl = resolver(zone_decl, detail_name);
    if (detail_decl == NULL) {
        semantic_error(ctx, detail_arg,
            "Unknown zone %s '%s' in %s(%s, ...).\n"
            "Reason:\n"
            "- zone '%s' does not declare a %s named '%s'\n"
            "Fix:\n"
            "- use a declared zone %s name\n"
            "- or declare '%s' on zone '%s'",
            detail_label, detail_name, builtin_name, zone_slot_name,
            zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
            detail_label, detail_name,
            detail_label,
            detail_name,
            zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
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
    if (strcmp(name, "Clone")           == 0) return BUILTIN_CLONE;
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
    if (strcmp(name, "ToTObject")       == 0) return BUILTIN_TO_TOBJECT;
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
    if (strcmp(name, "IntentActiveParentHandle") == 0)
        return BUILTIN_INTENT_ACTIVE_PARENT_HANDLE;
    if (strcmp(name, "IntentActiveTraceId") == 0) return BUILTIN_INTENT_ACTIVE_TRACE_ID;
    if (strcmp(name, "IntentActivePriority") == 0) return BUILTIN_INTENT_ACTIVE_PRIORITY;
    if (strcmp(name, "IntentActiveSubjectCount") == 0)
        return BUILTIN_INTENT_ACTIVE_SUBJECT_COUNT;
    if (strcmp(name, "IntentActiveStepCount") == 0)
        return BUILTIN_INTENT_ACTIVE_STEP_COUNT;
    if (strcmp(name, "IntentActiveConcurrent") == 0) return BUILTIN_INTENT_ACTIVE_CONCURRENT;
    if (strcmp(name, "IntentActiveFailed") == 0) return BUILTIN_INTENT_ACTIVE_FAILED;
    if (strcmp(name, "IntentActiveFailure") == 0) return BUILTIN_INTENT_ACTIVE_FAILURE;
    if (strcmp(name, "IntentActiveTrace") == 0) return BUILTIN_INTENT_ACTIVE_TRACE;
    if (strcmp(name, "IntentActiveStepName") == 0) return BUILTIN_INTENT_ACTIVE_STEP_NAME;
    if (strcmp(name, "IntentActiveStepZone") == 0) return BUILTIN_INTENT_ACTIVE_STEP_ZONE;
    if (strcmp(name, "IntentActiveStepPhase") == 0) return BUILTIN_INTENT_ACTIVE_STEP_PHASE;
    if (strcmp(name, "IntentActiveStepParticipant") == 0) return BUILTIN_INTENT_ACTIVE_STEP_PARTICIPANT;
    if (strcmp(name, "IntentActiveStepSlot") == 0) return BUILTIN_INTENT_ACTIVE_STEP_SLOT;
    if (strcmp(name, "IntentActiveStepFromZone") == 0) return BUILTIN_INTENT_ACTIVE_STEP_FROM_ZONE;
    if (strcmp(name, "IntentActiveStepFromSlot") == 0) return BUILTIN_INTENT_ACTIVE_STEP_FROM_SLOT;
    if (strcmp(name, "IntentActiveStepToZone") == 0) return BUILTIN_INTENT_ACTIVE_STEP_TO_ZONE;
    if (strcmp(name, "IntentActiveStepToSlot") == 0) return BUILTIN_INTENT_ACTIVE_STEP_TO_SLOT;
    if (strcmp(name, "IntentActiveStepOk") == 0) return BUILTIN_INTENT_ACTIVE_STEP_OK;
    if (strcmp(name, "IntentActiveStepFailure") == 0) return BUILTIN_INTENT_ACTIVE_STEP_FAILURE;
    if (strcmp(name, "IntentCurrentHandle") == 0) return BUILTIN_INTENT_CURRENT_HANDLE;
    if (strcmp(name, "IntentRecentCount") == 0) return BUILTIN_INTENT_RECENT_COUNT;
    if (strcmp(name, "IntentRecentHandle") == 0) return BUILTIN_INTENT_RECENT_HANDLE;
    if (strcmp(name, "IntentRecentTraceId") == 0) return BUILTIN_INTENT_RECENT_TRACE_ID;
    if (strcmp(name, "IntentRecentName") == 0) return BUILTIN_INTENT_RECENT_NAME;
    if (strcmp(name, "IntentRecentTrace") == 0) return BUILTIN_INTENT_RECENT_TRACE;
    if (strcmp(name, "IntentRecentFailure") == 0) return BUILTIN_INTENT_RECENT_FAILURE;
    if (strcmp(name, "IntentRecentStepCount") == 0) return BUILTIN_INTENT_RECENT_STEP_COUNT;
    if (strcmp(name, "IntentRecentFailed") == 0) return BUILTIN_INTENT_RECENT_FAILED;
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
        semantic_error_code(ctx, "PGY_SEM_MOVE_FROM_RELEASED", slot_arg,
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

    if (ctx->in_parallel && slot_type->data.slot.is_secure) {
        semantic_error(ctx, slot_arg,
            "Parallel context does not permit SecureSlot access yet; serialize capability-bearing slot reads/writes/releases outside the parallel block");
        return false;
    }

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_code(ctx, "PGY_SEM_SLOT_RELEASED", slot_arg,
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
                if (!validate_secure_token_arg(token_arg, sym, slot_type, ctx))
                    return false;
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

    if (ctx->in_parallel && slot_type->data.slot.is_secure) {
        semantic_error(ctx, slot_arg,
            "Parallel context does not permit SecureSlot access yet; serialize capability-bearing slot reads/writes/releases outside the parallel block");
        return TYPE_UNKNOWN;
    }

    if (slot_arg->type == AST_IDENTIFIER) {
        Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
        if (sym != NULL && sym->kind == SYMBOL_SLOT) {
            if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_code(ctx, "PGY_SEM_SLOT_RELEASED", slot_arg,
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
                if (!validate_secure_token_arg(token_arg, sym, slot_type, ctx))
                    return TYPE_UNKNOWN;
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

    if (ctx->in_parallel && sym->slot_info.is_secure) {
        semantic_error(ctx, slot_arg,
            "Parallel context does not permit SecureSlot access yet; serialize capability-bearing slot reads/writes/releases outside the parallel block");
        return false;
    }

    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
        semantic_error_code(ctx, "PGY_SEM_SLOT_DOUBLE_RELEASE", slot_arg,
            "Slot '%s' has already been released", slot_name);
        return false;
    }

    if (sym->slot_info.is_secure && call->data.call.arg_count < 2) {
        semantic_error(ctx, call,
            "Release of SecureSlot '%s' requires a token argument",
            slot_name);
        return false;
    }
    if (sym->slot_info.is_secure
        && !validate_secure_token_arg(call->data.call.arguments[1], sym, sym->type, ctx)) {
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

    if (ctx->in_parallel) {
        semantic_error(ctx, expr,
            "Parallel context does not permit DeviceSlot operations yet; keep device access serialized outside the parallel block");
        return TYPE_UNKNOWN;
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
        || strcmp(name, "Ceil") == 0
        || strcmp(name, "Sin") == 0 || strcmp(name, "Cos") == 0
        || strcmp(name, "Tan") == 0 || strcmp(name, "Asin") == 0
        || strcmp(name, "Acos") == 0 || strcmp(name, "Atan") == 0
        || strcmp(name, "Exp") == 0 || strcmp(name, "MathLog") == 0
        || strcmp(name, "Log10") == 0 || strcmp(name, "Log2") == 0
        || strcmp(name, "Round") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Atan2") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Clamp") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        Type *val = type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        type_check_expression(expr->data.call.arguments[2], ctx);
        return val;
    }
    if (strcmp(name, "PI") == 0 || strcmp(name, "E") == 0) {
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
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN; /* type resolved from let annotation */
    }
    if (strcmp(name, "MapSet") == 0) {
        Type *map_type;
        Type *key_type;
        Type *value_type;
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        key_type = type_check_expression(expr->data.call.arguments[1], ctx);
        value_type = type_check_expression(expr->data.call.arguments[2], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[2], "map", "MapSet", ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            require_assignable(key_type,
                map_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
            require_assignable(value_type,
                map_type->data.constructed.args[1],
                expr->data.call.arguments[2], ctx);
            if (map_type->data.constructed.args[0] != NULL
                && map_type->data.constructed.args[0]->name != NULL
                && strcmp(map_type->data.constructed.args[0]->name, "String") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Int") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Long") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Bool") != 0) {
                semantic_error(ctx, expr->data.call.arguments[0],
                    "MapSet currently supports only HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, and HashMap<Bool, T>, got '%s'",
                    map_type->name != NULL ? map_type->name : "<type>");
            }
        } else if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "MapSet expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "MapGet") == 0) {
        Type *map_type;
        Type *key_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        key_type = type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            require_assignable(key_type,
                map_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
            if (map_type->data.constructed.args[0] != NULL
                && map_type->data.constructed.args[0]->name != NULL
                && strcmp(map_type->data.constructed.args[0]->name, "String") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Int") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Long") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Bool") != 0) {
                semantic_error(ctx, expr->data.call.arguments[0],
                    "MapGet currently supports only HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, and HashMap<Bool, T>, got '%s'",
                    map_type->name != NULL ? map_type->name : "<type>");
            }
            return map_type->data.constructed.args[1];
        }
        if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "MapGet expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_UNKNOWN; /* resolved from context */
    }
    if (strcmp(name, "MapHas") == 0) {
        Type *map_type;
        Type *key_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        key_type = type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            require_assignable(key_type,
                map_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
            if (map_type->data.constructed.args[0] != NULL
                && map_type->data.constructed.args[0]->name != NULL
                && strcmp(map_type->data.constructed.args[0]->name, "String") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Int") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Long") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Bool") != 0) {
                semantic_error(ctx, expr->data.call.arguments[0],
                    "MapHas currently supports only HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, and HashMap<Bool, T>, got '%s'",
                    map_type->name != NULL ? map_type->name : "<type>");
            }
        } else if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "MapHas expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "MapRemove") == 0) {
        Type *map_type;
        Type *key_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        key_type = type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            require_assignable(key_type,
                map_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
            if (map_type->data.constructed.args[0] != NULL
                && map_type->data.constructed.args[0]->name != NULL
                && strcmp(map_type->data.constructed.args[0]->name, "String") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Int") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Long") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Bool") != 0) {
                semantic_error(ctx, expr->data.call.arguments[0],
                    "MapRemove currently supports only HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, and HashMap<Bool, T>, got '%s'",
                    map_type->name != NULL ? map_type->name : "<type>");
            }
        } else if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "MapRemove expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "MapSize") == 0) {
        Type *map_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (map_type != NULL && map_type != TYPE_UNKNOWN
            && !type_is_constructed_named(map_type, "HashMap")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "MapSize expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "MapKeys") == 0) {
        Type *map_type;
        Type *key_type;
        Type *args[1];
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            key_type = map_type->data.constructed.args[0];
            if (key_type != NULL
                && key_type->name != NULL
                && strcmp(key_type->name, "String") != 0
                && strcmp(key_type->name, "Int") != 0
                && strcmp(key_type->name, "Long") != 0
                && strcmp(key_type->name, "Bool") != 0) {
                semantic_error(ctx, expr->data.call.arguments[0],
                    "MapKeys currently supports only HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, and HashMap<Bool, T>, got '%s'",
                    map_type->name != NULL ? map_type->name : "<type>");
            }
            args[0] = key_type != NULL ? key_type : TYPE_UNKNOWN;
            return type_create_constructed(TYPE_ARRAY, args, 1);
        }
        if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "MapKeys expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_UNKNOWN;
    }
    /* List builtins */
    if (strcmp(name, "ListNew") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "ListPush") == 0) {
        Type *list_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        list_type = type_check_expression(expr->data.call.arguments[0], ctx);
        value_type = type_check_expression(expr->data.call.arguments[1], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[1], "list", "ListPush", ctx);
        if (type_is_constructed_named(list_type, "List")
            && list_type->data.constructed.arg_count == 1) {
            require_assignable(value_type,
                list_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
        } else if (list_type != NULL && list_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ListPush expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ListGet") == 0) {
        Type *list_type;
        Type *index_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        list_type = type_check_expression(expr->data.call.arguments[0], ctx);
        index_type = type_check_expression(expr->data.call.arguments[1], ctx);
        require_assignable(index_type, TYPE_INT, expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(list_type, "List")
            && list_type->data.constructed.arg_count == 1) {
            return list_type->data.constructed.args[0];
        }
        if (list_type != NULL && list_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ListGet expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "ListSet") == 0) {
        Type *list_type;
        Type *index_type;
        Type *value_type;
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        list_type = type_check_expression(expr->data.call.arguments[0], ctx);
        index_type = type_check_expression(expr->data.call.arguments[1], ctx);
        value_type = type_check_expression(expr->data.call.arguments[2], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[2], "list", "ListSet", ctx);
        require_assignable(index_type, TYPE_INT, expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(list_type, "List")
            && list_type->data.constructed.arg_count == 1) {
            require_assignable(value_type,
                list_type->data.constructed.args[0],
                expr->data.call.arguments[2], ctx);
        } else if (list_type != NULL && list_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ListSet expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ListSize") == 0) {
        Type *list_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        list_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (list_type != NULL && list_type != TYPE_UNKNOWN
            && !type_is_constructed_named(list_type, "List")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ListSize expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "ListRemove") == 0) {
        Type *list_type;
        Type *index_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        list_type = type_check_expression(expr->data.call.arguments[0], ctx);
        index_type = type_check_expression(expr->data.call.arguments[1], ctx);
        require_assignable(index_type, TYPE_INT, expr->data.call.arguments[1], ctx);
        if (list_type != NULL && list_type != TYPE_UNKNOWN
            && !type_is_constructed_named(list_type, "List")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "ListRemove expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_INT;
    }
    /* Set builtins */
    if (strcmp(name, "SetNew") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "SetAdd") == 0 || strcmp(name, "SetRemove") == 0) {
        Type *set_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        set_type = type_check_expression(expr->data.call.arguments[0], ctx);
        value_type = type_check_expression(expr->data.call.arguments[1], ctx);
        if (strcmp(name, "SetAdd") == 0) {
            reject_borrowed_boundary_container_store(
                expr->data.call.arguments[1], "set", "SetAdd", ctx);
        }
        if (type_is_constructed_named(set_type, "Set")
            && set_type->data.constructed.arg_count == 1) {
            require_assignable(value_type,
                set_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
        } else if (set_type != NULL && set_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "%s expects Set<T> as first argument, got '%s'",
                name,
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "SetHas") == 0) {
        Type *set_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        set_type = type_check_expression(expr->data.call.arguments[0], ctx);
        value_type = type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(set_type, "Set")
            && set_type->data.constructed.arg_count == 1) {
            require_assignable(value_type,
                set_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
        } else if (set_type != NULL && set_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "SetHas expects Set<T> as first argument, got '%s'",
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "SetSize") == 0) {
        Type *set_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        set_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (set_type != NULL && set_type != TYPE_UNKNOWN
            && !type_is_constructed_named(set_type, "Set")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "SetSize expects Set<T> as first argument, got '%s'",
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_INT;
    }
    /* Queue builtins */
    if (strcmp(name, "QueueNew") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "QueuePush") == 0) {
        Type *queue_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = type_check_expression(expr->data.call.arguments[0], ctx);
        value_type = type_check_expression(expr->data.call.arguments[1], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[1], "queue", "QueuePush", ctx);
        if (type_is_constructed_named(queue_type, "Queue")
            && queue_type->data.constructed.arg_count == 1) {
            require_assignable(value_type,
                queue_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
        } else if (queue_type != NULL && queue_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "QueuePush expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "QueuePop") == 0) {
        Type *queue_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (type_is_constructed_named(queue_type, "Queue")
            && queue_type->data.constructed.arg_count == 1) {
            return queue_type->data.constructed.args[0];
        }
        if (queue_type != NULL && queue_type != TYPE_UNKNOWN) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "QueuePop expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "QueueSize") == 0) {
        Type *queue_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (queue_type != NULL && queue_type != TYPE_UNKNOWN
            && !type_is_constructed_named(queue_type, "Queue")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "QueueSize expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "QueueEmpty") == 0) {
        Type *queue_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (queue_type != NULL && queue_type != TYPE_UNKNOWN
            && !type_is_constructed_named(queue_type, "Queue")) {
            semantic_error(ctx, expr->data.call.arguments[0],
                "QueueEmpty expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
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
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[1], "array", "ArrayPush", ctx);
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
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[2], "array", "ArraySet", ctx);
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
        if (source_status == 2) {
            semantic_error(ctx, call,
                "%s target field '%s' is ambiguous in source subject '%s'.\n"
                "Reason:\n"
                "- multiple projection source paths match field '%s'\n"
                "- automatic projection cannot choose one path safely\n"
                "Fix:\n"
                "- rename one of the source fields to make the path unique\n"
                "- or expose the desired value directly on the subject host",
                builtin_name,
                target_field->name,
                source_type->name != NULL ? source_type->name : "<unknown>",
                target_field->name);
            continue;
        }
        if (source_status == 0 || source_field_type == NULL) {
            semantic_error(ctx, call,
                "%s target field '%s' is missing from source subject '%s'",
                builtin_name,
                target_field->name,
                source_type->name != NULL ? source_type->name : "<unknown>");
            continue;
        }

        target_field_type = resolve_type_node(target_field->type, ctx);
        require_assignable(source_field_type, target_field_type, call, ctx);
    }

    if (!in_projection_context) {
        if (expected_kind == NOMINAL_DECL_TOBJECT) {
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
        NOMINAL_DECL_TOBJECT, "tobject");
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
    case BUILTIN_CLONE:
        return type_check_stdlib_call(call, "Clone", ctx);
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
        case BUILTIN_TO_TOBJECT:
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
    case BUILTIN_INTENT_ACTIVE_PARENT_HANDLE:
        check_call_arity(call, 1, "IntentActiveParentHandle", ctx);
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
    case BUILTIN_INTENT_ACTIVE_SUBJECT_COUNT:
        check_call_arity(call, 1, "IntentActiveSubjectCount", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_STEP_COUNT:
        check_call_arity(call, 1, "IntentActiveStepCount", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_CONCURRENT:
        check_call_arity(call, 1, "IntentActiveConcurrent", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_ACTIVE_FAILED:
        check_call_arity(call, 1, "IntentActiveFailed", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_ACTIVE_FAILURE:
        check_call_arity(call, 1, "IntentActiveFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_TRACE:
        check_call_arity(call, 1, "IntentActiveTrace", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_NAME:
        check_call_arity(call, 2, "IntentActiveStepName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_ZONE:
        check_call_arity(call, 2, "IntentActiveStepZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_PHASE:
        check_call_arity(call, 2, "IntentActiveStepPhase", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_PARTICIPANT:
        check_call_arity(call, 2, "IntentActiveStepParticipant", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_SLOT:
        check_call_arity(call, 2, "IntentActiveStepSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_ZONE:
        check_call_arity(call, 2, "IntentActiveStepFromZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_SLOT:
        check_call_arity(call, 2, "IntentActiveStepFromSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_TO_ZONE:
        check_call_arity(call, 2, "IntentActiveStepToZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_TO_SLOT:
        check_call_arity(call, 2, "IntentActiveStepToSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_OK:
        check_call_arity(call, 2, "IntentActiveStepOk", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_ACTIVE_STEP_FAILURE:
        check_call_arity(call, 2, "IntentActiveStepFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_CURRENT_HANDLE:
        check_call_arity(call, 0, "IntentCurrentHandle", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_COUNT:
        check_call_arity(call, 0, "IntentRecentCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_HANDLE:
        check_call_arity(call, 1, "IntentRecentHandle", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_TRACE_ID:
        check_call_arity(call, 1, "IntentRecentTraceId", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_NAME:
        check_call_arity(call, 1, "IntentRecentName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_RECENT_TRACE:
        check_call_arity(call, 1, "IntentRecentTrace", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_RECENT_FAILURE:
        check_call_arity(call, 1, "IntentRecentFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_RECENT_STEP_COUNT:
        check_call_arity(call, 1, "IntentRecentStepCount", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_FAILED:
        check_call_arity(call, 1, "IntentRecentFailed", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    default:
        return TYPE_UNKNOWN;
    }
}
