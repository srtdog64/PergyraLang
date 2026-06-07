#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_sync_frontier.h"
#include "../runtime/pgy_frontier_policy.h"

void
llvm_emit_sync_generation_increment(LLVMGenCtx *ctx,
                                    LLVMClassTypeEntry *decl_cls,
                                    LLVMValueRef self_ptr)
{
    int generation_idx;
    LLVMValueRef generation_ptr;
    LLVMValueRef one;

    if (ctx == NULL || decl_cls == NULL || self_ptr == NULL)
        return;

    generation_idx = llvm_class_field_index(decl_cls, "__sync_generation");
    if (generation_idx < 0)
        return;

    /*
     * Atomic increment with release ordering. Matches the C-backend
     * macro PGY_ZONE_GENERATION_INC which uses
     * atomic_fetch_add_explicit(memory_order_release). The atomic
     * operation is the minimum fix for the data race observed when
     * parallel/spawn code paths share a zone pointer; the rwlock
     * (PGY_ZONE_THREADSAFE in C-emit) protects the other zone fields.
     * The result of the RMW is unused; the call is purely for the
     * side-effect on the counter.
     */
    generation_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
        self_ptr, (unsigned)generation_idx, llvm_tmp_name(ctx));
    one = LLVMConstInt(ctx->type_i32, 1, 0);
    (void)LLVMBuildAtomicRMW(ctx->builder, LLVMAtomicRMWBinOpAdd,
        generation_ptr, one,
        LLVMAtomicOrderingRelease,
        /*singleThread=*/0);
}

void
llvm_emit_frontier_overflow_abort(LLVMGenCtx *ctx, const char *reason)
{
    LLVMFuncEntry *panic_fn;

    if (ctx == NULL)
        return;

    panic_fn = llvm_lookup_function(ctx,
        "pgy_runtime_panic_internal_invariant_export");
    if (panic_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, NULL,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM frontier overflow requires registered runtime function '%s'",
            "pgy_runtime_panic_internal_invariant_export");
        LLVMBuildUnreachable(ctx->builder);
        return;
    }
    if (panic_fn != NULL) {
        LLVMValueRef reason_arg = LLVMBuildGlobalStringPtr(ctx->builder,
            reason != NULL ? reason : PGY_FRONTIER_REASON_GENERIC_OVERFLOW,
            llvm_tmp_name(ctx));
        LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
            &reason_arg, 1, "");
    }
    LLVMBuildUnreachable(ctx->builder);
}

void
llvm_finish_domain_sync_emit(LLVMGenCtx *ctx,
                             LLVMValueRef saved_fn,
                             LLVMTypeRef saved_ret,
                             LLVMTypeRef saved_function_ret,
                             const char *saved_return_type_name,
                             ASTNode *saved_return_callable_type,
                             ASTNode *saved_host_decl,
                             LLVMBasicBlockRef saved_bb)
{
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    ctx->current_function_ret_type = saved_function_ret;
    ctx->current_return_type_name = saved_return_type_name;
    ctx->current_return_callable_type = saved_return_callable_type;
    llvm_restore_current_host_decl(ctx, saved_host_decl);

    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
}

#endif
