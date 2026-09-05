#include "mir_decl_header_methods.h"
#include "mir_decl_headers.h"
#include "mir_ability_ref.h"
#include "mir_decl_method_projection.h"
#include "mir_type_helpers.h"

#include "../common/string_compat.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void
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
    if (meta->authorized_by_names != NULL) {
        for (size_t i = 0; i < meta->authorized_by_count; i++)
            free(meta->authorized_by_names[i]);
    }
    free(meta->authorized_by_names);
    if (meta->required_ability_refs != NULL) {
        for (size_t i = 0; i < meta->required_ability_ref_count; i++)
            mir_ability_ref_clear(&meta->required_ability_refs[i]);
    }
    free(meta->required_ability_refs);
    mir_decl_method_projection_metadata_clear(meta);
    meta->param_type_names = NULL;
    meta->return_type_name = NULL;
    meta->authorized_by_names = NULL;
    meta->authorized_by_count = 0;
    meta->required_ability_refs = NULL;
    meta->required_ability_ref_count = 0;
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
    meta->source_syntax_id = ast_node_stable_id(method);
    meta->params = ast_func_params(method, &meta->param_count);
    meta->return_type = ast_func_return_type(method);
    mir_decl_method_metadata_capture_type_names(meta);
    if (meta->return_type_name == NULL
        && ast_func_semantic_return_type_name(method) != NULL) {
        meta->return_type_name = mir_capture_type_name(
            NULL, ast_func_semantic_return_type_name(method));
    }
    meta->is_async = method->is_async_decl;
    meta->is_action_like = ast_func_is_action(method);
    {
        size_t required_count = ast_func_required_ability_count(method);
        meta->required_ability_ref_count = required_count;
        if (required_count > 0 && required_count <= SIZE_MAX /
            sizeof(MIRAbilityRef)) {
            meta->required_ability_refs = calloc(
                required_count, sizeof(MIRAbilityRef));
            if (meta->required_ability_refs != NULL) {
                for (size_t i = 0; i < required_count; i++) {
                    ASTNode *required =
                        ast_func_required_ability(method, i);
                    if (!mir_ability_ref_capture(
                            &meta->required_ability_refs[i], required)) {
                        mir_ability_ref_clear(
                            &meta->required_ability_refs[i]);
                    }
                }
            }
        }
    }
    meta->within_zone = ast_func_within_zone(method);
    meta->causes_effect = ast_func_causes_effect(method);
    meta->authorized_by_count = ast_func_authorized_by_count(method);
    if (meta->authorized_by_count > 0) {
        meta->authorized_by_names =
            calloc(meta->authorized_by_count, sizeof(char *));
        if (meta->authorized_by_names != NULL) {
            for (size_t i = 0; i < meta->authorized_by_count; i++) {
                const char *name = ast_func_authorized_by(method, i);
                if (name != NULL)
                    meta->authorized_by_names[i] = pergyra_strdup(name);
            }
        }
    }
    meta->has_caps_clause = ast_func_has_caps_clause(method);
    meta->declared_capabilities = ast_func_declared_capabilities(method);
    meta->has_effects_clause = ast_func_has_effects_clause(method);
    meta->declared_effects = ast_func_declared_effects(method);
    (void)mir_decl_method_projection_metadata_capture(
        meta, ast_func_body(method));
}

bool
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

bool
mir_decl_header_set_role_impl_methods(MIRDeclHeader *header, ASTNode *role_decl)
{
    size_t count = 0;
    size_t out = 0;

    if (header == NULL || role_decl == NULL || role_decl->type != AST_ROLE_DECL)
        return false;

    if (!ast_role_impl_method_total_count(role_decl, &count))
        return false;
    for (size_t i = 0; i < ast_role_impl_count(role_decl); i++) {
        ASTNode *impl = ast_role_impl(role_decl, i);
        if (impl == NULL || impl->type != AST_OVERRIDE_FUNC
            || ast_override_func_decl(impl) == NULL)
            continue;
        if (count == SIZE_MAX)
            return false;
        count++;
    }
    header->method_count = count;
    header->role_override_method_count = 0;
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
    for (size_t i = 0; i < ast_role_impl_count(role_decl); i++) {
        ASTNode *impl = ast_role_impl(role_decl, i);
        ASTNode *method;
        MIRDeclMethod *meta;
        if (impl == NULL || impl->type != AST_OVERRIDE_FUNC)
            continue;
        method = ast_override_func_decl(impl);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        meta = &header->method_metadata[out++];
        mir_decl_method_metadata_init(meta, header, method);
        header->role_override_method_count++;
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
