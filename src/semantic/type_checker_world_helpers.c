#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include "type_checker_world_internal.h"

#include <stdlib.h>
#include <string.h>

Type *
world_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_type_ref_or_materialize(ctx, type_ref);
}

Type *
world_resolve_domain_slot_type(ASTNode *slot, SemanticContext *ctx)
{
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return TYPE_UNKNOWN;
    return world_resolve_type_ref(slot->data.domain_slot.type, ctx);
}

ASTNode *
find_world_zone_slot_local(ASTNode *world, const char *slot_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.zone_count; i++) {
        ASTNode *zone = world->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

ASTNode *
find_world_state_local(ASTNode *world, const char *state_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.state_count; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

ASTNode *
find_world_state_before_local(ASTNode *world, const char *state_name, size_t limit)
{
    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    if (limit > world->data.world_decl.state_count)
        limit = world->data.world_decl.state_count;

    for (size_t i = 0; i < limit; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
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
    if (slot == NULL || slot->data.world_zone.zone_type == NULL)
        return NULL;

    return find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
        slot->data.world_zone.zone_type);
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
        && state->data.world_state.source_kind == WORLD_STATE_SOURCE_ZONE) {
        return state->data.world_state.zone_slot_name;
    }

    return NULL;
}

ASTNode *
find_zone_layer_slot_local(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

ASTNode *
find_zone_state_decl_local(ASTNode *zone, const char *state_name)
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
                *zone_slot_name_out = zone->data.world_zone.slot_name;
            return true;
        }
    }
    if (state == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, site,
            "World %s references unknown zone/state '%s'",
            action_name, state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (state->data.world_state.source_kind != WORLD_STATE_SOURCE_ZONE) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_WORLD_CONTRACT_INVALID, PGY_CAUSE_WORLD_CONTRACT, PGY_FIX_ALIGN_WORLD_ZONE_STATE_COMPOSITION, site,
            "World %s cannot target derived state '%s'; use the underlying zone slot or a plain 'state name: zone slot' alias",
            action_name, state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = state->data.world_state.zone_slot_name;
    return true;
}
