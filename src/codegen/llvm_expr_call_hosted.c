#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_dispatch.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "../parser/ast_api.h"

static LLVMValueRef
llvm_emit_hosted_self_arg(ASTNode *arg_node, LLVMGenCtx *ctx,
                          FuncParam *param, LLVMValueRef fallback)
{
    const char *param_type_name = NULL;
    LLVMClassTypeEntry *param_class = NULL;

    if (param == NULL || param->type == NULL || param->type->type != AST_TYPE)
        return fallback;

    param_type_name = ast_type_name(param->type);
    param_class = param_type_name != NULL
        ? llvm_lookup_class(ctx, param_type_name)
        : NULL;
    if (param_class == NULL || !param_class->is_pointer_self_host)
        return fallback;
    if (arg_node == NULL || arg_node->type != AST_IDENTIFIER)
        return fallback;

    const char *arg_name = ast_identifier_name(arg_node);
    LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
    if (arg_var == NULL)
        return fallback;
    if (LLVMGetTypeKind(arg_var->type) == LLVMPointerTypeKind) {
        return LLVMBuildLoad2(ctx->builder, arg_var->type,
                              arg_var->alloca, llvm_tmp_name(ctx));
    }
    return arg_var->alloca;
}

static FuncParam *
llvm_hosted_self_logical_param(ASTNode *host_method, size_t arg_index)
{
    size_t logical_index = 0;

    for (size_t i = 0; i < ast_func_param_count(host_method); i++) {
        FuncParam *param = ast_func_param(host_method, i);
        if (param == NULL || param->name == NULL)
            continue;
        if (param->type == NULL && strcmp(param->name, "self") == 0)
            continue;
        if (logical_index == arg_index)
            return param;
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
    ASTNode *host_method = llvm_current_host_method_decl(ctx, callee_name);
    size_t argc = ast_call_arg_count(node);
    char full_name[256];
    LLVMFuncEntry *fn = NULL;
    LLVMValueRef *args = NULL;

    if (host_name == NULL || host_method == NULL)
        return NULL;

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
        LLVMValueRef arg_value = llvm_emit_expression(arg_node, ctx);
        FuncParam *param = llvm_hosted_self_logical_param(host_method, i);

        arg_value = llvm_emit_hosted_self_arg(arg_node, ctx, param, arg_value);
        if (arg_value == NULL)
            return llvm_call_arg_error_recovery(ctx, node, callee_name, i);
        args[i + 1] = arg_value;
    }

    if (fn->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                       args, (unsigned)(argc + 1), "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                          args, (unsigned)(argc + 1), llvm_tmp_name(ctx));
}

#endif
