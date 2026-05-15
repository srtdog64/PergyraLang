#include <stdlib.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"

Type *
create_overlay_nominal_type(const char *name)
{
    Type *type = calloc(1, sizeof(Type));
    if (type == NULL)
        return TYPE_UNKNOWN;
    type->kind = TYPE_KIND_CLASS;
    type->nominal_flavor = TYPE_NOMINAL_CLASS;
    type->name = pergyra_strdup(name);
    return type;
}

size_t
overlay_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    switch (decl->type) {
    case AST_PARTY_DECL:
        return ast_party_shared_count(decl);
    case AST_ROSTER_DECL:
        return ast_roster_party_count(decl) + ast_roster_shared_count(decl);
    case AST_WORLD_DECL: {
        size_t roster_count;
        size_t zone_count;
        size_t shared_count;
        (void)ast_world_rosters(decl, &roster_count);
        (void)ast_world_zones(decl, &zone_count);
        (void)ast_world_shared_fields(decl, &shared_count);
        return roster_count + zone_count + shared_count;
    }
    case AST_ZONE_DECL: {
        size_t slot_count;
        size_t shared_count;
        (void)ast_zone_slots(decl, &slot_count);
        (void)ast_zone_shared_fields(decl, &shared_count);
        return slot_count + shared_count;
    }
    case AST_RELATION_DECL: {
        size_t slot_count;
        size_t shared_count;
        (void)ast_relation_slots(decl, &slot_count);
        (void)ast_relation_shared_fields(decl, &shared_count);
        return slot_count + shared_count;
    }
    case AST_EFFECT_DECL: {
        size_t slot_count;
        size_t shared_count;
        (void)ast_effect_slots(decl, &slot_count);
        (void)ast_effect_shared_fields(decl, &shared_count);
        return slot_count + shared_count;
    }
    default:
        return 0;
    }
}

ASTNode *
overlay_field_decl_at(ASTNode *decl, size_t index, const char **field_name_out)
{
    if (field_name_out != NULL)
        *field_name_out = NULL;
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_PARTY_DECL: {
        if (index < ast_party_shared_count(decl)) {
            ASTNode *shared = ast_party_shared(decl, index);
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    case AST_ROSTER_DECL: {
        size_t party_count = ast_roster_party_count(decl);
        if (index < party_count)
            return NULL;
        index -= party_count;
        if (index < ast_roster_shared_count(decl)) {
            ASTNode *shared = ast_roster_shared(decl, index);
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    case AST_WORLD_DECL: {
        size_t roster_count;
        size_t zone_count;
        size_t shared_count;
        ASTNode **shared_fields;
        (void)ast_world_rosters(decl, &roster_count);
        (void)ast_world_zones(decl, &zone_count);
        shared_fields = ast_world_shared_fields(decl, &shared_count);
        if (index < roster_count)
            return NULL;
        index -= roster_count;
        if (index < zone_count)
            return NULL;
        index -= zone_count;
        if (index < shared_count) {
            ASTNode *shared = shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    case AST_ZONE_DECL: {
        size_t slot_count;
        size_t shared_count;
        ASTNode **slots = ast_zone_slots(decl, &slot_count);
        ASTNode **shared_fields = ast_zone_shared_fields(decl, &shared_count);
        if (index < slot_count) {
            ASTNode *slot = slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = ast_domain_slot_name(slot);
            return ast_domain_slot_type(slot);
        }
        index -= slot_count;
        if (index < shared_count) {
            ASTNode *shared = shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    case AST_RELATION_DECL: {
        size_t slot_count;
        size_t shared_count;
        ASTNode **slots = ast_relation_slots(decl, &slot_count);
        ASTNode **shared_fields = ast_relation_shared_fields(decl, &shared_count);
        if (index < slot_count) {
            ASTNode *slot = slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = ast_domain_slot_name(slot);
            return ast_domain_slot_type(slot);
        }
        index -= slot_count;
        if (index < shared_count) {
            ASTNode *shared = shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    case AST_EFFECT_DECL: {
        size_t slot_count;
        size_t shared_count;
        ASTNode **slots = ast_effect_slots(decl, &slot_count);
        ASTNode **shared_fields = ast_effect_shared_fields(decl, &shared_count);
        if (index < slot_count) {
            ASTNode *slot = slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = ast_domain_slot_name(slot);
            return ast_domain_slot_type(slot);
        }
        index -= slot_count;
        if (index < shared_count) {
            ASTNode *shared = shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    default:
        break;
    }

    return NULL;
}
