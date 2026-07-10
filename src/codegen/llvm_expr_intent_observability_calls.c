#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_expr_intent_observability_calls.h"

#include "../common/intent_observability_abi.h"

bool
llvm_emit_intent_observability_call(ASTNode *node, LLVMGenCtx *ctx,
                                    const char *callee_name, LLVMValueRef *out)
{
    const PgyIntentObservabilityAbiRow *row;
    LLVMFuncEntry *fn;

    if (out == NULL)
        return false;

    row = pgy_intent_observability_abi_row_by_source(callee_name);
    if (row == NULL)
        return false;
    if (ast_call_arg_count(node) != row->arg_count) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM intent observability builtin '%s' requires exactly %zu argument%s",
            row->source_name, row->arg_count, row->arg_count == 1 ? "" : "s");
        *out = NULL;
        return true;
    }

    ctx->uses_intent_observability = true;
    fn = llvm_lookup_function(ctx, row->runtime_name);
    if (fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM intent observability builtin '%s' requires registered runtime function '%s'",
            row->source_name, row->runtime_name);
        *out = NULL;
        return true;
    }

    if (row->arg_count == 0) {
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0,
            llvm_tmp_name(ctx));
    } else {
        *out = llvm_emit_function_call_args(ctx, fn,
            ast_call_arguments(node, NULL), row->arg_count);
        if (*out == NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM intent observability builtin '%s' could not lower argument expression",
                row->source_name);
        }
    }
    return true;
}

#endif /* PGY_LLVM_ENABLED */
