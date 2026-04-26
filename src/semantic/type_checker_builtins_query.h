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

bool
check_call_arity(ASTNode *expr, size_t expected, const char *name,
                 SemanticContext *ctx)
{
    if (expr->data.call.arg_count != expected) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
            "'%s' expects %llu argument(s), got %llu",
            name,
            (unsigned long long) expected,
            (unsigned long long) expr->data.call.arg_count);
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, token_arg,
            "SecureSlot '%s' requires a named paired token identifier",
            slot_sym->name != NULL ? slot_sym->name : "<slot>");
        return false;
    }

    token_name = token_arg->data.identifier.name;
    token_sym = scope_lookup(ctx->scope, token_name);
    if (token_sym == NULL || token_sym->kind != SYMBOL_TOKEN) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            token_arg,
            "'%s' is not a capability token for slot '%s'",
            token_name, slot_sym->name != NULL ? slot_sym->name : "<slot>");
        return false;
    }

    if (slot_sym->slot_info.paired_token_name == NULL
        || strcmp(slot_sym->slot_info.paired_token_name, token_name) != 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, token_arg,
            "Token '%s' is not paired with slot '%s'",
            token_name, slot_sym->name);
        return false;
    }

    token_args[0] = slot_type->data.slot.inner_type;
    expected_token_type = type_create_constructed(TYPE_TOKEN, token_args, 1);
    if (token_sym->type != NULL && expected_token_type != NULL
        && !type_equals(token_sym->type, expected_token_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, token_arg,
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

void
reject_borrowed_boundary_container_store(ASTNode *value_expr,
                                         const Type *stored_value_type,
                                         const char *container_kind,
                                         const char *container_name,
                                         SemanticContext *ctx)
{
    const char *borrowed_root_name =
        builtin_borrowed_boundary_root_name(value_expr, ctx);

    if (value_expr == NULL || ctx == NULL
        || borrowed_root_name == NULL) {
        return;
    }

    if (semantic_classify_ownership_type(stored_value_type, ctx)
        == OWNERSHIP_TYPE_COPY_ONLY) {
        return;
    }
    semantic_validate_borrowed_escape(
        value_expr, value_expr, ctx, stored_value_type, borrowed_root_name,
        OWNERSHIP_CONSUMER_CONTAINER_STORE, NULL,
        container_kind != NULL ? container_kind : "container",
        container_name != NULL ? container_name : "<container store>",
        false, NULL, NULL);
}

#include "type_checker_builtins_query_channel.inc"

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
    const char *host_name = NULL;

    if (call->data.call.arg_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "'HasProjection' expects exactly 1 argument, got %llu",
            (unsigned long long) call->data.call.arg_count);
        return TYPE_BOOL;
    }

    host = current_projection_host_decl(ctx, &label, &slots, &slot_count);
    if (host == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
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
        host_name = host->data.relation_decl.name;
        refreshes = host->data.relation_decl.refreshes;
        refresh_count = host->data.relation_decl.refresh_count;
    } else if (host->type == AST_EFFECT_DECL) {
        host_name = host->data.effect_decl.name;
        refreshes = host->data.effect_decl.refreshes;
        refresh_count = host->data.effect_decl.refresh_count;
    } else if (host->type == AST_ZONE_DECL) {
        host_name = host->data.zone_decl.name;
        refreshes = host->data.zone_decl.refreshes;
        refresh_count = host->data.zone_decl.refresh_count;
    }

    arg = call->data.call.arguments[0];
    if (arg == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
            "HasProjection(...) requires an object/tobject slot name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        slot_name = arg->data.identifier.name;
    } else if (arg->type == AST_STRING) {
        slot_name = arg->data.string.value;
    } else {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
            "HasProjection(...) expects an object/tobject slot identifier or string literal");
        return TYPE_BOOL;
    }

    if (slot_name == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
            "HasProjection(...) requires a valid object/tobject slot name");
        return TYPE_BOOL;
    }

    slot = find_domain_projection_slot_local(slots, slot_count,
        refreshes, refresh_count, slot_name);
    if (slot == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
            "Unknown %s projection slot '%s' in HasProjection(...).\n"
            "Reason:\n"
            "- HasProjection(...) only accepts object/tobject slots declared on the current %s\n"
            "- '%s' is either missing or not a projection slot\n"
            "Contract source:\n"
            "- current %s: %s\n"
            "- projection consumer path is HasProjection(%s)\n"
            "Fix:\n"
            "- declare an object/tobject slot named '%s'\n"
            "- or call HasProjection(...) with an existing projection slot name",
            label != NULL ? label : "domain",
            slot_name,
            label != NULL ? label : "domain",
            slot_name,
            label != NULL ? label : "domain",
            host_name != NULL ? host_name : "<anonymous>",
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
            "'HasLayer' expects exactly 1 argument, got %llu",
            (unsigned long long) call->data.call.arg_count);
        return TYPE_BOOL;
    }

    zone = ctx->current_zone;
    if (zone == NULL || zone->type != AST_ZONE_DECL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
            "HasLayer(...) requires a relation/effect slot name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        slot_name = arg->data.identifier.name;
    } else if (arg->type == AST_STRING) {
        slot_name = arg->data.string.value;
    } else {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
            "HasLayer(...) expects a layer-slot identifier or string literal");
        return TYPE_BOOL;
    }

    if (slot_name == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
            "HasLayer(...) requires a valid relation/effect slot name");
        return TYPE_BOOL;
    }

    layer_slot = find_zone_layer_slot_local(zone, slot_name);
    if (layer_slot == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
            "Unknown zone layer slot '%s' in HasLayer(...).\n"
            "Reason:\n"
            "- '%s' is not a declared relation/effect slot in the current zone\n"
            "Contract source:\n"
            "- current zone: %s\n"
            "- layer query path is HasLayer(%s)\n"
            "Fix:\n"
            "- declare a relation/effect slot named '%s'\n"
            "- or call HasLayer(...) with an existing zone layer slot name",
            slot_name, slot_name,
            zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
            slot_name,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID,
            PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
            call,
            "'HasState' expects 1 to 3 argument(s), got %llu",
            (unsigned long long) call->data.call.arg_count);
        return TYPE_BOOL;
    }

    zone = ctx->current_zone;
    if (zone == NULL || zone->type != AST_ZONE_DECL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
            "HasState(...) requires a zone state name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        state_name = arg->data.identifier.name;
    } else if (arg->type == AST_STRING) {
        state_name = arg->data.string.value;
    } else {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
            "HasState(...) expects a state identifier or string literal");
        return TYPE_BOOL;
    }

    if (state_name == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
            "Unknown zone state '%s' in HasState(...).\n"
            "Reason:\n"
            "- '%s' is not a declared state in the current zone\n"
            "Contract source:\n"
            "- current zone: %s\n"
            "- state query path is HasState(%s)\n"
            "Fix:\n"
            "- declare a zone state named '%s'\n"
            "- or call HasState(...) with an existing state name",
            state_name, state_name,
            zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
            state_name,
            state_name);
        return TYPE_BOOL;
    }

    if (call->data.call.arg_count == 1)
        return TYPE_BOOL;

    if (call->data.call.arguments[1] == NULL
        || call->data.call.arguments[1]->type != AST_IDENTIFIER
        || call->data.call.arguments[1]->data.identifier.name == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call->data.call.arguments[1],
            "HasState(...) slot arguments must be zone slot identifiers");
        return TYPE_BOOL;
    }
    if (find_zone_domain_slot_local(zone,
            call->data.call.arguments[1]->data.identifier.name) == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID,
            PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
            call->data.call.arguments[1],
            "Unknown zone slot '%s' in HasState(...)",
            call->data.call.arguments[1]->data.identifier.name);
        return TYPE_BOOL;
    }

    if (!state->data.zone_state.is_relation) {
        slot_name = call->data.call.arguments[1]->data.identifier.name;
        if (call->data.call.arg_count != 2) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
                "Effect state '%s' in HasState(...) accepts at most one zone slot target.\n"
                "Contract source:\n"
                "- current zone: %s\n"
                "- state '%s' targets slot '%s'",
                state_name,
                zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
                state_name,
                state->data.zone_state.left_or_target_slot_name != NULL
                    ? state->data.zone_state.left_or_target_slot_name : "<slot>");
            return TYPE_BOOL;
        }
        if (strcmp(slot_name, state->data.zone_state.left_or_target_slot_name) != 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID,
                PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
                call->data.call.arguments[1],
                "State '%s' is declared on slot '%s', not '%s'.\n"
                "Contract source:\n"
                "- current zone: %s\n"
                "- effect state contract originates from state '%s'",
                state_name,
                state->data.zone_state.left_or_target_slot_name,
                slot_name,
                zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
                state_name);
        }
        return TYPE_BOOL;
    }

    if (call->data.call.arg_count != 3) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
            "Relation state '%s' in HasState(...) requires exactly two endpoint slots.\n"
            "Contract source:\n"
            "- current zone: %s\n"
            "- relation state '%s' is declared between '%s' and '%s'",
            state_name,
            zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
            state_name,
            state->data.zone_state.left_or_target_slot_name != NULL
                ? state->data.zone_state.left_or_target_slot_name : "<left>",
            state->data.zone_state.right_slot_name != NULL
                ? state->data.zone_state.right_slot_name : "<right>");
        return TYPE_BOOL;
    }

    if (call->data.call.arguments[2] == NULL
        || call->data.call.arguments[2]->type != AST_IDENTIFIER
        || call->data.call.arguments[2]->data.identifier.name == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call->data.call.arguments[2],
            "HasState(...) relation endpoint arguments must be zone slot identifiers");
        return TYPE_BOOL;
    }
    if (find_zone_domain_slot_local(zone,
            call->data.call.arguments[2]->data.identifier.name) == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID,
            PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
            call->data.call.arguments[2],
            "Unknown zone slot '%s' in HasState(...)",
            call->data.call.arguments[2]->data.identifier.name);
        return TYPE_BOOL;
    }

    left_slot_name = call->data.call.arguments[1]->data.identifier.name;
    right_slot_name = call->data.call.arguments[2]->data.identifier.name;
    if (strcmp(left_slot_name, state->data.zone_state.left_or_target_slot_name) != 0
        || strcmp(right_slot_name, state->data.zone_state.right_slot_name) != 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID,
            PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
            call,
            "State '%s' is declared between '%s' and '%s', not '%s' and '%s'.\n"
            "Contract source:\n"
            "- current zone: %s\n"
            "- relation state contract originates from state '%s'",
            state_name,
            state->data.zone_state.left_or_target_slot_name,
            state->data.zone_state.right_slot_name,
            left_slot_name,
            right_slot_name,
            zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
            state_name);
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "'HasZone' expects exactly 1 argument, got %llu.\n"
            "Reason:\n"
            "- world zone/state observability requires a single zone-slot or world-state name\n"
            "Fix:\n"
            "- call HasZone(zoneOrState)\n"
            "- or remove the extra argument(s)",
            (unsigned long long) call->data.call.arg_count);
        return TYPE_BOOL;
    }

    world = ctx->current_world;
    if (world == NULL || world->type != AST_WORLD_DECL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
            "HasZone(...) expects a zone/state identifier or string literal.\n"
            "Reason:\n"
            "- world observability queries resolve names, not arbitrary expressions\n"
            "Fix:\n"
            "- pass a zone/state identifier\n"
            "- or pass a string literal with the declared name");
        return TYPE_BOOL;
    }

    if (name == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, arg,
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

    semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, arg,
        "Unknown world zone/state '%s' in HasZone(...).\n"
        "Reason:\n"
        "- current world '%s' does not declare a zone slot or state named '%s'\n"
        "Contract source:\n"
        "- current world: %s\n"
        "- world observability path is HasZone(%s)\n"
        "Fix:\n"
        "- use a declared world zone slot/state name\n"
        "- or declare '%s' on the current world",
        name,
        world->data.world_decl.name != NULL ? world->data.world_decl.name : "<world>",
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "'%s' expects exactly 2 argument(s), got %llu.\n"
            "Reason:\n"
            "- %s needs a world zone slot and a zone %s name\n"
            "Fix:\n"
            "- call %s(zoneSlot, %sName)\n"
            "- or remove the extra argument(s)",
            builtin_name, (unsigned long long) call->data.call.arg_count,
            builtin_name, detail_label,
            builtin_name, detail_label);
        return TYPE_BOOL;
    }

    world = ctx->current_world;
    if (world == NULL || world->type != AST_WORLD_DECL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID,
            PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION,
            call,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID,
            PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION,
            call,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, zone_arg,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, detail_arg,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID,
            PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION,
            call,
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
        semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, zone_arg,
            "Unknown world zone slot '%s' in %s(...).\n"
            "Reason:\n"
            "- current world '%s' does not declare a zone slot named '%s'\n"
            "Contract source:\n"
            "- current world: %s\n"
            "- world embedding query path is %s(%s, %s)\n"
            "Fix:\n"
            "- use a declared world zone slot name\n"
            "- or declare zone slot '%s' on the current world",
            zone_slot_name, builtin_name,
            world->data.world_decl.name != NULL ? world->data.world_decl.name : "<world>",
            zone_slot_name,
            world->data.world_decl.name != NULL ? world->data.world_decl.name : "<world>",
            builtin_name, zone_slot_name, detail_name,
            zone_slot_name);
        return TYPE_BOOL;
    }

    detail_decl = resolver(zone_decl, detail_name);
    if (detail_decl == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, detail_arg,
            "Unknown zone %s '%s' in %s(%s, ...).\n"
            "Reason:\n"
            "- zone '%s' does not declare a %s named '%s'\n"
            "Contract source:\n"
            "- current world: %s\n"
            "- embedded zone slot '%s' resolves to zone '%s'\n"
            "- world embedding query path is %s(%s, %s)\n"
            "Fix:\n"
            "- use a declared zone %s name\n"
            "- or declare '%s' on zone '%s'",
            detail_label, detail_name, builtin_name, zone_slot_name,
            zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
            detail_label, detail_name,
            world->data.world_decl.name != NULL ? world->data.world_decl.name : "<world>",
            zone_slot_name,
            zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
            builtin_name, zone_slot_name, detail_name,
            detail_label,
            detail_name,
            zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
        return TYPE_BOOL;
    }

    return TYPE_BOOL;
}
