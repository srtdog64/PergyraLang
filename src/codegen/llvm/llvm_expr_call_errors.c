#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_dispatch.h"

LLVMValueRef
llvm_call_error_recovery(LLVMGenCtx *ctx, ASTNode *node, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_SYMBOL_UNDEFINED,
            PGY_FIX_IMPORT_OR_DECLARE_SYMBOL,
            "%s", message != NULL ? message : "LLVM call target is invalid");
    }
    return NULL;
}

LLVMValueRef
llvm_call_arg_error_recovery(LLVMGenCtx *ctx, ASTNode *node,
                             const char *callee_name, size_t arg_index)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM call target '%s' could not lower argument %zu",
            callee_name != NULL ? callee_name : "<unknown>",
            arg_index);
    }
    return NULL;
}

#endif
