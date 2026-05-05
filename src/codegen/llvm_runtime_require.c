/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM runtime dependency diagnostics.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMFuncEntry *
llvm_required_runtime_function(LLVMGenCtx *ctx,
                               ASTNode *node,
                               const char *family_name,
                               const char *callee_name,
                               const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;

    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM %s builtin '%s' requires registered runtime function '%s'",
            family_name != NULL ? family_name : "runtime",
            callee_name != NULL ? callee_name : "<unknown>",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

#endif /* PGY_LLVM_ENABLED */
