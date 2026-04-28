/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * World query builtin predicates.
 */

#include <stdio.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_query_domain.h"


Type *
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

    zone_decl = builtin_resolve_world_zone_decl_local(ctx, world, zone_slot_name);
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

Type *
type_check_has_zone_projection_builtin(ASTNode *call, SemanticContext *ctx)
{
    return type_check_has_world_zone_detail(call, ctx, "HasZoneProjection",
        find_zone_projection_slot_local, "projection slot");
}

Type *
type_check_has_zone_layer_builtin(ASTNode *call, SemanticContext *ctx)
{
    return type_check_has_world_zone_detail(call, ctx, "HasZoneLayer",
        builtin_find_zone_layer_slot_local, "layer slot");
}

Type *
type_check_has_zone_state_builtin(ASTNode *call, SemanticContext *ctx)
{
    return type_check_has_world_zone_detail(call, ctx, "HasZoneState",
        find_zone_state_decl_local_builtin, "state");
}
