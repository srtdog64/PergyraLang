#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_methods_vtable_dispatch.h"

#include <stdio.h>

#include "llvm_internal_api.h"

static LLVMValueRef
llvm_vtable_dispatch_error(ASTNode *node, LLVMGenCtx *ctx,
                           const char *method_name,
                           const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM vtable method '%s' %s",
            method_name != NULL ? method_name : "<method>",
            message != NULL ? message : "could not be lowered");
    }
    return NULL;
}

/* Every role implementing an ability shares one method signature, so any
 * registered `<Role>_<Method>` impl yields the function type for dispatch.
 * Needed because LLVM-15 opaque pointers hide the vtable slot's pointee. */
static LLVMFuncEntry *
llvm_lookup_role_method_impl(LLVMGenCtx *ctx, const char *method_name)
{
    size_t mlen;

    if (ctx == NULL || method_name == NULL)
        return NULL;
    mlen = strlen(method_name);
    for (int i = 0; i < ctx->func_count; i++) {
        const char *name = ctx->functions[i].name;
        size_t nlen;

        if (name == NULL)
            continue;
        nlen = strlen(name);
        if (nlen <= mlen + 1)
            continue;
        if (name[nlen - mlen - 1] == '_'
            && strcmp(name + nlen - mlen, method_name) == 0)
            return &ctx->functions[i];
    }
    return NULL;
}

LLVMValueRef
llvm_emit_member_call_vtable_dispatch(ASTNode *node, LLVMGenCtx *ctx,
                                      ASTNode *obj_node,
                                      const char *method_name)
{
    if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
        && method_name != NULL) {
        ASTNode *party_node = ast_member_object(obj_node);
        const char *slot_name = ast_member_name(obj_node);

        if (party_node != NULL && party_node->type == AST_IDENTIFIER
            && slot_name != NULL) {
            const char *party_var = ast_identifier_name(party_node);
            const char *party_class = llvm_lookup_var_class(ctx, party_var);
            LLVMClassTypeEntry *cls = party_class
                ? llvm_lookup_class(ctx, party_class) : NULL;

            if (cls != NULL) {
                char vt_field[256];
                int vt_idx;
                snprintf(vt_field, sizeof(vt_field), "%s_vtable", slot_name);
                vt_idx = llvm_class_field_index(cls, vt_field);

                if (vt_idx >= 0) {
                    int method_idx = -1;
                    LLVMClassTypeEntry *vt_cls =
                        llvm_lookup_vtable_class_with_method(ctx, method_name,
                            &method_idx);

                    if (vt_cls != NULL && method_idx >= 0) {
                        LLVMVarEntry pvar;
                        bool has_pvar =
                            llvm_scope_lookup_snapshot(ctx, party_var, &pvar);
                        LLVMValueRef vt_ptr_field;
                        LLVMValueRef vt_raw;
                        LLVMTypeRef vt_ptr_ty;
                        LLVMValueRef vt_typed;
                        LLVMValueRef fn_ptr_field;
                        size_t argc = ast_call_arg_count(node);
                        LLVMTypeRef fn_ptr_ty;
                        LLVMTypeRef fn_type;
                        LLVMTypeRef ret_type;
                        LLVMValueRef fn_ptr;
                        LLVMValueRef *args;
                        LLVMValueRef result;

                        if (!has_pvar) {
                            return llvm_vtable_dispatch_error(node, ctx,
                                method_name,
                                "requires a registered receiver variable");
                        }
                        fn_ptr_ty =
                            llvm_class_field_type_at_index(vt_cls, method_idx);
                        if (fn_ptr_ty == NULL) {
                            return llvm_vtable_dispatch_error(node, ctx,
                                method_name,
                                "requires concrete function pointer metadata");
                        }
                        args = pgy_arena_calloc(&ctx->scratch,
                            (argc + 1) * sizeof(LLVMValueRef));
                        if (args == NULL) {
                            return llvm_vtable_dispatch_error(node, ctx,
                                method_name,
                                "could not allocate call arguments");
                        }

                        vt_ptr_field = LLVMBuildStructGEP2(ctx->builder,
                            cls->struct_type, pvar.alloca,
                            (unsigned)vt_idx, llvm_tmp_name(ctx));
                        vt_raw = LLVMBuildLoad2(ctx->builder,
                            ctx->type_i8ptr, vt_ptr_field, llvm_tmp_name(ctx));
                        vt_ptr_ty = LLVMPointerType(vt_cls->struct_type, 0);
                        vt_typed = LLVMBuildBitCast(ctx->builder, vt_raw,
                            vt_ptr_ty, llvm_tmp_name(ctx));
                        fn_ptr_field = LLVMBuildStructGEP2(ctx->builder,
                            vt_cls->struct_type, vt_typed,
                            (unsigned)method_idx, llvm_tmp_name(ctx));
                        /* LLVM-15 opaque pointers: the vtable slot is `ptr`,
                         * so the function type is recovered from a registered
                         * role implementation rather than the slot pointee. */
                        {
                            LLVMFuncEntry *rep =
                                llvm_lookup_role_method_impl(ctx, method_name);
                            if (rep == NULL || rep->fn_type == NULL) {
                                return llvm_vtable_dispatch_error(node, ctx,
                                    method_name,
                                    "requires a registered role implementation");
                            }
                            fn_type = rep->fn_type;
                        }
                        ret_type = LLVMGetReturnType(fn_type);
                        fn_ptr = LLVMBuildLoad2(ctx->builder, fn_ptr_ty,
                            fn_ptr_field, llvm_tmp_name(ctx));

                        args[0] = LLVMBuildBitCast(ctx->builder,
                            pvar.alloca, ctx->type_i8ptr,
                            llvm_tmp_name(ctx));
                        for (size_t ai = 0; ai < argc; ai++) {
                            args[ai + 1] = llvm_emit_expression(
                                ast_call_argument(node, ai), ctx);
                            if (args[ai + 1] == NULL) {
                                return llvm_vtable_dispatch_error(node, ctx,
                                    method_name,
                                    "could not lower call argument");
                            }
                        }

                        if (ret_type == ctx->type_void) {
                            LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                                args, (unsigned)(argc + 1), "");
                            result = llvm_void_expression_placeholder(ctx,
                                node, method_name);
                        } else {
                            result = LLVMBuildCall2(ctx->builder, fn_type,
                                fn_ptr, args, (unsigned)(argc + 1),
                                llvm_tmp_name(ctx));
                        }
                        return result;
                    }
                }
            }
        }
    }

    return NULL;
}

#endif
