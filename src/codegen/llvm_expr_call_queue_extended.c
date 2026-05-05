#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_queue_extended.h"

#include <string.h>

#include "llvm_expr_call_collections_extended.h"
#include "llvm_internal_api.h"

bool
llvm_emit_queue_extended_call(ASTNode *node, LLVMGenCtx *ctx,
                              const char *callee_name,
                              LLVMValueRef *out)
{
    if (strcmp(callee_name, "QueuePush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        inner_name = llvm_lookup_queue_inner(ctx, queue_arg->data.identifier.name);
        if (queue_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        elem_ty = llvm_collection_required_value_type(ctx, node, "Queue",
            queue_arg->data.identifier.name, inner_name, out);
        if (elem_ty == NULL)
            return true;
        value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_queue_push_raw_export");
        if (fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                llvm_sizeof_type_i64(ctx, elem_ty)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        }
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }

    if (strcmp(callee_name, "QueuePop") == 0 && node->data.call.arg_count == 1) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        inner_name = llvm_lookup_queue_inner(ctx, queue_arg->data.identifier.name);
        if (queue_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        elem_ty = llvm_collection_required_value_type(ctx, node, "Queue",
            queue_arg->data.identifier.name, inner_name, out);
        if (elem_ty == NULL)
            return true;
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, LLVMConstNull(elem_ty), tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_queue_pop_raw_export");
        if (fn == NULL)
            { *out = LLVMConstNull(elem_ty); return true; }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        { *out = LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }

    if (strcmp(callee_name, "QueueSize") == 0 && node->data.call.arg_count == 1) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        if (queue_var == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_queue_size_raw_export");
        if (fn == NULL)
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx)); return true; }
        }
    }

    if (strcmp(callee_name, "QueueEmpty") == 0 && node->data.call.arg_count == 1) {
        ASTNode *queue_arg = node->data.call.arguments[0];
        LLVMVarEntry *queue_var;
        LLVMFuncEntry *fn;
        if (queue_arg == NULL || queue_arg->type != AST_IDENTIFIER)
            { *out = LLVMConstInt(ctx->type_i1, 1, 0); return true; }
        queue_var = llvm_scope_lookup(ctx, queue_arg->data.identifier.name);
        if (queue_var == NULL)
            { *out = LLVMConstInt(ctx->type_i1, 1, 0); return true; }
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_queue_empty_raw_export");
        if (fn == NULL)
            { *out = LLVMConstInt(ctx->type_i1, 1, 0); return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx)); return true; }
        }
    }

    return false;
}

#endif
