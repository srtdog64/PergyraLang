#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_methods_domain_slice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_slot_type_policy.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

static LLVMValueRef
llvm_domain_slice_error(ASTNode *node, LLVMGenCtx *ctx, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "%s",
            message != NULL ? message
                : "LLVM domain/slice method could not be lowered");
    }
    return NULL;
}

static bool
llvm_domain_slot_format_runtime_name(char *out, size_t out_size,
                                     const char *prefix, const char *inner)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || inner == NULL)
        return false;
    written = snprintf(out, out_size, "%s_%s", prefix, inner);
    return written >= 0 && (size_t)written < out_size;
}

static LLVMValueRef
llvm_domain_slot_runtime_name_error(ASTNode *node, LLVMGenCtx *ctx,
                                    const char *method_name)
{
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM slot method '%s' runtime function name is too long",
        method_name != NULL ? method_name : "<unknown>");
    return NULL;
}

static bool
llvm_emit_slice_bounds_check(LLVMGenCtx *ctx, ASTNode *node,
                             LLVMValueRef data_ptr,
                             LLVMValueRef start64,
                             LLVMValueRef len64,
                             LLVMValueRef length64)
{
    LLVMValueRef current_fn;
    LLVMFuncEntry *panic_fn;
    LLVMValueRef start_oob;
    LLVMValueRef remaining;
    LLVMValueRef len_oob;
    LLVMValueRef len_nonzero;
    LLVMValueRef data_null;
    LLVMValueRef null_oob;
    LLVMValueRef bounds_oob;
    LLVMValueRef reason_arg;
    LLVMBasicBlockRef ok_bb;
    LLVMBasicBlockRef fail_bb;

    if (ctx == NULL || data_ptr == NULL || start64 == NULL
        || len64 == NULL || length64 == NULL) {
        return false;
    }

    current_fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    if (current_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM Slice() bounds check requires an active function insertion block");
        return false;
    }

    panic_fn = llvm_lookup_function(ctx,
        "pgy_runtime_panic_out_of_bounds_export");
    if (panic_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM Slice() bounds check requires registered runtime function '%s'",
            "pgy_runtime_panic_out_of_bounds_export");
        return false;
    }

    start_oob = LLVMBuildICmp(ctx->builder, LLVMIntUGT, start64,
        length64, llvm_tmp_name(ctx));
    remaining = LLVMBuildSub(ctx->builder, length64, start64,
        llvm_tmp_name(ctx));
    len_oob = LLVMBuildICmp(ctx->builder, LLVMIntUGT, len64,
        remaining, llvm_tmp_name(ctx));
    len_nonzero = LLVMBuildICmp(ctx->builder, LLVMIntNE, len64,
        LLVMConstInt(ctx->type_i64, 0, 0), llvm_tmp_name(ctx));
    data_null = LLVMBuildICmp(ctx->builder, LLVMIntEQ, data_ptr,
        LLVMConstNull(LLVMTypeOf(data_ptr)), llvm_tmp_name(ctx));
    null_oob = LLVMBuildAnd(ctx->builder, len_nonzero, data_null,
        llvm_tmp_name(ctx));
    bounds_oob = LLVMBuildOr(ctx->builder,
        LLVMBuildOr(ctx->builder, start_oob, len_oob, llvm_tmp_name(ctx)),
        null_oob, llvm_tmp_name(ctx));

    fail_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
        "slice.bounds.panic");
    ok_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
        "slice.bounds.ok");
    LLVMBuildCondBr(ctx->builder, bounds_oob, fail_bb, ok_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    reason_arg = LLVMBuildGlobalStringPtr(ctx->builder,
        "slice out of bounds", llvm_tmp_name(ctx));
    LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
        &reason_arg, 1, "");
    LLVMBuildUnreachable(ctx->builder);

    LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
    return true;
}

LLVMValueRef
llvm_emit_member_call_slot_method(ASTNode *node, LLVMGenCtx *ctx,
                                  ASTNode *obj_node,
                                  const char *method_name)
{
    if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
        && method_name != NULL
        && pgy_codegen_call_name_is_slot_operation(method_name)) {
        const char *slot_name = ast_identifier_name(obj_node);
        const char *inner = llvm_lookup_slot_inner(ctx, slot_name);
        bool is_secure = llvm_lookup_slot_is_secure(ctx, slot_name);
        LLVMVarEntry *slot_var = inner != NULL ? llvm_scope_lookup(ctx, slot_name) : NULL;
        if (inner != NULL && slot_var == NULL) {
            llvm_set_error_at_with_hints(ctx, obj_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM slot method '%s' requires registered slot local '%s'",
                method_name, slot_name);
            return NULL;
        }
        if (inner != NULL && slot_var != NULL) {
            if (pgy_codegen_call_name_is_write(method_name)
                && ast_call_arg_count(node) >= 1) {
                LLVMValueRef val = llvm_emit_expression(
                    ast_call_argument(node, 0), ctx);
                if (val == NULL)
                    return llvm_domain_slice_error(node, ctx,
                        "LLVM slot Write() could not lower value expression");
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_require_secure_token_var(ctx,
                        node, slot_name, method_name);
                    if (token_var == NULL)
                        return NULL;
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        if (!llvm_domain_slot_format_runtime_name(fn_name,
                                sizeof(fn_name), "pgy_secure_write", inner))
                            return llvm_domain_slot_runtime_name_error(
                                node, ctx, method_name);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = {
                                llvm_slot_runtime_arg(ctx, slot_var),
                                val,
                                token_var->alloca
                            };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
                        } else if (!llvm_slot_inner_has_external_runtime_helpers(inner)) {
                            llvm_emit_structural_secure_slot_write(ctx, slot_var, val);
                        } else {
                            llvm_required_runtime_function(ctx, node,
                                "secure slot", method_name, fn_name);
                            return NULL;
                        }
                    }
                } else {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    if (!llvm_domain_slot_format_runtime_name(fn_name,
                            sizeof(fn_name), "pgy_write", inner))
                        return llvm_domain_slot_runtime_name_error(
                            node, ctx, method_name);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = {
                            llvm_slot_runtime_arg(ctx, slot_var),
                            val
                        };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    } else if (llvm_slot_inner_has_external_runtime_helpers(inner)) {
                        llvm_required_runtime_function(ctx, node,
                            "slot", method_name, fn_name);
                        return NULL;
                    } else {
                        llvm_direct_slot_write(ctx, slot_var, val);
                    }
                }
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }

            if (pgy_codegen_call_name_is_read(method_name)) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_require_secure_token_var(ctx,
                        node, slot_name, method_name);
                    if (token_var == NULL)
                        return NULL;
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        if (!llvm_domain_slot_format_runtime_name(fn_name,
                                sizeof(fn_name), "pgy_secure_read", inner))
                            return llvm_domain_slot_runtime_name_error(
                                node, ctx, method_name);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = {
                                llvm_slot_runtime_arg(ctx, slot_var),
                                token_var->alloca
                            };
                            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                args, 2, llvm_tmp_name(ctx));
                        }
                        if (!llvm_slot_inner_has_external_runtime_helpers(inner))
                            return llvm_emit_structural_secure_slot_read(ctx,
                                slot_var, inner);
                        llvm_required_runtime_function(ctx, node,
                            "secure slot", method_name, fn_name);
                        return NULL;
                    }
                }

                {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    if (!llvm_domain_slot_format_runtime_name(fn_name,
                            sizeof(fn_name), "pgy_read", inner))
                        return llvm_domain_slot_runtime_name_error(
                            node, ctx, method_name);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = {
                            llvm_slot_runtime_arg(ctx, slot_var)
                        };
                        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, 1, llvm_tmp_name(ctx));
                    }
                    if (llvm_slot_inner_has_external_runtime_helpers(inner)) {
                        llvm_required_runtime_function(ctx, node,
                            "slot", method_name, fn_name);
                        return NULL;
                    }
                    return llvm_direct_slot_read(ctx, slot_var, inner);
                }
            }

            if (pgy_codegen_call_name_is_release(method_name)) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_require_secure_token_var(ctx,
                        node, slot_name, method_name);
                    if (token_var == NULL)
                        return NULL;
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        if (!llvm_domain_slot_format_runtime_name(fn_name,
                                sizeof(fn_name), "pgy_secure_release", inner))
                            return llvm_domain_slot_runtime_name_error(
                                node, ctx, method_name);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = {
                                llvm_slot_runtime_arg(ctx, slot_var),
                                token_var->alloca
                            };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                        } else if (!llvm_slot_inner_has_external_runtime_helpers(inner)) {
                            llvm_emit_structural_secure_slot_release(ctx, slot_var);
                        } else {
                            llvm_required_runtime_function(ctx, node,
                                "secure slot", method_name, fn_name);
                            return NULL;
                        }
                    }
                } else {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    if (!llvm_domain_slot_format_runtime_name(fn_name,
                            sizeof(fn_name), "pgy_release", inner))
                        return llvm_domain_slot_runtime_name_error(
                            node, ctx, method_name);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = {
                            llvm_slot_runtime_arg(ctx, slot_var)
                        };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
                    } else if (llvm_slot_inner_has_external_runtime_helpers(inner)) {
                        llvm_required_runtime_function(ctx, node,
                            "slot", method_name, fn_name);
                        return NULL;
                    } else {
                        llvm_direct_slot_release(ctx, slot_var);
                    }
                }
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
        }
    }

    return NULL;
}

LLVMValueRef
llvm_emit_member_call_slice(ASTNode *node, LLVMGenCtx *ctx,
                            ASTNode *obj_node,
                            const char *method_name)
{
    if (obj_node != NULL && method_name != NULL
        && strcmp(method_name, "Slice") == 0
        && ast_call_arg_count(node) == 2) {
        LLVMValueRef receiver;
        LLVMValueRef start;
        LLVMValueRef len;
        const char *suffix = NULL;
        LLVMTypeRef elem_type = NULL;
        LLVMTypeRef slice_type = NULL;
        LLVMValueRef data_ptr;
        LLVMValueRef receiver_length;
        LLVMValueRef start64;
        LLVMValueRef len64;
        LLVMValueRef offset_ptr;
        LLVMValueRef slice_data_ptr;
        LLVMValueRef result;
        LLVMValueRef len_is_zero;
        unsigned field_count = 0;

        if (llvm_debug_detail_enabled())
            fprintf(stderr, "[llvm slice] phase=begin\n");
        receiver = llvm_emit_expression(obj_node, ctx);
        start = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        len = llvm_emit_expression(ast_call_argument(node, 1), ctx);

        if (receiver == NULL || start == NULL || len == NULL)
            return llvm_domain_slice_error(node, ctx,
                "LLVM Slice() could not lower receiver/start/length operands");
        if (llvm_debug_detail_enabled())
            fprintf(stderr, "[llvm slice] phase=operands-ready\n");

        if (llvm_debug_detail_enabled())
            fprintf(stderr, "[llvm slice] phase=receiver-type kind=%d\n",
                (int)LLVMGetTypeKind(LLVMTypeOf(receiver)));
        if (LLVMGetTypeKind(LLVMTypeOf(receiver)) == LLVMStructTypeKind)
            field_count = LLVMCountStructElementTypes(LLVMTypeOf(receiver));
        if (llvm_debug_detail_enabled())
            fprintf(stderr, "[llvm slice] phase=field-count count=%u\n", field_count);
        data_ptr = llvm_array_data_ptr(ctx, receiver);
        if (llvm_debug_detail_enabled())
            fprintf(stderr, "[llvm slice] phase=data-ptr\n");
        if (data_ptr == NULL)
            return llvm_domain_slice_error(node, ctx,
                "LLVM Slice() receiver did not expose array data storage");
        receiver_length = llvm_array_length_i64(ctx, receiver);
        if (receiver_length == NULL)
            return llvm_domain_slice_error(node, ctx,
                "LLVM Slice() receiver did not expose collection length");
        if (LLVMGetTypeKind(LLVMTypeOf(data_ptr)) != LLVMPointerTypeKind) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Slice() receiver requires concrete Array<T>/Slice<T> storage");
            return NULL;
        }
        elem_type = llvm_stmt_resolve_array_elem_type(ctx, obj_node, data_ptr);
        if (elem_type == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Slice() receiver requires concrete element type metadata");
            return NULL;
        }
        suffix = llvm_type_to_suffix(ctx, elem_type);
        if (llvm_debug_detail_enabled())
            fprintf(stderr, "[llvm slice] phase=elem ptr-kind=%d elem-kind=%d suffix=%s\n",
                (int)LLVMGetTypeKind(LLVMTypeOf(data_ptr)),
                elem_type != NULL ? (int)LLVMGetTypeKind(elem_type) : -1,
                suffix != NULL ? suffix : "(null)");
        if (suffix == NULL || strcmp(suffix, "Unknown") == 0) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Slice() receiver requires registered Slice<T> element metadata");
            return NULL;
        }
        slice_type = llvm_slice_struct_type(ctx, suffix);
        start64 = (LLVMTypeOf(start) == ctx->type_i64)
            ? start
            : LLVMBuildSExt(ctx->builder, start, ctx->type_i64, llvm_tmp_name(ctx));
        len64 = (LLVMTypeOf(len) == ctx->type_i64)
            ? len
            : LLVMBuildSExt(ctx->builder, len, ctx->type_i64, llvm_tmp_name(ctx));

        if (!llvm_emit_slice_bounds_check(ctx, node, data_ptr, start64,
                len64, receiver_length)) {
            return NULL;
        }

        offset_ptr = LLVMBuildGEP2(ctx->builder, elem_type, data_ptr, &start64, 1,
            llvm_tmp_name(ctx));
        len_is_zero = LLVMBuildICmp(ctx->builder, LLVMIntEQ, len64,
            LLVMConstInt(ctx->type_i64, 0, 0), llvm_tmp_name(ctx));
        slice_data_ptr = LLVMBuildSelect(ctx->builder, len_is_zero,
            LLVMConstNull(LLVMTypeOf(data_ptr)), offset_ptr,
            llvm_tmp_name(ctx));
        if (llvm_debug_detail_enabled())
            fprintf(stderr, "[llvm slice] phase=manual-emit\n");
        result = LLVMGetUndef(slice_type);
        result = LLVMBuildInsertValue(ctx->builder, result, slice_data_ptr, 0, llvm_tmp_name(ctx));
        result = LLVMBuildInsertValue(ctx->builder, result, len64, 1, llvm_tmp_name(ctx));
        return result;
    }

    return NULL;
}

#endif
