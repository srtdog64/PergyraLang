#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_dispatch.h"
#include "llvm_stmt_source_local_fallback.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"
#include "../parser/ast_api.h"

static LLVMValueRef
llvm_emit_hosted_self_arg(ASTNode *arg_node, LLVMGenCtx *ctx,
                          FuncParam *param,
                          const char *param_type_name,
                          LLVMValueRef fallback)
{
    LLVMClassTypeEntry *param_class = NULL;

    if (param == NULL)
        return fallback;

    if (param_type_name == NULL) {
        if (param->type == NULL || param->type->type != AST_TYPE)
            return fallback;
        param_type_name = ast_type_name(param->type);
    }
    param_class = param_type_name != NULL
        ? llvm_lookup_class(ctx, param_type_name)
        : NULL;
    if (param_class == NULL || !param_class->is_pointer_self_host)
        return fallback;
    if (arg_node == NULL || arg_node->type != AST_IDENTIFIER)
        return fallback;

    const char *arg_name = ast_identifier_name(arg_node);
    LLVMVarEntry arg_var;
    if (!llvm_scope_lookup_snapshot(ctx, arg_name, &arg_var))
        return fallback;
    if (LLVMGetTypeKind(arg_var.type) == LLVMPointerTypeKind) {
        return LLVMBuildLoad2(ctx->builder, arg_var.type,
                              arg_var.alloca, llvm_tmp_name(ctx));
    }
    return arg_var.alloca;
}

static FuncParam *
llvm_hosted_self_logical_param(const MIRDeclMethod *method_meta,
                               ASTNode *host_method,
                               size_t arg_index,
                               bool allow_ast_compat,
                               const char **type_name_out)
{
    size_t logical_index = 0;
    size_t param_count = method_meta != NULL
        ? llvm_mir_decl_method_param_count(method_meta)
        : (allow_ast_compat ? ast_func_param_count(host_method) : 0);

    if (type_name_out != NULL)
        *type_name_out = NULL;
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = method_meta != NULL
            ? llvm_mir_decl_method_param(method_meta, i)
            : (allow_ast_compat ? ast_func_param(host_method, i) : NULL);
        if (param == NULL || param->name == NULL)
            continue;
        if (param->type == NULL && strcmp(param->name, "self") == 0)
            continue;
        if (logical_index == arg_index) {
            if (type_name_out != NULL && method_meta != NULL)
                *type_name_out =
                    llvm_mir_decl_method_param_type_name(method_meta, i);
            return param;
        }
        logical_index++;
    }
    return NULL;
}

LLVMValueRef
llvm_emit_hosted_self_call(ASTNode *node, LLVMGenCtx *ctx,
                           const char *callee_name)
{
    ASTNode *host_decl = llvm_current_host_decl(ctx);
    const char *host_name = llvm_decl_node_name(host_decl);
    const MIRDeclMethod *method_meta = NULL;
    ASTNode *host_method = NULL;
    size_t argc = ast_call_arg_count(node);
    char full_name[256];
    LLVMFuncEntry *fn = NULL;
    LLVMValueRef *args = NULL;

    if (host_name == NULL)
        return NULL;
    method_meta =
        llvm_find_host_method_metadata_in_context(ctx, host_name, callee_name);
    if (method_meta == NULL) {
        host_method = llvm_stmt_host_method_ast_decl(ctx, host_name,
            callee_name);
        if (llvm_active_has_mir(ctx)) {
            if (host_method == NULL
                && llvm_find_callable_decl(ctx, callee_name) != NULL) {
                return NULL;
            }
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing hosted self-call method metadata for '%s.%s'",
                host_name != NULL ? host_name : "(anonymous)",
                callee_name != NULL ? callee_name : "(anonymous)");
            return NULL;
        }
        if (host_method == NULL)
            host_method = llvm_current_host_method_decl(ctx, callee_name);
    }
    if (method_meta == NULL && host_method == NULL)
        return NULL;
    if (!llvm_mir_decl_method_metadata_complete_for(ctx,
            method_meta,
            host_name,
            callee_name,
            LLVM_MIR_DECL_METHOD_REQUIRE_PARAM_TYPE_NAMES,
            NULL,
            "MIR-only LLVM path missing hosted self-call parameter type-name metadata for '%s.%s'")) {
        return NULL;
    }

    snprintf(full_name, sizeof(full_name), "%s_%s", host_name, callee_name);
    fn = llvm_lookup_function(ctx, full_name);
    if (fn == NULL)
        return NULL;

    args = pgy_arena_calloc(&ctx->scratch, (argc + 1) * sizeof(LLVMValueRef));
    if (args == NULL) {
        return llvm_call_error_recovery(ctx, node,
            "LLVM hosted method call argument allocation failed");
    }

    args[0] = llvm_current_self_call_arg(ctx);
    if (args[0] == NULL) {
        return llvm_call_error_recovery(ctx, node,
            "LLVM hosted method call requires a self receiver");
    }

    for (size_t i = 0; i < argc; i++) {
        ASTNode *arg_node = ast_call_argument(node, i);
        LLVMTypeRef arg_type = llvm_stmt_infer_expr_type(ctx, arg_node);
        LLVMValueRef arg_value;
        const char *param_type_name = NULL;
        FuncParam *param = llvm_hosted_self_logical_param(
            method_meta,
            host_method,
            i,
            method_meta == NULL && !llvm_active_has_mir(ctx),
            &param_type_name);

        if (ctx->has_error)
            return NULL;
        if (arg_type == ctx->type_void) {
            llvm_set_error_at_with_hints(ctx, arg_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ALIGN_ARG_TYPE,
                "LLVM hosted method call '%s' cannot consume a Void expression as argument %zu",
                full_name, i + 1);
            return NULL;
        }
        arg_value = llvm_emit_expression(arg_node, ctx);
        arg_value = llvm_emit_hosted_self_arg(arg_node, ctx, param,
            param_type_name, arg_value);
        if (ctx->has_error)
            return NULL;
        if (arg_value == NULL)
            return llvm_call_arg_error_recovery(ctx, node, callee_name, i);
        args[i + 1] = arg_value;
    }

    if (fn->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                       args, (unsigned)(argc + 1), "");
        return llvm_void_expression_placeholder(ctx, node,
            "hosted-self-call");
    }

    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                          args, (unsigned)(argc + 1), llvm_tmp_name(ctx));
}

#endif
