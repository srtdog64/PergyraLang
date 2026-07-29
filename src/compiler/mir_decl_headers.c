#include "mir_decl_headers.h"
#include "mir_decl_header_authority.h"
#include "mir_decl_header_fields.h"
#include "mir_decl_header_generic_metadata.h"
#include "mir_decl_header_methods.h"
#include "mir_decl_header_refresh.h"
#include "mir_decl_header_shape.h"
#include "mir_decl_header_variants.h"
#include "mir_decl_header_zone_state.h"
#include "mir_decl_header_world_directive.h"
#include "mir_decl_header_world_state.h"
#include "mir_ability_ref.h"
#include "mir_type_helpers.h"

#include "../common/string_compat.h"
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
mir_decl_header_event_params_clear(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    if (header->event_param_names != NULL) {
        for (size_t i = 0; i < header->event_param_count; i++)
            free(header->event_param_names[i]);
    }
    if (header->event_param_type_names != NULL) {
        for (size_t i = 0; i < header->event_param_count; i++)
            free(header->event_param_type_names[i]);
    }
    free(header->event_param_names);
    free(header->event_param_type_names);
    header->event_param_names = NULL;
    header->event_param_type_names = NULL;
    header->event_param_count = 0;
    header->event_param_metadata_present = false;
}

static bool
mir_decl_header_set_event_params(MIRDeclHeader *header, ASTNode *event_decl)
{
    size_t count;

    if (header == NULL || event_decl == NULL
        || event_decl->type != AST_EVENT_DECL)
        return true;

    count = ast_event_param_count(event_decl);
    header->event_param_metadata_present = true;
    header->event_param_count = count;
    if (count == 0)
        return true;
    if (count > SIZE_MAX / sizeof(char *))
        return false;

    header->event_param_names = calloc(count, sizeof(char *));
    header->event_param_type_names = calloc(count, sizeof(char *));
    if (header->event_param_names == NULL
        || header->event_param_type_names == NULL) {
        mir_decl_header_event_params_clear(header);
        return false;
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode *param = ast_event_param(event_decl, i);
        const char *name = param != NULL ? ast_let_name(param) : NULL;

        if (param == NULL || param->type != AST_LET_DECL || name == NULL
            || ast_let_type(param) == NULL) {
            mir_decl_header_event_params_clear(header);
            return false;
        }
        header->event_param_names[i] = pergyra_strdup(name);
        header->event_param_type_names[i] =
            mir_capture_type_name(ast_let_type(param), NULL);
        if (header->event_param_names[i] == NULL
            || header->event_param_type_names[i] == NULL) {
            mir_decl_header_event_params_clear(header);
            return false;
        }
    }
    return true;
}

static bool
mir_decl_header_count_role_impls(ASTNode *role_decl, size_t *count_out)
{
    size_t count = 0;

    if (count_out == NULL)
        return false;
    *count_out = 0;
    if (role_decl == NULL || role_decl->type != AST_ROLE_DECL)
        return false;

    for (size_t i = 0; i < ast_role_impl_count(role_decl); i++) {
        ASTNode *impl = ast_role_impl(role_decl, i);
        if (impl != NULL && impl->type == AST_IMPL_ABILITY)
            count++;
    }
    *count_out = count;
    return true;
}

static void
mir_decl_header_free_role_impls(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    if (header->role_impl_metadata != NULL) {
        for (size_t i = 0; i < header->role_impl_metadata_count; i++)
            mir_ability_ref_clear(&header->role_impl_metadata[i].ability_ref);
    }
    free(header->role_impl_metadata);
    header->role_impl_metadata = NULL;
    header->role_impl_metadata_count = 0;
    header->role_impl_count = 0;
}

static bool
mir_decl_header_set_role_impls(MIRDeclHeader *header, ASTNode *role_decl)
{
    size_t count;
    size_t out = 0;
    size_t method_index = 0;

    if (header == NULL || role_decl == NULL || role_decl->type != AST_ROLE_DECL)
        return false;
    if (!mir_decl_header_count_role_impls(role_decl, &count))
        return false;
    header->role_impl_count = count;
    header->role_impl_metadata = NULL;
    header->role_impl_metadata_count = 0;
    if (count == 0)
        return true;
    if (count > SIZE_MAX / sizeof(MIRDeclRoleImpl))
        return false;

    header->role_impl_metadata = calloc(count, sizeof(MIRDeclRoleImpl));
    if (header->role_impl_metadata == NULL)
        return false;

    for (size_t i = 0; i < ast_role_impl_count(role_decl); i++) {
        ASTNode *impl = ast_role_impl(role_decl, i);
        MIRDeclRoleImpl *meta;

        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        meta = &header->role_impl_metadata[out];
        meta->owner_name = header->name;
        meta->method_start_index = method_index;
        meta->method_count = ast_impl_ability_method_count(impl);
        if (!mir_ability_ref_capture(
                &meta->ability_ref, ast_impl_ability_ref(impl))) {
            header->role_impl_metadata_count = out + 1;
            mir_decl_header_free_role_impls(header);
            return false;
        }
        method_index += meta->method_count;
        out++;
    }
    header->role_impl_metadata_count = out;
    return out == count;
}

static void
mir_decl_header_free_role_includes(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    free(header->role_include_metadata);
    header->role_include_metadata = NULL;
    header->role_include_metadata_count = 0;
    header->role_include_count = 0;
}

static bool
mir_decl_header_set_role_includes(MIRDeclHeader *header, ASTNode *role_decl)
{
    size_t count;

    if (header == NULL || role_decl == NULL || role_decl->type != AST_ROLE_DECL)
        return false;

    count = ast_role_include_count(role_decl);
    header->role_include_count = count;
    header->role_include_metadata = NULL;
    header->role_include_metadata_count = 0;
    if (count == 0)
        return true;
    if (count > SIZE_MAX / sizeof(MIRDeclRoleInclude))
        return false;

    header->role_include_metadata =
        calloc(count, sizeof(MIRDeclRoleInclude));
    if (header->role_include_metadata == NULL)
        return false;

    for (size_t i = 0; i < count; i++) {
        ASTNode *include_stmt = ast_role_include(role_decl, i);
        MIRDeclRoleInclude *meta = &header->role_include_metadata[i];
        meta->owner_name = header->name;
        meta->role_name = ast_include_role_name(include_stmt);
        if (meta->role_name == NULL) {
            mir_decl_header_free_role_includes(header);
            return false;
        }
        header->role_include_metadata_count = i + 1;
    }
    return true;
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
    header.source_syntax_id = ast_node_stable_id(decl);

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
        methods = ast_ability_methods(decl, &method_count);
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
        header.role_subject_type_name =
            mir_capture_type_name(ast_role_for_type(decl), NULL);
        if (ast_role_for_type(decl) != NULL
            && header.role_subject_type_name == NULL) {
            return false;
        }
        if (!mir_decl_header_set_fields(&header, decl)) {
            free(header.role_subject_type_name);
            return false;
        }
        if (!mir_decl_header_set_generics(&header, decl)) {
            free(header.role_subject_type_name);
            mir_decl_header_free_fields(&header);
            return false;
        }
        if (!mir_decl_header_set_role_impls(&header, decl)) {
            free(header.role_subject_type_name);
            mir_decl_header_free_fields(&header);
            mir_decl_header_free_generics(&header);
            return false;
        }
        if (!mir_decl_header_set_role_includes(&header, decl)) {
            free(header.role_subject_type_name);
            mir_decl_header_free_role_impls(&header);
            mir_decl_header_free_fields(&header);
            mir_decl_header_free_generics(&header);
            return false;
        }
        if (!mir_decl_header_set_role_impl_methods(&header, decl)) {
            for (size_t i = 0; i < header.method_metadata_count; i++)
                mir_decl_method_metadata_clear(&header.method_metadata[i]);
            free(header.method_metadata);
            free(header.role_subject_type_name);
            mir_decl_header_free_role_includes(&header);
            mir_decl_header_free_role_impls(&header);
            mir_decl_header_free_fields(&header);
            mir_decl_header_free_generics(&header);
            return false;
        }
        if (!mir_append_decl_header(mir, header)) {
            for (size_t i = 0; i < header.method_metadata_count; i++)
                mir_decl_method_metadata_clear(&header.method_metadata[i]);
            free(header.method_metadata);
            free(header.role_subject_type_name);
            mir_decl_header_free_role_includes(&header);
            mir_decl_header_free_role_impls(&header);
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
    if (!mir_decl_header_set_zone_states(&header, decl)) {
        free(header.type_alias_target_type_name);
        mir_decl_header_free_refreshes(&header);
        mir_decl_header_free_generics(&header);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_decl_header_set_world_states(&header, decl)) {
        free(header.type_alias_target_type_name);
        mir_decl_header_free_zone_states(&header);
        mir_decl_header_free_refreshes(&header);
        mir_decl_header_free_generics(&header);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_decl_header_set_world_directives(&header, decl)) {
        free(header.type_alias_target_type_name);
        mir_decl_header_free_world_states(&header);
        mir_decl_header_free_zone_states(&header);
        mir_decl_header_free_refreshes(&header);
        mir_decl_header_free_generics(&header);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_decl_header_set_methods(&header, methods, method_count)) {
        free(header.type_alias_target_type_name);
        mir_decl_header_free_world_directives(&header);
        mir_decl_header_free_world_states(&header);
        mir_decl_header_free_zone_states(&header);
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
        mir_decl_header_free_world_directives(&header);
        mir_decl_header_free_world_states(&header);
        mir_decl_header_free_zone_states(&header);
        mir_decl_header_free_refreshes(&header);
        mir_decl_header_free_generics(&header);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_fields(&header);
        return false;
    }
    if (!mir_decl_header_set_event_params(&header, decl)) {
        free(header.type_alias_target_type_name);
        for (size_t i = 0; i < header.method_metadata_count; i++)
            mir_decl_method_metadata_clear(&header.method_metadata[i]);
        free(header.method_metadata);
        mir_decl_header_free_world_directives(&header);
        mir_decl_header_free_world_states(&header);
        mir_decl_header_free_zone_states(&header);
        mir_decl_header_free_refreshes(&header);
        mir_decl_header_free_generics(&header);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_fields(&header);
        mir_decl_header_free_variants(&header);
        return false;
    }
    if (!mir_append_decl_header(mir, header)) {
        free(header.type_alias_target_type_name);
        for (size_t i = 0; i < header.method_metadata_count; i++)
            mir_decl_method_metadata_clear(&header.method_metadata[i]);
        free(header.method_metadata);
        mir_decl_header_free_world_directives(&header);
        mir_decl_header_free_world_states(&header);
        mir_decl_header_free_fields(&header);
        mir_decl_header_free_generics(&header);
        mir_decl_header_free_authorities(&header);
        mir_decl_header_free_refreshes(&header);
        mir_decl_header_free_zone_states(&header);
        mir_decl_header_free_variants(&header);
        mir_decl_header_event_params_clear(&header);
        return false;
    }
    return true;
}
