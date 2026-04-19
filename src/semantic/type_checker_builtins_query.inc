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
                "Borrowed ref subject '%s' cannot escape through channel send from '%s'.\n"
                "Reason:\n"
                "- consumer path is function '%s'\n"
                "- '%s' entered this function as a borrowed 'ref' subject\n"
                "- '%s' is derived from that borrowed subject provenance\n"
                "- %s would transfer that borrow beyond the current call boundary\n"
                "Fix:\n"
                "- send a projection/object/tobject/value snapshot instead\n"
                "- or change the parameter to 'own' before transfer",
                expr->data.call.arguments[1]->data.identifier.name,
                source_path != NULL
                    ? source_path
                    : expr->data.call.arguments[1]->data.identifier.name,
                ctx->current_function_decl != NULL
                    && ctx->current_function_decl->data.func_decl.name != NULL
                        ? ctx->current_function_decl->data.func_decl.name
                        : "<anonymous>",
                expr->data.call.arguments[1]->data.identifier.name,
                source_path != NULL
                    ? source_path
                    : expr->data.call.arguments[1]->data.identifier.name,
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
                "Borrowed ref boundary value '%s' cannot escape through channel send from '%s'.\n"
                "Reason:\n"
                "- consumer path is function '%s'\n"
                "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                "- '%s' is derived from that borrowed boundary provenance\n"
                "- %s would transfer that borrow beyond the current call boundary\n"
                "Fix:\n"
                "- send a copied/value/projection snapshot instead\n"
                "- or change the parameter to 'own' before transfer",
                expr->data.call.arguments[1]->data.identifier.name,
                source_path != NULL
                    ? source_path
                    : expr->data.call.arguments[1]->data.identifier.name,
                ctx->current_function_decl != NULL
                    && ctx->current_function_decl->data.func_decl.name != NULL
                        ? ctx->current_function_decl->data.func_decl.name
                        : "<anonymous>",
                expr->data.call.arguments[1]->data.identifier.name,
                source_path != NULL
                    ? source_path
                    : expr->data.call.arguments[1]->data.identifier.name,
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
