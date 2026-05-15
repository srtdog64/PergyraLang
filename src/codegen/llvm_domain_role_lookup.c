/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain role lookup helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_domain_role_helpers.h"

ASTNode *
llvm_find_role_decl(LLVMGenCtx *ctx, const char *role_name)
{
    if (ctx == NULL || role_name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_ROLE_DECL, role_name);
}

ASTNode *
llvm_role_for_type_node(ASTNode *role)
{
    return ast_role_for_type(role);
}

const char *
llvm_role_for_type_name(ASTNode *role)
{
    ASTNode *for_type = llvm_role_for_type_node(role);

    return ast_type_name(for_type);
}

ASTNode *
llvm_find_role_operator_method(LLVMGenCtx *ctx, ASTNode *role,
                               PgyTokenType op, int depth)
{
    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < ast_role_impl_count(role); i++) {
        ASTNode *impl = ast_role_impl(role, i);
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
            ASTNode *method = ast_impl_ability_method(impl, j);
            const char *method_name = ast_declaration_name(method);
            if (method != NULL && method->type == AST_FUNC_DECL
                && method_name != NULL
                && llvm_operator_method_name_matches(op, method_name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < ast_role_include_count(role); i++) {
        ASTNode *inc = ast_role_include(role, i);
        const char *role_name = ast_include_role_name(inc);
        ASTNode *included;
        ASTNode *method;

        if (role_name == NULL)
            continue;
        included = llvm_find_role_decl(ctx, role_name);
        method = llvm_find_role_operator_method(ctx, included, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
