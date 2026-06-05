#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_variable.h"

#include "llvm_expr_call_dispatch.h"
#include "llvm_expr_member_lvalue.h"
#include "llvm_expr_scalar_core.h"
#include "llvm_internal_api.h"

LLVMValueRef
llvm_emit_callable_variable_call(ASTNode *node,
                                 LLVMGenCtx *ctx,
                                 const char *callee_name,
                                 LLVMValueRef *args,
                                 unsigned emitted_argc)
{
    LLVMVarEntry callee_var;
    bool has_callee_var = false;
    LLVMValueRef fn_ptr = NULL;
    LLVMTypeRef fn_type = NULL;
    LLVMTypeRef callable_ptr_ty = NULL;
    LLVMCallableVarEntry *callable_entry = NULL;
    LLVMValueRef result;

    if (node == NULL || ctx == NULL || callee_name == NULL
        || ast_call_callee(node) == NULL
        || ast_call_callee(node)->type != AST_IDENTIFIER)
        return NULL;

    has_callee_var = llvm_scope_lookup_snapshot(ctx, callee_name, &callee_var);
    if (!has_callee_var)
        return NULL;

    callable_entry = llvm_lookup_callable_entry(ctx, callee_name);
    if (LLVMGetTypeKind(callee_var.type) == LLVMPointerTypeKind)
        fn_type = LLVMGetElementType(callee_var.type);
    if (fn_type == NULL || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind) {
        if (callable_entry != NULL) {
            fn_type = llvm_function_signature_from_callable_entry(ctx, callable_entry);
            if (ctx->has_error || fn_type == NULL)
                return llvm_call_error_recovery(ctx, node,
                    "LLVM callable variable call could not lower callable signature");
        }
    }
    if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind)
        callable_ptr_ty = LLVMPointerType(fn_type, 0);

    if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMPointerTypeKind) {
        callable_ptr_ty = fn_type;
        fn_type = LLVMGetElementType(fn_type);
    }
    if (callable_ptr_ty == NULL
        && fn_type != NULL
        && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind) {
        callable_ptr_ty = LLVMPointerType(fn_type, 0);
    }

    fn_ptr = llvm_emit_expression(ast_call_callee(node), ctx);
    if (fn_ptr == NULL)
        return llvm_call_error_recovery(ctx, node,
            "LLVM callable variable call could not lower callee expression");
    if (fn_ptr != NULL && callable_ptr_ty != NULL
        && LLVMTypeOf(fn_ptr) != callable_ptr_ty) {
        fn_ptr = LLVMBuildBitCast(ctx->builder, fn_ptr, callable_ptr_ty,
            llvm_tmp_name(ctx));
    }
    if (fn_type == NULL && LLVMGetTypeKind(LLVMTypeOf(fn_ptr)) == LLVMFunctionTypeKind)
        fn_type = LLVMTypeOf(fn_ptr);
    if (fn_type == NULL || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind)
        return NULL;

    if (LLVMGetReturnType(fn_type) == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, fn_type, fn_ptr, args, emitted_argc, "");
        result = llvm_void_expression_placeholder(ctx, node,
            "callable-variable-call");
    } else {
        result = LLVMBuildCall2(ctx->builder, fn_type, fn_ptr, args,
            emitted_argc, llvm_tmp_name(ctx));
    }
    return result;
}

#endif
