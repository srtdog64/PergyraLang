#ifdef PGY_LLVM_ENABLED

#include "llvm_member_call_emit.h"

#include <string.h>

#include "llvm_expr_call_methods_domain_slice.h"
#include "llvm_expr_call_methods_vtable_dispatch.h"
#include "llvm_expr_call_methods_world_effect_sync.h"
#include "llvm_expr_call_projection_sync.h"
#include "llvm_expr_member_lvalue.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_host_methods.h"
#include "llvm_member_call_internal.h"
#include "parser/ast_api.h"

LLVMValueRef
llvm_emit_member_call(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *callee_node = ast_call_callee(node);
    ASTNode *obj_node = callee_node != NULL && callee_node->type == AST_MEMBER_ACCESS
        ? ast_member_object(callee_node)
        : NULL;
    const char *method_name = callee_node != NULL
        && callee_node->type == AST_MEMBER_ACCESS
        ? ast_member_name(callee_node)
        : NULL;
    LLVMValueRef handled;

    handled = llvm_emit_member_call_vtable_dispatch(node, ctx, obj_node, method_name);
    if (ctx->has_error)
        return NULL;
    if (handled != NULL)
        return handled;

    handled = llvm_emit_member_call_slot_method(node, ctx, obj_node, method_name);
    if (ctx->has_error)
        return NULL;
    if (handled != NULL)
        return handled;

    handled = llvm_emit_member_call_slice(node, ctx, obj_node, method_name);
    if (ctx->has_error)
        return NULL;
    if (handled != NULL)
        return handled;

    if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
        && method_name != NULL) {
        if (llvm_is_upper_ident(obj_node)) {
            char *full_name = llvm_member_call_mangle_method_name(ctx, node,
                ast_identifier_name(obj_node), method_name);
            if (full_name == NULL)
                return NULL;
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
            if (fn != NULL) {
                return llvm_emit_function_call_args(ctx, fn,
                    ast_call_arguments(node, NULL), ast_call_arg_count(node));
            }
        }

        const char *var_name = ast_identifier_name(obj_node);
        LLVMVarEntry var;
        bool has_var = llvm_scope_lookup_snapshot(ctx, var_name, &var);
        const char *class_name = llvm_lookup_var_class(ctx, var_name);
        if (class_name == NULL)
            class_name = llvm_expr_custom_type_name(obj_node, ctx);
        LLVMClassTypeEntry *parent_cls = NULL;
        int field_idx = -1;

        if (!has_var) {
            ASTNode *host_decl = llvm_current_host_decl(ctx);
            const char *host_name = llvm_decl_node_name(host_decl);
            class_name = llvm_current_field_class_name(ctx, var_name);
            if (class_name != NULL && host_name != NULL) {
                parent_cls = llvm_lookup_class(ctx, host_name);
                if (parent_cls != NULL)
                    field_idx = llvm_class_field_index(parent_cls, var_name);
            }
        }

        if (class_name != NULL
            && (has_var || (parent_cls != NULL && field_idx >= 0))) {
            LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, class_name);
            if (cls != NULL) {
                LLVMFuncEntry *fn;
                LLVMValueRef fn_value;
                LLVMTypeRef fn_type;
                LLVMTypeRef ret_type;
                char *full_name = llvm_member_call_mangle_method_name(ctx,
                    node, class_name, method_name);
                if (full_name == NULL)
                    return NULL;
                fn = llvm_lookup_function(ctx, full_name);
                fn_value = fn != NULL ? fn->fn
                    : LLVMGetNamedFunction(ctx->module, full_name);
                fn_type = fn != NULL
                    ? fn->fn_type
                    : (fn_value != NULL ? LLVMGlobalGetValueType(fn_value) : NULL);
                ret_type = fn != NULL
                    ? fn->ret_type
                    : (fn_type != NULL ? LLVMGetReturnType(fn_type) : NULL);
                const MIRDeclMethod *method_meta =
                    llvm_find_host_method_metadata_in_context(ctx,
                        class_name, method_name);
                ASTNode *method_decl =
                    llvm_mir_decl_method_source_ast(method_meta);
                if (method_decl == NULL && method_meta == NULL)
                    method_decl = llvm_find_nominal_host_method_decl(ctx,
                        class_name, method_name);
                if (fn_value != NULL && fn_type != NULL && ret_type != NULL) {
                    /* subject methods receive a self pointer; class methods a self value */
                    size_t argc = ast_call_arg_count(node);
                    LLVMValueRef *args = llvm_member_call_alloc_args(
                        ctx, node, class_name, method_name, argc);
                    if (args == NULL)
                        return NULL;
                    if (llvm_type_name_uses_pointer_self(ctx, class_name)) {
                        if (has_var && strcmp(var_name, "self") == 0) {
                            if (var.type == LLVMPointerType(cls->struct_type, 0))
                                args[0] = LLVMBuildLoad2(ctx->builder,
                                    var.type, var.alloca, llvm_tmp_name(ctx));
                            else
                                args[0] = var.alloca;
                        } else if (has_var) {
                            if (var.type == LLVMPointerType(cls->struct_type, 0))
                                args[0] = LLVMBuildLoad2(ctx->builder,
                                    var.type, var.alloca, llvm_tmp_name(ctx));
                            else
                                args[0] = var.alloca;
                        } else {
                            LLVMValueRef base_ptr =
                                llvm_current_self_base_ptr(ctx, parent_cls);
                            if (base_ptr == NULL)
                                return llvm_member_call_error_recovery(ctx,
                                    node, class_name, method_name,
                                    "requires a self receiver");
                            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                                ctx->builder, parent_cls->struct_type, base_ptr,
                                (unsigned)field_idx, llvm_tmp_name(ctx));
                            args[0] = field_ptr;
                        }
                    } else {
                        if (has_var) {
                            args[0] = LLVMBuildLoad2(ctx->builder,
                                var.type, var.alloca, llvm_tmp_name(ctx));
                        } else {
                            LLVMValueRef base_ptr =
                                llvm_current_self_base_ptr(ctx, parent_cls);
                            if (base_ptr == NULL)
                                return llvm_member_call_error_recovery(ctx,
                                    node, class_name, method_name,
                                    "requires a self receiver");
                            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                                ctx->builder, parent_cls->struct_type, base_ptr,
                                (unsigned)field_idx, llvm_tmp_name(ctx));
                            args[0] = LLVMBuildLoad2(ctx->builder,
                                cls->struct_type, field_ptr, llvm_tmp_name(ctx));
                        }
                    }
                    for (size_t i = 0; i < argc; i++) {
                        ASTNode *arg_node = ast_call_argument(node, i);
                        LLVMValueRef arg_val = llvm_emit_expression(
                            arg_node, ctx);
                        arg_val = llvm_member_call_adjust_pointer_self_arg(
                            ctx, method_meta, method_decl, i, arg_node,
                            arg_val);
                        if (!llvm_member_call_store_arg(ctx, node, class_name,
                                method_name, args, i, arg_val))
                            return NULL;
                    }

                    LLVMValueRef result;
                    if (ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, fn_type,
                            fn_value, args, (unsigned)(argc + 1), "");
                        llvm_emit_current_zone_subject_projection_sync(ctx, obj_node);
                        llvm_emit_world_embedded_action_effect_sync(ctx,
                            obj_node, method_meta, method_decl);
                        llvm_emit_world_embedded_receiver_projection_sync(ctx, obj_node);
                        result = llvm_void_expression_placeholder(ctx, node,
                            "member-call");
                    } else {
                        result = LLVMBuildCall2(ctx->builder,
                            fn_type, fn_value, args,
                            (unsigned)(argc + 1), llvm_tmp_name(ctx));
                        llvm_emit_current_zone_subject_projection_sync(ctx, obj_node);
                        llvm_emit_world_embedded_action_effect_sync(ctx,
                            obj_node, method_meta, method_decl);
                        llvm_emit_world_embedded_receiver_projection_sync(ctx, obj_node);
                    }
                    return result;
                }
            }
            if (cls == NULL) {
                char *full_name = llvm_member_call_mangle_method_name(ctx,
                    node, class_name, method_name);
                if (full_name == NULL)
                    return NULL;
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
                LLVMValueRef fn_value = fn != NULL ? fn->fn
                    : LLVMGetNamedFunction(ctx->module, full_name);
                LLVMTypeRef fn_type = fn != NULL
                    ? fn->fn_type
                    : (fn_value != NULL ? LLVMGlobalGetValueType(fn_value) : NULL);
                LLVMTypeRef ret_type = fn != NULL
                    ? fn->ret_type
                    : (fn_type != NULL ? LLVMGetReturnType(fn_type) : NULL);
                if (fn_value != NULL && fn_type != NULL && ret_type != NULL) {
                    size_t argc = ast_call_arg_count(node);
                    LLVMValueRef *args = llvm_member_call_alloc_args(
                        ctx, node, class_name, method_name, argc);
                    if (args == NULL)
                        return NULL;
                    args[0] = llvm_emit_expression(obj_node, ctx);
                    if (args[0] == NULL)
                        return llvm_member_call_error_recovery(ctx, node,
                            class_name, method_name, "could not lower receiver");
                    for (size_t i = 0; i < argc; i++) {
                        LLVMValueRef arg_val = llvm_emit_expression(
                            ast_call_argument(node, i), ctx);
                        if (!llvm_member_call_store_arg(ctx, node, class_name,
                                method_name, args, i, arg_val))
                            return NULL;
                    }
                    if (ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, fn_type, fn_value,
                                       args, (unsigned)(argc + 1), "");
                        return llvm_void_expression_placeholder(ctx, node,
                            "member-call");
                    } else {
                        LLVMValueRef result = LLVMBuildCall2(
                            ctx->builder, fn_type, fn_value,
                            args, (unsigned)(argc + 1), llvm_tmp_name(ctx));
                        return result;
                    }
                }
            }
        }
    }

    if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
        && method_name != NULL) {
        const char *class_name = llvm_expr_custom_type_name(obj_node, ctx);
        LLVMClassTypeEntry *host_cls = class_name != NULL
            ? llvm_lookup_class(ctx, class_name) : NULL;

        if (host_cls != NULL) {
            LLVMFuncEntry *fn;
            LLVMValueRef fn_value;
            LLVMTypeRef fn_type;
            LLVMTypeRef ret_type;
            const MIRDeclMethod *method_meta;
            ASTNode *method_decl;
            LLVMValueRef *args;
            LLVMValueRef self_ptr;
            char *full_name = llvm_member_call_mangle_method_name(ctx, node,
                class_name, method_name);
            if (full_name == NULL)
                return NULL;

            fn = llvm_lookup_function(ctx, full_name);
            fn_value = fn != NULL ? fn->fn
                : LLVMGetNamedFunction(ctx->module, full_name);
            fn_type = fn != NULL
                ? fn->fn_type
                : (fn_value != NULL ? LLVMGlobalGetValueType(fn_value) : NULL);
            ret_type = fn != NULL
                ? fn->ret_type
                : (fn_type != NULL ? LLVMGetReturnType(fn_type) : NULL);
            method_meta = llvm_find_host_method_metadata_in_context(ctx,
                class_name, method_name);
            method_decl = llvm_mir_decl_method_source_ast(method_meta);
            if (method_decl == NULL && method_meta == NULL)
                method_decl = llvm_find_nominal_host_method_decl(ctx,
                    class_name, method_name);
            if (fn_value != NULL && fn_type != NULL && ret_type != NULL) {
                size_t argc = ast_call_arg_count(node);
                args = llvm_member_call_alloc_args(ctx, node, class_name,
                    method_name, argc);
                if (args == NULL)
                    return NULL;
                self_ptr = llvm_emit_member_lvalue_ptr(obj_node, ctx, NULL);
                if (self_ptr == NULL) {
                    LLVMValueRef recv_val = llvm_emit_expression(obj_node, ctx);
                    if (recv_val != NULL
                        && LLVMGetTypeKind(LLVMTypeOf(recv_val))
                           == LLVMStructTypeKind) {
                        self_ptr = llvm_create_entry_alloca(ctx,
                            host_cls->struct_type, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, recv_val, self_ptr);
                    }
                }
                if (self_ptr == NULL) {
                    return llvm_member_call_error_recovery(ctx, node,
                        class_name, method_name, "could not lower receiver");
                }

                if (llvm_type_name_uses_pointer_self(ctx, class_name)) {
                    args[0] = self_ptr;
                } else {
                    args[0] = LLVMBuildLoad2(ctx->builder,
                        host_cls->struct_type, self_ptr, llvm_tmp_name(ctx));
                }

                for (size_t i = 0; i < argc; i++) {
                    ASTNode *arg_node = ast_call_argument(node, i);
                    LLVMValueRef arg_val = llvm_emit_expression(
                        arg_node, ctx);
                    arg_val = llvm_member_call_adjust_pointer_self_arg(
                        ctx, method_meta, method_decl, i, arg_node, arg_val);
                    if (!llvm_member_call_store_arg(ctx, node, class_name,
                            method_name, args, i, arg_val))
                        return NULL;
                }

                if (ret_type == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn_type, fn_value,
                        args, (unsigned)(argc + 1), "");
                    llvm_emit_world_embedded_action_effect_sync(ctx,
                        obj_node, method_meta, method_decl);
                    llvm_emit_world_embedded_receiver_projection_sync(ctx, obj_node);
                    return llvm_void_expression_placeholder(ctx, node,
                        "member-call");
                }

                {
                    LLVMValueRef result = LLVMBuildCall2(ctx->builder,
                        fn_type, fn_value, args,
                        (unsigned)(argc + 1), llvm_tmp_name(ctx));
                    llvm_emit_world_embedded_action_effect_sync(ctx,
                        obj_node, method_meta, method_decl);
                    llvm_emit_world_embedded_receiver_projection_sync(ctx, obj_node);
                    return result;
                }
            }
        }
    }

    if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
        && ast_member_object(obj_node) != NULL
        && ast_member_object(obj_node)->type == AST_IDENTIFIER
        && ast_identifier_name(ast_member_object(obj_node)) != NULL
        && strcmp(ast_identifier_name(ast_member_object(obj_node)), "self") == 0
        && ast_member_name(obj_node) != NULL
        && method_name != NULL) {
        ASTNode *host_decl = llvm_current_host_decl(ctx);
        const char *host_name = llvm_decl_node_name(host_decl);
        const char *field_name = ast_member_name(obj_node);
        const char *class_name = llvm_current_field_class_name(ctx, field_name);
        LLVMClassTypeEntry *parent_cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
        LLVMClassTypeEntry *host_cls = class_name != NULL
            ? llvm_lookup_class(ctx, class_name) : NULL;
        int field_idx = parent_cls != NULL
            ? llvm_class_field_index(parent_cls, field_name) : -1;

        if (parent_cls != NULL && host_cls != NULL && field_idx >= 0) {
            char *full_name = llvm_member_call_mangle_method_name(ctx, node,
                class_name, method_name);
            if (full_name == NULL)
                return NULL;
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
            if (fn != NULL) {
                size_t argc = ast_call_arg_count(node);
                LLVMValueRef *args = llvm_member_call_alloc_args(ctx, node,
                    class_name, method_name, argc);
                if (args == NULL)
                    return NULL;
                LLVMValueRef base_ptr = llvm_current_self_base_ptr(ctx, parent_cls);
                if (base_ptr == NULL)
                    return llvm_member_call_error_recovery(ctx, node,
                        class_name, method_name, "requires a self receiver");
                LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
                    parent_cls->struct_type, base_ptr, (unsigned)field_idx,
                    llvm_tmp_name(ctx));
                const MIRDeclMethod *method_meta =
                    llvm_find_host_method_metadata_in_context(ctx,
                        class_name, method_name);
                ASTNode *method_decl =
                    llvm_mir_decl_method_source_ast(method_meta);
                if (method_decl == NULL && method_meta == NULL)
                    method_decl = llvm_find_nominal_host_method_decl(ctx,
                        class_name, method_name);

                if (llvm_type_name_uses_pointer_self(ctx, class_name))
                    args[0] = field_ptr;
                else
                    args[0] = LLVMBuildLoad2(ctx->builder,
                        host_cls->struct_type, field_ptr, llvm_tmp_name(ctx));

                for (size_t i = 0; i < argc; i++) {
                    ASTNode *arg_node = ast_call_argument(node, i);
                    LLVMValueRef arg_val = llvm_emit_expression(
                        arg_node, ctx);
                    arg_val = llvm_member_call_adjust_pointer_self_arg(
                        ctx, method_meta, method_decl, i, arg_node, arg_val);
                    if (!llvm_member_call_store_arg(ctx, node, class_name,
                            method_name, args, i, arg_val))
                        return NULL;
                }

                LLVMValueRef result;
                if (fn->ret_type == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, (unsigned)(argc + 1), "");
                    result = llvm_void_expression_placeholder(ctx, node,
                        "member-call");
                } else {
                    result = LLVMBuildCall2(ctx->builder,
                        fn->fn_type, fn->fn, args,
                        (unsigned)(argc + 1), llvm_tmp_name(ctx));
                }
                return result;
            }
        }
    }

    if (obj_node != NULL && obj_node->type == AST_CALL
        && method_name != NULL) {
        const char *class_name = llvm_expr_custom_type_name(obj_node, ctx);
        LLVMClassTypeEntry *cls = class_name != NULL
            ? llvm_lookup_class(ctx, class_name) : NULL;
        if (cls != NULL) {
            char *full_name = llvm_member_call_mangle_method_name(ctx, node,
                class_name, method_name);
            if (full_name == NULL)
                return NULL;
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
            if (fn != NULL) {
                size_t argc = ast_call_arg_count(node);
                LLVMValueRef *args = llvm_member_call_alloc_args(ctx, node,
                    class_name, method_name, argc);
                if (args == NULL)
                    return NULL;
                LLVMValueRef recv = llvm_emit_expression(obj_node, ctx);
                if (recv == NULL)
                    return llvm_member_call_error_recovery(ctx, node,
                        class_name, method_name, "could not lower receiver");
                args[0] = recv;
                for (size_t i = 0; i < argc; i++) {
                    LLVMValueRef arg_val = llvm_emit_expression(
                        ast_call_argument(node, i), ctx);
                    if (!llvm_member_call_store_arg(ctx, node, class_name,
                            method_name, args, i, arg_val))
                        return NULL;
                }
                LLVMValueRef result;
                if (fn->ret_type == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, (unsigned)(argc + 1), "");
                    result = llvm_void_expression_placeholder(ctx, node,
                        "member-call");
                } else {
                    result = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, (unsigned)(argc + 1), llvm_tmp_name(ctx));
                }
                return result;
            }
        }
    }

    return llvm_member_call_error_recovery(ctx, node, NULL, method_name,
        "is not declared in the backend method registry");
}

#endif
