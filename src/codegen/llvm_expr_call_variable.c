#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_variable.h"

#include "llvm_expr_call_dispatch.h"
#include "llvm_expr_member_lvalue.h"
#include "llvm_expr_scalar_core.h"
#include "llvm_internal_api.h"
#include "llvm_mir_store_coercion.h"

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

    /* A callable variable whose storage is a struct (not a bare function
     * pointer) is a closure value { fn, env } (docs/135 Stage A). The
     * is_closure registry flag is advisory; the variable type is authoritative
     * across both the AST and MIR let-lowering paths. */
    if (callable_entry != NULL
        && LLVMGetTypeKind(callee_var.type) == LLVMStructTypeKind) {
        /* Closure dispatch: load fn from field 0 and pass &env (field 1) as the
         * hidden leading argument. */
        LLVMTypeRef clo_ty = callee_var.type;
        LLVMTypeRef base_fn_ty =
            llvm_function_signature_from_callable_entry(ctx, callable_entry);
        if (ctx->has_error || base_fn_ty == NULL
            || LLVMGetTypeKind(clo_ty) != LLVMStructTypeKind)
            return llvm_call_error_recovery(ctx, node,
                "LLVM closure call could not lower the callee signature");

        unsigned base_argc = LLVMCountParamTypes(base_fn_ty);
        if (base_argc != emitted_argc || emitted_argc + 1u > 16u)
            return llvm_call_error_recovery(ctx, node,
                "LLVM closure call argument count mismatch");

        LLVMTypeRef env_ty = LLVMStructGetTypeAtIndex(clo_ty, 1);
        LLVMTypeRef fn_ptr_ty = LLVMStructGetTypeAtIndex(clo_ty, 0);
        LLVMTypeRef base_params[16];
        LLVMTypeRef call_params[17];
        LLVMValueRef call_args[17];
        LLVMGetParamTypes(base_fn_ty, base_params);
        call_params[0] = LLVMPointerType(env_ty, 0);
        for (unsigned i = 0; i < base_argc; i++)
            call_params[i + 1] = base_params[i];
        LLVMTypeRef call_fn_ty = LLVMFunctionType(
            LLVMGetReturnType(base_fn_ty), call_params, base_argc + 1, 0);

        LLVMValueRef fn_addr = LLVMBuildStructGEP2(ctx->builder, clo_ty,
            callee_var.alloca, 0, llvm_tmp_name(ctx));
        LLVMValueRef fn_ptr_val = LLVMBuildLoad2(ctx->builder, fn_ptr_ty,
            fn_addr, llvm_tmp_name(ctx));
        LLVMValueRef env_ptr = LLVMBuildStructGEP2(ctx->builder, clo_ty,
            callee_var.alloca, 1, llvm_tmp_name(ctx));

        call_args[0] = env_ptr;
        /* Coerce each argument to the closure's declared parameter type; the
         * args were emitted with their natural types (e.g. an Int literal is
         * i32 but a Long param is i64), so an uncoerced call fails LLVM verify. */
        for (unsigned i = 0; i < emitted_argc; i++)
            call_args[i + 1] = llvm_mir_coerce_value_for_store(ctx, args[i],
                base_params[i]);

        if (LLVMGetReturnType(base_fn_ty) == ctx->type_void) {
            LLVMBuildCall2(ctx->builder, call_fn_ty, fn_ptr_val, call_args,
                emitted_argc + 1, "");
            return llvm_void_expression_placeholder(ctx, node, "closure-call");
        }
        return LLVMBuildCall2(ctx->builder, call_fn_ty, fn_ptr_val, call_args,
            emitted_argc + 1, llvm_tmp_name(ctx));
    }

    /* LLVM-15 opaque pointers: a function-typed variable is just `ptr`, so the
     * function type cannot be read back from it via LLVMGetElementType (that
     * dereferences a null pointee and crashes). Recover the signature from the
     * callable entry's recorded metadata instead. */
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

    unsigned fn_argc = LLVMCountParamTypes(fn_type);
    if (fn_argc != emitted_argc || emitted_argc > 64u)
        return llvm_call_error_recovery(ctx, node,
            "LLVM callable variable call argument count mismatch");

    LLVMTypeRef param_types[64];
    LLVMValueRef coerced_args[64];
    LLVMGetParamTypes(fn_type, param_types);
    for (unsigned i = 0; i < emitted_argc; i++)
        coerced_args[i] = llvm_mir_coerce_value_for_store(ctx, args[i],
            param_types[i]);

    if (LLVMGetReturnType(fn_type) == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, fn_type, fn_ptr, coerced_args,
            emitted_argc, "");
        result = llvm_void_expression_placeholder(ctx, node,
            "callable-variable-call");
    } else {
        result = LLVMBuildCall2(ctx->builder, fn_type, fn_ptr, coerced_args,
            emitted_argc, llvm_tmp_name(ctx));
    }
    return result;
}

#endif
