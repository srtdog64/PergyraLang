#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_array_raw_nominal_calls.h"

#include <string.h>

#include "llvm_internal_api.h"

static bool
llvm_array_raw_nominal_error_out(ASTNode *node, LLVMGenCtx *ctx,
                                 const char *message, LLVMValueRef *out)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM raw nominal Array<T> operation could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

bool
llvm_array_entry_uses_raw_nominal(LLVMGenCtx *ctx, LLVMArrayVarEntry *entry)
{
    const char *suffix;

    if (ctx == NULL || entry == NULL || entry->elem_type == NULL)
        return false;
    suffix = llvm_type_to_suffix(ctx, entry->elem_type);
    if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
        return false;
    return entry->elem_name != NULL && entry->elem_name[0] != '\0';
}

static LLVMValueRef
llvm_array_raw_nominal_elem_size_i64(LLVMGenCtx *ctx, LLVMTypeRef elem_type)
{
    LLVMValueRef size = LLVMSizeOf(elem_type);
    if (LLVMTypeOf(size) != ctx->type_i64)
        size = LLVMBuildZExtOrBitCast(ctx->builder, size, ctx->type_i64,
            llvm_tmp_name(ctx));
    return size;
}

static LLVMValueRef
llvm_array_raw_nominal_array_ptr(LLVMGenCtx *ctx, LLVMValueRef arr_alloca)
{
    return LLVMBuildBitCast(ctx->builder, arr_alloca, ctx->type_i8ptr,
        llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_array_raw_nominal_value_ptr(LLVMGenCtx *ctx, LLVMTypeRef elem_type,
                                 LLVMValueRef value)
{
    LLVMValueRef value_alloca = llvm_create_entry_alloca(ctx, elem_type,
        llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, value, value_alloca);
    return LLVMBuildBitCast(ctx->builder, value_alloca, ctx->type_i8ptr,
        llvm_tmp_name(ctx));
}

static bool
llvm_array_emit_raw_nominal_call(LLVMGenCtx *ctx, ASTNode *node,
                                 const char *callee_name,
                                 const char *runtime_name,
                                 LLVMValueRef *args,
                                 unsigned arg_count,
                                 LLVMValueRef *out)
{
    LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
        "array", callee_name, runtime_name);
    if (fn == NULL)
        return llvm_array_raw_nominal_error_out(node, ctx,
            "LLVM raw nominal Array<T> operation requires registered runtime function",
            out);
    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, arg_count, "");
    *out = llvm_void_expression_placeholder(ctx, node, callee_name);
    return true;
}

bool
llvm_array_emit_raw_nominal_push(LLVMGenCtx *ctx, ASTNode *node,
                                 const char *callee_name,
                                 LLVMValueRef arr_alloca,
                                 LLVMArrayVarEntry *entry,
                                 LLVMValueRef value,
                                 LLVMValueRef *out)
{
    LLVMValueRef args[] = {
        llvm_array_raw_nominal_array_ptr(ctx, arr_alloca),
        llvm_array_raw_nominal_value_ptr(ctx, entry->elem_type, value),
        llvm_array_raw_nominal_elem_size_i64(ctx, entry->elem_type)
    };
    return llvm_array_emit_raw_nominal_call(ctx, node, callee_name,
        "pgy_array_push_raw_export", args, 3, out);
}

bool
llvm_array_emit_raw_nominal_set(LLVMGenCtx *ctx, ASTNode *node,
                                const char *callee_name,
                                LLVMValueRef arr_alloca,
                                LLVMArrayVarEntry *entry,
                                LLVMValueRef index64,
                                LLVMValueRef value,
                                LLVMValueRef *out)
{
    LLVMValueRef args[] = {
        llvm_array_raw_nominal_array_ptr(ctx, arr_alloca),
        index64,
        llvm_array_raw_nominal_value_ptr(ctx, entry->elem_type, value),
        llvm_array_raw_nominal_elem_size_i64(ctx, entry->elem_type)
    };
    return llvm_array_emit_raw_nominal_call(ctx, node, callee_name,
        "pgy_array_set_raw_export", args, 4, out);
}

bool
llvm_array_emit_raw_nominal_pop(LLVMGenCtx *ctx, ASTNode *node,
                                const char *callee_name,
                                LLVMValueRef arr_alloca,
                                LLVMArrayVarEntry *entry,
                                LLVMValueRef *out)
{
    LLVMValueRef args[] = {
        llvm_array_raw_nominal_array_ptr(ctx, arr_alloca)
    };
    (void)entry;
    return llvm_array_emit_raw_nominal_call(ctx, node, callee_name,
        "pgy_array_pop_raw_export", args, 1, out);
}

#endif
