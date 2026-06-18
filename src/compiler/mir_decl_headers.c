#include "mir_decl_headers.h"
#include "mir_decl_header_authority.h"
#include "mir_decl_header_fields.h"
#include "mir_decl_header_refresh.h"
#include "mir_decl_header_shape.h"
#include "mir_decl_header_variants.h"
#include "mir_decl_method_projection.h"
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
    mir_decl_method_projection_metadata_clear(meta);
    meta->param_type_names = NULL;
    meta->return_type_name = NULL;
}

static void
mir_decl_generic_metadata_clear(MIRDeclGenericParam *metadata, size_t count)
{
    if (metadata == NULL)
        return;
    for (size_t i = 0; i < count; i++) {
        free(metadata[i].bound_type_name);
        free(metadata[i].default_arg_type_name);
        metadata[i].bound_type_name = NULL;
        metadata[i].default_arg_type_name = NULL;
    }
}

static void
mir_decl_header_free_generics(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    mir_decl_generic_metadata_clear(
        header->generic_metadata, header->generic_metadata_count);
    free(header->generic_metadata);
    header->generic_metadata = NULL;
    header->generic_metadata_count = 0;
    header->generic_param_count = 0;
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
        ASTNode *constraint;
        ASTNode *default_type;

        if (param == NULL) {
            mir_decl_header_free_generics(header);
            return false;
        }

        constraint = ast_generic_param_constraint(param);
        default_type = ast_generic_param_default_type(param);
        meta->name = ast_generic_param_name(param);
        meta->bound_type_name = mir_capture_type_name(constraint, NULL);
        meta->default_arg_type_name =
            mir_capture_type_name(default_type, NULL);
        if (meta->name == NULL
            || (constraint != NULL && meta->bound_type_name == NULL)
            || (default_type != NULL
                && meta->default_arg_type_name == NULL)) {
            mir_decl_header_free_generics(header);
            return false;
        }
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
    (void)mir_decl_method_projection_metadata_capture(
        meta, ast_func_body(method));
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
    header.ast_type = decl->type;

    switch (decl->type) {
    case AST_FUNC_DECL:
        header.name = ast_declaration_name(decl);
        break;
    case AST_TYPE_ALIAS:
        header.name = ast_type_alias_name(decl);
        header.type_alias_target_type_name =
            mir_capture_type_name(ast_type_alias_target_type(decl), NULL);
        break;
    case AST_CLASS_DECL:
        header.name = ast_class_name(decl);
        methods = ast_class_methods(decl, &method_count);
        header.nominal_kind = ast_class_nominal_kind(decl);
        header.uses_pointer_self =
            header.nominal_kind == NOMINAL_DECL_SUBJECT
            || header.nominal_kind == NOMINAL_DECL_VESSEL;
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
        header.intent_retry_count = ast_intent_decl_retry_count(decl);
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
            mir_decl_header_free_generics(&header);
            return false;
        }
        if (!mir_append_decl_header(mir, header)) {
            for (size_t i = 0; i < header.method_metadata_count; i++)
                mir_decl_method_metadata_clear(&header.method_metadata[i]);
            free(header.method_metadata);
            mir_decl_header_free_fields(&header);
            mir_decl_header_free_generics(&header);
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
    if (!mir_decl_header_set_fields(&header, decl)) {
        free(header.type_alias_target_type_name);
        return false;
    }
    if (!mir_decl_header_set_authorities(&header, decl)) {
        free(header.type_alias_target_type_name);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_decl_header_set_generics(&header, decl)) {
        free(header.type_alias_target_type_name);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_decl_header_set_refreshes(&header, decl)) {
        free(header.type_alias_target_type_name);
        mir_decl_header_free_generics(&header);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_decl_header_set_methods(&header, methods, method_count)) {
        free(header.type_alias_target_type_name);
        mir_decl_header_free_refreshes(&header);
        mir_decl_header_free_generics(&header);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_decl_header_set_variants(&header, decl)) {
        free(header.type_alias_target_type_name);
        for (size_t i = 0; i < header.method_metadata_count; i++)
            mir_decl_method_metadata_clear(&header.method_metadata[i]);
        free(header.method_metadata);
        mir_decl_header_free_refreshes(&header);
        mir_decl_header_free_generics(&header);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_append_decl_header(mir, header)) {
        free(header.type_alias_target_type_name);
        for (size_t i = 0; i < header.method_metadata_count; i++)
            mir_decl_method_metadata_clear(&header.method_metadata[i]);
        free(header.method_metadata);
        mir_decl_header_free_fields(&header);
        mir_decl_header_free_generics(&header);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_refreshes(&header);
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
