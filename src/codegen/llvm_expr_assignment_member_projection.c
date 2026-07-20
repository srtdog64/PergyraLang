#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_assignment_member_projection.h"

#include <stdio.h>
#include <string.h>

#include "codegen_slot_type_policy.h"
#include "llvm_expr_assignment_projection.h"
#include "llvm_expr_member_lvalue.h"
#include "llvm_internal_api.h"
#include "llvm_runtime_internal.h"
#include "llvm_mir_local_expected_type.h"
#include "../compiler/mir_abi_layout.h"

static LLVMValueRef
llvm_assignment_error(LLVMGenCtx *ctx, ASTNode *node, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s", message != NULL ? message
                : "LLVM assignment requires registered target metadata");
    }
    return NULL;
}

static LLVMValueRef
llvm_emit_current_host_field_assignment(ASTNode *node,
                                        LLVMGenCtx *ctx,
                                        const char *name,
                                        ASTNode *target,
                                        ASTNode *value)
{
    const char *host_name;
    LLVMClassTypeEntry *cls;
    LLVMValueRef base_ptr;
    LLVMValueRef gep;
    LLVMValueRef val;
    LLVMTypeRef field_type;
    int field_idx;

    if (ctx == NULL || name == NULL || value == NULL)
        return NULL;
    host_name = llvm_current_host_class_name(ctx);
    if (host_name == NULL)
        return NULL;
    cls = llvm_lookup_class(ctx, host_name);
    field_idx = cls != NULL ? llvm_class_field_index(cls, name) : -1;
    if (field_idx < 0)
        return NULL;

    base_ptr = llvm_current_self_base_ptr(ctx, cls);
    if (base_ptr == NULL)
        return NULL;
    field_type = llvm_class_field_type_at_index(cls, field_idx);
    if (field_type == NULL)
        return NULL;

    val = llvm_emit_expression(value, ctx);
    if (val == NULL)
        return llvm_assignment_error(ctx, node,
            "LLVM host field assignment could not lower value expression");
    if (LLVMTypeOf(val) != field_type) {
        if ((field_type == ctx->type_i32 || field_type == ctx->type_i64)
            && (LLVMTypeOf(val) == ctx->type_f32
                || LLVMTypeOf(val) == ctx->type_f64)) {
            val = llvm_build_checked_fptosi(ctx, val, field_type,
                llvm_tmp_name(ctx));
        } else if ((field_type == ctx->type_f32
                    || field_type == ctx->type_f64)
                   && (LLVMTypeOf(val) == ctx->type_i32
                       || LLVMTypeOf(val) == ctx->type_i64)) {
            val = LLVMBuildSIToFP(ctx->builder, val, field_type,
                llvm_tmp_name(ctx));
        } else if ((field_type == ctx->type_i32 || field_type == ctx->type_i64)
                   && (LLVMTypeOf(val) == ctx->type_i32
                       || LLVMTypeOf(val) == ctx->type_i64)) {
            val = LLVMGetIntTypeWidth(field_type)
                    > LLVMGetIntTypeWidth(LLVMTypeOf(val))
                ? LLVMBuildSExt(ctx->builder, val, field_type,
                    llvm_tmp_name(ctx))
                : LLVMBuildTrunc(ctx->builder, val, field_type,
                    llvm_tmp_name(ctx));
        }
    }

    gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
        (unsigned)field_idx, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, val, gep);
    {
        LLVMVarEntry local_alias;
        if (llvm_scope_lookup_snapshot(ctx, name, &local_alias)
            && local_alias.alloca != NULL
            && local_alias.alloca != gep
            && local_alias.type == field_type) {
            LLVMBuildStore(ctx->builder, val, local_alias.alloca);
        }
    }
    llvm_emit_host_projection_invalidations(ctx, target);
    llvm_emit_world_embedded_assignment_sync(ctx, target);
    return val;
}

LLVMValueRef
llvm_emit_assignment_parts(ASTNode *diagnostic_anchor,
                           ASTNode *target,
                           ASTNode *value,
                           LLVMGenCtx *ctx)
{
    ASTNode *node = diagnostic_anchor != NULL ? diagnostic_anchor : target;

    if (target == NULL)
        return llvm_assignment_error(ctx, node,
            "LLVM assignment requires a target expression");

    if (target->type == AST_ARRAY_ACCESS) {
        ASTNode *array_node = ast_array_access_array(target);
        if (array_node == NULL)
            return llvm_assignment_error(ctx, node,
                "LLVM indexed array assignment requires a receiver");
        {
            LLVMValueRef array_ptr = NULL;
            LLVMTypeRef array_type = NULL;
            LLVMTypeRef elem_type = NULL;
            const char *recv_struct = NULL;
            bool recv_is_slice = false;
            LLVMValueRef idx = llvm_emit_expression(
                ast_array_access_index(target), ctx);
            LLVMValueRef val = llvm_emit_expression(value, ctx);
            const char *suffix;
            char fn_name[64];
            LLVMFuncEntry *fn;
            LLVMValueRef index64;

            if (array_node->type == AST_IDENTIFIER) {
                const char *name = ast_identifier_name(array_node);
                LLVMVarEntry arr_var;
                LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
                if (llvm_scope_lookup_snapshot(ctx, name, &arr_var)) {
                    array_ptr = arr_var.alloca;
                    array_type = arr_var.type;
                }
                elem_type = entry != NULL
                    ? entry->elem_type
                    : llvm_stmt_resolve_array_elem_type(ctx, array_node, NULL);
            } else if (array_node->type == AST_MEMBER_ACCESS) {
                array_ptr = llvm_emit_member_lvalue_ptr(
                    array_node, ctx, &array_type);
            } else {
                return llvm_assignment_error(ctx, node,
                    "LLVM indexed array assignment requires an identifier or member receiver");
            }
            if (array_type != NULL
                && LLVMGetTypeKind(array_type) == LLVMStructTypeKind) {
                recv_struct = LLVMGetStructName(array_type);
            }
            if (recv_struct != NULL
                && (strncmp(recv_struct, "PgyArray_", 9) == 0
                    || strncmp(recv_struct, "PgySlice_", 9) == 0)) {
                recv_is_slice = strncmp(recv_struct, "PgySlice_", 9) == 0;
                if (elem_type == NULL)
                    elem_type = pergyra_type_to_llvm(ctx, recv_struct + 9);
            }

            if (array_ptr == NULL || elem_type == NULL)
                return llvm_assignment_error(ctx, node,
                    "LLVM indexed array assignment requires concrete Array<T> receiver metadata");
            if (idx == NULL)
                return llvm_assignment_error(ctx, node,
                    "LLVM indexed array assignment could not lower index expression");
            if (val == NULL)
                return llvm_assignment_error(ctx, node,
                    "LLVM indexed array assignment could not lower value expression");

            suffix = llvm_type_to_suffix(ctx, elem_type);
            if (suffix == NULL || strcmp(suffix, "Unknown") == 0) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM indexed array assignment requires concrete Array<T> element metadata");
                return NULL;
            }
            if (LLVMTypeOf(val) != elem_type) {
                if ((elem_type == ctx->type_i32
                     || elem_type == ctx->type_i64)
                    && (LLVMTypeOf(val) == ctx->type_f32
                        || LLVMTypeOf(val) == ctx->type_f64)) {
                    val = llvm_build_checked_fptosi(ctx, val,
                        elem_type, llvm_tmp_name(ctx));
                } else if ((elem_type == ctx->type_f32
                            || elem_type == ctx->type_f64)
                           && (LLVMTypeOf(val) == ctx->type_i32
                               || LLVMTypeOf(val) == ctx->type_i64)) {
                    val = LLVMBuildSIToFP(ctx->builder, val,
                        elem_type, llvm_tmp_name(ctx));
                }
            }
            snprintf(fn_name, sizeof(fn_name),
                recv_is_slice ? "pgy_slice_set_%s" : "pgy_array_set_%s",
                suffix);
            fn = llvm_required_runtime_function(ctx, node,
                recv_is_slice
                    ? "indexed slice assignment"
                    : "indexed array assignment",
                recv_is_slice ? "SliceSet" : "ArraySet", fn_name);
            if (fn == NULL)
                return NULL;
            index64 = idx;
            if (LLVMTypeOf(index64) != ctx->type_i64) {
                index64 = LLVMBuildSExtOrBitCast(ctx->builder, index64,
                    ctx->type_i64, llvm_tmp_name(ctx));
            }
            LLVMValueRef args[] = { array_ptr, index64, val };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
            return val;
        }
    }

    if (target->type == AST_MEMBER_ACCESS) {
        LLVMTypeRef field_type = NULL;
        LLVMValueRef gep = llvm_emit_member_lvalue_ptr(
            target, ctx, &field_type);
        LLVMValueRef val;
        if (gep == NULL || field_type == NULL)
            return llvm_assignment_error(ctx, node,
                "LLVM member assignment requires a writable member lvalue");
        val = llvm_emit_expression(value, ctx);
        if (val == NULL)
            return llvm_assignment_error(ctx, node,
                "LLVM member assignment could not lower value expression");
        if (LLVMTypeOf(val) != field_type) {
            if ((field_type == ctx->type_i32 || field_type == ctx->type_i64)
                && (LLVMTypeOf(val) == ctx->type_f32 || LLVMTypeOf(val) == ctx->type_f64)) {
                val = llvm_build_checked_fptosi(ctx, val, field_type, llvm_tmp_name(ctx));
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
        llvm_emit_host_projection_invalidations(ctx, target);
        llvm_emit_world_embedded_assignment_sync(ctx, target);
        return val;
    }

    const char *name = NULL;
    if (target->type == AST_IDENTIFIER)
        name = ast_identifier_name(target);

    if (name == NULL)
        return llvm_assignment_error(ctx, node,
            "LLVM assignment requires an identifier, member, or indexed target");

    {
        LLVMValueRef host_field_value =
            llvm_emit_current_host_field_assignment(node, ctx, name, target,
                value);
        if (host_field_value != NULL || ctx->has_error)
            return host_field_value;
    }

    LLVMVarEntry var;
    if (!llvm_scope_lookup_snapshot(ctx, name, &var))
        return llvm_assignment_error(ctx, node,
            "LLVM assignment requires a registered local or host field target");

    {
        const char *slot_inner = llvm_lookup_slot_inner(ctx, name);
        if (slot_inner != NULL) {
            bool is_secure = llvm_lookup_slot_is_secure(ctx, name);
            const MIRResourceRuntimeRow *runtime_row =
                llvm_slot_runtime_row_for_operation(node, ctx,
                    is_secure ? MIR_RESOURCE_ABI_SECURE_SLOT
                              : MIR_RESOURCE_ABI_SLOT,
                    slot_inner, "Write");
            if (ctx->has_error)
                return NULL;
            const char *runtime_fn = runtime_row != NULL
                ? runtime_row->runtime_fn : NULL;
            LLVMFuncEntry *fn = runtime_fn != NULL
                ? llvm_lookup_function(ctx, runtime_fn)
                : NULL;
            LLVMValueRef val = llvm_emit_expression(value, ctx);
            if (val == NULL)
                return llvm_assignment_error(ctx, node,
                    "LLVM slot assignment could not lower value expression");
            if (fn != NULL) {
                if (is_secure) {
                    LLVMVarEntry token_var;
                    if (!llvm_require_secure_token_var(ctx, node, name,
                            "assignment", &token_var))
                        return NULL;
                    LLVMValueRef args[] = {
                        llvm_slot_runtime_arg(ctx, &var),
                        val,
                        token_var.alloca
                    };
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
                } else {
                    LLVMValueRef args[] = {
                        llvm_slot_runtime_arg(ctx, &var),
                        val
                    };
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                }
            } else if (llvm_slot_inner_has_external_runtime_helpers(slot_inner)) {
                if (runtime_fn != NULL) {
                    llvm_required_runtime_function(ctx, node,
                        is_secure ? "secure slot" : "slot",
                        "assignment", runtime_fn);
                    return NULL;
                }
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM slot assignment requires MIR ABI runtime function row");
                return NULL;
            } else {
                if (is_secure)
                    llvm_emit_structural_secure_slot_write(ctx, &var, val);
                else
                    llvm_direct_slot_write(ctx, &var, val);
            }
            return val;
        }
    }

    {
        LLVMTypeRef saved_current_ret_type = ctx->current_ret_type;
        LLVMValueRef val;
        ctx->current_ret_type = var.type;
        val = llvm_emit_expression(value, ctx);
        ctx->current_ret_type = saved_current_ret_type;
        if (val == NULL)
            return llvm_assignment_error(ctx, node,
                "LLVM assignment could not lower value expression");

        LLVMBuildStore(ctx->builder, val, var.alloca);
        return val;
    }
}

LLVMValueRef
llvm_emit_mir_assignment_parts(const MIRRoutine *routine,
                               const MIRInstruction *inst,
                               ASTNode *diagnostic_anchor,
                               ASTNode *target,
                               ASTNode *value,
                               LLVMGenCtx *ctx)
{
    const char *saved_expected_type_name;
    LLVMTypeRef saved_current_ret_type;
    const char *expected_type_name;
    LLVMValueRef assigned;

    if (ctx == NULL)
        return NULL;
    saved_expected_type_name = ctx->expected_type_name;
    saved_current_ret_type = ctx->current_ret_type;
    expected_type_name = llvm_mir_local_expected_type_name(
        routine, inst, inst != NULL ? inst->arg0 : NULL);
    if (expected_type_name != NULL && expected_type_name[0] != '\0') {
        ctx->expected_type_name = expected_type_name;
        ctx->current_ret_type = pergyra_type_to_llvm(ctx, expected_type_name);
    }
    assigned = llvm_emit_assignment_parts(
        diagnostic_anchor, target, value, ctx);
    ctx->current_ret_type = saved_current_ret_type;
    ctx->expected_type_name = saved_expected_type_name;
    return assigned;
}

LLVMValueRef
llvm_emit_assignment(ASTNode *node, LLVMGenCtx *ctx)
{
    return llvm_emit_assignment_parts(node,
        ast_assignment_target(node),
        ast_assignment_value(node),
        ctx);
}

#endif
