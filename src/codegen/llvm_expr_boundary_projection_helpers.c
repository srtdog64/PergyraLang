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
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"

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

ASTNode *
llvm_find_function_decl(LLVMGenCtx *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_FUNC_DECL, name);
    if (decl != NULL)
        return decl;
    return llvm_lookup_generic_template(ctx, name);
}

ASTNode *
llvm_find_intent_decl(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_INTENT_DECL, name);
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

    if (out_count != NULL)
        *out_count = 0;
    if (ctx == NULL || decl == NULL || decl->type != AST_FUNC_DECL)
        return NULL;
    size_t param_count = ast_func_param_count(decl);
    if (argc != param_count)
        return llvm_boundary_args_error(ctx, decl,
            "LLVM boundary call source argument count does not match function signature");

    for (size_t i = 0; i < param_count; i++) {
        bool is_secure = false;
        FuncParam *p = ast_func_param(decl, i);
        emitted_count++;
        if (llvm_boundary_slot_inner_name(ctx, p, &is_secure) != NULL && is_secure)
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
        FuncParam *p = ast_func_param(decl, i);
        const char *inner = llvm_boundary_slot_inner_name(ctx, p, &is_secure);
        ASTNode *arg_node = arg_nodes[arg_idx++];
        bool pointer_self = llvm_boundary_param_uses_pointer_self(ctx, p);

        if (inner != NULL && arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
            const char *source_name = ast_identifier_name(arg_node);
            LLVMVarEntry *slot_var = llvm_scope_lookup(ctx, source_name);
            args[emitted_idx++] = slot_var != NULL
                ? llvm_boundary_slot_runtime_arg(ctx, slot_var)
                : llvm_emit_expression(arg_node, ctx);
            if (args[emitted_idx - 1] == NULL)
                return llvm_boundary_args_error(ctx, arg_node,
                    "LLVM boundary slot argument could not be lowered");
            if (is_secure) {
                LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
                if (token_var != NULL) {
                    LLVMTypeRef token_ty = token_var->type;
                    args[emitted_idx++] = LLVMBuildLoad2(ctx->builder, token_ty,
                        token_var->alloca, llvm_tmp_name(ctx));
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
                LLVMVarEntry *var = llvm_scope_lookup(ctx, source_name);
                if (var != NULL) {
                    args[emitted_idx++] = LLVMGetTypeKind(var->type) == LLVMPointerTypeKind
                        ? LLVMBuildLoad2(ctx->builder, var->type, var->alloca, llvm_tmp_name(ctx))
                        : var->alloca;
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

        args[emitted_idx++] = llvm_emit_expression(arg_node, ctx);
        if (args[emitted_idx - 1] == NULL)
            return llvm_boundary_args_error(ctx, arg_node,
                "LLVM boundary call argument could not be lowered");
    }

    if (out_count != NULL)
        *out_count = emitted_idx;
    return args;
}

void
llvm_append_mangled_suffix(char *buf, size_t buf_size, const char *suffix)
{
    if (buf == NULL || buf_size == 0 || suffix == NULL)
        return;

    size_t len = strlen(buf);
    if (len >= buf_size - 1)
        return;

    buf[len++] = '_';

    size_t remaining = buf_size - len - 1;
    size_t suffix_len = strlen(suffix);
    if (suffix_len > remaining)
        suffix_len = remaining;

    memcpy(buf + len, suffix, suffix_len);
    buf[len + suffix_len] = '\0';
}

#endif
