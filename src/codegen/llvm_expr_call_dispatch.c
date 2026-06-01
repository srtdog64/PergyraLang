#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_dispatch.h"

#include "llvm_expr_array_calls.h"
#include "llvm_expr_boundary_projection_helpers.h"
#include "llvm_expr_call_inline_policy.h"
#include "llvm_expr_call_collections_extended.h"
#include "llvm_expr_collection_base_calls.h"
#include "llvm_expr_call_variable.h"
#include "llvm_expr_constructor_calls.h"
#include "llvm_expr_domain_query_calls.h"
#include "llvm_expr_event_calls.h"
#include "llvm_expr_intent_observability_calls.h"
#include "llvm_expr_log_calls.h"
#include "llvm_expr_math_calls.h"
#include "llvm_expr_member_lvalue.h"
#include "llvm_expr_projection_path_helpers.h"
#include "llvm_expr_rc_calls.h"
#include "llvm_expr_result_option_calls.h"
#include "llvm_expr_scalar_core.h"
#include "llvm_expr_slot_device_calls.h"
#include "llvm_expr_spawn_call_helpers.h"
#include "llvm_expr_stdlib_scalar_io_calls.h"
#include "llvm_expr_task_channel_calls.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_member_call_emit.h"
#include "../parser/ast_api.h"

static ASTNode *
llvm_intent_call_binding_at(ASTNode *intent_decl, size_t index,
                            size_t *binding_count_out)
{
    size_t binding_count = 0;
    size_t involve_count = 0;
    size_t value_count = 0;
    ASTNode **bindings = ast_intent_decl_bindings(intent_decl, &binding_count);
    ASTNode **involves = ast_intent_decl_involves(intent_decl, &involve_count);
    ASTNode **values = ast_intent_decl_values(intent_decl, &value_count);

    if (binding_count_out != NULL) {
        *binding_count_out = binding_count > 0
            ? binding_count
            : (involve_count + value_count);
    }
    if (binding_count > 0)
        return (bindings != NULL && index < binding_count) ? bindings[index] : NULL;
    if (index < involve_count)
        return involves != NULL ? involves[index] : NULL;
    index -= involve_count;
    return (values != NULL && index < value_count) ? values[index] : NULL;
}

static bool
llvm_call_arg_can_take_subject_address(ASTNode *arg_node)
{
    if (arg_node == NULL)
        return false;
    return arg_node->type == AST_IDENTIFIER
        || arg_node->type == AST_MEMBER_ACCESS
        || arg_node->type == AST_ARRAY_ACCESS;
}

LLVMValueRef
llvm_emit_call(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *callee_node = ast_call_callee(node);
    size_t argc = ast_call_arg_count(node);
    ASTNode **call_args = ast_call_arguments(node, NULL);

    if (callee_node == NULL)
        return llvm_call_error_recovery(ctx, node,
            "LLVM call expression requires a callee");

    /* Method call: obj.method(args) */
    if (callee_node->type == AST_MEMBER_ACCESS)
        return llvm_emit_member_call(node, ctx);

    /* Get callee name */
    const char *callee_name = NULL;
    if (callee_node->type == AST_IDENTIFIER)
        callee_name = ast_identifier_name(callee_node);

    if (callee_name == NULL)
        return llvm_call_error_recovery(ctx, node,
            "LLVM call expression requires an identifier or member callee");

    LLVMCallInlineOp inline_op = llvm_call_inline_lookup(callee_name, argc);

    if (inline_op == LLVM_CALL_INLINE_OP_CLONE) {
        return llvm_emit_expression(ast_call_argument(node, 0), ctx);
    }

    {
        LLVMValueRef constructor_value =
            llvm_emit_constructor_call(node, ctx, callee_name);
        if (ctx->has_error)
            return NULL;
        if (constructor_value != NULL)
            return constructor_value;
    }

    if (inline_op == LLVM_CALL_INLINE_OP_TO_OBJECT) {
        LLVMValueRef projection = llvm_emit_subject_projection(node, ctx);
        if (ctx->has_error)
            return NULL;
        return projection;
    }

    {
        LLVMValueRef domain_query = NULL;
        if (llvm_emit_domain_query_call(node, ctx, callee_name, &domain_query))
            return domain_query;
    }

    {
        LLVMValueRef log_call = NULL;
        if (llvm_emit_log_family_call(node, ctx, callee_name, &log_call)) {
            if (ctx->has_error)
                return NULL;
            return log_call;
        }
    }

    {
        LLVMValueRef rc_call = NULL;
        if (llvm_emit_rc_builtin_call(node, ctx, callee_name, &rc_call)) {
            if (ctx->has_error)
                return NULL;
            return rc_call;
        }
    }

    {
        LLVMValueRef slot_builtin = NULL;
        if (llvm_emit_slot_builtin_call(node, ctx, callee_name, &slot_builtin)) {
            if (ctx->has_error)
                return NULL;
            return slot_builtin;
        }
    }

    {
        LLVMValueRef event_call = NULL;
        if (llvm_emit_event_invocation_call(node, ctx, callee_name, &event_call)) {
            if (ctx->has_error)
                return NULL;
            return event_call;
        }
    }

    {
        LLVMValueRef math_call = NULL;
        if (llvm_emit_scalar_math_call(node, ctx, callee_name, &math_call))
            return math_call;
    }

    {
        LLVMValueRef array_call = NULL;
        if (llvm_emit_array_builtin_call(node, ctx, callee_name, &array_call))
            return array_call;
    }

    {
        LLVMValueRef collection_call = NULL;
        if (llvm_emit_collection_base_call(node, ctx, callee_name, &collection_call))
            return collection_call;
    }

    {
        LLVMValueRef collection_ext = NULL;
        if (llvm_emit_collection_extended_call(node, ctx, callee_name, &collection_ext))
            return collection_ext;
    }

    {
        LLVMValueRef stdlib_call = NULL;
        if (llvm_emit_stdlib_string_file_call(node, ctx, callee_name, &stdlib_call)) {
            if (ctx->has_error)
                return NULL;
            return stdlib_call;
        }
    }

    {
        LLVMValueRef intent_call = NULL;
        if (llvm_emit_intent_observability_call(node, ctx, callee_name, &intent_call))
            return intent_call;
    }
    {
        LLVMValueRef runtime_io_call = NULL;
        if (llvm_emit_stdlib_runtime_io_call(node, ctx, callee_name,
                &runtime_io_call)) {
            if (ctx->has_error)
                return NULL;
            return runtime_io_call;
        }
    }

    {
        LLVMValueRef result_option_call = llvm_emit_result_option_call(node, ctx, callee_name);
        if (ctx->has_error)
            return NULL;
        if (result_option_call != NULL)
            return result_option_call;
    }

    {
        LLVMValueRef task_channel_call = llvm_emit_task_channel_call(node, ctx, callee_name);
        if (ctx->has_error)
            return NULL;
        if (task_channel_call != NULL)
            return task_channel_call;
    }
    if (callee_node->type == AST_IDENTIFIER) {
        LLVMValueRef hosted_call =
            llvm_emit_hosted_self_call(node, ctx, callee_name);
        if (ctx->has_error)
            return NULL;
        if (hosted_call != NULL)
            return hosted_call;
    }

    ASTNode *callable_decl = llvm_find_callable_decl(ctx, callee_name);
    ASTNode *decl = callable_decl != NULL
        && callable_decl->type == AST_FUNC_DECL ? callable_decl : NULL;
    ASTNode *intent_decl = callable_decl != NULL
        && callable_decl->type == AST_INTENT_DECL ? callable_decl : NULL;
    unsigned emitted_argc = 0;
    LLVMValueRef *args = NULL;
    LLVMFuncEntry *predeclared_func = NULL;

    if (decl != NULL)
        args = llvm_build_boundary_call_args(ctx, decl, call_args,
            argc, &emitted_argc);
    if (args == NULL && decl != NULL && ctx->has_error)
        return llvm_call_error_recovery(ctx, node,
            "LLVM boundary call argument lowering failed");
    if (args == NULL && intent_decl != NULL) {
        args = pgy_arena_calloc(&ctx->scratch,
            (argc > 0 ? argc : 1) * sizeof(LLVMValueRef));
        if (args != NULL) {
            for (size_t i = 0; i < argc; i++) {
                const char *type_name = NULL;
                bool pointer_self = false;
                ASTNode *arg_node = ast_call_argument(node, i);

                {
                    size_t binding_count = 0;
                    ASTNode *binding = llvm_intent_call_binding_at(
                        intent_decl, i, &binding_count);
                    if (i < binding_count) {
                        ASTNode *binding_type = NULL;
                        if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                            && ast_intent_involves_subject_type(binding) != NULL
                            && ast_intent_involves_subject_type(binding)->type == AST_TYPE) {
                            binding_type = ast_intent_involves_subject_type(binding);
                            type_name = ast_type_name(binding_type);
                        } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                            && ast_intent_value_type(binding) != NULL
                            && ast_intent_value_type(binding)->type == AST_TYPE) {
                            binding_type = ast_intent_value_type(binding);
                            type_name = ast_type_name(binding_type);
                        }
                    }
                }
                pointer_self = llvm_type_name_uses_pointer_self(ctx, type_name);
                if (pointer_self) {
                    if (arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
                        const char *arg_name = ast_identifier_name(arg_node);
                        LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                        if (arg_var != NULL) {
                            if (LLVMGetTypeKind(arg_var->type) == LLVMPointerTypeKind)
                                args[i] = LLVMBuildLoad2(ctx->builder, arg_var->type,
                                    arg_var->alloca, llvm_tmp_name(ctx));
                            else
                                args[i] = arg_var->alloca;
                        } else {
                            ASTNode *host_decl = llvm_current_host_decl(ctx);
                            const char *host_name = llvm_decl_node_name(host_decl);
                            LLVMClassTypeEntry *host_cls =
                                host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
                            if (host_cls != NULL) {
                                int field_idx = llvm_class_field_index(host_cls, arg_name);
                                if (field_idx >= 0) {
                                    LLVMValueRef base_ptr = llvm_current_self_base_ptr(ctx, host_cls);
                                    if (base_ptr != NULL) {
                                        args[i] = LLVMBuildStructGEP2(ctx->builder,
                                            host_cls->struct_type, base_ptr,
                                            (unsigned)field_idx, llvm_tmp_name(ctx));
                                    }
                                }
                            }
                        }
                    } else if (arg_node != NULL && arg_node->type == AST_MEMBER_ACCESS) {
                        args[i] = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
                    }
                    if (args[i] == NULL
                        && !llvm_call_arg_can_take_subject_address(arg_node)) {
                        llvm_set_error_at_with_hints(ctx, arg_node,
                            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                            PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
                            "LLVM intent subject argument %zu for '%s' requires addressable storage",
                            i + 1, callee_name != NULL ? callee_name : "<intent>");
                        return NULL;
                    }
                }
                if (args[i] == NULL) {
                    LLVMTypeRef arg_type = llvm_stmt_infer_expr_type(ctx,
                        arg_node);
                    if (ctx->has_error)
                        return NULL;
                    if (arg_type == ctx->type_void) {
                        llvm_set_error_at_with_hints(ctx, arg_node,
                            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                            PGY_FIX_ALIGN_ARG_TYPE,
                            "LLVM intent call '%s' cannot consume a Void expression as argument %zu",
                            callee_name != NULL ? callee_name : "<intent>",
                            i + 1);
                        return NULL;
                    }
                    args[i] = llvm_emit_expression(arg_node, ctx);
                }
                if (args[i] == NULL)
                    return llvm_call_arg_error_recovery(ctx, node,
                        callee_name, i);
            }
            emitted_argc = (unsigned)argc;
        }
    }
    if (args == NULL && decl == NULL && intent_decl == NULL)
        predeclared_func = llvm_lookup_function(ctx, callee_name);
    if (args == NULL) {
        args = pgy_arena_calloc(&ctx->scratch,
            (argc > 0 ? argc : 1) * sizeof(LLVMValueRef));
        if (args == NULL)
            return llvm_call_error_recovery(ctx, node,
                "LLVM call argument allocation failed");
        for (size_t i = 0; i < argc; i++) {
            LLVMTypeRef saved_ret = ctx->current_ret_type;
            LLVMTypeRef expected_ty = NULL;
            ASTNode *arg_node = ast_call_argument(node, i);
            LLVMTypeRef arg_type = llvm_stmt_infer_expr_type(ctx, arg_node);
            if (ctx->has_error)
                return NULL;
            if (arg_type == ctx->type_void) {
                llvm_set_error_at_with_hints(ctx, arg_node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ALIGN_ARG_TYPE,
                    "LLVM call '%s' cannot consume a Void expression as argument %zu",
                    callee_name != NULL ? callee_name : "<call>",
                    i + 1);
                return NULL;
            }
            if (predeclared_func != NULL
                && i < LLVMCountParams(predeclared_func->fn)) {
                expected_ty = LLVMTypeOf(
                    LLVMGetParam(predeclared_func->fn, (unsigned)i));
            }
            if (expected_ty != NULL
                && LLVMGetTypeKind(expected_ty) == LLVMStructTypeKind) {
                ctx->current_ret_type = expected_ty;
            }
            args[i] = llvm_emit_expression(arg_node, ctx);
            ctx->current_ret_type = saved_ret;
            if (args[i] == NULL)
                return llvm_call_arg_error_recovery(ctx, node,
                    callee_name, i);
        }
        emitted_argc = (unsigned)argc;
    }

    LLVMFuncEntry *func = llvm_resolve_callee_entry(ctx, callee_name, args, argc);
    if (func == NULL && decl != NULL && decl->type == AST_FUNC_DECL) {
        if (llvm_active_has_mir(ctx) && ast_func_body(decl) != NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing registered function call target '%s'",
                callee_name != NULL ? callee_name : "(anonymous-function)");
            return NULL;
        }
        llvm_forward_declare_func(decl, ctx);
        func = llvm_lookup_function(ctx, callee_name);
    }
    if (func == NULL && intent_decl != NULL) {
        if (llvm_active_has_mir(ctx)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing registered intent call target '%s'",
                callee_name != NULL ? callee_name : "(anonymous-intent)");
            return NULL;
        }

        LLVMTypeRef *param_types = NULL;
        LLVMTypeRef fn_type;
        LLVMValueRef fn;
        size_t forward_param_count = 0;

        (void)llvm_intent_call_binding_at(intent_decl, 0, &forward_param_count);
        if (forward_param_count > 0) {
            param_types = pgy_arena_calloc(&ctx->scratch,
                forward_param_count * sizeof(LLVMTypeRef));
            if (param_types == NULL) {
                return llvm_call_error_recovery(ctx, node,
                    "LLVM intent forward declaration parameter allocation failed");
            }
            for (size_t i = 0; i < forward_param_count; i++) {
                LLVMTypeRef pt = ctx->type_i8ptr;
                ASTNode *binding = llvm_intent_call_binding_at(
                    intent_decl, i, NULL);
                const char *type_name = NULL;
                ASTNode *binding_type = NULL;

                if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                    && ast_intent_involves_subject_type(binding) != NULL
                    && ast_intent_involves_subject_type(binding)->type == AST_TYPE) {
                    binding_type = ast_intent_involves_subject_type(binding);
                    type_name = ast_type_name(binding_type);
                    pt = ast_type_to_llvm(ctx, binding_type);
                    if (ctx->has_error || pt == NULL)
                        return llvm_call_error_recovery(ctx, node,
                            "LLVM intent forward declaration could not lower participant type");
                    if (llvm_type_name_uses_pointer_self(ctx, type_name))
                        pt = LLVMPointerType(pt, 0);
                } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                    && ast_intent_value_type(binding) != NULL) {
                    binding_type = ast_intent_value_type(binding);
                    pt = ast_type_to_llvm(ctx, binding_type);
                    if (ctx->has_error || pt == NULL)
                        return llvm_call_error_recovery(ctx, node,
                            "LLVM intent forward declaration could not lower value type");
                }
                param_types[i] = pt;
            }
        }

        fn_type = LLVMFunctionType(ctx->type_i1, param_types,
            (unsigned)forward_param_count, 0);
        fn = LLVMAddFunction(ctx->module, callee_name, fn_type);
        llvm_register_function(ctx, callee_name, fn, fn_type, ctx->type_i1);
        func = llvm_lookup_function(ctx, callee_name);
    }
    if (func == NULL) {
        LLVMValueRef callable_result = llvm_emit_callable_variable_call(
            node, ctx, callee_name, args, emitted_argc);
        if (ctx->has_error)
            return NULL;
        if (callable_result != NULL)
            return callable_result;

        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_SYMBOL_UNDEFINED,
                PGY_FIX_IMPORT_OR_DECLARE_SYMBOL,
                "LLVM call target '%s' is not declared in the backend function registry",
                callee_name != NULL ? callee_name : "<unknown>");
        }
        return NULL;
    }

    for (size_t i = 0; decl == NULL && i < argc; i++) {
        ASTNode *arg_node = ast_call_argument(node, i);
        unsigned param_count = LLVMCountParams(func->fn);
        LLVMTypeRef param_ty = (i < param_count)
            ? LLVMTypeOf(LLVMGetParam(func->fn, (unsigned)i))
            : NULL;
        if (param_ty != NULL
            && LLVMGetTypeKind(param_ty) == LLVMPointerTypeKind) {
            if (arg_node->type == AST_IDENTIFIER) {
                const char *arg_name = ast_identifier_name(arg_node);
                LLVMVarEntry *v = llvm_scope_lookup(ctx, arg_name);
                LLVMCallableVarEntry *callable_entry =
                    llvm_lookup_callable_entry(ctx, arg_name);
                if (v != NULL) {
                    if (callable_entry != NULL) {
                        LLVMTypeRef callable_sig =
                            llvm_function_signature_from_callable_entry(ctx, callable_entry);
                        if (ctx->has_error || callable_sig == NULL)
                            return llvm_call_error_recovery(ctx, node,
                                "LLVM callable argument could not lower callable signature");
                        LLVMTypeRef callable_ptr_ty =
                            LLVMPointerType(callable_sig, 0);
                        args[i] = LLVMBuildLoad2(ctx->builder, callable_ptr_ty,
                            v->alloca, llvm_tmp_name(ctx));
                    } else if (LLVMGetTypeKind(v->type) == LLVMPointerTypeKind) {
                        args[i] = LLVMBuildLoad2(ctx->builder, v->type,
                            v->alloca, llvm_tmp_name(ctx));
                    } else {
                        args[i] = v->alloca;
                    }
                }
            } else if (arg_node->type == AST_MEMBER_ACCESS) {
                args[i] = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
            }
            if (args[i] == NULL && arg_node->type == AST_CALL) {
                LLVMValueRef maybe_value = llvm_emit_expression(arg_node, ctx);
                if (maybe_value != NULL
                    && LLVMGetTypeKind(LLVMTypeOf(maybe_value)) == LLVMPointerTypeKind) {
                    args[i] = maybe_value;
                }
            }
        }
    }

    LLVMValueRef result;
    if (func->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                       args, emitted_argc, "");
        result = llvm_void_expression_placeholder(ctx, node,
            "direct-call");
    } else {
        result = LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                                args, emitted_argc, llvm_tmp_name(ctx));
    }

    return result;
}

#endif
