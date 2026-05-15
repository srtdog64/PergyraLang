/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_member_lvalue.h"

#include "llvm_internal_api.h"

LLVMValueRef
llvm_emit_member_lvalue_ptr(ASTNode *node, LLVMGenCtx *ctx,
                            LLVMTypeRef *out_field_type)
{
    ASTNode *obj_node;
    const char *field_name;
    const char *class_name;
    LLVMClassTypeEntry *cls;
    LLVMValueRef base_ptr = NULL;
    int field_idx;

    if (out_field_type != NULL)
        *out_field_type = NULL;
    if (node == NULL || node->type != AST_MEMBER_ACCESS)
        return NULL;

    obj_node = ast_member_object(node);
    field_name = ast_member_name(node);
    if (obj_node == NULL || field_name == NULL)
        return NULL;

    class_name = llvm_expr_custom_type_name(obj_node, ctx);
    if (class_name == NULL)
        return NULL;

    cls = llvm_lookup_class(ctx, class_name);
    if (cls == NULL)
        return NULL;

    if (obj_node->type == AST_IDENTIFIER) {
        const char *var_name = ast_identifier_name(obj_node);
        base_ptr = llvm_identifier_base_ptr(ctx, var_name, cls);
        if (base_ptr == NULL)
            return NULL;
    } else if (obj_node->type == AST_MEMBER_ACCESS) {
        base_ptr = llvm_emit_member_lvalue_ptr(obj_node, ctx, NULL);
        if (base_ptr == NULL)
            return NULL;
    } else {
        return NULL;
    }

    field_idx = llvm_class_field_index(cls, field_name);
    if (field_idx < 0)
        return NULL;

    if (out_field_type != NULL)
        *out_field_type = cls->fields[field_idx].field_type;
    return LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
        (unsigned)field_idx, llvm_tmp_name(ctx));
}

#endif
