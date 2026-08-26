#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_expr_intent_observability_calls.h"

#include "../common/intent_observability_abi.h"
#include "../semantic/builtin_kind.h"

bool
llvm_emit_intent_observability_call(ASTNode *node, LLVMGenCtx *ctx,
                                    const char *callee_name, LLVMValueRef *out)
{
    const PgyIntentObservabilityAbiRow *row;
    LLVMFuncEntry *fn;
    size_t argument_count;
    uint32_t builtin_kind = 0;
    uint32_t runtime_call_abi_id = 0;

    if (out == NULL)
        return false;

    if (!ast_call_semantic_callee_builtin_kind(node, &builtin_kind)
        || builtin_kind != (uint32_t)BUILTIN_INTENT_OBSERVABILITY) {
        return false;
    }
    row = NULL;
    if (ast_call_semantic_runtime_call_abi_id(
            node, &runtime_call_abi_id)) {
        row = pgy_intent_observability_abi_row_for_carried_identity(
            runtime_call_abi_id, callee_name);
    }
    if (row == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM intent observability builtin '%s' is missing its carried ABI identity",
            callee_name != NULL ? callee_name : "<missing>");
        *out = NULL;
        return true;
    }
    argument_count = pgy_intent_observability_argument_count(row);
    if (ast_call_arg_count(node) != argument_count) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM intent observability builtin '%s' requires exactly %zu argument%s",
            row->source_name, argument_count,
            argument_count == 1 ? "" : "s");
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

    if (argument_count == 0) {
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0,
            llvm_tmp_name(ctx));
    } else {
        *out = llvm_emit_function_call_args(ctx, fn,
            ast_call_arguments(node, NULL), argument_count);
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
