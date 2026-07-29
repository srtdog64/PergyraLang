/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM enum-variant constructor payload lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_constructor_internal.h"

#include "llvm_inventory_decl_lookup.h"
#include "../compiler/mir_decl_headers.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

LLVMValueRef
llvm_emit_enum_variant_constructor(ASTNode *node, LLVMGenCtx *ctx,
                                   const char *callee_name)
{
    LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, callee_name);
    if (variant == NULL)
        return NULL;

    LLVMClassTypeEntry *enum_cls = llvm_lookup_class(ctx, variant->enum_name);
    if (enum_cls == NULL)
        return llvm_constructor_error(node, ctx,
            "LLVM enum variant constructor requires class metadata");

    size_t variant_index = (size_t)variant->value;
    const MIRDeclHeader *enum_header = llvm_find_decl_header_in_context_of_type(
        ctx, AST_ENUM_DECL, variant->enum_name);
    if (enum_header == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing enum constructor variant metadata for '%s'",
            variant->enum_name != NULL ? variant->enum_name : "<anonymous-enum>");
        return NULL;
    }
    const MIRDeclEnumVariant *variant_meta =
        mir_decl_header_enum_variant(enum_header, variant_index);
    if (variant_meta == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path has invalid enum constructor variant metadata for '%s'",
            variant->enum_name != NULL ? variant->enum_name : "<anonymous-enum>");
        return NULL;
    }
    size_t param_count = mir_decl_enum_variant_param_count(variant_meta);
    LLVMValueRef enum_val = LLVMGetUndef(enum_cls->struct_type);
    enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
        LLVMConstInt(ctx->type_i32, (unsigned long long)variant->value, 0),
        0, llvm_tmp_name(ctx));

    if (param_count > 0) {
        int field_idx = llvm_class_field_index(enum_cls, callee_name);
        if (field_idx > 0) {
            LLVMTypeRef payload_ty =
                llvm_class_field_type_at_index(enum_cls, field_idx);
            if (payload_ty == NULL) {
                return llvm_constructor_error(node, ctx,
                    "LLVM enum variant payload type metadata not found for field index");
            }
            LLVMValueRef payload = LLVMGetUndef(payload_ty);
            LLVMClassTypeEntry *payload_cls = llvm_lookup_class_by_type(ctx, payload_ty);

            for (size_t i = 0; i < param_count
                && i < ast_call_arg_count(node); i++) {
                ASTNode *arg_node = ast_call_argument(node, i);
                LLVMTypeRef arg_type = llvm_stmt_infer_expr_type(ctx,
                    arg_node);
                LLVMValueRef arg;
                if (ctx->has_error)
                    return NULL;
                if (arg_type == ctx->type_void) {
                    llvm_set_error_at_with_hints(ctx, arg_node,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_ALIGN_ARG_TYPE,
                        "LLVM enum variant constructor '%s' cannot consume a Void expression as payload %zu",
                        callee_name != NULL ? callee_name : "<variant>",
                        i + 1);
                    return NULL;
                }
                arg = llvm_emit_expression(arg_node, ctx);
                if (arg == NULL)
                    return llvm_constructor_error(node, ctx,
                        "LLVM enum variant constructor could not lower payload argument");
                if (payload_cls != NULL
                    && i < (size_t)llvm_class_field_count(payload_cls)) {
                    LLVMTypeRef target_ty =
                        llvm_class_field_type_at(payload_cls, (int)i);
                    if (target_ty != NULL && target_ty != LLVMTypeOf(arg)) {
                        if ((target_ty == ctx->type_i32 || target_ty == ctx->type_i64)
                            && (LLVMTypeOf(arg) == ctx->type_i32
                                || LLVMTypeOf(arg) == ctx->type_i64)) {
                            arg = (LLVMGetIntTypeWidth(target_ty)
                                > LLVMGetIntTypeWidth(LLVMTypeOf(arg)))
                                ? LLVMBuildSExt(ctx->builder, arg, target_ty,
                                    llvm_tmp_name(ctx))
                                : LLVMBuildTrunc(ctx->builder, arg, target_ty,
                                    llvm_tmp_name(ctx));
                        }
                    }
                }
                payload = LLVMBuildInsertValue(ctx->builder, payload, arg,
                    (unsigned)i, llvm_tmp_name(ctx));
            }
            enum_val = LLVMBuildInsertValue(ctx->builder, enum_val, payload,
                (unsigned)field_idx, llvm_tmp_name(ctx));
        }
    }

    return enum_val;
}

#endif /* PGY_LLVM_ENABLED */
