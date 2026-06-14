#include "mir_decl_headers.h"
#include "mir_decl_header_shape.h"
#include "mir_decl_header_variants.h"
#include "mir_type_helpers.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
mir_decl_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    size_t current;
    size_t next;

    if (capacity == NULL || elem_size == 0)
        return false;
    current = *capacity;
    if (current == 0) {
        next = initial;
    } else {
        if (current > SIZE_MAX / 2)
            return false;
        next = current * 2;
    }
    if (next > SIZE_MAX / elem_size)
        return false;
    *capacity = next;
    return true;
}

static bool
mir_append_decl_header(MIRProgram *mir, MIRDeclHeader header)
{
    if (mir == NULL)
        return false;
    if (mir->decl_header_count == mir->decl_header_capacity) {
        size_t next_capacity = mir->decl_header_capacity;
        MIRDeclHeader *grown =
            NULL;
        if (!mir_decl_next_capacity(
                &next_capacity, 8, sizeof(MIRDeclHeader))) {
            return false;
        }
        grown = realloc(
            mir->decl_headers, next_capacity * sizeof(MIRDeclHeader));
        if (grown == NULL)
            return false;
        mir->decl_headers = grown;
        mir->decl_header_capacity = next_capacity;
    }
    mir->decl_headers[mir->decl_header_count++] = header;
    return true;
}

static void
mir_decl_method_metadata_clear(MIRDeclMethod *meta)
{
    if (meta == NULL)
        return;
    if (meta->param_type_names != NULL) {
        for (size_t i = 0; i < meta->param_count; i++)
            free(meta->param_type_names[i]);
    }
    free(meta->param_type_names);
    free(meta->return_type_name);
    meta->param_type_names = NULL;
    meta->return_type_name = NULL;
}

static void
mir_decl_field_metadata_clear(MIRDeclField *meta)
{
    if (meta == NULL)
        return;
    free(meta->type_name);
    meta->type_name = NULL;
}

static void
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

static bool
mir_decl_header_set_generics(MIRDeclHeader *header, ASTNode *decl)
{
    GenericParams *params;
    size_t count;

    if (header == NULL)
        return false;

    params = ast_declaration_generic_params(decl);
    count = ast_generic_param_count(params);
    header->generic_param_count = count;
    header->generic_metadata = NULL;
    header->generic_metadata_count = 0;

    if (count == 0)
        return true;
    if (count > SIZE_MAX / sizeof(MIRDeclGenericParam))
        return false;
    header->generic_metadata = calloc(count, sizeof(MIRDeclGenericParam));
    if (header->generic_metadata == NULL)
        return false;

    for (size_t i = 0; i < count; i++) {
        GenericParam *param = ast_generic_param_at(params, i);
        MIRDeclGenericParam *meta = &header->generic_metadata[i];
        meta->source_param = param;
        meta->name = ast_generic_param_name(param);
        meta->bound_ast = ast_generic_param_constraint(param);
        meta->default_arg_ast = ast_generic_param_default_type(param);
    }
    header->generic_metadata_count = count;
    return true;
}

static void
mir_decl_method_metadata_capture_type_names(MIRDeclMethod *meta)
{
    if (meta == NULL)
        return;

    if (meta->param_count > 0) {
        meta->param_type_names = calloc(meta->param_count, sizeof(char *));
        if (meta->param_type_names != NULL) {
            for (size_t i = 0; i < meta->param_count; i++) {
                FuncParam *param = meta->params != NULL ? meta->params[i] : NULL;
                if (param != NULL && param->type != NULL)
                    meta->param_type_names[i] =
                        mir_capture_type_name(param->type, NULL);
            }
        }
    }
    if (meta->return_type != NULL)
        meta->return_type_name =
            mir_capture_type_name(meta->return_type, NULL);
}

static void
mir_decl_method_metadata_init(MIRDeclMethod *meta,
                              const MIRDeclHeader *header,
                              ASTNode *method)
{
    if (meta == NULL || header == NULL)
        return;

    meta->source_ast = method;
    meta->owner_name = header->name;
    if (method == NULL || method->type != AST_FUNC_DECL)
        return;

    meta->name = ast_declaration_name(method);
    meta->params = ast_func_params(method, &meta->param_count);
    meta->return_type = ast_func_return_type(method);
    mir_decl_method_metadata_capture_type_names(meta);
    meta->is_async = method->is_async_decl;
    meta->is_action_like = ast_func_is_action(method);
    meta->within_zone = ast_func_within_zone(method);
    meta->causes_effect = ast_func_causes_effect(method);
}

static bool
mir_decl_header_set_methods(MIRDeclHeader *header,
                            ASTNode **methods,
                            size_t method_count)
{
    if (header == NULL)
        return false;

    header->method_count = method_count;
    header->method_metadata = NULL;
    header->method_metadata_count = 0;

    if (method_count == 0)
        return true;

    if (method_count > SIZE_MAX / sizeof(MIRDeclMethod))
        return false;
    header->method_metadata = calloc(method_count, sizeof(MIRDeclMethod));
    if (header->method_metadata == NULL)
        return false;

    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods != NULL ? methods[i] : NULL;
        MIRDeclMethod *meta = &header->method_metadata[i];
        mir_decl_method_metadata_init(meta, header, method);
    }
    header->method_metadata_count = method_count;
    return true;
}

static bool
mir_decl_header_set_role_impl_methods(MIRDeclHeader *header, ASTNode *role_decl)
{
    size_t count;
    size_t out = 0;

    if (header == NULL || role_decl == NULL || role_decl->type != AST_ROLE_DECL)
        return false;

    if (!ast_role_impl_method_total_count(role_decl, &count))
        return false;
    header->method_count = count;
    header->method_metadata = NULL;
    header->method_metadata_count = 0;
    if (count == 0)
        return true;

    if (count > SIZE_MAX / sizeof(MIRDeclMethod))
        return false;
    header->method_metadata = calloc(count, sizeof(MIRDeclMethod));
    if (header->method_metadata == NULL)
        return false;

    for (size_t i = 0; i < ast_role_impl_count(role_decl); i++) {
        ASTNode *impl = ast_role_impl(role_decl, i);
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
            ASTNode *method = ast_impl_ability_method(impl, j);
            MIRDeclMethod *meta = &header->method_metadata[out++];
            mir_decl_method_metadata_init(meta, header, method);
        }
    }
    header->method_metadata_count = out;
    return true;
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

static bool
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

static bool
mir_decl_method_matches_routine(const MIRDeclMethod *method,
                                const MIRRoutine *routine)
{
    if (method == NULL || routine == NULL)
        return false;
    if (method->name == NULL || routine->name == NULL)
        return false;
    if (method->owner_name == NULL || routine->owner_name == NULL)
        return false;
    return routine->kind == MIR_SCOPE_METHOD
        && strcmp(routine->name, method->name) == 0
        && strcmp(routine->owner_name, method->owner_name) == 0;
}

bool
mir_record_decl_header(MIRProgram *mir, ASTNode *decl)
{
    MIRDeclHeader header;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (mir == NULL || decl == NULL)
        return true;

    memset(&header, 0, sizeof(header));  /* zero-inits variant_metadata too */
    header.source_ast = decl;
    header.ast_type = decl->type;

    switch (decl->type) {
    case AST_FUNC_DECL:
        header.name = ast_declaration_name(decl);
        break;
    case AST_TYPE_ALIAS:
        header.name = ast_type_alias_name(decl);
        break;
    case AST_CLASS_DECL:
        header.name = ast_class_name(decl);
        methods = ast_class_methods(decl, &method_count);
        header.uses_pointer_self =
            ast_class_nominal_kind(decl) == NOMINAL_DECL_SUBJECT
            || ast_class_nominal_kind(decl) == NOMINAL_DECL_VESSEL;
        break;
    case AST_ENUM_DECL:
        header.name = ast_enum_name(decl);
        methods = ast_enum_methods(decl, &method_count);
        break;
    case AST_ABILITY_DECL:
        header.name = ast_ability_name(decl);
        break;
    case AST_EVENT_DECL:
        header.name = ast_event_name(decl);
        break;
    case AST_INTENT_DECL:
        header.name = ast_intent_decl_name(decl);
        break;
    case AST_PARTY_DECL:
        header.name = ast_party_name(decl);
        methods = ast_party_methods(decl, NULL);
        method_count = ast_party_method_count(decl);
        header.uses_pointer_self = true;
        break;
    case AST_ROSTER_DECL:
        header.name = ast_roster_name(decl);
        methods = ast_roster_methods(decl, NULL);
        method_count = ast_roster_method_count(decl);
        header.uses_pointer_self = true;
        break;
    case AST_WORLD_DECL:
        header.name = ast_world_name(decl);
        methods = ast_world_methods(decl, &method_count);
        header.uses_pointer_self = true;
        break;
    case AST_RELATION_DECL:
        header.name = ast_relation_name(decl);
        methods = ast_relation_methods(decl, &method_count);
        header.uses_pointer_self = true;
        break;
    case AST_EFFECT_DECL:
        header.name = ast_effect_name(decl);
        methods = ast_effect_methods(decl, &method_count);
        header.uses_pointer_self = true;
        break;
    case AST_ROLE_DECL:
        header.name = ast_role_name(decl);
        header.uses_pointer_self = true;
        if (header.name == NULL)
            return true;
        if (!mir_decl_header_set_fields(&header, decl))
            return false;
        if (!mir_decl_header_set_generics(&header, decl)) {
            mir_decl_header_free_fields(&header);
            return false;
        }
        if (!mir_decl_header_set_role_impl_methods(&header, decl)) {
            for (size_t i = 0; i < header.method_metadata_count; i++)
                mir_decl_method_metadata_clear(&header.method_metadata[i]);
            free(header.method_metadata);
            mir_decl_header_free_fields(&header);
            free(header.generic_metadata);
            return false;
        }
        if (!mir_append_decl_header(mir, header)) {
            for (size_t i = 0; i < header.method_metadata_count; i++)
                mir_decl_method_metadata_clear(&header.method_metadata[i]);
            free(header.method_metadata);
            mir_decl_header_free_fields(&header);
            free(header.generic_metadata);
            return false;
        }
        return true;
    case AST_ZONE_DECL:
        header.name = ast_zone_name(decl);
        methods = ast_zone_methods(decl, &method_count);
        header.uses_pointer_self = true;
        break;
    default:
        return true;
    }

    if (header.name == NULL)
        return true;
    if (!mir_decl_header_set_fields(&header, decl))
        return false;
    if (!mir_decl_header_set_generics(&header, decl)) {
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_decl_header_set_methods(&header, methods, method_count)) {
        free(header.generic_metadata);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_decl_header_set_variants(&header, decl)) {
        for (size_t i = 0; i < header.method_metadata_count; i++)
            mir_decl_method_metadata_clear(&header.method_metadata[i]);
        free(header.method_metadata);
        free(header.generic_metadata);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_append_decl_header(mir, header)) {
        for (size_t i = 0; i < header.method_metadata_count; i++)
            mir_decl_method_metadata_clear(&header.method_metadata[i]);
        free(header.method_metadata);
        mir_decl_header_free_fields(&header);
        free(header.generic_metadata);
        mir_decl_header_free_variants(&header);
        return false;
    }
    return true;
}

void
mir_link_decl_method_routines(MIRProgram *mir)
{
    MIRRoutineInventory inventory;
    if (mir == NULL)
        return;
    mir_routine_inventory_from_program(mir, &inventory);

    for (size_t hi = 0; hi < mir->decl_header_count; hi++) {
        MIRDeclHeader *header = &mir->decl_headers[hi];
        for (size_t mi = 0; mi < header->method_metadata_count; mi++) {
            MIRDeclMethod *method = &header->method_metadata[mi];
            method->has_routine = false;
            method->routine_index = 0;
            if (method->name == NULL)
                continue;

            for (size_t ri = 0; ri < inventory.count; ri++) {
                const MIRRoutine *routine =
                    mir_routine_inventory_get(&inventory, ri);
                if (!mir_decl_method_matches_routine(method, routine))
                    continue;
                method->has_routine = true;
                method->routine_index = ri;
                break;
            }
        }
    }
}
