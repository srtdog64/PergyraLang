#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_query_domain.h"
#include "parser/ast_api.h"

ASTNode *
find_zone_domain_slot_local(ASTNode *zone, const char *slot_name)
{
    ASTNode **slots;
    size_t slot_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;
    slots = ast_zone_slots(zone, &slot_count);

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *candidate_name = ast_domain_slot_name(slot);
        if (slot != NULL
            && slot->type == AST_DOMAIN_SLOT
            && candidate_name != NULL
            && strcmp(candidate_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

ASTNode *
builtin_find_zone_layer_slot_local(ASTNode *zone, const char *slot_name)
{
    ASTNode **layer_slots;
    size_t layer_slot_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;
    layer_slots = ast_zone_layer_slots(zone, &layer_slot_count);

    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && ast_zone_layer_slot_name(slot) != NULL
            && strcmp(ast_zone_layer_slot_name(slot), slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

ASTNode *
find_domain_projection_slot_local(ASTNode **slots, size_t slot_count,
                                  ASTNode **refreshes, size_t refresh_count,
                                  const char *slot_name);

static ASTNode *
find_world_zone_slot_local_builtin(ASTNode *world, const char *slot_name)
{
    ASTNode **zones;
    size_t zone_count;

    if (world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;
    zones = ast_world_zones(world, &zone_count);

    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        const char *zone_slot_name = ast_world_zone_slot_name(zone);
        if (zone_slot_name != NULL && strcmp(zone_slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

ASTNode *
builtin_resolve_world_zone_decl_local(SemanticContext *ctx, ASTNode *world,
                                      const char *slot_name)
{
    ASTNode *zone_slot;

    if (ctx == NULL || world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    zone_slot = find_world_zone_slot_local_builtin(world, slot_name);
    const char *zone_type_name = ast_world_zone_type_name(zone_slot);
    if (zone_type_name == NULL)
        return NULL;

    return semantic_find_zone_decl_by_name(ctx, zone_type_name);
}

ASTNode *
find_zone_projection_slot_local(ASTNode *zone, const char *slot_name)
{
    ASTNode **slots;
    ASTNode **refreshes;
    size_t slot_count;
    size_t refresh_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL)
        return NULL;
    slots = ast_zone_slots(zone, &slot_count);
    refreshes = ast_zone_refreshes(zone, &refresh_count);
    return find_domain_projection_slot_local(slots, slot_count, refreshes,
                                             refresh_count, slot_name);
}

ASTNode *
find_zone_state_decl_local_builtin(ASTNode *zone, const char *state_name)
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

ASTNode *
find_domain_projection_slot_local(ASTNode **slots, size_t slot_count,
                                  ASTNode **refreshes, size_t refresh_count,
                                  const char *slot_name)
{
    if (slots == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *candidate_name = ast_domain_slot_name(slot);
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || candidate_name == NULL
            || strcmp(candidate_name, slot_name) != 0) {
            continue;
        }
        if (!ast_domain_slot_is_subject(slot)) {
            for (size_t j = 0; j < refresh_count; j++) {
                ASTNode *refresh = refreshes[j];
                if (refresh != NULL && refresh->type == AST_ZONE_REFRESH
                    && ast_zone_refresh_object_slot_name(refresh) != NULL
                    && strcmp(ast_zone_refresh_object_slot_name(refresh),
                              slot_name) == 0) {
                    return slot;
                }
            }
            if (ast_domain_slot_is_tobject(slot))
                return slot;
        }
        return NULL;
    }

    return NULL;
}

ASTNode *
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
        ASTNode **slots;
        size_t slot_count;

        slots = ast_relation_slots(ctx->current_relation, &slot_count);
        if (label_out != NULL)
            *label_out = "relation";
        if (slots_out != NULL)
            *slots_out = slots;
        if (slot_count_out != NULL)
            *slot_count_out = slot_count;
        return ctx->current_relation;
    }

    if (ctx->current_effect != NULL
        && ctx->current_effect->type == AST_EFFECT_DECL) {
        ASTNode **slots;
        size_t slot_count;

        slots = ast_effect_slots(ctx->current_effect, &slot_count);
        if (label_out != NULL)
            *label_out = "effect";
        if (slots_out != NULL)
            *slots_out = slots;
        if (slot_count_out != NULL)
            *slot_count_out = slot_count;
        return ctx->current_effect;
    }

    if (ctx->current_zone != NULL && ctx->current_zone->type == AST_ZONE_DECL) {
        ASTNode **slots;
        size_t slot_count;

        slots = ast_zone_slots(ctx->current_zone, &slot_count);
        if (label_out != NULL)
            *label_out = "zone";
        if (slots_out != NULL)
            *slots_out = slots;
        if (slot_count_out != NULL)
            *slot_count_out = slot_count;
        return ctx->current_zone;
    }
    return NULL;
}

bool
decl_is_subject_nominal(ASTNode *decl)
{
    return (decl != NULL
            && decl->type == AST_CLASS_DECL
            && !ast_class_is_struct(decl)
            && ast_class_nominal_kind(decl) == NOMINAL_DECL_SUBJECT);
}

static size_t
projection_source_field_count_local(ASTNode *decl)
{
    size_t field_count = 0;

    if (decl == NULL)
        return 0;
    if (decl->type == AST_CLASS_DECL) {
        (void) ast_class_fields(decl, &field_count);
        return field_count;
    }
    return 0;
}

static ClassField *
projection_source_field_at_local(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        size_t field_count = 0;
        ClassField **fields = ast_class_fields(decl, &field_count);
        if (index < field_count && fields != NULL)
            return fields[index];
        return NULL;
    }
    return NULL;
}

static int
resolve_projection_source_field_type_rec(ASTNode *source_decl,
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
    if (ctx == NULL || source_decl == NULL || field_name == NULL || depth > 8)
        return 0;

    field_count = projection_source_field_count_local(source_decl);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at_local(source_decl, i);
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            if (field_type_out != NULL)
                *field_type_out = field->type != NULL
                    ? projection_resolve_type_ref(field->type, ctx)
                    : TYPE_UNKNOWN;
            return 1;
        }
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = projection_source_field_at_local(source_decl, i);
        ASTNode *vessel_decl;
        Type *nested_type = NULL;
        int nested_status;
        const char *field_type_name = field != NULL ? ast_type_name(field->type) : NULL;

        if (field == NULL || !field->is_vessel_field
            || field_type_name == NULL) {
            continue;
        }

        vessel_decl = semantic_find_class_decl_by_name(ctx, field_type_name);
        if (vessel_decl == NULL || ast_class_nominal_kind(vessel_decl) != NOMINAL_DECL_VESSEL)
            continue;

        nested_status = resolve_projection_source_field_type_rec(
            vessel_decl, field_name, depth + 1, ctx, &nested_type);
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

int
semantic_resolve_projection_source_field_type(SemanticContext *ctx,
                                              ASTNode *source_decl,
                                              const char *field_name,
                                              Type **field_type_out)
{
    return resolve_projection_source_field_type_rec(
        source_decl, field_name, 0, ctx, field_type_out);
}
