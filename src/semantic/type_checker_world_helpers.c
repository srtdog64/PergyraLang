#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

#include "type_checker_world_internal.h"

#include <stdlib.h>
#include <string.h>

Type *
world_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved;

    if (type_ref == NULL || ctx == NULL)
        return TYPE_UNKNOWN;
    resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

Type *
world_resolve_domain_slot_type(ASTNode *slot, SemanticContext *ctx)
{
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return TYPE_UNKNOWN;
    return world_resolve_type_ref(ast_domain_slot_type(slot), ctx);
}

ASTNode *
find_world_zone_slot_local(ASTNode *world, const char *slot_name)
{
    ASTNode **zones;
    size_t zone_count;

    if (world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;
    zones = ast_world_zones(world, &zone_count);

    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        const char *zone_slot_name = ast_world_zone_slot_name(zone);
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone_slot_name != NULL
            && strcmp(zone_slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

ASTNode *
find_world_state_local(ASTNode *world, const char *state_name)
{
    ASTNode **states;
    size_t state_count;

    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;
    states = ast_world_states(world, &state_count);

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

ASTNode *
find_world_state_before_local(ASTNode *world, const char *state_name, size_t limit)
{
    ASTNode **states;
    size_t state_count;

    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;
    states = ast_world_states(world, &state_count);

    if (limit > state_count)
        limit = state_count;

    for (size_t i = 0; i < limit; i++) {
        ASTNode *state = states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && ast_world_state_name(state) != NULL
            && strcmp(ast_world_state_name(state), state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

ASTNode *
resolve_world_zone_decl_local(ASTNode *world, SemanticContext *ctx, const char *slot_name)
{
    ASTNode *slot;
    if (world == NULL || world->type != AST_WORLD_DECL
        || ctx == NULL || slot_name == NULL) {
        return NULL;
    }

    slot = find_world_zone_slot_local(world, slot_name);
    const char *zone_type = ast_world_zone_type_name(slot);
    if (slot == NULL || zone_type == NULL)
        return NULL;

    return find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL, zone_type);
}

const char *
resolve_world_plain_zone_input_name(ASTNode *world, const char *input_name)
{
    ASTNode *state;

    if (world == NULL || world->type != AST_WORLD_DECL || input_name == NULL)
        return NULL;

    if (find_world_zone_slot_local(world, input_name) != NULL)
        return input_name;

    state = find_world_state_local(world, input_name);
    if (state != NULL && state->type == AST_WORLD_STATE
        && ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ZONE) {
        return ast_world_state_zone_slot_name(state);
    }

    return NULL;
}

ASTNode *
find_zone_layer_slot_local(ASTNode *zone, const char *slot_name)
{
    ASTNode **layer_slots;
    size_t layer_slot_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;
    layer_slots = ast_zone_layer_slots(zone, &layer_slot_count);

    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && ast_zone_layer_slot_name(slot) != NULL
            && strcmp(ast_zone_layer_slot_name(slot), slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

ASTNode *
find_zone_state_decl_local(ASTNode *zone, const char *state_name)
{
    ASTNode **states;
    size_t state_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;
    states = ast_zone_states(zone, &state_count);

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

bool
resolve_world_zone_state(ASTNode *world, ASTNode *site, const char *state_name,
                         SemanticContext *ctx, const char *action_name,
                         const char **zone_slot_name_out)
{
    ASTNode *state = find_world_state_local(world, state_name);
    if (state == NULL) {
        ASTNode *zone = find_world_zone_slot_local(world, state_name);
        if (zone != NULL) {
            if (zone_slot_name_out != NULL)
                *zone_slot_name_out = ast_world_zone_slot_name(zone);
            return true;
        }
    }
    if (state == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, site,
            "World %s references unknown zone/state '%s'",
            action_name, state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (ast_world_state_source_kind(state) != WORLD_STATE_SOURCE_ZONE) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, site,
            "World %s cannot target derived state '%s'; use the underlying zone slot or a plain 'state name: zone slot' alias",
            action_name, state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = ast_world_state_zone_slot_name(state);
    return true;
}
