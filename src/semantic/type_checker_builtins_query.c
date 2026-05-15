/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker built-in dispatch and stdlib helpers
 */

#include <string.h>
#include "../common/string_compat.h"
#include "diag_codes.h"
#include "type_checker_channel_transport_internal.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"

bool
check_call_arity(ASTNode *expr, size_t expected, const char *name,
                 SemanticContext *ctx)
{
    if (ast_call_arg_count(expr) != expected) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
            "'%s' expects %llu argument(s), got %llu",
            name,
            (unsigned long long) expected,
            (unsigned long long) ast_call_arg_count(expr));
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
            && ast_identifier_name(cursor) != NULL
            && identifier_is_borrowed_boundary_param(cursor, ctx)) {
            return ast_identifier_name(cursor);
        }
        if (cursor->type == AST_MEMBER_ACCESS) {
            cursor = ast_member_object(cursor);
            continue;
        }
        if (cursor->type == AST_ARRAY_ACCESS) {
            cursor = ast_array_access_array(cursor);
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

    if (semantic_reject_active_slot_owner_escape(
            value_expr, ctx,
            container_kind != NULL ? container_kind : "container",
            container_name != NULL ? container_name : "<container store>")) {
        return;
    }

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

#include "type_checker_builtins_query_domain.h"

Type *
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

    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, call,
            "'HasProjection' expects exactly 1 argument, got %llu",
            (unsigned long long) ast_call_arg_count(call));
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
        host_name = ast_relation_name(host);
        refreshes = ast_relation_refreshes(host, &refresh_count);
    } else if (host->type == AST_EFFECT_DECL) {
        host_name = ast_effect_name(host);
        refreshes = ast_effect_refreshes(host, &refresh_count);
    } else if (host->type == AST_ZONE_DECL) {
        host_name = ast_zone_name(host);
        refreshes = ast_zone_refreshes(host, &refresh_count);
    }

    arg = ast_call_argument(call, 0);
    if (arg == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
            "HasProjection(...) requires an object/tobject slot name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        slot_name = ast_identifier_name(arg);
    } else if (arg->type == AST_STRING) {
        slot_name = ast_string_value(arg);
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

Type *
type_check_has_layer(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *zone;
    ASTNode *arg;
    const char *slot_name = NULL;
    ASTNode *layer_slot;

    if (ast_call_arg_count(call) != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
            "'HasLayer' expects exactly 1 argument, got %llu",
            (unsigned long long) ast_call_arg_count(call));
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

    arg = ast_call_argument(call, 0);
    if (arg == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
            "HasLayer(...) requires a relation/effect slot name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        slot_name = ast_identifier_name(arg);
    } else if (arg->type == AST_STRING) {
        slot_name = ast_string_value(arg);
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

    layer_slot = builtin_find_zone_layer_slot_local(zone, slot_name);
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
            ast_zone_name(zone) != NULL ? ast_zone_name(zone) : "<zone>",
            slot_name,
            slot_name);
        return TYPE_BOOL;
    }

    return TYPE_BOOL;
}

Type *
type_check_has_state(ASTNode *call, SemanticContext *ctx)
{
    ASTNode *zone;
    ASTNode *arg;
    ASTNode *state = NULL;
    const char *state_name = NULL;
    const char *slot_name = NULL;
    const char *left_slot_name = NULL;
    const char *right_slot_name = NULL;
    ASTNode **states = NULL;
    size_t state_count = 0;

    if (ast_call_arg_count(call) < 1 || ast_call_arg_count(call) > 3) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID,
            PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
            call,
            "'HasState' expects 1 to 3 argument(s), got %llu",
            (unsigned long long) ast_call_arg_count(call));
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
    states = ast_zone_states(zone, &state_count);

    arg = ast_call_argument(call, 0);
    if (arg == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
            "HasState(...) requires a zone state name");
        return TYPE_BOOL;
    }

    if (arg->type == AST_IDENTIFIER) {
        state_name = ast_identifier_name(arg);
    } else if (arg->type == AST_STRING) {
        state_name = ast_string_value(arg);
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

    for (size_t i = 0; i < state_count; i++) {
        state = states[i];
        if (state != NULL
            && state->type == AST_ZONE_STATE
            && ast_zone_state_name(state) != NULL
            && strcmp(ast_zone_state_name(state), state_name) == 0) {
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
            ast_zone_name(zone) != NULL ? ast_zone_name(zone) : "<zone>",
            state_name,
            state_name);
        return TYPE_BOOL;
    }

    if (ast_call_arg_count(call) == 1)
        return TYPE_BOOL;

    if (ast_call_argument(call, 1) == NULL
        || ast_call_argument(call, 1)->type != AST_IDENTIFIER
        || ast_identifier_name(ast_call_argument(call, 1)) == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, ast_call_argument(call, 1),
            "HasState(...) slot arguments must be zone slot identifiers");
        return TYPE_BOOL;
    }
    if (find_zone_domain_slot_local(zone,
            ast_identifier_name(ast_call_argument(call, 1))) == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID,
            PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
            ast_call_argument(call, 1),
            "Unknown zone slot '%s' in HasState(...)",
            ast_identifier_name(ast_call_argument(call, 1)));
        return TYPE_BOOL;
    }

    if (!ast_zone_state_is_relation(state)) {
        slot_name = ast_identifier_name(ast_call_argument(call, 1));
        if (ast_call_arg_count(call) != 2) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
                "Effect state '%s' in HasState(...) accepts at most one zone slot target.\n"
                "Contract source:\n"
                "- current zone: %s\n"
                "- state '%s' targets slot '%s'",
                state_name,
                ast_zone_name(zone) != NULL ? ast_zone_name(zone) : "<zone>",
                state_name,
                ast_zone_state_left_or_target_slot_name(state) != NULL
                    ? ast_zone_state_left_or_target_slot_name(state) : "<slot>");
            return TYPE_BOOL;
        }
        if (strcmp(slot_name, ast_zone_state_left_or_target_slot_name(state)) != 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID,
                PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
                ast_call_argument(call, 1),
                "State '%s' is declared on slot '%s', not '%s'.\n"
                "Contract source:\n"
                "- current zone: %s\n"
                "- effect state contract originates from state '%s'",
                state_name,
                ast_zone_state_left_or_target_slot_name(state),
                slot_name,
                ast_zone_name(zone) != NULL ? ast_zone_name(zone) : "<zone>",
                state_name);
        }
        return TYPE_BOOL;
    }

    if (ast_call_arg_count(call) != 3) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, call,
            "Relation state '%s' in HasState(...) requires exactly two endpoint slots.\n"
            "Contract source:\n"
            "- current zone: %s\n"
            "- relation state '%s' is declared between '%s' and '%s'",
            state_name,
            ast_zone_name(zone) != NULL ? ast_zone_name(zone) : "<zone>",
            state_name,
            ast_zone_state_left_or_target_slot_name(state) != NULL
                ? ast_zone_state_left_or_target_slot_name(state) : "<left>",
            ast_zone_state_right_slot_name(state) != NULL
                ? ast_zone_state_right_slot_name(state) : "<right>");
        return TYPE_BOOL;
    }

    if (ast_call_argument(call, 2) == NULL
        || ast_call_argument(call, 2)->type != AST_IDENTIFIER
        || ast_identifier_name(ast_call_argument(call, 2)) == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID, PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST, ast_call_argument(call, 2),
            "HasState(...) relation endpoint arguments must be zone slot identifiers");
        return TYPE_BOOL;
    }
    if (find_zone_domain_slot_local(zone,
            ast_identifier_name(ast_call_argument(call, 2))) == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID,
            PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
            ast_call_argument(call, 2),
            "Unknown zone slot '%s' in HasState(...)",
            ast_identifier_name(ast_call_argument(call, 2)));
        return TYPE_BOOL;
    }

    left_slot_name = ast_identifier_name(ast_call_argument(call, 1));
    right_slot_name = ast_identifier_name(ast_call_argument(call, 2));
    if (strcmp(left_slot_name, ast_zone_state_left_or_target_slot_name(state)) != 0
        || strcmp(right_slot_name, ast_zone_state_right_slot_name(state)) != 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PREDICATE_ARGS_INVALID,
            PGY_CAUSE_PREDICATE_ARGS, PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
            call,
            "State '%s' is declared between '%s' and '%s', not '%s' and '%s'.\n"
            "Contract source:\n"
            "- current zone: %s\n"
            "- relation state contract originates from state '%s'",
            state_name,
            ast_zone_state_left_or_target_slot_name(state),
            ast_zone_state_right_slot_name(state),
            left_slot_name,
            right_slot_name,
            ast_zone_name(zone) != NULL ? ast_zone_name(zone) : "<zone>",
            state_name);
    }
    return TYPE_BOOL;
}


