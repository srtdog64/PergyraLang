/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared LLVM domain-query expression helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

const char *
llvm_call_name_or_string_arg(ASTNode *node, size_t index)
{
    ASTNode *arg;
    if (node == NULL || index >= node->data.call.arg_count)
        return NULL;
    arg = node->data.call.arguments[index];
    if (arg == NULL)
        return NULL;
    if (arg->type == AST_IDENTIFIER)
        return arg->data.identifier.name;
    if (arg->type == AST_STRING)
        return arg->data.string.value;
    return NULL;
}

LLVMValueRef
llvm_domain_query_false(LLVMGenCtx *ctx)
{
    return LLVMConstInt(ctx->type_i1, 0, 0);
}

#endif /* PGY_LLVM_ENABLED */
