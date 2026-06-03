/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM expression shared helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

bool
llvm_is_upper_ident(ASTNode *node)
{
    const char *name = ast_identifier_name(node);
    if (node == NULL || node->type != AST_IDENTIFIER
        || name == NULL
        || name[0] == '\0')
        return false;

    return name[0] >= 'A' && name[0] <= 'Z';
}

LLVMValueRef
llvm_current_self_base_ptr(LLVMGenCtx *ctx, LLVMClassTypeEntry *cls)
{
    LLVMVarEntry *self_var;

    if (ctx == NULL || cls == NULL)
        return NULL;

    self_var = llvm_scope_lookup(ctx, "self");
    if (self_var == NULL)
        return NULL;

    if (self_var->type == LLVMPointerType(cls->struct_type, 0))
        return LLVMBuildLoad2(ctx->builder, self_var->type, self_var->alloca,
            llvm_tmp_name(ctx));
    return self_var->alloca;
}

LLVMValueRef
llvm_identifier_base_ptr(LLVMGenCtx *ctx, const char *name, LLVMClassTypeEntry *cls)
{
    LLVMVarEntry *var;

    if (ctx == NULL || name == NULL || cls == NULL)
        return NULL;

    var = llvm_scope_lookup(ctx, name);
    if (var != NULL) {
        LLVMValueRef base_ptr = var->alloca;
        if (var->type == LLVMPointerType(cls->struct_type, 0)) {
            base_ptr = LLVMBuildLoad2(ctx->builder, var->type, var->alloca,
                llvm_tmp_name(ctx));
        }
        return base_ptr;
    }

    {
        const char *host_name = llvm_current_host_class_name(ctx);
        LLVMClassTypeEntry *parent_cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
        int parent_field_idx;
        LLVMValueRef self_ptr;
        if (parent_cls == NULL)
            return NULL;
        parent_field_idx = llvm_class_field_index(parent_cls, name);
        if (parent_field_idx < 0)
            return NULL;
        self_ptr = llvm_current_self_base_ptr(ctx, parent_cls);
        if (self_ptr == NULL)
            return NULL;
        return LLVMBuildStructGEP2(ctx->builder, parent_cls->struct_type, self_ptr,
            (unsigned)parent_field_idx, llvm_tmp_name(ctx));
    }

    return NULL;
}

ASTNode *
llvm_current_host_method_decl(LLVMGenCtx *ctx, const char *method_name)
{
    const char *host_name;

    host_name = llvm_current_host_class_name(ctx);

    if (ctx == NULL || method_name == NULL || host_name == NULL)
        return NULL;
    return llvm_find_nominal_host_method_decl(ctx, host_name,
                                              method_name);
}

LLVMValueRef
llvm_current_self_call_arg(LLVMGenCtx *ctx)
{
    LLVMVarEntry *self_var;

    if (ctx == NULL)
        return NULL;

    self_var = llvm_scope_lookup(ctx, "self");
    if (self_var == NULL)
        return NULL;

    return LLVMBuildLoad2(ctx->builder, self_var->type, self_var->alloca,
        llvm_tmp_name(ctx));
}

const char *
llvm_operator_overload_suffix(PgyTokenType op)
{
    switch (op) {
    case TOKEN_PLUS: return "add";
    case TOKEN_MINUS: return "sub";
    case TOKEN_STAR: return "mul";
    case TOKEN_SLASH: return "div";
    case TOKEN_PERCENT: return "mod";
    case TOKEN_EQUAL: return "eq";
    case TOKEN_NOT_EQUAL: return "ne";
    case TOKEN_LESS: return "lt";
    case TOKEN_LESS_EQUAL: return "le";
    case TOKEN_GREATER: return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default: return NULL;
    }
}

static const char *
llvm_find_local_let_type_in_block(ASTNode *body, const char *name)
{
    if (body == NULL || name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(body); i++) {
            const char *found = llvm_find_local_let_type_in_block(
                ast_block_statement(body, i), name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && ast_let_name(body) != NULL
        && strcmp(ast_let_name(body), name) == 0
        && ast_let_type(body) != NULL
        && ast_let_type(body)->type == AST_TYPE) {
        return ast_type_name(ast_let_type(body));
    }
    return NULL;
}

const char *
llvm_expr_custom_type_name(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_IDENTIFIER: {
        const char *name = ast_identifier_name(node);
        const char *class_name = llvm_lookup_var_class(ctx, name);
        if (class_name != NULL)
            return class_name;
        if (strcmp(name, "self") != 0) {
            const char *host_name = llvm_current_host_class_name(ctx);
            LLVMClassTypeEntry *host_cls = host_name != NULL
                ? llvm_lookup_class(ctx, host_name) : NULL;
            if (host_cls != NULL) {
                int field_idx = llvm_class_field_index(host_cls, name);
                if (field_idx >= 0) {
                    LLVMTypeRef field_ty =
                        llvm_class_field_type_at_index(host_cls, field_idx);
                    LLVMClassTypeEntry *field_cls =
                        llvm_lookup_class_by_type(ctx, field_ty);
                    if (field_cls != NULL)
                        return field_cls->class_name;
                }
            }
        }
        {
            LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, name);
            if (variant != NULL)
                return variant->enum_name;
        }
        if (ctx != NULL && ctx->current_func_decl != NULL
            && ctx->current_func_decl->type == AST_FUNC_DECL) {
            const char *let_type = llvm_find_local_let_type_in_block(
                ast_func_body(ctx->current_func_decl), name);
            if (let_type != NULL
                && llvm_lookup_class(ctx, let_type) != NULL) {
                return let_type;
            }
        }
        return NULL;
    }
    case AST_MEMBER_ACCESS:
        if (llvm_is_upper_ident(ast_member_object(node))) {
            const char *object_name = ast_identifier_name(ast_member_object(node));
            LLVMEnumVariantEntry *variant =
                llvm_lookup_enum_variant_qualified(ctx,
                    object_name,
                    ast_member_name(node));
            if (variant != NULL)
                return variant->enum_name;
        }
        {
            const char *base_name =
                llvm_expr_custom_type_name(ast_member_object(node), ctx);
            LLVMClassTypeEntry *base_cls = NULL;
            if (base_name != NULL)
                base_cls = llvm_lookup_class(ctx, base_name);
            if (base_cls != NULL) {
                int field_idx = llvm_class_field_index(base_cls,
                    ast_member_name(node));
                if (field_idx >= 0) {
                    LLVMTypeRef field_ty =
                        llvm_class_field_type_at_index(base_cls, field_idx);
                    LLVMClassTypeEntry *field_cls =
                        llvm_lookup_class_by_type(ctx, field_ty);
                    if (field_cls != NULL)
                        return field_cls->class_name;
                }
            }
        }
        return NULL;
    case AST_CALL:
        if (ast_call_callee(node) != NULL
            && ast_call_callee(node)->type == AST_IDENTIFIER) {
            const char *callee = ast_identifier_name(ast_call_callee(node));
            if (llvm_lookup_class(ctx, callee) != NULL)
                return callee;
            {
                LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, callee);
                if (variant != NULL)
                    return variant->enum_name;
            }
            {
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, callee);
                ASTNode *host_decl = llvm_current_host_decl(ctx);
                const char *host_name = llvm_decl_node_name(host_decl);
                if (fn == NULL && host_name != NULL) {
                    char full_name[256];
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                        host_name, callee);
                    fn = llvm_lookup_function(ctx, full_name);
                }
                if (fn != NULL) {
                    LLVMClassTypeEntry *ret_cls =
                        llvm_lookup_class_by_type(ctx, fn->ret_type);
                    if (ret_cls != NULL)
                        return ret_cls->class_name;
                }
            }
        }
        if (ast_call_callee(node) != NULL
            && ast_call_callee(node)->type == AST_MEMBER_ACCESS) {
            const char *recv_cls = llvm_expr_custom_type_name(
                ast_member_object(ast_call_callee(node)), ctx);
            const char *method = ast_member_name(ast_call_callee(node));
            if (recv_cls != NULL && method != NULL) {
                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s_%s",
                    recv_cls, method);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
                if (fn != NULL) {
                    LLVMClassTypeEntry *ret_cls =
                        llvm_lookup_class_by_type(ctx, fn->ret_type);
                    if (ret_cls != NULL)
                        return ret_cls->class_name;
                }
            }
        }
        return NULL;
    default:
        return NULL;
    }
}

LLVMClassTypeEntry *
llvm_lookup_class_by_type(LLVMGenCtx *ctx, LLVMTypeRef ty)
{
    return llvm_lookup_class_by_struct_type(ctx, ty);
}

ASTNode *
llvm_find_enum_decl(LLVMGenCtx *ctx, const char *enum_name)
{
    if (ctx == NULL || enum_name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_ENUM_DECL, enum_name);
}

LLVMValueRef
llvm_emit_number(ASTNode *node, LLVMGenCtx *ctx)
{
    double val = ast_number_value(node);

    if (ast_number_is_long(node)) {
        return LLVMConstInt(ctx->type_i64, (unsigned long long)(int64_t)val, 1);
    }

    if (ast_number_is_float(node))
        return LLVMConstReal(ctx->type_f32, val);

    if (val == (int64_t)val && val >= -2147483648.0 && val <= 2147483647.0)
        return LLVMConstInt(ctx->type_i32, (unsigned long long)(int32_t)val, 1);

    if (val == (double)(int64_t)val
        && val >= -9.2233720368547758e+18
        && val <=  9.2233720368547758e+18)
        return LLVMConstInt(ctx->type_i64, (unsigned long long)(int64_t)val, 1);

    return LLVMConstReal(ctx->type_f64, val);
}

LLVMValueRef
llvm_emit_string(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *str = ast_string_value(node);
    LLVMValueRef global = LLVMBuildGlobalStringPtr(ctx->builder, str,
                                                    llvm_tmp_name(ctx));
    return global;
}

#endif /* PGY_LLVM_ENABLED */
