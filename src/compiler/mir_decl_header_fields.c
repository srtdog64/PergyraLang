#include "mir_decl_header_fields.h"

#include "mir_decl_header_shape.h"
#include "mir_type_helpers.h"

#include <stdint.h>
#include <stdlib.h>

static void
mir_ability_ref_clear(MIRAbilityRef *ref)
{
    if (ref == NULL)
        return;
    free(ref->base_name);
    if (ref->actual_arg_type_names != NULL) {
        for (size_t i = 0; i < ref->actual_arg_count; i++)
            free(ref->actual_arg_type_names[i]);
        free(ref->actual_arg_type_names);
    }
    ref->base_name = NULL;
    ref->actual_arg_count = 0;
    ref->actual_arg_type_names = NULL;
}

static char *
mir_capture_generic_actual_type_name(GenericParam *param)
{
    ASTNode *constraint;
    const char *name;

    if (param == NULL)
        return NULL;
    constraint = ast_generic_param_constraint(param);
    if (constraint != NULL)
        return mir_capture_type_name(constraint, NULL);
    name = ast_generic_param_name(param);
    return mir_capture_type_name(NULL, name);
}

static bool
mir_ability_ref_capture(MIRAbilityRef *ref, ASTNode *ability)
{
    GenericParams *actuals;
    size_t actual_count;

    if (ref == NULL)
        return false;
    ref->base_name = NULL;
    ref->actual_arg_count = 0;
    ref->actual_arg_type_names = NULL;

    if (ability == NULL || ability->type != AST_TYPE
        || ast_type_name(ability) == NULL) {
        return false;
    }

    ref->base_name = mir_capture_type_name(NULL, ast_type_name(ability));
    if (ref->base_name == NULL)
        return false;

    actuals = ast_type_generic_args(ability);
    actual_count = ast_generic_param_count(actuals);
    if (actual_count == 0)
        return true;
    if (actual_count > SIZE_MAX / sizeof(char *))
        return false;

    ref->actual_arg_type_names = calloc(actual_count, sizeof(char *));
    if (ref->actual_arg_type_names == NULL)
        return false;
    ref->actual_arg_count = actual_count;
    for (size_t i = 0; i < actual_count; i++) {
        ref->actual_arg_type_names[i] =
            mir_capture_generic_actual_type_name(
                ast_generic_param_at(actuals, i));
        if (ref->actual_arg_type_names[i] == NULL)
            return false;
    }
    return true;
}

static void
mir_decl_field_metadata_clear(MIRDeclField *meta)
{
    if (meta == NULL)
        return;
    free(meta->type_name);
    if (meta->required_ability_refs != NULL) {
        for (size_t i = 0; i < meta->required_ability_ref_count; i++)
            mir_ability_ref_clear(&meta->required_ability_refs[i]);
        free(meta->required_ability_refs);
    }
    meta->type_name = NULL;
    meta->required_ability_refs = NULL;
    meta->required_ability_ref_count = 0;
}

void
mir_decl_header_free_fields(MIRDeclHeader *header)
{
    if (header == NULL || header->field_metadata == NULL)
        return;
    for (size_t i = 0; i < header->field_metadata_count; i++)
        mir_decl_field_metadata_clear(&header->field_metadata[i]);
    free(header->field_metadata);
    header->field_metadata = NULL;
    header->field_metadata_count = 0;
}

static void
mir_decl_field_metadata_init(MIRDeclField *meta,
                             const MIRDeclHeader *header,
                             ASTNode *source_ast,
                             const char *name,
                             ASTNode *type,
                             const char *type_name,
                             MIRDeclFieldKind kind)
{
    if (meta == NULL || header == NULL)
        return;

    meta->source_ast = source_ast;
    meta->owner_name = header->name;
    meta->name = name;
    meta->type = type;
    meta->type_name = mir_capture_type_name(type, type_name);
    meta->kind = kind;
    meta->required_ability_ref_count = 0;
    meta->required_ability_refs = NULL;
}

static void
mir_decl_field_metadata_init_class(MIRDeclField *meta,
                                   const MIRDeclHeader *header,
                                   ClassField *field)
{
    if (meta == NULL || field == NULL)
        return;
    mir_decl_field_metadata_init(
        meta, header, NULL, field->name, field->type, NULL,
        MIR_DECL_FIELD_CLASS);
    meta->is_subject_like = field->is_vessel_field;
}

static void
mir_decl_field_metadata_init_shared(MIRDeclField *meta,
                                    const MIRDeclHeader *header,
                                    ASTNode *field)
{
    if (meta == NULL || field == NULL)
        return;
    mir_decl_field_metadata_init(
        meta, header, field, ast_party_shared_name(field),
        ast_party_shared_type(field), NULL, MIR_DECL_FIELD_SHARED);
}

static void
mir_decl_field_metadata_init_role_slot(MIRDeclField *meta,
                                       const MIRDeclHeader *header,
                                       ASTNode *slot)
{
    if (meta == NULL || slot == NULL)
        return;
    mir_decl_field_metadata_init(
        meta, header, slot, ast_role_slot_name(slot), NULL, NULL,
        MIR_DECL_FIELD_ROLE_SLOT);
    meta->is_dynamic = ast_role_slot_is_dynamic(slot);
    {
        size_t ability_count = ast_role_slot_required_ability_count(slot);
        if (ability_count > 0) {
            meta->required_ability_refs =
                calloc(ability_count, sizeof(MIRAbilityRef));
            if (meta->required_ability_refs != NULL) {
                for (size_t a = 0; a < ability_count; a++) {
                    ASTNode *ability =
                        ast_role_slot_required_ability(slot, a);
                    (void) mir_ability_ref_capture(
                        &meta->required_ability_refs[a], ability);
                }
                meta->required_ability_ref_count = ability_count;
            }
        }
    }
}

static void
mir_decl_field_metadata_init_roster_slot(MIRDeclField *meta,
                                         const MIRDeclHeader *header,
                                         ASTNode *slot)
{
    if (meta == NULL || slot == NULL)
        return;
    mir_decl_field_metadata_init(
        meta, header, slot, ast_roster_slot_name(slot), NULL,
        ast_roster_slot_party_type(slot), MIR_DECL_FIELD_ROSTER_SLOT);
}

static void
mir_decl_field_metadata_init_world_slot(MIRDeclField *meta,
                                        const MIRDeclHeader *header,
                                        ASTNode *slot,
                                        MIRDeclFieldKind kind)
{
    const char *name;
    const char *type_name;

    if (meta == NULL || slot == NULL)
        return;
    if (kind == MIR_DECL_FIELD_WORLD_ROSTER_SLOT) {
        name = ast_world_roster_slot_name(slot);
        type_name = ast_world_roster_type_name(slot);
    } else {
        name = ast_world_zone_slot_name(slot);
        type_name = ast_world_zone_type_name(slot);
    }
    mir_decl_field_metadata_init(meta, header, slot, name, NULL, type_name, kind);
}

static void
mir_decl_field_metadata_init_domain_slot(MIRDeclField *meta,
                                         const MIRDeclHeader *header,
                                         ASTNode *slot)
{
    if (meta == NULL || slot == NULL)
        return;
    mir_decl_field_metadata_init(
        meta, header, slot, ast_domain_slot_name(slot),
        ast_domain_slot_type(slot),
        NULL,
        MIR_DECL_FIELD_DOMAIN_SLOT);
    meta->is_subject_like = ast_domain_slot_is_subject(slot);
    meta->is_tobject_like = ast_domain_slot_is_tobject(slot);
    meta->is_binding_like = ast_domain_slot_is_binding(slot);
}

static void
mir_decl_field_metadata_init_zone_layer(MIRDeclField *meta,
                                        const MIRDeclHeader *header,
                                        ASTNode *slot)
{
    if (meta == NULL || slot == NULL)
        return;
    mir_decl_field_metadata_init(
        meta, header, slot, ast_zone_layer_slot_name(slot), NULL,
        ast_zone_layer_slot_layer_type(slot), MIR_DECL_FIELD_ZONE_LAYER_SLOT);
    meta->is_relation_layer = ast_zone_layer_slot_is_relation(slot);
    meta->is_pool_layer = ast_zone_layer_slot_is_pool(slot);
    meta->pool_capacity = ast_zone_layer_slot_pool_capacity(slot);
}

static bool
mir_decl_header_alloc_fields(MIRDeclHeader *header, size_t field_count)
{
    if (header == NULL)
        return false;

    header->field_count = field_count;
    header->field_metadata = NULL;
    header->field_metadata_count = 0;
    if (field_count == 0)
        return true;
    if (field_count > SIZE_MAX / sizeof(MIRDeclField))
        return false;
    header->field_metadata = calloc(field_count, sizeof(MIRDeclField));
    return header->field_metadata != NULL;
}

static void
mir_decl_header_append_shared_fields(MIRDeclHeader *header,
                                     ASTNode **fields,
                                     size_t count,
                                     size_t *out)
{
    if (header == NULL || out == NULL)
        return;
    for (size_t i = 0; fields != NULL && i < count; i++) {
        mir_decl_field_metadata_init_shared(
            &header->field_metadata[(*out)++], header, fields[i]);
    }
}

bool
mir_decl_header_set_fields(MIRDeclHeader *header, ASTNode *decl)
{
    size_t field_count;
    size_t out = 0;
    size_t count = 0;
    size_t shared_count = 0;
    ASTNode **nodes = NULL;
    ASTNode **shared = NULL;

    if (header == NULL)
        return false;
    field_count = mir_decl_header_ast_field_count(decl);
    if (!mir_decl_header_alloc_fields(header, field_count))
        return false;

    switch (decl != NULL ? decl->type : AST_PROGRAM) {
    case AST_CLASS_DECL: {
        ClassField **fields = ast_class_fields(decl, &count);
        for (size_t i = 0; fields != NULL && i < count; i++) {
            mir_decl_field_metadata_init_class(
                &header->field_metadata[out++], header, fields[i]);
        }
        break;
    }
    case AST_PARTY_DECL:
        for (size_t i = 0; i < ast_party_role_count(decl); i++) {
            mir_decl_field_metadata_init_role_slot(
                &header->field_metadata[out++], header,
                ast_party_role(decl, i));
        }
        shared = ast_party_shared_fields(decl, &shared_count);
        mir_decl_header_append_shared_fields(header, shared, shared_count, &out);
        break;
    case AST_ROSTER_DECL:
        for (size_t i = 0; i < ast_roster_party_count(decl); i++) {
            mir_decl_field_metadata_init_roster_slot(
                &header->field_metadata[out++], header,
                ast_roster_party(decl, i));
        }
        shared = ast_roster_shared_fields(decl, &shared_count);
        mir_decl_header_append_shared_fields(header, shared, shared_count, &out);
        break;
    case AST_WORLD_DECL:
        nodes = ast_world_rosters(decl, &count);
        for (size_t i = 0; nodes != NULL && i < count; i++) {
            mir_decl_field_metadata_init_world_slot(
                &header->field_metadata[out++], header, nodes[i],
                MIR_DECL_FIELD_WORLD_ROSTER_SLOT);
        }
        nodes = ast_world_zones(decl, &count);
        for (size_t i = 0; nodes != NULL && i < count; i++) {
            mir_decl_field_metadata_init_world_slot(
                &header->field_metadata[out++], header, nodes[i],
                MIR_DECL_FIELD_WORLD_ZONE_SLOT);
        }
        shared = ast_world_shared_fields(decl, &shared_count);
        mir_decl_header_append_shared_fields(header, shared, shared_count, &out);
        break;
    case AST_RELATION_DECL:
        nodes = ast_relation_slots(decl, &count);
        for (size_t i = 0; nodes != NULL && i < count; i++) {
            mir_decl_field_metadata_init_domain_slot(
                &header->field_metadata[out++], header, nodes[i]);
        }
        shared = ast_relation_shared_fields(decl, &shared_count);
        mir_decl_header_append_shared_fields(header, shared, shared_count, &out);
        break;
    case AST_EFFECT_DECL:
        nodes = ast_effect_slots(decl, &count);
        for (size_t i = 0; nodes != NULL && i < count; i++) {
            mir_decl_field_metadata_init_domain_slot(
                &header->field_metadata[out++], header, nodes[i]);
        }
        shared = ast_effect_shared_fields(decl, &shared_count);
        mir_decl_header_append_shared_fields(header, shared, shared_count, &out);
        break;
    case AST_ZONE_DECL:
        nodes = ast_zone_slots(decl, &count);
        for (size_t i = 0; nodes != NULL && i < count; i++) {
            mir_decl_field_metadata_init_domain_slot(
                &header->field_metadata[out++], header, nodes[i]);
        }
        nodes = ast_zone_layer_slots(decl, &count);
        for (size_t i = 0; nodes != NULL && i < count; i++) {
            mir_decl_field_metadata_init_zone_layer(
                &header->field_metadata[out++], header, nodes[i]);
        }
        shared = ast_zone_shared_fields(decl, &shared_count);
        mir_decl_header_append_shared_fields(header, shared, shared_count, &out);
        break;
    default:
        break;
    }

    header->field_metadata_count = out;
    return out == field_count;
}
