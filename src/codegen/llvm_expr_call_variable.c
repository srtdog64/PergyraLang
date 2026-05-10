#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_variable.h"

#include <string.h>

#include "llvm_expr_boundary_projection_helpers.h"
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
    LLVMVarEntry *callee_var = NULL;
    LLVMValueRef fn_ptr = NULL;
    LLVMTypeRef fn_type = NULL;
    LLVMTypeRef callable_ptr_ty = NULL;
    LLVMCallableVarEntry *callable_entry = NULL;
    LLVMValueRef result;

    if (node == NULL || ctx == NULL || callee_name == NULL
        || node->data.call.callee == NULL
        || node->data.call.callee->type != AST_IDENTIFIER)
        return NULL;

    callee_var = llvm_scope_lookup(ctx, callee_name);
    if (callee_var == NULL)
        return NULL;

    callable_entry = llvm_lookup_callable_entry(ctx, callee_name);
    if (LLVMGetTypeKind(callee_var->type) == LLVMPointerTypeKind)
        fn_type = LLVMGetElementType(callee_var->type);
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

    if ((fn_type == NULL || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind)
        && ctx->current_function != NULL) {
        const char *current_name = LLVMGetValueName(ctx->current_function);
        ASTNode *current_decl = current_name != NULL
            ? llvm_find_function_decl(ctx, current_name) : NULL;
        if (current_decl != NULL && current_decl->type == AST_FUNC_DECL) {
            for (size_t i = 0; i < current_decl->data.func_decl.param_count; i++) {
                FuncParam *p = current_decl->data.func_decl.params[i];
                if (p == NULL || p->name == NULL || p->type == NULL)
                    continue;
                if (strcmp(p->name, callee_name) != 0)
                    continue;
                if (p->type->type == AST_EVENT_HANDLER_TYPE) {
                    size_t pc = p->type->data.event_handler_type.param_count;
                    LLVMTypeRef *pts = NULL;
                    LLVMTypeRef ret = ctx->type_void;
                    if (p->type->data.event_handler_type.return_type != NULL) {
                        ret = ast_type_to_llvm(ctx,
                            p->type->data.event_handler_type.return_type);
                        if (ctx->has_error || ret == NULL)
                            return llvm_call_error_recovery(ctx, node,
                                "LLVM event-handler callable could not lower return type");
                    }
                    if (pc > 0) {
                        pts = pgy_arena_calloc(&ctx->scratch,
                            pc * sizeof(LLVMTypeRef));
                        if (pts == NULL) {
                            return llvm_call_error_recovery(ctx, node,
                                "LLVM event-handler callable parameter allocation failed");
                        }
                        for (size_t pi = 0; pi < pc; pi++) {
                            pts[pi] = ast_type_to_llvm(ctx,
                                p->type->data.event_handler_type.param_types[pi]);
                            if (ctx->has_error || pts[pi] == NULL)
                                return llvm_call_error_recovery(ctx, node,
                                    "LLVM event-handler callable could not lower parameter type");
                        }
                    }
                    fn_type = LLVMFunctionType(ret, pts, (unsigned)pc, 0);
                } else {
                    LLVMTypeRef declared_ptr_ty = ast_type_to_llvm(ctx, p->type);
                    if (ctx->has_error || declared_ptr_ty == NULL)
                        return llvm_call_error_recovery(ctx, node,
                            "LLVM callable parameter declaration type could not be lowered");
                    if (LLVMGetTypeKind(declared_ptr_ty) == LLVMPointerTypeKind)
                        fn_type = LLVMGetElementType(declared_ptr_ty);
                    else
                        fn_type = declared_ptr_ty;
                }
                break;
            }
        }
    }

    if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMPointerTypeKind) {
        callable_ptr_ty = fn_type;
        fn_type = LLVMGetElementType(fn_type);
    }
    if (callable_ptr_ty == NULL
        && fn_type != NULL
        && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind) {
        callable_ptr_ty = LLVMPointerType(fn_type, 0);
    }

    fn_ptr = llvm_emit_expression(node->data.call.callee, ctx);
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
        result = LLVMConstInt(ctx->type_i32, 0, 0);
    } else {
        result = LLVMBuildCall2(ctx->builder, fn_type, fn_ptr, args,
            emitted_argc, llvm_tmp_name(ctx));
    }
    return result;
}

#endif
