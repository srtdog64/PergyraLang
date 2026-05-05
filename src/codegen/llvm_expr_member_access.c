#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_member_access.h"

#include <string.h>

#include "llvm_expr_projection_path_helpers.h"
#include "llvm_internal_api.h"

LLVMValueRef
llvm_emit_member_access(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *obj_node = node->data.member.object;
    const char *field_name = node->data.member.name;

    if (obj_node == NULL || field_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (llvm_is_upper_ident(obj_node)) {
        LLVMEnumVariantEntry *variant =
            llvm_lookup_enum_variant_qualified(ctx,
                obj_node->data.identifier.name, field_name);
        if (variant != NULL)
            return LLVMConstInt(ctx->type_i32,
                (unsigned long long)variant->value, 0);
    }

    {
        const char *class_name = llvm_expr_custom_type_name(obj_node, ctx);
        LLVMClassTypeEntry *cls;
        int field_idx;

        if (class_name == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        cls = llvm_lookup_class(ctx, class_name);
        if (cls == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        field_idx = llvm_class_field_index(cls, field_name);
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        if (obj_node->type == AST_IDENTIFIER) {
            const char *var_name = obj_node->data.identifier.name;
            LLVMProjectionBorrowEntry *projection_borrow =
                llvm_lookup_projection_borrow(ctx, var_name);
            if (projection_borrow != NULL) {
                LLVMClassTypeEntry *source_cls;
                ASTNode *source_decl;
                LLVMVarEntry *source_var;
                const char *source_class_name = llvm_lookup_var_class(ctx,
                    projection_borrow->source_name);
                if (source_class_name == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                source_cls = llvm_lookup_class(ctx, source_class_name);
                source_decl = llvm_find_projection_nominal_decl(ctx, source_class_name);
                source_var = llvm_scope_lookup(ctx, projection_borrow->source_name);
                if (source_cls == NULL || source_decl == NULL || source_var == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                {
                    LLVMValueRef source_base = source_var->alloca;
                    if (source_var->type == LLVMPointerType(source_cls->struct_type, 0)) {
                        source_base = LLVMBuildLoad2(ctx->builder, source_var->type,
                            source_var->alloca, llvm_tmp_name(ctx));
                    }
                    return llvm_load_projection_path_value(ctx, source_decl, source_cls,
                        source_base, field_name);
                }
            }
            {
                LLVMValueRef base_ptr = llvm_identifier_base_ptr(ctx, var_name, cls);
                if (base_ptr != NULL) {
                    LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                        cls->struct_type, base_ptr, (unsigned)field_idx,
                        llvm_tmp_name(ctx));
                    LLVMTypeRef field_type = cls->fields[field_idx].field_type;
                    return LLVMBuildLoad2(ctx->builder, field_type, gep,
                        llvm_tmp_name(ctx));
                }
            }
        }

        {
            LLVMValueRef obj_val = llvm_emit_expression(obj_node, ctx);
            if (obj_val == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);

            if (LLVMTypeOf(obj_val) == LLVMPointerType(cls->struct_type, 0)) {
                LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                    cls->struct_type, obj_val, (unsigned)field_idx,
                    llvm_tmp_name(ctx));
                LLVMTypeRef field_type = cls->fields[field_idx].field_type;
                return LLVMBuildLoad2(ctx->builder, field_type, gep,
                    llvm_tmp_name(ctx));
            }

            if (LLVMTypeOf(obj_val) == cls->struct_type) {
                return LLVMBuildExtractValue(ctx->builder, obj_val,
                    (unsigned)field_idx, llvm_tmp_name(ctx));
            }
        }
    }

    return LLVMConstInt(ctx->type_i32, 0, 0);
}

#endif
