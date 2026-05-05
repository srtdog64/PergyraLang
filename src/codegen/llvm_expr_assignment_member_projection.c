#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_assignment_member_projection.h"

#include <stdio.h>
#include <string.h>

#include "codegen_slot_type_policy.h"
#include "llvm_expr_assignment_projection.h"
#include "llvm_expr_member_lvalue.h"
#include "llvm_internal_api.h"

LLVMValueRef
llvm_emit_assignment(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.assignment.target == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (node->data.assignment.target->type == AST_ARRAY_ACCESS) {
        ASTNode *array_node = node->data.assignment.target->data.array_access.array;
        if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
            const char *name = array_node->data.identifier.name;
            LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, name);
            LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
            LLVMValueRef idx = llvm_emit_expression(
                node->data.assignment.target->data.array_access.index, ctx);
            LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
            if (arr_var != NULL && entry != NULL && idx != NULL && val != NULL) {
                const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
                char fn_name[64];
                LLVMFuncEntry *fn;
                LLVMValueRef index64;

                if (suffix == NULL || strcmp(suffix, "Unknown") == 0) {
                    llvm_set_error_at_with_hints(ctx, node,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "LLVM indexed array assignment requires concrete Array<T> element metadata");
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                }
                if (LLVMTypeOf(val) != entry->elem_type) {
                    if ((entry->elem_type == ctx->type_i32
                         || entry->elem_type == ctx->type_i64)
                        && (LLVMTypeOf(val) == ctx->type_f32
                            || LLVMTypeOf(val) == ctx->type_f64)) {
                        val = LLVMBuildFPToSI(ctx->builder, val,
                            entry->elem_type, llvm_tmp_name(ctx));
                    } else if ((entry->elem_type == ctx->type_f32
                                || entry->elem_type == ctx->type_f64)
                               && (LLVMTypeOf(val) == ctx->type_i32
                                   || LLVMTypeOf(val) == ctx->type_i64)) {
                        val = LLVMBuildSIToFP(ctx->builder, val,
                            entry->elem_type, llvm_tmp_name(ctx));
                    }
                }
                snprintf(fn_name, sizeof(fn_name), "pgy_array_set_%s", suffix);
                fn = llvm_required_runtime_function(ctx, node,
                    "indexed array assignment", "ArraySet", fn_name);
                if (fn == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                index64 = idx;
                if (LLVMTypeOf(index64) != ctx->type_i64) {
                    index64 = LLVMBuildSExtOrBitCast(ctx->builder, index64,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                LLVMValueRef args[] = { arr_var->alloca, index64, val };
                LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
                return val;
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (node->data.assignment.target->type == AST_MEMBER_ACCESS) {
        LLVMTypeRef field_type = NULL;
        LLVMValueRef gep = llvm_emit_member_lvalue_ptr(
            node->data.assignment.target, ctx, &field_type);
        LLVMValueRef val;
        if (gep == NULL || field_type == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        val = llvm_emit_expression(node->data.assignment.value, ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(val) != field_type) {
            if ((field_type == ctx->type_i32 || field_type == ctx->type_i64)
                && (LLVMTypeOf(val) == ctx->type_f32 || LLVMTypeOf(val) == ctx->type_f64)) {
                val = LLVMBuildFPToSI(ctx->builder, val, field_type, llvm_tmp_name(ctx));
            } else if ((field_type == ctx->type_f32 || field_type == ctx->type_f64)
                && (LLVMTypeOf(val) == ctx->type_i32 || LLVMTypeOf(val) == ctx->type_i64)) {
                val = LLVMBuildSIToFP(ctx->builder, val, field_type, llvm_tmp_name(ctx));
            } else if ((field_type == ctx->type_i32 || field_type == ctx->type_i64)
                && (LLVMTypeOf(val) == ctx->type_i32 || LLVMTypeOf(val) == ctx->type_i64)) {
                val = (LLVMGetIntTypeWidth(field_type) > LLVMGetIntTypeWidth(LLVMTypeOf(val)))
                    ? LLVMBuildSExt(ctx->builder, val, field_type, llvm_tmp_name(ctx))
                    : LLVMBuildTrunc(ctx->builder, val, field_type, llvm_tmp_name(ctx));
            }
        }
        LLVMBuildStore(ctx->builder, val, gep);
        llvm_emit_host_projection_invalidations(ctx, node->data.assignment.target);
        llvm_emit_world_embedded_assignment_sync(ctx, node->data.assignment.target);
        return val;
    }

    const char *name = NULL;
    if (node->data.assignment.target->type == AST_IDENTIFIER)
        name = node->data.assignment.target->data.identifier.name;

    if (name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
    if (var == NULL && llvm_current_host_class_name(ctx) != NULL) {
        LLVMClassTypeEntry *cls =
            llvm_lookup_class(ctx, llvm_current_host_class_name(ctx));
        LLVMVarEntry *self_var = llvm_scope_lookup(ctx, "self");
        if (cls != NULL && self_var != NULL) {
            int field_idx = llvm_class_field_index(cls, name);
            if (field_idx >= 0) {
                LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
                LLVMValueRef base_ptr;
                LLVMValueRef gep;
                if (val == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                base_ptr = self_var->alloca;
                if (self_var->type == LLVMPointerType(cls->struct_type, 0))
                    base_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
                        self_var->alloca, llvm_tmp_name(ctx));
                gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
                    (unsigned)field_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, val, gep);
                llvm_emit_host_projection_invalidations(ctx, node->data.assignment.target);
                llvm_emit_world_embedded_assignment_sync(ctx, node->data.assignment.target);
                return val;
            }
        }
    }
    if (var == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    {
        const char *slot_inner = llvm_lookup_slot_inner(ctx, name);
        if (slot_inner != NULL) {
            bool is_secure = llvm_lookup_slot_is_secure(ctx, name);
            LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
            if (val == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name),
                is_secure ? "pgy_secure_write_%s" : "pgy_write_%s", slot_inner);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn != NULL) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_require_secure_token_var(ctx,
                        node, name, "assignment");
                    if (token_var == NULL)
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    LLVMValueRef args[] = { var->alloca, val, token_var->alloca };
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
                } else {
                    LLVMValueRef args[] = { var->alloca, val };
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                }
            } else if (pgy_classify_type(slot_inner) != PGY_TK_UNKNOWN) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM slot assignment requires registered runtime function '%s'",
                    fn_name);
                return LLVMConstInt(ctx->type_i32, 0, 0);
            } else {
                if (is_secure)
                    llvm_emit_structural_secure_slot_write(ctx, var, val);
                else
                    llvm_direct_slot_write(ctx, var, val);
            }
            return val;
        }
    }

    {
        LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMBuildStore(ctx->builder, val, var->alloca);
        return val;
    }
}

#endif
