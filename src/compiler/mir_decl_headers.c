#include "mir_decl_headers.h"

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
    meta->is_action_like = ast_func_is_action(method);
    meta->within_zone = ast_func_within_zone(method);
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

    memset(&header, 0, sizeof(header));
    header.source_ast = decl;
    header.ast_type = decl->type;

    switch (decl->type) {
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
        if (!mir_decl_header_set_role_impl_methods(&header, decl))
            return false;
        if (!mir_append_decl_header(mir, header)) {
            free(header.method_metadata);
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
    if (!mir_decl_header_set_methods(&header, methods, method_count))
        return false;
    if (!mir_append_decl_header(mir, header)) {
        free(header.method_metadata);
        return false;
    }
    return true;
}

void
mir_link_decl_method_routines(MIRProgram *mir)
{
    if (mir == NULL)
        return;

    for (size_t hi = 0; hi < mir->decl_header_count; hi++) {
        MIRDeclHeader *header = &mir->decl_headers[hi];
        for (size_t mi = 0; mi < header->method_metadata_count; mi++) {
            MIRDeclMethod *method = &header->method_metadata[mi];
            method->has_routine = false;
            method->routine_index = 0;
            if (method->name == NULL)
                continue;

            for (size_t ri = 0; ri < mir->routine_count; ri++) {
                const MIRRoutine *routine = &mir->routines[ri];
                if (!mir_decl_method_matches_routine(method, routine))
                    continue;
                method->has_routine = true;
                method->routine_index = ri;
                break;
            }
        }
    }
}
