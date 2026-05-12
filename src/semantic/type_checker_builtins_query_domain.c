#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_query_domain.h"

ASTNode *
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

ASTNode *
builtin_find_zone_layer_slot_local(ASTNode *zone, const char *slot_name)
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

ASTNode *
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

ASTNode *
builtin_resolve_world_zone_decl_local(SemanticContext *ctx, ASTNode *world,
                                      const char *slot_name)
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

ASTNode *
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

ASTNode *
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

ASTNode *
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

ASTNode *
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

bool
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

int
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
