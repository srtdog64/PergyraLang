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
llvm_find_role_operator_method(LLVMGenCtx *ctx, ASTNode *role,
                               PgyTokenType op, int depth)
{
    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && method->data.func_decl.name != NULL
                && llvm_operator_method_name_matches(op, method->data.func_decl.name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *inc = role->data.role_decl.includes[i];
        ASTNode *included;
        ASTNode *method;

        if (inc == NULL || inc->type != AST_INCLUDE_STMT
            || inc->data.include_stmt.role_name == NULL) {
            continue;
        }
        included = llvm_find_role_decl(ctx, inc->data.include_stmt.role_name);
        method = llvm_find_role_operator_method(ctx, included, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
