/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_boundary_projection_helpers.h"

#include <string.h>

#include "llvm_boundary_slot_param.h"
#include "llvm_expr_member_lvalue.h"
#include "llvm_internal_api.h"

static LLVMValueRef
llvm_boundary_slot_runtime_arg(LLVMGenCtx *ctx, LLVMVarEntry *slot_var)
{
    if (ctx == NULL || slot_var == NULL)
        return NULL;
    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        return LLVMBuildLoad2(ctx->builder, slot_var->type, slot_var->alloca,
                              llvm_tmp_name(ctx));
    }
    return slot_var->alloca;
}

static bool
llvm_boundary_param_uses_pointer_self(LLVMGenCtx *ctx, FuncParam *param)
{
    return param != NULL && llvm_ast_type_uses_pointer_self(ctx, param->type);
}

static bool
llvm_boundary_arg_can_take_subject_address(ASTNode *arg_node)
{
    if (arg_node == NULL)
        return false;
    return arg_node->type == AST_IDENTIFIER
        || arg_node->type == AST_MEMBER_ACCESS
        || arg_node->type == AST_ARRAY_ACCESS;
}

static bool
llvm_boundary_name_is_generic_param(ASTNode *decl, const char *type_name)
{
    GenericParams *params;

    if (decl == NULL || type_name == NULL)
        return false;
    params = ast_declaration_generic_params(decl);
    for (size_t i = 0; i < ast_generic_param_count(params); i++) {
        GenericParam *param = ast_generic_param_at(params, i);
        const char *param_name = ast_generic_param_name(param);
        if (param_name != NULL && strcmp(param_name, type_name) == 0)
            return true;
    }
    return false;
}

static bool
llvm_boundary_type_mentions_generic_param(ASTNode *decl, ASTNode *type_node)
{
    GenericParams *args;

    if (type_node == NULL)
        return false;
    if (type_node->type == AST_TYPE
        && llvm_boundary_name_is_generic_param(decl,
            ast_type_name(type_node))) {
        return true;
    }
    if (type_node->type != AST_TYPE)
        return false;
    args = ast_type_generic_args(type_node);
    for (size_t i = 0; i < ast_generic_param_count(args); i++) {
        GenericParam *arg = ast_generic_param_at(args, i);
        if (llvm_boundary_name_is_generic_param(decl,
                ast_generic_param_name(arg))) {
            return true;
        }
        if (llvm_boundary_type_mentions_generic_param(decl,
                ast_generic_param_constraint(arg))) {
            return true;
        }
        if (llvm_boundary_type_mentions_generic_param(decl,
                ast_generic_param_default_type(arg))) {
            return true;
        }
    }
    return false;
}

static LLVMValueRef *
llvm_boundary_args_error(LLVMGenCtx *ctx, ASTNode *node, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM boundary call argument lowering failed");
    }
    return NULL;
}

LLVMValueRef *
llvm_build_boundary_call_args(LLVMGenCtx *ctx, ASTNode *decl,
                              ASTNode **arg_nodes, size_t argc,
                              unsigned *out_count)
{
    LLVMValueRef *args;
    unsigned emitted_count = 0;
    const MIRRoutine *routine = NULL;
    bool routine_has_signature = false;
    const char *decl_name = NULL;
    bool decl_is_generic = false;
    size_t param_count;

    if (out_count != NULL)
        *out_count = 0;
    if (ctx == NULL || decl == NULL || decl->type != AST_FUNC_DECL)
        return NULL;

    decl_name = ast_declaration_name(decl);
    decl_is_generic =
        ast_generic_param_count(ast_declaration_generic_params(decl)) > 0;
    if (llvm_active_has_mir(ctx) && !decl_is_generic) {
        routine = llvm_active_function_routine_for_source_ast(ctx, decl);
        if (routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing boundary call routine for '%s'",
                decl_name != NULL ? decl_name : "(anonymous-function)");
            return NULL;
        }
        if (!llvm_mir_routine_has_signature(routine)) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing boundary call signature metadata for '%s'",
                decl_name != NULL ? decl_name : "(anonymous-function)");
            return NULL;
        }
        routine_has_signature = true;
    }

    param_count = routine_has_signature
        ? llvm_mir_routine_param_count(routine)
        : ast_func_param_count(decl);
    if (argc != param_count)
        return llvm_boundary_args_error(ctx, decl,
            "LLVM boundary call source argument count does not match function signature");

    for (size_t i = 0; i < param_count; i++) {
        bool is_secure = false;
        FuncParam *p = routine_has_signature
            ? llvm_mir_routine_param(routine, i)
            : ast_func_param(decl, i);
        const char *param_type_name = routine_has_signature
            ? llvm_mir_routine_param_type_name(routine, i)
            : NULL;
        const char *inner = NULL;
        if (routine_has_signature
            && p != NULL
            && p->type != NULL
            && p->type->type != AST_EVENT_HANDLER_TYPE
            && param_type_name == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing boundary call parameter type-name metadata for '%s'",
                decl_name != NULL ? decl_name : "(anonymous-function)");
            return NULL;
        }
        emitted_count++;
        inner = param_type_name != NULL
            ? llvm_boundary_slot_inner_name_from_type_name(ctx, p,
                param_type_name, &is_secure)
            : llvm_boundary_slot_inner_name(ctx, p, &is_secure);
        if (ctx->has_error)
            return NULL;
        if (inner != NULL && is_secure)
            emitted_count++;
    }

    args = pgy_arena_calloc(&ctx->scratch,
                            (emitted_count > 0 ? emitted_count : 1) * sizeof(LLVMValueRef));
    if (args == NULL)
        return llvm_boundary_args_error(ctx, decl,
            "LLVM boundary call argument allocation failed");

    unsigned arg_idx = 0;
    unsigned emitted_idx = 0;
    for (size_t i = 0; i < param_count && arg_idx < argc; i++) {
        bool is_secure = false;
        FuncParam *p = routine_has_signature
            ? llvm_mir_routine_param(routine, i)
            : ast_func_param(decl, i);
        const char *param_type_name = routine_has_signature
            ? llvm_mir_routine_param_type_name(routine, i)
            : NULL;
        const char *inner = param_type_name != NULL
            ? llvm_boundary_slot_inner_name_from_type_name(ctx, p,
                param_type_name, &is_secure)
            : llvm_boundary_slot_inner_name(ctx, p, &is_secure);
        ASTNode *arg_node = arg_nodes[arg_idx++];
        bool pointer_self = param_type_name != NULL
            ? llvm_type_name_uses_pointer_self(ctx, param_type_name)
            : llvm_boundary_param_uses_pointer_self(ctx, p);

        if (ctx->has_error)
            return NULL;

        if (inner != NULL && arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
            const char *source_name = ast_identifier_name(arg_node);
            LLVMVarEntry slot_var;
            bool has_slot_var =
                llvm_scope_lookup_snapshot(ctx, source_name, &slot_var);
            args[emitted_idx++] = has_slot_var
                ? llvm_boundary_slot_runtime_arg(ctx, &slot_var)
                : llvm_emit_expression(arg_node, ctx);
            if (args[emitted_idx - 1] == NULL)
                return llvm_boundary_args_error(ctx, arg_node,
                    "LLVM boundary slot argument could not be lowered");
            if (is_secure) {
                LLVMVarEntry token_var;
                if (llvm_lookup_secure_token_var(ctx, source_name, &token_var)) {
                    LLVMTypeRef token_ty = token_var.type;
                    args[emitted_idx++] = LLVMBuildLoad2(ctx->builder, token_ty,
                        token_var.alloca, llvm_tmp_name(ctx));
                } else {
                    return llvm_boundary_args_error(ctx, arg_node,
                        "LLVM secure boundary slot argument requires paired token binding");
                }
            }
            continue;
        }

        if (pointer_self && arg_node != NULL) {
            if (arg_node->type == AST_IDENTIFIER) {
                const char *source_name = ast_identifier_name(arg_node);
                LLVMVarEntry var;
                if (llvm_scope_lookup_snapshot(ctx, source_name, &var)) {
                    args[emitted_idx++] = LLVMGetTypeKind(var.type) == LLVMPointerTypeKind
                        ? LLVMBuildLoad2(ctx->builder, var.type, var.alloca, llvm_tmp_name(ctx))
                        : var.alloca;
                    continue;
                }
            } else if (arg_node->type == AST_MEMBER_ACCESS) {
                LLVMValueRef ptr = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
                if (ptr != NULL) {
                    args[emitted_idx++] = ptr;
                    continue;
                }
            }
            if (!llvm_boundary_arg_can_take_subject_address(arg_node)) {
                return llvm_boundary_args_error(ctx, arg_node,
                    "LLVM boundary subject argument requires addressable storage");
            }
        }

        {
            LLVMTypeRef saved_ret = ctx->current_ret_type;
            LLVMTypeRef expected_ty = param_type_name != NULL
                ? pergyra_type_to_llvm(ctx, param_type_name)
                : (p != NULL && p->type != NULL
                && !llvm_boundary_type_mentions_generic_param(decl, p->type)
                ? ast_type_to_llvm(ctx, p->type)
                : NULL);
            if (ctx->has_error) {
                ctx->current_ret_type = saved_ret;
                return llvm_boundary_args_error(ctx, arg_node,
                    "LLVM boundary call parameter type could not be lowered");
            }
            if (expected_ty != NULL)
                ctx->current_ret_type = expected_ty;
            args[emitted_idx++] = llvm_emit_expression(arg_node, ctx);
            ctx->current_ret_type = saved_ret;
        }
        if (args[emitted_idx - 1] == NULL)
            return llvm_boundary_args_error(ctx, arg_node,
                "LLVM boundary call argument could not be lowered");
    }

    if (out_count != NULL)
        *out_count = emitted_idx;
    return args;
}

#endif
