#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_assignment_member_projection.h"

#include <stdio.h>
#include <string.h>

#include "codegen_slot_type_policy.h"
#include "llvm_expr_assignment_projection.h"
#include "llvm_expr_member_lvalue.h"
#include "llvm_internal_api.h"

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
            val = LLVMBuildFPToSI(ctx->builder, val, field_type,
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
        LLVMVarEntry *local_alias = llvm_scope_lookup(ctx, name);
        if (local_alias != NULL && local_alias->alloca != NULL
            && local_alias->alloca != gep
            && local_alias->type == field_type) {
            LLVMBuildStore(ctx->builder, val, local_alias->alloca);
        }
    }
    llvm_emit_host_projection_invalidations(ctx, ast_assignment_target(node));
    llvm_emit_world_embedded_assignment_sync(ctx, ast_assignment_target(node));
    return val;
}

LLVMValueRef
llvm_emit_assignment(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *target = ast_assignment_target(node);
    ASTNode *value = ast_assignment_value(node);

    if (target == NULL)
        return llvm_assignment_error(ctx, node,
            "LLVM assignment requires a target expression");

    if (target->type == AST_ARRAY_ACCESS) {
        ASTNode *array_node = ast_array_access_array(target);
        if (array_node == NULL || array_node->type != AST_IDENTIFIER)
            return llvm_assignment_error(ctx, node,
                "LLVM indexed array assignment requires an identifier receiver");
        {
            const char *name = ast_identifier_name(array_node);
            LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, name);
            LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, name);
            LLVMTypeRef elem_type = entry != NULL
                ? entry->elem_type
                : llvm_stmt_resolve_array_elem_type(ctx, array_node, NULL);
            LLVMValueRef idx = llvm_emit_expression(
                ast_array_access_index(target), ctx);
            LLVMValueRef val = llvm_emit_expression(value, ctx);
            const char *suffix;
            char fn_name[64];
            LLVMFuncEntry *fn;
            LLVMValueRef index64;

            if (arr_var == NULL || elem_type == NULL)
                return llvm_assignment_error(ctx, node,
                    "LLVM indexed array assignment requires concrete Array<T> local metadata");
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
                    val = LLVMBuildFPToSI(ctx->builder, val,
                        elem_type, llvm_tmp_name(ctx));
                } else if ((elem_type == ctx->type_f32
                            || elem_type == ctx->type_f64)
                           && (LLVMTypeOf(val) == ctx->type_i32
                               || LLVMTypeOf(val) == ctx->type_i64)) {
                    val = LLVMBuildSIToFP(ctx->builder, val,
                        elem_type, llvm_tmp_name(ctx));
                }
            }
            snprintf(fn_name, sizeof(fn_name), "pgy_array_set_%s", suffix);
            fn = llvm_required_runtime_function(ctx, node,
                "indexed array assignment", "ArraySet", fn_name);
            if (fn == NULL)
                return NULL;
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
            llvm_emit_current_host_field_assignment(node, ctx, name, value);
        if (host_field_value != NULL || ctx->has_error)
            return host_field_value;
    }

    LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
    if (var == NULL)
        return llvm_assignment_error(ctx, node,
            "LLVM assignment requires a registered local or host field target");

    {
        const char *slot_inner = llvm_lookup_slot_inner(ctx, name);
        if (slot_inner != NULL) {
            bool is_secure = llvm_lookup_slot_is_secure(ctx, name);
            LLVMValueRef val = llvm_emit_expression(value, ctx);
            if (val == NULL)
                return llvm_assignment_error(ctx, node,
                    "LLVM slot assignment could not lower value expression");
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name),
                is_secure ? "pgy_secure_write_%s" : "pgy_write_%s", slot_inner);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn != NULL) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_require_secure_token_var(ctx,
                        node, name, "assignment");
                    if (token_var == NULL)
                        return NULL;
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
                return NULL;
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
        LLVMValueRef val = llvm_emit_expression(value, ctx);
        if (val == NULL)
            return llvm_assignment_error(ctx, node,
                "LLVM assignment could not lower value expression");

        LLVMBuildStore(ctx->builder, val, var->alloca);
        return val;
    }
}

#endif
