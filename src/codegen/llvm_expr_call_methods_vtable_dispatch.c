#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_methods_vtable_dispatch.h"

#include <stdio.h>
#include <string.h>

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

LLVMValueRef
llvm_emit_member_call_vtable_dispatch(ASTNode *node, LLVMGenCtx *ctx,
                                      ASTNode *obj_node,
                                      const char *method_name)
{
    if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
        && method_name != NULL) {
        ASTNode *party_node = obj_node->data.member.object;
        const char *slot_name = obj_node->data.member.name;

        if (party_node != NULL && party_node->type == AST_IDENTIFIER
            && slot_name != NULL) {
            const char *party_var = party_node->data.identifier.name;
            const char *party_class = llvm_lookup_var_class(ctx, party_var);
            LLVMClassTypeEntry *cls = party_class
                ? llvm_lookup_class(ctx, party_class) : NULL;

            if (cls != NULL) {
                char vt_field[256];
                int vt_idx = -1;
                snprintf(vt_field, sizeof(vt_field), "%s_vtable", slot_name);
                for (int fi = 0; fi < cls->field_count; fi++) {
                    if (strcmp(cls->fields[fi].field_name, vt_field) == 0) {
                        vt_idx = cls->fields[fi].index;
                        break;
                    }
                }

                if (vt_idx >= 0) {
                    int method_idx = -1;
                    LLVMClassTypeEntry *vt_cls = NULL;

                    for (int ci = 0; ci < ctx->class_type_count; ci++) {
                        const char *cn = ctx->class_types[ci].class_name;
                        if (cn != NULL && strstr(cn, "_vtable") != NULL) {
                            for (int fi = 0; fi < ctx->class_types[ci].field_count; fi++) {
                                if (strcmp(ctx->class_types[ci].fields[fi].field_name,
                                           method_name) == 0) {
                                    vt_cls = &ctx->class_types[ci];
                                    method_idx = ctx->class_types[ci].fields[fi].index;
                                    break;
                                }
                            }
                            if (method_idx >= 0)
                                break;
                        }
                    }

                    if (vt_cls != NULL && method_idx >= 0) {
                        LLVMVarEntry *pvar = llvm_scope_lookup(ctx, party_var);
                        LLVMValueRef vt_ptr_field;
                        LLVMValueRef vt_raw;
                        LLVMTypeRef vt_ptr_ty;
                        LLVMValueRef vt_typed;
                        LLVMValueRef fn_ptr_field;
                        size_t argc = node->data.call.arg_count;
                        LLVMTypeRef fn_ptr_ty;
                        LLVMTypeRef fn_type;
                        LLVMTypeRef ret_type;
                        LLVMValueRef fn_ptr;
                        LLVMValueRef *args;
                        LLVMValueRef result;

                        if (pvar == NULL) {
                            return llvm_vtable_dispatch_error(node, ctx,
                                method_name,
                                "requires a registered receiver variable");
                        }
                        fn_ptr_ty = vt_cls->fields[method_idx].field_type;
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
                            cls->struct_type, pvar->alloca,
                            (unsigned)vt_idx, llvm_tmp_name(ctx));
                        vt_raw = LLVMBuildLoad2(ctx->builder,
                            ctx->type_i8ptr, vt_ptr_field, llvm_tmp_name(ctx));
                        vt_ptr_ty = LLVMPointerType(vt_cls->struct_type, 0);
                        vt_typed = LLVMBuildBitCast(ctx->builder, vt_raw,
                            vt_ptr_ty, llvm_tmp_name(ctx));
                        fn_ptr_field = LLVMBuildStructGEP2(ctx->builder,
                            vt_cls->struct_type, vt_typed,
                            (unsigned)method_idx, llvm_tmp_name(ctx));
                        fn_type = LLVMGetElementType(fn_ptr_ty);
                        ret_type = LLVMGetReturnType(fn_type);
                        fn_ptr = LLVMBuildLoad2(ctx->builder, fn_ptr_ty,
                            fn_ptr_field, llvm_tmp_name(ctx));

                        args[0] = LLVMBuildBitCast(ctx->builder,
                            pvar->alloca, ctx->type_i8ptr,
                            llvm_tmp_name(ctx));
                        for (size_t ai = 0; ai < argc; ai++) {
                            args[ai + 1] = llvm_emit_expression(
                                node->data.call.arguments[ai], ctx);
                            if (args[ai + 1] == NULL) {
                                return llvm_vtable_dispatch_error(node, ctx,
                                    method_name,
                                    "could not lower call argument");
                            }
                        }

                        if (ret_type == ctx->type_void) {
                            LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                                args, (unsigned)(argc + 1), "");
                            result = LLVMConstInt(ctx->type_i32, 0, 0);
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
