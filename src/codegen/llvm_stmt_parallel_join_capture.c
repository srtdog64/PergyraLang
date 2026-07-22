#ifdef PGY_LLVM_ENABLED
#include "llvm_stmt_parallel_join_capture.h"
#include <string.h>

void
llvm_parallel_join_set_error(LLVMGenCtx *ctx, ASTNode *node,
                             const char *fmt, const char *arg)
{
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        fmt, arg != NULL ? arg : "<binding>");
}

bool
llvm_parallel_join_collect_captures(
    LLVMGenCtx *ctx,
    ASTNode *node,
    ASTNode *body,
    const char *collection_name,
    const MIRParallelCaptureBoundaryFact *capture_boundary,
    LLVMParallelJoinCapture captures[PJOIN_MAX_CAPTURES],
    size_t *capture_count_out)
{
    size_t capture_count = 0;

    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count; j++) {
            const char *name = frame->entries[j].name;
            if (!llvm_capture_entry_is_required(ctx, body, frame, j))
                continue;
            if (collection_name != NULL && name != NULL
                && strcmp(name, collection_name) == 0) {
                continue;
            }
            if (capture_count >= PJOIN_MAX_CAPTURES) {
                llvm_parallel_join_set_error(ctx, node,
                    "LLVM parallel join capture registry overflow%s", "");
                return false;
            }
            if (frame->entries[j].alloca == NULL) {
                llvm_parallel_join_set_error(ctx, node,
                    "LLVM parallel join capture requires storage-backed binding '%s'",
                    name);
                return false;
            }

            LLVMArrayVarEntry *array = llvm_lookup_array_var(ctx, name);
            bool index_admitted = array != NULL
                && pgy_verified_parallel_capture_disposition_find(
                    ctx->parallel_capture_plan, capture_boundary, name,
                    MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT) != NULL;
            bool readonly_admitted = array != NULL
                && pgy_verified_parallel_capture_disposition_find(
                    ctx->parallel_capture_plan, capture_boundary, name,
                    MIR_PARALLEL_CAPTURE_JOIN_READONLY) != NULL;
            bool array_admitted = index_admitted || readonly_admitted;
            if (!array_admitted
                && llvm_capture_reject_shared_collection(
                    ctx, node, "parallel join", name, true)) {
                return false;
            }

            captures[capture_count] = (LLVMParallelJoinCapture){
                name,
                frame->entries[j].alloca,
                frame->entries[j].type,
                llvm_lookup_channel_inner(ctx, name),
                llvm_lookup_future_inner(ctx, name),
                llvm_lookup_slot_inner(ctx, name),
                llvm_lookup_future_is_remote(ctx, name),
                llvm_lookup_slot_is_secure(ctx, name),
                array_admitted,
                array_admitted ? array->elem_type : NULL,
                array_admitted ? array->elem_name : NULL
            };
            if (captures[capture_count].slot_inner != NULL) {
                llvm_parallel_join_set_error(ctx, node,
                    "parallel join rung 0 does not carry slot captures yet ('%s'); a later rung will (docs/181 SS1.4)",
                    name);
                return false;
            }
            capture_count++;
        }
    }

    *capture_count_out = capture_count;
    return true;
}

bool
llvm_parallel_join_emit_alias_guard(
    LLVMGenCtx *ctx,
    ASTNode *node,
    const MIRParallelCaptureBoundaryFact *capture_boundary,
    const LLVMParallelJoinCapture captures[PJOIN_MAX_CAPTURES],
    size_t capture_count)
{
    for (size_t wi = 0; wi < capture_count; wi++) {
        if (!captures[wi].is_admitted_array
            || pgy_verified_parallel_capture_disposition_find(
                   ctx->parallel_capture_plan, capture_boundary,
                   captures[wi].name,
                   MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT) == NULL) {
            continue;
        }
        for (size_t ri = 0; ri < capture_count; ri++) {
            if (!captures[ri].is_admitted_array
                || pgy_verified_parallel_capture_disposition_find(
                       ctx->parallel_capture_plan, capture_boundary,
                       captures[ri].name,
                       MIR_PARALLEL_CAPTURE_JOIN_READONLY) == NULL) {
                continue;
            }
            LLVMFuncEntry *panic_fn = llvm_lookup_function(ctx,
                "pgy_runtime_panic_authority_mismatch_export");
            if (panic_fn == NULL) {
                llvm_parallel_join_set_error(ctx, node,
                    "parallel join alias check requires the registered panic runtime%s",
                    "");
                return false;
            }
            LLVMTypeRef w_data_type = LLVMStructGetTypeAtIndex(
                captures[wi].type, 0);
            LLVMTypeRef r_data_type = LLVMStructGetTypeAtIndex(
                captures[ri].type, 0);
            LLVMValueRef w_field = LLVMBuildStructGEP2(ctx->builder,
                captures[wi].type, captures[wi].alloca, 0,
                llvm_tmp_name(ctx));
            LLVMValueRef w_data = LLVMBuildLoad2(ctx->builder, w_data_type,
                w_field, llvm_tmp_name(ctx));
            LLVMValueRef r_field = LLVMBuildStructGEP2(ctx->builder,
                captures[ri].type, captures[ri].alloca, 0,
                llvm_tmp_name(ctx));
            LLVMValueRef r_data = LLVMBuildLoad2(ctx->builder, r_data_type,
                r_field, llvm_tmp_name(ctx));
            LLVMValueRef w8 = LLVMBuildBitCast(ctx->builder, w_data,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMValueRef r8 = LLVMBuildBitCast(ctx->builder, r_data,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            LLVMValueRef same = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                w8, r8, llvm_tmp_name(ctx));
            LLVMValueRef nonnull = LLVMBuildICmp(ctx->builder, LLVMIntNE,
                w8, LLVMConstPointerNull(ctx->type_i8ptr),
                llvm_tmp_name(ctx));
            LLVMValueRef aliased = LLVMBuildAnd(ctx->builder, same,
                nonnull, llvm_tmp_name(ctx));
            LLVMBasicBlockRef panic_block = LLVMAppendBasicBlockInContext(
                ctx->context, ctx->current_function, "pj.alias.panic");
            LLVMBasicBlockRef continue_block = LLVMAppendBasicBlockInContext(
                ctx->context, ctx->current_function, "pj.alias.cont");
            LLVMBuildCondBr(ctx->builder, aliased, panic_block,
                            continue_block);
            LLVMPositionBuilderAtEnd(ctx->builder, panic_block);
            LLVMValueRef panic_args[] = {
                LLVMBuildGlobalStringPtr(ctx->builder,
                    "parallel join read-only capture aliases an index-written array",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
                panic_args, 1, "");
            LLVMBuildUnreachable(ctx->builder);
            LLVMPositionBuilderAtEnd(ctx->builder, continue_block);
        }
    }
    return true;
}

#endif /* PGY_LLVM_ENABLED */
