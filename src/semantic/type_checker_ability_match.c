/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Ability matching and subject role lookup helpers.
 */

#include <stdlib.h>
#include <string.h>

#include "type_checker_ability_match_internal.h"
#include "type_checker_decls_a_helpers_internal.h"

static bool
ability_ref_type_arg_equal(ASTNode *lhs, ASTNode *rhs)
{
    if (lhs == rhs)
        return true;
    if (lhs == NULL || rhs == NULL)
        return false;
    if (lhs->type != AST_TYPE || rhs->type != AST_TYPE)
        return false;
    const char *lhs_name = ast_type_name(lhs);
    const char *rhs_name = ast_type_name(rhs);
    if ((lhs_name == NULL) != (rhs_name == NULL))
        return false;
    if (lhs_name != NULL && strcmp(lhs_name, rhs_name) != 0) {
        return false;
    }

    {
        GenericParams *lhs_args = ast_type_generic_args(lhs);
        GenericParams *rhs_args = ast_type_generic_args(rhs);
        size_t lhs_count = lhs_args != NULL ? lhs_args->count : 0;
        size_t rhs_count = rhs_args != NULL ? rhs_args->count : 0;
        if (lhs_count != rhs_count)
            return false;
        for (size_t i = 0; i < lhs_count; i++) {
            GenericParam *lhs_gp = lhs_args->params[i];
            GenericParam *rhs_gp = rhs_args->params[i];
            ASTNode *lhs_arg = lhs_gp != NULL ? lhs_gp->constraint : NULL;
            ASTNode *rhs_arg = rhs_gp != NULL ? rhs_gp->constraint : NULL;
            if (!ability_ref_type_arg_equal(lhs_arg, rhs_arg))
                return false;
        }
    }

    return true;
}

bool
ability_ref_matches(ASTNode *program, ASTNode *impl_ref, ASTNode *required_ref)
{
    ASTNode *ability_decl;
    GenericParams *decl_params;
    ASTNode **impl_effective = NULL;
    ASTNode **required_effective = NULL;
    size_t impl_count = 0;
    size_t required_count = 0;
    const char *ability_name;
    bool matches = false;

    if (impl_ref == NULL || required_ref == NULL)
        return false;
    if (impl_ref->type != AST_TYPE || required_ref->type != AST_TYPE)
        return false;
    const char *impl_name = ast_type_name(impl_ref);
    const char *required_name = ast_type_name(required_ref);
    if (impl_name == NULL || required_name == NULL)
        return false;
    if (strcmp(impl_name, required_name) != 0)
        return false;

    ability_decl = find_ability_decl_by_name(program, impl_name);
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL)
        return ability_ref_type_arg_equal(impl_ref, required_ref);

    decl_params = ast_ability_generic_params(ability_decl);
    if (decl_params == NULL || decl_params->count == 0)
        return true;
    ability_name = ast_ability_name(ability_decl);

    impl_effective = collect_effective_generic_arg_nodes(
        decl_params, ast_type_generic_args(impl_ref), NULL, NULL,
        "Ability",
        ability_name != NULL ? ability_name : "<ability>",
        &impl_count);
    required_effective = collect_effective_generic_arg_nodes(
        decl_params, ast_type_generic_args(required_ref), NULL, NULL,
        "Ability",
        ability_name != NULL ? ability_name : "<ability>",
        &required_count);

    if (impl_effective == NULL || required_effective == NULL || impl_count != required_count)
        goto cleanup;

    matches = true;
    for (size_t i = 0; i < impl_count; i++) {
        if (!ability_ref_type_arg_equal(impl_effective[i], required_effective[i])) {
            matches = false;
            break;
        }
    }

cleanup:
    free(impl_effective);
    free(required_effective);
    return matches;
}

bool
role_decl_has_ability(ASTNode *role, ASTNode *program,
                      ASTNode *ability_ref, int depth)
{
    if (role == NULL || role->type != AST_ROLE_DECL
        || ability_ref == NULL || depth > 16) {
        return false;
    }

    for (size_t i = 0; i < ast_role_impl_count(role); i++) {
        ASTNode *impl = ast_role_impl(role, i);
        ASTNode *impl_ability_ref = ast_impl_ability_ref(impl);
        if (impl != NULL
            && impl->type == AST_IMPL_ABILITY
            && ability_ref_matches(program,
                impl_ability_ref, ability_ref)) {
            return true;
        }
    }

    for (size_t i = 0; i < ast_role_include_count(role); i++) {
        ASTNode *inc = ast_role_include(role, i);
        const char *role_name = ast_include_role_name(inc);
        ASTNode *included;
        if (role_name == NULL)
            continue;
        included = semantic_find_role_decl(program, role_name);
        if (role_decl_has_ability(included, program, ability_ref, depth + 1))
            return true;
    }

    return false;
}

bool
subject_type_has_ability(ASTNode *program, const char *type_name,
                         ASTNode *ability_ref)
{
    if (program == NULL || program->type != AST_PROGRAM
        || type_name == NULL || ability_ref == NULL) {
        return false;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        const char *role_type = semantic_role_for_type_name(stmt);
        if (role_type == NULL || strcmp(role_type, type_name) != 0) {
            continue;
        }
        if (role_decl_has_ability(stmt, program, ability_ref, 0))
            return true;
    }

    return false;
}

ASTNode *
subject_type_find_base_ability_impl(ASTNode *program, const char *type_name,
                                    const char *ability_name)
{
    if (program == NULL || program->type != AST_PROGRAM
        || type_name == NULL || ability_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        const char *role_type = semantic_role_for_type_name(stmt);
        if (role_type == NULL || strcmp(role_type, type_name) != 0) {
            continue;
        }
        for (size_t j = 0; j < ast_role_impl_count(stmt); j++) {
            ASTNode *impl = ast_role_impl(stmt, j);
            ASTNode *impl_ability_ref = ast_impl_ability_ref(impl);
            const char *impl_ability_name = ast_impl_ability_name(impl);
            if (impl != NULL
                && impl->type == AST_IMPL_ABILITY
                && impl_ability_name != NULL
                && strcmp(impl_ability_name, ability_name) == 0) {
                return impl_ability_ref;
            }
        }
    }

    return NULL;
}
