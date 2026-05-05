/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM backend early forward-declaration eligibility.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_internal.h"

#include <stdbool.h>

static bool
llvm_can_forward_declare_type_early(LLVMGenCtx *ctx, ASTNode *type_node)
{
    const char *name;
    PgyTypeKind kind;

    if (ctx == NULL || type_node == NULL)
        return true;
    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return true;

    name = type_node->data.type.name;
    kind = pgy_classify_type(name);
    if (pgy_kind_to_llvm(ctx, kind) != NULL)
        return true;

    switch (kind) {
    case PGY_TK_RESULT:
    case PGY_TK_OPTION:
    case PGY_TK_SLOT:
    case PGY_TK_SECURE_SLOT:
    case PGY_TK_DEVICE_SLOT:
    case PGY_TK_REMOTE_FUTURE:
    case PGY_TK_ARRAY:
    case PGY_TK_SLICE:
    case PGY_TK_CHANNEL:
    case PGY_TK_BOX:
    case PGY_TK_RC:
    case PGY_TK_WEAK:
    case PGY_TK_FUTURE:
        return true;
    case PGY_TK_UNKNOWN:
    case PGY_TK_CLASS:
        return llvm_lookup_class(ctx, name) != NULL;
    default:
        return true;
    }
}

bool
llvm_can_forward_declare_func_early(LLVMGenCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    if (func->data.func_decl.generic_params != NULL
        && func->data.func_decl.generic_params->count > 0)
        return false;
    if (!llvm_can_forward_declare_type_early(ctx, func->data.func_decl.return_type))
        return false;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *param = func->data.func_decl.params[i];
        if (param == NULL || param->type == NULL)
            continue;
        if (!llvm_can_forward_declare_type_early(ctx, param->type))
            return false;
    }
    return true;
}

#endif /* PGY_LLVM_ENABLED */
