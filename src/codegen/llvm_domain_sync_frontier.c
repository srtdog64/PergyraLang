#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_sync_frontier.h"

void
llvm_emit_sync_generation_increment(LLVMGenCtx *ctx,
                                    LLVMClassTypeEntry *decl_cls,
                                    LLVMValueRef self_ptr)
{
    int generation_idx;
    LLVMValueRef generation_ptr;
    LLVMValueRef generation_val;

    if (ctx == NULL || decl_cls == NULL || self_ptr == NULL)
        return;

    generation_idx = llvm_class_field_index(decl_cls, "__sync_generation");
    if (generation_idx < 0)
        return;

    generation_ptr = LLVMBuildStructGEP2(ctx->builder, decl_cls->struct_type,
        self_ptr, (unsigned)generation_idx, llvm_tmp_name(ctx));
    generation_val = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
        generation_ptr, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMBuildAdd(ctx->builder, generation_val,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx)),
        generation_ptr);
}

void
llvm_emit_frontier_overflow_abort(LLVMGenCtx *ctx, const char *reason)
{
    LLVMTypeRef panic_ft;
    LLVMFuncEntry *panic_fn;

    if (ctx == NULL)
        return;

    panic_ft = LLVMFunctionType(ctx->type_void, &ctx->type_i8ptr, 1, 0);
    panic_fn = llvm_lookup_or_create_function(ctx,
        "pgy_runtime_panic_internal_invariant_export",
        panic_ft, ctx->type_void);
    if (panic_fn != NULL) {
        LLVMValueRef reason_arg = LLVMBuildGlobalStringPtr(ctx->builder,
            reason != NULL ? reason : "frontier recompute exceeded bounded pass limit",
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
                             ASTNode *saved_host_decl)
{
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    llvm_restore_current_host_decl(ctx, saved_host_decl);

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
}

#endif
