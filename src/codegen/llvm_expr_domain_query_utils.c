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
    if (node == NULL || index >= ast_call_arg_count(node))
        return NULL;
    arg = ast_call_argument(node, index);
    if (arg == NULL)
        return NULL;
    if (arg->type == AST_IDENTIFIER)
        return ast_identifier_name(arg);
    if (arg->type == AST_STRING)
        return ast_string_value(arg);
    return NULL;
}

LLVMValueRef
llvm_domain_query_false(LLVMGenCtx *ctx)
{
    return LLVMConstInt(ctx->type_i1, 0, 0);
}

#endif /* PGY_LLVM_ENABLED */
