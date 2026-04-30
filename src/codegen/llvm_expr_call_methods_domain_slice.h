#include "llvm_expr_call_methods_world_effect_sync.h"

#include "llvm_expr_call_methods_vtable_dispatch.h"

static LLVMValueRef
llvm_emit_member_call_slot_method(ASTNode *node, LLVMGenCtx *ctx,
                                  ASTNode *obj_node,
                                  const char *method_name)
{
    if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
        && method_name != NULL
        && (strcmp(method_name, "Write") == 0
            || strcmp(method_name, "Read") == 0
            || strcmp(method_name, "Release") == 0)) {
        const char *slot_name = obj_node->data.identifier.name;
        const char *inner = llvm_lookup_slot_inner(ctx, slot_name);
        bool is_secure = llvm_lookup_slot_is_secure(ctx, slot_name);
        LLVMVarEntry *slot_var = inner != NULL ? llvm_scope_lookup(ctx, slot_name) : NULL;
        if (inner != NULL && slot_var != NULL) {
            if (strcmp(method_name, "Write") == 0 && node->data.call.arg_count >= 1) {
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
                if (val == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, slot_name);
                    if (token_var == NULL)
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        snprintf(fn_name, sizeof(fn_name), "pgy_secure_write_%s", inner);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = {
                                llvm_slot_runtime_arg(ctx, slot_var),
                                val,
                                token_var->alloca
                            };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
                        } else {
                            llvm_direct_secure_slot_write(ctx, slot_var, val);
                        }
                    }
                } else {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", inner);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = {
                            llvm_slot_runtime_arg(ctx, slot_var),
                            val
                        };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    } else {
                        llvm_direct_slot_write(ctx, slot_var, val);
                    }
                }
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }

            if (strcmp(method_name, "Read") == 0) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, slot_name);
                    if (token_var == NULL)
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        snprintf(fn_name, sizeof(fn_name), "pgy_secure_read_%s", inner);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = {
                                llvm_slot_runtime_arg(ctx, slot_var),
                                token_var->alloca
                            };
                            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                args, 2, llvm_tmp_name(ctx));
                        }
                        return llvm_direct_secure_slot_read(ctx, slot_var, inner);
                    }
                }

                {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    snprintf(fn_name, sizeof(fn_name), "pgy_read_%s", inner);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = {
                            llvm_slot_runtime_arg(ctx, slot_var)
                        };
                        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, 1, llvm_tmp_name(ctx));
                    }
                    return llvm_direct_slot_read(ctx, slot_var, inner);
                }
            }

            if (strcmp(method_name, "Release") == 0) {
                if (is_secure) {
                    LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, slot_name);
                    if (token_var == NULL)
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    {
                        char fn_name[64];
                        LLVMFuncEntry *fn;
                        snprintf(fn_name, sizeof(fn_name), "pgy_secure_release_%s", inner);
                        fn = llvm_lookup_function(ctx, fn_name);
                        if (fn != NULL) {
                            LLVMValueRef args[] = {
                                llvm_slot_runtime_arg(ctx, slot_var),
                                token_var->alloca
                            };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                        } else {
                            llvm_direct_secure_slot_release(ctx, slot_var);
                        }
                    }
                } else {
                    char fn_name[64];
                    LLVMFuncEntry *fn;
                    snprintf(fn_name, sizeof(fn_name), "pgy_release_%s", inner);
                    fn = llvm_lookup_function(ctx, fn_name);
                    if (fn != NULL) {
                        LLVMValueRef args[] = {
                            llvm_slot_runtime_arg(ctx, slot_var)
                        };
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
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

static LLVMValueRef
llvm_emit_member_call_slice(ASTNode *node, LLVMGenCtx *ctx,
                            ASTNode *obj_node,
                            const char *method_name)
{
    if (obj_node != NULL && method_name != NULL
        && strcmp(method_name, "Slice") == 0
        && node->data.call.arg_count == 2) {
        LLVMValueRef receiver;
        LLVMValueRef start;
        LLVMValueRef len;
        const char *suffix = NULL;
        LLVMTypeRef elem_type = NULL;
        LLVMTypeRef slice_type = NULL;
        LLVMValueRef data_ptr;
        LLVMValueRef start64;
        LLVMValueRef len64;
        LLVMValueRef offset_ptr;
        LLVMValueRef result;
        unsigned field_count = 0;

        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=begin\n");
        receiver = llvm_emit_expression(obj_node, ctx);
        start = llvm_emit_expression(node->data.call.arguments[0], ctx);
        len = llvm_emit_expression(node->data.call.arguments[1], ctx);

        if (receiver == NULL || start == NULL || len == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=operands-ready\n");

        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=receiver-type kind=%d\n",
                (int)LLVMGetTypeKind(LLVMTypeOf(receiver)));
        if (LLVMGetTypeKind(LLVMTypeOf(receiver)) == LLVMStructTypeKind)
            field_count = LLVMCountStructElementTypes(LLVMTypeOf(receiver));
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=field-count count=%u\n", field_count);
        data_ptr = llvm_array_data_ptr(ctx, receiver);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=data-ptr\n");
        if (LLVMGetTypeKind(LLVMTypeOf(data_ptr)) != LLVMPointerTypeKind)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        elem_type = llvm_stmt_resolve_array_elem_type(ctx, obj_node, data_ptr);
        if (elem_type == NULL)
            elem_type = ctx->type_i32;
        suffix = llvm_type_to_suffix(ctx, elem_type);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=elem ptr-kind=%d elem-kind=%d suffix=%s\n",
                (int)LLVMGetTypeKind(LLVMTypeOf(data_ptr)),
                elem_type != NULL ? (int)LLVMGetTypeKind(elem_type) : -1,
                suffix != NULL ? suffix : "(null)");
        if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
            slice_type = llvm_slice_struct_type(ctx, suffix);
        else
            slice_type = LLVMStructTypeInContext(ctx->context,
                (LLVMTypeRef[]){ LLVMPointerType(elem_type, 0), ctx->type_i64 }, 2, 0);
        start64 = (LLVMTypeOf(start) == ctx->type_i64)
            ? start
            : LLVMBuildSExt(ctx->builder, start, ctx->type_i64, llvm_tmp_name(ctx));
        len64 = (LLVMTypeOf(len) == ctx->type_i64)
            ? len
            : LLVMBuildSExt(ctx->builder, len, ctx->type_i64, llvm_tmp_name(ctx));

        offset_ptr = LLVMBuildGEP2(ctx->builder, elem_type, data_ptr, &start64, 1,
            llvm_tmp_name(ctx));
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
            fprintf(stderr, "[llvm slice] phase=manual-emit\n");
        result = LLVMGetUndef(slice_type);
        result = LLVMBuildInsertValue(ctx->builder, result, offset_ptr, 0, llvm_tmp_name(ctx));
        result = LLVMBuildInsertValue(ctx->builder, result, len64, 1, llvm_tmp_name(ctx));
        return result;
    }

    return NULL;
}
