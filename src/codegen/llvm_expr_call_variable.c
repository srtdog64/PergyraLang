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
                                 const char *callee_name)
{
    LLVMVarEntry callee_var;
    bool has_callee_var = false;
    LLVMValueRef fn_ptr = NULL;
    LLVMTypeRef fn_type = NULL;
    LLVMTypeRef callable_ptr_ty = NULL;
    LLVMCallableVarEntry callable_snapshot;
    const LLVMCallableVarEntry *callable_entry = NULL;
    LLVMValueRef result;

    if (node == NULL || ctx == NULL || callee_name == NULL
        || ast_call_callee(node) == NULL
        || ast_call_callee(node)->type != AST_IDENTIFIER)
        return NULL;

    has_callee_var = llvm_scope_lookup_snapshot(ctx, callee_name, &callee_var);
    if (!has_callee_var)
        return llvm_call_error_recovery(ctx, node,
            "LLVM callable variable call requires local storage metadata");

    callable_entry = llvm_lookup_callable_entry(ctx, callee_name);
    if (callable_entry == NULL)
        return llvm_call_error_recovery(ctx, node,
            "LLVM callable variable call requires callable signature metadata");
    /* Recursive argument emission can grow both registries. Do not retain
     * their borrowed entries across it; signature payloads are owner-stable. */
    callable_snapshot = *callable_entry;
    callable_entry = &callable_snapshot;
    fn_type = llvm_function_signature_from_callable_entry(ctx, callable_entry);
    if (ctx->has_error || fn_type == NULL
        || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind)
        return llvm_call_error_recovery(ctx, node,
            "LLVM callable variable call could not lower callable signature");

    size_t argc = ast_call_arg_count(node);
    bool is_closure = LLVMGetTypeKind(callee_var.type) == LLVMStructTypeKind;
    if (argc != LLVMCountParamTypes(fn_type) || argc > 64u
        || (is_closure && argc + 1u > 16u))
        return llvm_call_error_recovery(ctx, node,
            "LLVM callable variable call argument count mismatch");
    unsigned emitted_argc = (unsigned)argc;
    LLVMValueRef args[64];
    for (size_t i = 0; i < argc; i++) {
        ASTNode *arg = ast_call_argument(node, i);
        LLVMTypeRef arg_type = llvm_stmt_infer_expr_type(ctx, arg);
        if (ctx->has_error)
            return NULL;
        if (arg_type == ctx->type_void)
            return llvm_call_error_recovery(ctx, node,
                "LLVM callable variable call cannot consume a Void argument");
        args[i] = llvm_emit_expression(arg, ctx);
        if (ctx->has_error)
            return NULL;
        if (args[i] == NULL)
            return llvm_call_arg_error_recovery(ctx, node, callee_name, i);
    }

    /* A callable variable whose storage is a struct (not a bare function
     * pointer) is a closure value { fn, env } (docs/135 Stage A). The
     * is_closure registry flag is advisory; the variable type is authoritative
     * across both the AST and MIR let-lowering paths. */
    if (is_closure) {
        /* Closure dispatch: load fn from field 0 and pass &env (field 1) as the
         * hidden leading argument. */
        LLVMTypeRef clo_ty = callee_var.type;
        LLVMTypeRef base_fn_ty = fn_type;
        unsigned base_argc = LLVMCountParamTypes(base_fn_ty);
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
    callable_ptr_ty = LLVMPointerType(fn_type, 0);

    fn_ptr = llvm_emit_expression(ast_call_callee(node), ctx);
    if (fn_ptr == NULL)
        return llvm_call_error_recovery(ctx, node,
            "LLVM callable variable call could not lower callee expression");
    if (fn_ptr != NULL && callable_ptr_ty != NULL
        && LLVMTypeOf(fn_ptr) != callable_ptr_ty) {
        fn_ptr = LLVMBuildBitCast(ctx->builder, fn_ptr, callable_ptr_ty,
            llvm_tmp_name(ctx));
    }
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
