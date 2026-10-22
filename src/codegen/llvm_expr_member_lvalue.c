/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_member_lvalue.h"

#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_mir_local_type_lookup.h"

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
    } else if (obj_node->type == AST_ARRAY_ACCESS) {
        LLVMTypeRef element_type = llvm_mir_local_array_access_type(
            ctx->current_mir_routine, ctx, obj_node);
        if (ctx->has_error)
            return NULL;
        if (element_type != NULL && element_type != cls->struct_type) {
            llvm_set_error_at_with_hints(ctx, obj_node, PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED, PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM indexed member place disagrees with its MIR element layout");
            return NULL;
        }
        LLVMValueRef aggregate = llvm_emit_expression(ast_array_access_array(obj_node), ctx);
        LLVMValueRef index = llvm_emit_expression(ast_array_access_index(obj_node), ctx);
        LLVMTypeRef aggregate_type = aggregate != NULL ? LLVMTypeOf(aggregate) : NULL;
        const char *storage = aggregate_type != NULL && LLVMGetTypeKind(aggregate_type) == LLVMStructTypeKind
            ? LLVMGetStructName(aggregate_type) : NULL;
        if (storage != NULL && (strncmp(storage, "PgyArray_", 9) == 0 || strncmp(storage, "PgySlice_", 9) == 0))
            base_ptr = llvm_emit_checked_collection_address(ctx, aggregate, cls->struct_type, index, storage);
        if (base_ptr == NULL) {
            if (!ctx->has_error)
                llvm_set_error_at_with_hints(ctx, obj_node, PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED, PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM indexed member place requires checked Array/Slice backing metadata");
            return NULL;
        }
    } else {
        return NULL;
    }

    field_idx = llvm_class_field_index(cls, field_name);
    if (field_idx < 0)
        return NULL;

    if (out_field_type != NULL) {
        *out_field_type = llvm_class_field_type_at_index(cls, field_idx);
        if (*out_field_type == NULL)
            return NULL;
    }
    return LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
        (unsigned)field_idx, llvm_tmp_name(ctx));
}

#endif
