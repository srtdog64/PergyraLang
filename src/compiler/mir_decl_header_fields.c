#include "mir_decl_header_fields.h"

#include "mir_ability_ref.h"
#include "mir_decl_header_shape.h"
#include "mir_type_helpers.h"
#include "decl_field_model.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static void
mir_decl_field_claim_clear(MIRDeclFieldClaim *claim)
{
    if (claim == NULL)
        return;
    free(claim->inner_type_name);
    claim->inner_type_name = NULL;
}

void
mir_decl_header_free_fields(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    for (size_t i = 0; header->field_metadata != NULL
         && i < header->field_metadata_count; i++) {
        mir_decl_field_metadata_clear(&header->field_metadata[i]);
    }
    free(header->field_metadata);
    header->field_metadata = NULL;
    header->field_metadata_count = 0;
    for (size_t i = 0; header->field_claim_metadata != NULL
         && i < header->field_claim_metadata_count; i++) {
        mir_decl_field_claim_clear(&header->field_claim_metadata[i]);
    }
    free(header->field_claim_metadata);
    header->field_claim_metadata = NULL;
    header->field_claim_metadata_count = 0;
}

static void
mir_decl_field_metadata_init(MIRDeclField *meta,
                             const MIRDeclHeader *header,
                             const char *name,
                             ASTNode *type,
                             const char *type_name,
                             MIRDeclFieldKind kind)
{
    if (meta == NULL || header == NULL)
        return;

    meta->owner_name = header->name;
    meta->name = name;
    meta->type = type;
    meta->initializer = NULL;
    meta->type_name = mir_capture_type_name(type, type_name);
    meta->kind = kind;
    meta->required_ability_ref_count = 0;
    meta->required_ability_refs = NULL;
}

/* F2 (docs/144) Phase 4: the MIR class-field builder consumes the pre-semantic
   PgyDeclField model, so class-field shape has one owner shared with semantic
   instead of each layer re-reading ast_class_fields. (Class fields carry no
   initializer here, so the model's syntactic facts are sufficient.) */
static void
mir_decl_field_metadata_init_class(MIRDeclField *meta,
                                   const MIRDeclHeader *header,
                                   PgyDeclField field)
{
    if (meta == NULL || field.name == NULL)
        return;
    mir_decl_field_metadata_init(
        meta, header, field.name, field.type_ast, NULL,
        MIR_DECL_FIELD_CLASS);
    meta->is_subject_like = field.is_vessel_field;
}

static void
mir_decl_field_metadata_init_shared(MIRDeclField *meta,
                                    const MIRDeclHeader *header,
                                    ASTNode *field)
{
    if (meta == NULL || field == NULL)
        return;
    mir_decl_field_metadata_init(
        meta, header, ast_party_shared_name(field),
        ast_party_shared_type(field), NULL, MIR_DECL_FIELD_SHARED);
    meta->initializer = ast_party_shared_initializer(field);
}

static void
mir_decl_field_metadata_init_role_slot(MIRDeclField *meta,
                                       const MIRDeclHeader *header,
                                       ASTNode *slot)
{
    if (meta == NULL || slot == NULL)
        return;
    mir_decl_field_metadata_init(
        meta, header, ast_role_slot_name(slot), NULL, NULL,
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
        meta, header, ast_roster_slot_name(slot), NULL,
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
    mir_decl_field_metadata_init(meta, header, name, NULL, type_name, kind);
}

static void
mir_decl_field_metadata_init_domain_slot(MIRDeclField *meta,
                                         const MIRDeclHeader *header,
                                         ASTNode *slot)
{
    if (meta == NULL || slot == NULL)
        return;
    mir_decl_field_metadata_init(
        meta, header, ast_domain_slot_name(slot),
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
        meta, header, ast_zone_layer_slot_name(slot), NULL,
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

static bool
mir_class_field_claim_shape(ASTNode *group, bool *is_secure_out)
{
    ASTNode *init;
    const char *callee;
    bool is_secure;

    if (is_secure_out != NULL)
        *is_secure_out = false;
    if (group == NULL || ast_let_destructure_name_count(group) < 1)
        return false;
    init = ast_let_destructure_initializer(group);
    if (init == NULL || ast_call_callee(init) == NULL)
        return false;
    callee = ast_identifier_name(ast_call_callee(init));
    is_secure = callee != NULL && strcmp(callee, "ClaimSecureSlot") == 0;
    if (!is_secure && !(callee != NULL && strcmp(callee, "ClaimSlot") == 0))
        return false;
    if (is_secure_out != NULL)
        *is_secure_out = is_secure;
    return true;
}

static size_t
mir_class_field_claim_count(ASTNode *decl)
{
    size_t count = 0;

    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return 0;
    for (size_t i = 0; i < ast_class_field_destructure_count(decl); i++) {
        if (mir_class_field_claim_shape(
                ast_class_field_destructure_at(decl, i), NULL)) {
            count++;
        }
    }
    return count;
}

static bool
mir_decl_field_claim_capture(MIRDeclFieldClaim *claim,
                             const MIRDeclHeader *header,
                             ASTNode *group)
{
    ASTNode *init;
    bool is_secure;

    if (claim == NULL || header == NULL)
        return false;
    if (!mir_class_field_claim_shape(group, &is_secure))
        return false;
    init = ast_let_destructure_initializer(group);
    claim->owner_name = header->name;
    claim->slot_name = ast_let_destructure_name(group, 0);
    claim->token_name = is_secure && ast_let_destructure_name_count(group) >= 2
        ? ast_let_destructure_name(group, 1)
        : NULL;
    claim->is_secure = is_secure;
    if (ast_call_generic_arg_count(init) > 0) {
        claim->inner_type_name = mir_capture_generic_actual_type_name(
            ast_call_generic_arg(init, 0));
    } else {
        claim->inner_type_name = mir_capture_type_name(NULL, "Int");
    }
    return claim->slot_name != NULL && claim->inner_type_name != NULL;
}

static bool
mir_decl_header_set_class_field_claims(MIRDeclHeader *header, ASTNode *decl)
{
    size_t claim_count;
    size_t out = 0;

    if (header == NULL)
        return false;
    header->field_claim_count = 0;
    header->field_claim_metadata = NULL;
    header->field_claim_metadata_count = 0;
    claim_count = mir_class_field_claim_count(decl);
    header->field_claim_count = claim_count;
    if (claim_count == 0)
        return true;
    if (claim_count > SIZE_MAX / sizeof(MIRDeclFieldClaim))
        return false;
    header->field_claim_metadata =
        calloc(claim_count, sizeof(MIRDeclFieldClaim));
    if (header->field_claim_metadata == NULL)
        return false;
    for (size_t i = 0; i < ast_class_field_destructure_count(decl); i++) {
        ASTNode *group = ast_class_field_destructure_at(decl, i);
        if (!mir_class_field_claim_shape(group, NULL))
            continue;
        if (!mir_decl_field_claim_capture(
                &header->field_claim_metadata[out++], header, group)) {
            return false;
        }
    }
    header->field_claim_metadata_count = out;
    return out == claim_count;
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
    if (!mir_decl_header_set_class_field_claims(header, decl))
        return false;

    switch (decl != NULL ? decl->type : AST_PROGRAM) {
    case AST_CLASS_DECL: {
        PgyDeclField *fields = NULL;
        size_t model_count = pgy_class_decl_field_model_build(decl, &fields);
        for (size_t i = 0; i < model_count; i++) {
            mir_decl_field_metadata_init_class(
                &header->field_metadata[out++], header, fields[i]);
        }
        pgy_decl_field_model_free(fields, model_count);
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
