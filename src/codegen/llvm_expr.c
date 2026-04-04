/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — expression emission
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include "llvm_expr_helpers.inc"
static LLVMValueRef llvm_emit_member_lvalue_ptr(ASTNode *node, LLVMGenCtx *ctx,
                                                LLVMTypeRef *out_field_type);
static LLVMValueRef
llvm_coerce_value_to_string(LLVMValueRef value, LLVMGenCtx *ctx)
{
    LLVMTypeRef value_type;
    LLVMFuncEntry *fn;
    LLVMValueRef args[1];

    if (value == NULL || ctx == NULL)
        return NULL;

    value_type = LLVMTypeOf(value);
    if (value_type == ctx->type_i8ptr)
        return value;

    if (LLVMGetTypeKind(value_type) == LLVMIntegerTypeKind) {
        unsigned width = LLVMGetIntTypeWidth(value_type);
        if (width == 1) {
            value = LLVMBuildZExt(ctx->builder, value, ctx->type_i32,
                llvm_tmp_name(ctx));
        } else if (width < 32) {
            value = LLVMBuildSExt(ctx->builder, value, ctx->type_i32,
                llvm_tmp_name(ctx));
        } else if (width > 32) {
            value = LLVMBuildTrunc(ctx->builder, value, ctx->type_i32,
                llvm_tmp_name(ctx));
        }

        fn = llvm_lookup_function(ctx, "pgy_int_to_string");
        if (fn == NULL)
            return NULL;
        args[0] = value;
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
            llvm_tmp_name(ctx));
    }

    return NULL;
}

static LLVMValueRef
llvm_emit_binary(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef left  = llvm_emit_expression(node->data.binary.left, ctx);
    LLVMValueRef right = llvm_emit_expression(node->data.binary.right, ctx);
    if (left == NULL || right == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMTypeRef left_type  = LLVMTypeOf(left);
    LLVMTypeRef right_type = LLVMTypeOf(right);
    {
        const char *suffix = llvm_operator_overload_suffix(
            node->data.binary.op.type);
        const char *type_name = llvm_expr_custom_type_name(
            node->data.binary.left, ctx);

        if (type_name == NULL && left_type == right_type) {
            const char *primitive_suffix = llvm_type_to_suffix(ctx, left_type);
            if (primitive_suffix != NULL
                && strcmp(primitive_suffix, "Unknown") != 0) {
                type_name = primitive_suffix;
            }
        }

        if (type_name != NULL && suffix != NULL) {
            char fn_name[256];
            snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, type_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn != NULL) {
                LLVMValueRef args[] = { left, right };
                if (fn->ret_type == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                }
                return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                      args, 2, llvm_tmp_name(ctx));
            }
        }
    }

    /* String + X / X + String → StringConcat after scalar coercion. */
    if (node->data.binary.op.type == TOKEN_PLUS
        && (left_type == ctx->type_i8ptr || right_type == ctx->type_i8ptr)) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringConcat");
        if (left_type != ctx->type_i8ptr)
            left = llvm_coerce_value_to_string(left, ctx);
        if (right_type != ctx->type_i8ptr)
            right = llvm_coerce_value_to_string(right, ctx);
        if (fn != NULL && left != NULL && right != NULL
            && LLVMTypeOf(left) == ctx->type_i8ptr
            && LLVMTypeOf(right) == ctx->type_i8ptr) {
            LLVMValueRef args[] = { left, right };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 2, llvm_tmp_name(ctx));
        }
    }

    /* Promote: if one side is double, convert the other */
    bool is_float = (left_type == ctx->type_f64 || left_type == ctx->type_f32
                  || right_type == ctx->type_f64 || right_type == ctx->type_f32);

    if (is_float) {
        if (left_type == ctx->type_i32)
            left = LLVMBuildSIToFP(ctx->builder, left, ctx->type_f64,
                                    llvm_tmp_name(ctx));
        if (right_type == ctx->type_i32)
            right = LLVMBuildSIToFP(ctx->builder, right, ctx->type_f64,
                                     llvm_tmp_name(ctx));
    }

    const char *tmp = llvm_tmp_name(ctx);

    switch (node->data.binary.op.type) {
    case TOKEN_PLUS:
        return is_float
            ? LLVMBuildFAdd(ctx->builder, left, right, tmp)
            : LLVMBuildAdd(ctx->builder, left, right, tmp);

    case TOKEN_MINUS:
        return is_float
            ? LLVMBuildFSub(ctx->builder, left, right, tmp)
            : LLVMBuildSub(ctx->builder, left, right, tmp);

    case TOKEN_STAR:
        return is_float
            ? LLVMBuildFMul(ctx->builder, left, right, tmp)
            : LLVMBuildMul(ctx->builder, left, right, tmp);

    case TOKEN_SLASH:
        return is_float
            ? LLVMBuildFDiv(ctx->builder, left, right, tmp)
            : LLVMBuildSDiv(ctx->builder, left, right, tmp);

    case TOKEN_PERCENT:
        return LLVMBuildSRem(ctx->builder, left, right, tmp);

    case TOKEN_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntEQ, left, right, tmp);

    case TOKEN_NOT_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealONE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntNE, left, right, tmp);

    case TOKEN_LESS:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOLT, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSLT, left, right, tmp);

    case TOKEN_LESS_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOLE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSLE, left, right, tmp);

    case TOKEN_GREATER:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOGT, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSGT, left, right, tmp);

    case TOKEN_GREATER_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOGE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSGE, left, right, tmp);

    case TOKEN_AND:
        return LLVMBuildAnd(ctx->builder, left, right, tmp);

    case TOKEN_OR:
        return LLVMBuildOr(ctx->builder, left, right, tmp);

    default:
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }
}

static LLVMValueRef
llvm_emit_unary(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.unary.op.type == TOKEN_QUESTION) {
        LLVMValueRef result = llvm_emit_expression(node->data.unary.operand, ctx);
        LLVMTypeRef result_ty = LLVMTypeOf(result);
        unsigned field_count = LLVMCountStructElementTypes(result_ty);

        if (result == NULL || LLVMGetTypeKind(result_ty) != LLVMStructTypeKind
            || field_count < 2 || ctx->current_function == NULL) {
            return LLVMConstInt(ctx->type_i32, 0, 0);
        }

        LLVMTypeRef fields[8];
        LLVMGetStructElementTypes(result_ty, fields);

        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, result, 0, llvm_tmp_name(ctx));
        LLVMValueRef is_ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));

        LLVMValueRef ok_alloca = llvm_create_entry_alloca(ctx, fields[1], llvm_tmp_name(ctx));
        LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.ok");
        LLVMBasicBlockRef err_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.err");
        LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.cont");

        LLVMBuildCondBr(ctx->builder, is_ok, ok_bb, err_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
        {
            LLVMValueRef ok_value = LLVMBuildExtractValue(ctx->builder, result, 1,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, ok_value, ok_alloca);
            LLVMBuildBr(ctx->builder, cont_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, err_bb);
        if (ctx->current_ret_type == result_ty) {
            LLVMBuildRet(ctx->builder, result);
        } else {
            LLVMBuildUnreachable(ctx->builder);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        return LLVMBuildLoad2(ctx->builder, fields[1], ok_alloca, llvm_tmp_name(ctx));
    }

    LLVMValueRef operand = llvm_emit_expression(node->data.unary.operand, ctx);
    if (operand == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    const char *tmp = llvm_tmp_name(ctx);

    switch (node->data.unary.op.type) {
    case TOKEN_MINUS:
        if (LLVMTypeOf(operand) == ctx->type_f64 ||
            LLVMTypeOf(operand) == ctx->type_f32)
            return LLVMBuildFNeg(ctx->builder, operand, tmp);
        return LLVMBuildNeg(ctx->builder, operand, tmp);

    case TOKEN_NOT:
        return LLVMBuildNot(ctx->builder, operand, tmp);

    default:
        return operand;
    }
}

static LLVMValueRef
llvm_emit_call(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.call.callee == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Method call: obj.method(args) */
    if (node->data.call.callee->type == AST_MEMBER_ACCESS) {
        ASTNode *obj_node = node->data.call.callee->data.member.object;
        const char *method_name = node->data.call.callee->data.member.name;

        /* Dynamic vtable dispatch: party.slot.method()
         * AST: call(member(member(party_var, slot_name), method_name)) */
        if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
            && method_name != NULL) {
            ASTNode *party_node = obj_node->data.member.object;
            const char *slot_name = obj_node->data.member.name;

            if (party_node != NULL && party_node->type == AST_IDENTIFIER
                && slot_name != NULL) {
                const char *party_var = party_node->data.identifier.name;
                const char *party_class = llvm_lookup_var_class(ctx, party_var);
                LLVMClassTypeEntry *cls = party_class
                    ? llvm_lookup_class(ctx, party_class) : NULL;

                if (cls != NULL) {
                    /* Find vtable pointer field: slot_name + "_vtable" */
                    char vt_field[256];
                    snprintf(vt_field, sizeof(vt_field), "%s_vtable", slot_name);
                    int vt_idx = -1;
                    for (int fi = 0; fi < cls->field_count; fi++) {
                        if (strcmp(cls->fields[fi].field_name, vt_field) == 0) {
                            vt_idx = cls->fields[fi].index;
                            break;
                        }
                    }

                    if (vt_idx >= 0) {
                        /* Find which ability this slot requires, then look up
                         * the method index in that ability's vtable struct */
                        int method_idx = -1;
                        LLVMClassTypeEntry *vt_cls = NULL;

                        /* Search for *_vtable class that has this method */
                        for (int ci = 0; ci < ctx->class_type_count; ci++) {
                            const char *cn = ctx->class_types[ci].class_name;
                            if (cn != NULL && strstr(cn, "_vtable") != NULL) {
                                for (int fi = 0; fi < ctx->class_types[ci].field_count; fi++) {
                                    if (strcmp(ctx->class_types[ci].fields[fi].field_name,
                                              method_name) == 0) {
                                        vt_cls = &ctx->class_types[ci];
                                        method_idx = ctx->class_types[ci].fields[fi].index;
                                        break;
                                    }
                                }
                                if (method_idx >= 0) break;
                            }
                        }

                        if (vt_cls != NULL && method_idx >= 0) {
                            LLVMVarEntry *pvar = llvm_scope_lookup(ctx, party_var);
                            if (pvar != NULL) {
                                /* Load vtable pointer from party struct */
                                LLVMValueRef vt_ptr_field = LLVMBuildStructGEP2(
                                    ctx->builder, cls->struct_type, pvar->alloca,
                                    (unsigned)vt_idx, llvm_tmp_name(ctx));
                                LLVMValueRef vt_raw = LLVMBuildLoad2(ctx->builder,
                                    ctx->type_i8ptr, vt_ptr_field, llvm_tmp_name(ctx));

                                /* Cast to vtable struct pointer */
                                LLVMTypeRef vt_ptr_ty = LLVMPointerType(
                                    vt_cls->struct_type, 0);
                                LLVMValueRef vt_typed = LLVMBuildBitCast(
                                    ctx->builder, vt_raw, vt_ptr_ty,
                                    llvm_tmp_name(ctx));

                                /* GEP to method function pointer */
                                LLVMValueRef fn_ptr_field = LLVMBuildStructGEP2(
                                    ctx->builder, vt_cls->struct_type, vt_typed,
                                    (unsigned)method_idx, llvm_tmp_name(ctx));

                                /* Build the function type: ret(self_ptr, user_args...) */
                                size_t argc = node->data.call.arg_count;
                                LLVMTypeRef *fn_params = calloc(argc + 1,
                                    sizeof(LLVMTypeRef));
                                fn_params[0] = ctx->type_i8ptr; /* self */
                                for (size_t ai = 0; ai < argc; ai++)
                                    fn_params[ai + 1] = ctx->type_i32; /* TODO: resolve arg types */

                                /* Determine return type from callee name lookup:
                                 * search registered role methods for this name */
                                LLVMTypeRef ret_type = ctx->type_i32; /* default */
                                /* Look for any Role_MethodName function */
                                for (int ri = 0; ri < ctx->func_count; ri++) {
                                    const char *fn_name = LLVMGetValueName(ctx->functions[ri].fn);
                                    if (fn_name != NULL
                                        && strlen(fn_name) > strlen(method_name) + 1) {
                                        const char *suffix = fn_name + strlen(fn_name) - strlen(method_name);
                                        if (suffix > fn_name && *(suffix - 1) == '_'
                                            && strcmp(suffix, method_name) == 0) {
                                            ret_type = ctx->functions[ri].ret_type;
                                            break;
                                        }
                                    }
                                }

                                LLVMTypeRef fn_type = LLVMFunctionType(ret_type,
                                    fn_params, (unsigned)(argc + 1), 0);
                                LLVMTypeRef fn_ptr_ty = LLVMPointerType(fn_type, 0);

                                /* Load the function pointer from vtable */
                                LLVMValueRef fn_ptr = LLVMBuildLoad2(ctx->builder,
                                    fn_ptr_ty, fn_ptr_field, llvm_tmp_name(ctx));

                                /* Build args: self (party_alloca as i8*) + user args */
                                LLVMValueRef *args = calloc(argc + 1,
                                    sizeof(LLVMValueRef));
                                args[0] = LLVMBuildBitCast(ctx->builder,
                                    pvar->alloca, ctx->type_i8ptr,
                                    llvm_tmp_name(ctx));
                                for (size_t ai = 0; ai < argc; ai++)
                                    args[ai + 1] = llvm_emit_expression(
                                        node->data.call.arguments[ai], ctx);

                                LLVMValueRef result;
                                if (ret_type == ctx->type_void) {
                                    LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                                        args, (unsigned)(argc + 1), "");
                                    result = LLVMConstInt(ctx->type_i32, 0, 0);
                                } else {
                                    result = LLVMBuildCall2(ctx->builder, fn_type,
                                        fn_ptr, args, (unsigned)(argc + 1),
                                        llvm_tmp_name(ctx));
                                }
                                free(fn_params);
                                free(args);
                                return result;
                            }
                        }
                    }
                }
            }
        }

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
                                LLVMValueRef args[] = { slot_var->alloca, val, token_var->alloca };
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
                            LLVMValueRef args[] = { slot_var->alloca, val };
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
                                LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
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
                            LLVMValueRef args[] = { slot_var->alloca };
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
                                LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
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
                            LLVMValueRef args[] = { slot_var->alloca };
                            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
                        } else {
                            llvm_direct_slot_release(ctx, slot_var);
                        }
                    }
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                }
            }
        }

        if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
            && method_name != NULL) {
            if (llvm_is_upper_ident(obj_node)) {
                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s_%s",
                         obj_node->data.identifier.name, method_name);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
                if (fn != NULL) {
                    return llvm_emit_function_call_args(ctx, fn,
                        node->data.call.arguments, node->data.call.arg_count);
                }
            }

            const char *var_name = obj_node->data.identifier.name;
            const char *class_name = llvm_lookup_var_class(ctx, var_name);
            LLVMVarEntry *var = llvm_scope_lookup(ctx, var_name);
            LLVMClassTypeEntry *parent_cls = NULL;
            int field_idx = -1;

            if (class_name == NULL && ctx->current_class_name != NULL) {
                class_name = llvm_current_field_class_name(ctx, var_name);
                if (class_name != NULL) {
                    parent_cls = llvm_lookup_class(ctx, ctx->current_class_name);
                    if (parent_cls != NULL)
                        field_idx = llvm_class_field_index(parent_cls, var_name);
                }
            }

            if (class_name != NULL && (var != NULL || (parent_cls != NULL && field_idx >= 0))) {
                LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, class_name);
                if (cls != NULL) {
                    char full_name[256];
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                             class_name, method_name);
                    LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
                    ASTNode *method_decl = llvm_find_nominal_host_method_decl(ctx,
                        class_name, method_name);
                    if (fn != NULL) {
                        /* subject methods receive a self pointer; class methods a self value */
                        size_t argc = node->data.call.arg_count;
                        LLVMValueRef *args = calloc(argc + 1,
                                                     sizeof(LLVMValueRef));
                        if (llvm_nominal_uses_pointer_self(ctx, class_name)) {
                            if (var != NULL && strcmp(var_name, "self") == 0) {
                                args[0] = LLVMBuildLoad2(ctx->builder,
                                    var->type, var->alloca, llvm_tmp_name(ctx));
                            } else if (var != NULL) {
                                if (var->type == LLVMPointerType(cls->struct_type, 0))
                                    args[0] = LLVMBuildLoad2(ctx->builder,
                                        var->type, var->alloca, llvm_tmp_name(ctx));
                                else
                                    args[0] = var->alloca;
                            } else {
                                LLVMValueRef base_ptr =
                                    llvm_current_self_base_ptr(ctx, parent_cls);
                                LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                                    ctx->builder, parent_cls->struct_type, base_ptr,
                                    (unsigned)field_idx, llvm_tmp_name(ctx));
                                args[0] = field_ptr;
                            }
                        } else {
                            if (var != NULL) {
                                args[0] = LLVMBuildLoad2(ctx->builder,
                                    var->type, var->alloca, llvm_tmp_name(ctx));
                            } else {
                                LLVMValueRef base_ptr =
                                    llvm_current_self_base_ptr(ctx, parent_cls);
                                LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                                    ctx->builder, parent_cls->struct_type, base_ptr,
                                    (unsigned)field_idx, llvm_tmp_name(ctx));
                                args[0] = LLVMBuildLoad2(ctx->builder,
                                    cls->struct_type, field_ptr, llvm_tmp_name(ctx));
                            }
                        }
                        for (size_t i = 0; i < argc; i++) {
                            LLVMValueRef arg_val = llvm_emit_expression(
                                node->data.call.arguments[i], ctx);
                            if (method_decl != NULL) {
                                size_t logical_idx = 0;
                                for (size_t pk = 0;
                                     pk < method_decl->data.func_decl.param_count; pk++) {
                                    FuncParam *p = method_decl->data.func_decl.params[pk];
                                    const char *ptn = NULL;
                                    LLVMClassTypeEntry *param_cls = NULL;
                                    if (p->type == NULL
                                        && strcmp(p->name, "self") == 0) {
                                        continue;
                                    }
                                    if (logical_idx == i) {
                                        if (p->type != NULL && p->type->type == AST_TYPE)
                                            ptn = p->type->data.type.name;
                                        param_cls = ptn != NULL
                                            ? llvm_lookup_class(ctx, ptn) : NULL;
                                        if (param_cls != NULL && param_cls->is_pointer_self_host
                                            && node->data.call.arguments[i] != NULL
                                            && node->data.call.arguments[i]->type == AST_IDENTIFIER) {
                                            const char *arg_name =
                                                node->data.call.arguments[i]->data.identifier.name;
                                            LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                                            if (arg_var != NULL) {
                                                if (arg_var->type == LLVMPointerType(param_cls->struct_type, 0))
                                                    arg_val = LLVMBuildLoad2(ctx->builder,
                                                        arg_var->type, arg_var->alloca, llvm_tmp_name(ctx));
                                                else
                                                    arg_val = arg_var->alloca;
                                            }
                                        }
                                        break;
                                    }
                                    logical_idx++;
                                }
                            }
                            args[i + 1] = arg_val;
                        }

                        LLVMValueRef result;
                        if (fn->ret_type == ctx->type_void) {
                            LLVMBuildCall2(ctx->builder, fn->fn_type,
                                fn->fn, args, (unsigned)(argc + 1), "");
                            result = LLVMConstInt(ctx->type_i32, 0, 0);
                        } else {
                            result = LLVMBuildCall2(ctx->builder,
                                fn->fn_type, fn->fn, args,
                                (unsigned)(argc + 1), llvm_tmp_name(ctx));
                        }
                        free(args);
                        return result;
                    }
                }
            }
        }

        if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
            && method_name != NULL) {
            const char *class_name = llvm_expr_custom_type_name(obj_node, ctx);
            LLVMClassTypeEntry *host_cls = class_name != NULL
                ? llvm_lookup_class(ctx, class_name) : NULL;

            if (host_cls != NULL) {
                char full_name[256];
                LLVMFuncEntry *fn;
                ASTNode *method_decl;
                size_t argc = node->data.call.arg_count;
                LLVMValueRef *args;
                LLVMValueRef self_ptr;

                snprintf(full_name, sizeof(full_name), "%s_%s",
                    class_name, method_name);
                fn = llvm_lookup_function(ctx, full_name);
                method_decl = llvm_find_nominal_host_method_decl(ctx,
                    class_name, method_name);
                if (fn != NULL) {
                    args = calloc(argc + 1, sizeof(LLVMValueRef));
                    self_ptr = llvm_emit_member_lvalue_ptr(obj_node, ctx, NULL);
                    if (self_ptr == NULL) {
                        free(args);
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    }

                    if (llvm_nominal_uses_pointer_self(ctx, class_name)) {
                        args[0] = self_ptr;
                    } else {
                        args[0] = LLVMBuildLoad2(ctx->builder,
                            host_cls->struct_type, self_ptr, llvm_tmp_name(ctx));
                    }

                    for (size_t i = 0; i < argc; i++) {
                        LLVMValueRef arg_val = llvm_emit_expression(
                            node->data.call.arguments[i], ctx);
                        if (method_decl != NULL) {
                            size_t logical_idx = 0;
                            for (size_t pk = 0;
                                 pk < method_decl->data.func_decl.param_count; pk++) {
                                FuncParam *p = method_decl->data.func_decl.params[pk];
                                const char *ptn = NULL;
                                LLVMClassTypeEntry *param_cls = NULL;
                                if (p->type == NULL
                                    && strcmp(p->name, "self") == 0) {
                                    continue;
                                }
                                if (logical_idx == i) {
                                    if (p->type != NULL && p->type->type == AST_TYPE)
                                        ptn = p->type->data.type.name;
                                    param_cls = ptn != NULL
                                        ? llvm_lookup_class(ctx, ptn) : NULL;
                                    if (param_cls != NULL && param_cls->is_pointer_self_host
                                        && node->data.call.arguments[i] != NULL
                                        && node->data.call.arguments[i]->type == AST_IDENTIFIER) {
                                        const char *arg_name =
                                            node->data.call.arguments[i]->data.identifier.name;
                                        LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                                        if (arg_var != NULL) {
                                            if (arg_var->type == LLVMPointerType(param_cls->struct_type, 0))
                                                arg_val = LLVMBuildLoad2(ctx->builder,
                                                    arg_var->type, arg_var->alloca, llvm_tmp_name(ctx));
                                            else
                                                arg_val = arg_var->alloca;
                                        }
                                    }
                                    break;
                                }
                                logical_idx++;
                            }
                        }
                        args[i + 1] = arg_val;
                    }

                    if (fn->ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, (unsigned)(argc + 1), "");
                        free(args);
                        return LLVMConstInt(ctx->type_i32, 0, 0);
                    }

                    {
                        LLVMValueRef result = LLVMBuildCall2(ctx->builder,
                            fn->fn_type, fn->fn, args,
                            (unsigned)(argc + 1), llvm_tmp_name(ctx));
                        free(args);
                        return result;
                    }
                }
            }
        }

        if (obj_node != NULL && obj_node->type == AST_MEMBER_ACCESS
            && obj_node->data.member.object != NULL
            && obj_node->data.member.object->type == AST_IDENTIFIER
            && obj_node->data.member.object->data.identifier.name != NULL
            && strcmp(obj_node->data.member.object->data.identifier.name, "self") == 0
            && obj_node->data.member.name != NULL
            && method_name != NULL) {
            const char *field_name = obj_node->data.member.name;
            const char *class_name = llvm_current_field_class_name(ctx, field_name);
            LLVMClassTypeEntry *parent_cls = llvm_lookup_class(ctx, ctx->current_class_name);
            LLVMClassTypeEntry *host_cls = class_name != NULL
                ? llvm_lookup_class(ctx, class_name) : NULL;
            int field_idx = parent_cls != NULL
                ? llvm_class_field_index(parent_cls, field_name) : -1;

            if (parent_cls != NULL && host_cls != NULL && field_idx >= 0) {
                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s_%s",
                         class_name, method_name);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
                if (fn != NULL) {
                    size_t argc = node->data.call.arg_count;
                    LLVMValueRef *args = calloc(argc + 1, sizeof(LLVMValueRef));
                    LLVMValueRef base_ptr = llvm_current_self_base_ptr(ctx, parent_cls);
                    LLVMValueRef field_ptr = LLVMBuildStructGEP2(ctx->builder,
                        parent_cls->struct_type, base_ptr, (unsigned)field_idx,
                        llvm_tmp_name(ctx));
                    ASTNode *method_decl = llvm_find_nominal_host_method_decl(ctx,
                        class_name, method_name);

                    if (llvm_nominal_uses_pointer_self(ctx, class_name))
                        args[0] = field_ptr;
                    else
                        args[0] = LLVMBuildLoad2(ctx->builder,
                            host_cls->struct_type, field_ptr, llvm_tmp_name(ctx));

                    for (size_t i = 0; i < argc; i++) {
                        LLVMValueRef arg_val = llvm_emit_expression(
                            node->data.call.arguments[i], ctx);
                        if (method_decl != NULL) {
                            size_t logical_idx = 0;
                            for (size_t pk = 0;
                                 pk < method_decl->data.func_decl.param_count; pk++) {
                                FuncParam *p = method_decl->data.func_decl.params[pk];
                                const char *ptn = NULL;
                                LLVMClassTypeEntry *param_cls = NULL;
                                if (p->type == NULL
                                    && strcmp(p->name, "self") == 0) {
                                    continue;
                                }
                                if (logical_idx == i) {
                                    if (p->type != NULL && p->type->type == AST_TYPE)
                                        ptn = p->type->data.type.name;
                                    param_cls = ptn != NULL
                                        ? llvm_lookup_class(ctx, ptn) : NULL;
                                    if (param_cls != NULL && param_cls->is_pointer_self_host
                                        && node->data.call.arguments[i] != NULL
                                        && node->data.call.arguments[i]->type == AST_IDENTIFIER) {
                                        const char *arg_name =
                                            node->data.call.arguments[i]->data.identifier.name;
                                        LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                                        if (arg_var != NULL) {
                                            if (arg_var->type == LLVMPointerType(param_cls->struct_type, 0))
                                                arg_val = LLVMBuildLoad2(ctx->builder,
                                                    arg_var->type, arg_var->alloca, llvm_tmp_name(ctx));
                                            else
                                                arg_val = arg_var->alloca;
                                        }
                                    }
                                    break;
                                }
                                logical_idx++;
                            }
                        }
                        args[i + 1] = arg_val;
                    }

                    LLVMValueRef result;
                    if (fn->ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, (unsigned)(argc + 1), "");
                        result = LLVMConstInt(ctx->type_i32, 0, 0);
                    } else {
                        result = LLVMBuildCall2(ctx->builder,
                            fn->fn_type, fn->fn, args,
                            (unsigned)(argc + 1), llvm_tmp_name(ctx));
                    }
                    free(args);
                    return result;
                }
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Get callee name */
    const char *callee_name = NULL;
    if (node->data.call.callee->type == AST_IDENTIFIER)
        callee_name = node->data.call.callee->data.identifier.name;

    if (callee_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    {
        LLVMEnumVariantEntry *variant = llvm_lookup_enum_variant(ctx, callee_name);
        if (variant != NULL) {
            ASTNode *enum_decl = llvm_find_enum_decl(ctx, variant->enum_name);
            LLVMClassTypeEntry *enum_cls = llvm_lookup_class(ctx, variant->enum_name);
            if (enum_decl != NULL && enum_cls != NULL) {
                size_t variant_index = (size_t)variant->value;
                size_t param_count =
                    (enum_decl->data.enum_decl.variant_param_counts != NULL)
                    ? enum_decl->data.enum_decl.variant_param_counts[variant_index] : 0;
                LLVMValueRef enum_val = LLVMGetUndef(enum_cls->struct_type);
                enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
                    LLVMConstInt(ctx->type_i32,
                        (unsigned long long)variant->value, 0),
                    0, llvm_tmp_name(ctx));

                if (param_count > 0) {
                    int field_idx = llvm_class_field_index(enum_cls, callee_name);
                    if (field_idx > 0) {
                        LLVMTypeRef payload_ty = enum_cls->fields[field_idx].field_type;
                        LLVMValueRef payload = LLVMGetUndef(payload_ty);
                        LLVMClassTypeEntry *payload_cls =
                            llvm_lookup_class_by_type(ctx, payload_ty);

                        for (size_t i = 0; i < param_count
                             && i < node->data.call.arg_count; i++) {
                            LLVMValueRef arg = llvm_emit_expression(
                                node->data.call.arguments[i], ctx);
                            if (arg == NULL)
                                continue;
                            if (payload_cls != NULL
                                && i < (size_t)payload_cls->field_count
                                && payload_cls->fields[i].field_type != LLVMTypeOf(arg)) {
                                LLVMTypeRef target_ty = payload_cls->fields[i].field_type;
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
                            payload = LLVMBuildInsertValue(ctx->builder, payload, arg,
                                (unsigned)i, llvm_tmp_name(ctx));
                        }
                        enum_val = LLVMBuildInsertValue(ctx->builder, enum_val,
                            payload, (unsigned)field_idx, llvm_tmp_name(ctx));
                    }
                }
                return enum_val;
            }
        }
    }

    {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee_name);
        if (cls != NULL) {
            LLVMValueRef object = LLVMConstNull(cls->struct_type);
            for (size_t i = 0; i < node->data.call.arg_count
                 && i < (size_t)cls->field_count; i++) {
                LLVMValueRef arg = llvm_emit_expression(
                    node->data.call.arguments[i], ctx);
                if (arg == NULL)
                    continue;
                object = LLVMBuildInsertValue(ctx->builder, object, arg,
                    (unsigned)cls->fields[i].index, llvm_tmp_name(ctx));
            }
            {
                ASTNode *relation_decl = llvm_find_named_domain_decl(ctx, AST_RELATION_DECL,
                    callee_name);
                ASTNode *effect_decl = llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL,
                    callee_name);
                ASTNode *zone_decl = llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
                    callee_name);
                ASTNode *world_decl = llvm_find_named_domain_decl(ctx, AST_WORLD_DECL,
                    callee_name);
                ASTNode **shared_fields = NULL;
                size_t shared_count = 0;
                if (relation_decl != NULL) {
                    shared_fields = relation_decl->data.relation_decl.shared_fields;
                    shared_count = relation_decl->data.relation_decl.shared_count;
                } else if (effect_decl != NULL) {
                    shared_fields = effect_decl->data.effect_decl.shared_fields;
                    shared_count = effect_decl->data.effect_decl.shared_count;
                } else if (zone_decl != NULL) {
                    shared_fields = zone_decl->data.zone_decl.shared_fields;
                    shared_count = zone_decl->data.zone_decl.shared_count;
                } else if (world_decl != NULL) {
                    shared_fields = world_decl->data.world_decl.shared_fields;
                    shared_count = world_decl->data.world_decl.shared_count;
                }
                for (size_t i = 0; i < shared_count; i++) {
                    ASTNode *shared = shared_fields[i];
                    int field_idx;
                    LLVMValueRef init_val;
                    if (shared == NULL || shared->data.party_shared.name == NULL
                        || shared->data.party_shared.initializer == NULL) {
                        continue;
                    }
                    field_idx = llvm_class_field_index(cls, shared->data.party_shared.name);
                    if (field_idx < 0 || (size_t)field_idx < node->data.call.arg_count)
                        continue;
                    init_val = llvm_emit_expression(shared->data.party_shared.initializer, ctx);
                    if (init_val == NULL)
                        continue;
                    object = LLVMBuildInsertValue(ctx->builder, object, init_val,
                        (unsigned)field_idx, llvm_tmp_name(ctx));
                }
                if (world_decl != NULL && world_decl->type == AST_WORLD_DECL) {
                    int derived_idx = llvm_class_field_index(cls, "__world_derived_dirty");
                    if (derived_idx >= 0) {
                        object = LLVMBuildInsertValue(ctx->builder, object,
                            LLVMConstInt(ctx->type_i1, 1, 0),
                            (unsigned)derived_idx, llvm_tmp_name(ctx));
                    }
                    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
                        ASTNode *zone = world_decl->data.world_decl.zones[i];
                        char dirty_field[256];
                        int dirty_idx;
                        const char *slot_name = zone != NULL
                            ? zone->data.world_zone.slot_name
                            : NULL;
                        if (slot_name == NULL)
                            continue;
                        snprintf(dirty_field, sizeof(dirty_field), "__zone_dirty_%s", slot_name);
                        dirty_idx = llvm_class_field_index(cls, dirty_field);
                        if (dirty_idx < 0)
                            continue;
                        object = LLVMBuildInsertValue(ctx->builder, object,
                            LLVMConstInt(ctx->type_i1, 1, 0),
                            (unsigned)dirty_idx, llvm_tmp_name(ctx));
                    }
                }
            }
            return object;
        }
    }

    if ((strcmp(callee_name, "ToDto") == 0 || strcmp(callee_name, "ToObject") == 0)
        && node->data.call.arg_count == 2) {
        return llvm_emit_subject_projection(node, ctx);
    }

    if (strcmp(callee_name, "HasProjection") == 0 && node->data.call.arg_count == 1) {
        ASTNode *decl = llvm_find_named_domain_decl(ctx, AST_RELATION_DECL,
            ctx->current_class_name);
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        const char *slot_name = NULL;
        int field_idx;
        LLVMValueRef base_ptr;
        LLVMValueRef gep;
        if (decl == NULL) {
            decl = llvm_find_named_domain_decl(ctx, AST_EFFECT_DECL,
                ctx->current_class_name);
        }
        if (decl == NULL) {
            decl = llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
                ctx->current_class_name);
        }
        if (decl == NULL || cls == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (node->data.call.arguments[0]->type == AST_IDENTIFIER)
            slot_name = node->data.call.arguments[0]->data.identifier.name;
        else if (node->data.call.arguments[0]->type == AST_STRING)
            slot_name = node->data.call.arguments[0]->data.string.value;
        if (slot_name == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        {
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__projection_ready_%s", slot_name);
            field_idx = llvm_class_field_index(cls, field_name);
        }
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        base_ptr = llvm_current_self_base_ptr(ctx, cls);
        if (base_ptr == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "HasLayer") == 0 && node->data.call.arg_count == 1) {
        ASTNode *zone_decl = llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
            ctx->current_class_name);
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        const char *layer_name = NULL;
        int field_idx;
        LLVMValueRef base_ptr;
        LLVMValueRef gep;
        if (zone_decl == NULL || cls == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (node->data.call.arguments[0]->type == AST_IDENTIFIER)
            layer_name = node->data.call.arguments[0]->data.identifier.name;
        else if (node->data.call.arguments[0]->type == AST_STRING)
            layer_name = node->data.call.arguments[0]->data.string.value;
        if (layer_name == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        {
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__layer_active_%s", layer_name);
            field_idx = llvm_class_field_index(cls, field_name);
        }
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        base_ptr = llvm_current_self_base_ptr(ctx, cls);
        if (base_ptr == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "HasState") == 0 && node->data.call.arg_count >= 1) {
        ASTNode *zone_decl = llvm_find_named_domain_decl(ctx, AST_ZONE_DECL,
            ctx->current_class_name);
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        const char *state_name = NULL;
        ASTNode *state_decl;
        int field_idx;
        LLVMValueRef base_ptr;
        LLVMValueRef gep;
        if (zone_decl == NULL || cls == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (node->data.call.arguments[0]->type == AST_IDENTIFIER)
            state_name = node->data.call.arguments[0]->data.identifier.name;
        else if (node->data.call.arguments[0]->type == AST_STRING)
            state_name = node->data.call.arguments[0]->data.string.value;
        state_decl = llvm_find_zone_state_decl(ctx, zone_decl, state_name);
        if (state_decl == NULL || state_name == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        {
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__state_%s", state_name);
            field_idx = llvm_class_field_index(cls, field_name);
        }
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        base_ptr = llvm_current_self_base_ptr(ctx, cls);
        if (base_ptr == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "HasZone") == 0 && node->data.call.arg_count == 1) {
        ASTNode *world_decl = llvm_find_named_domain_decl(ctx, AST_WORLD_DECL,
            ctx->current_class_name);
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
        const char *name = NULL;
        ASTNode *state_decl = NULL;
        int field_idx = -1;
        LLVMValueRef base_ptr;
        LLVMValueRef gep;
        if (world_decl == NULL || cls == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (node->data.call.arguments[0]->type == AST_IDENTIFIER)
            name = node->data.call.arguments[0]->data.identifier.name;
        else if (node->data.call.arguments[0]->type == AST_STRING)
            name = node->data.call.arguments[0]->data.string.value;
        if (name == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        state_decl = llvm_find_world_state_decl(ctx, world_decl, name);
        if (state_decl != NULL) {
            if (state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                || state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
                LLVMValueRef result = LLVMConstInt(ctx->type_i1,
                    state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL ? 1 : 0, 0);
                base_ptr = llvm_current_self_base_ptr(ctx, cls);
                if (base_ptr == NULL)
                    return LLVMConstInt(ctx->type_i1, 0, 0);
                for (size_t i = 0; i < state_decl->data.world_state.input_count; i++) {
                    const char *input_name = state_decl->data.world_state.input_names[i];
                    int input_idx = -1;
                    char field_name[256];
                    LLVMValueRef input_ptr;
                    LLVMValueRef input_val;
                    if (input_name == NULL)
                        continue;
                    if (llvm_world_has_zone_slot(world_decl, input_name)) {
                        snprintf(field_name, sizeof(field_name), "__zone_active_%s", input_name);
                    } else {
                        snprintf(field_name, sizeof(field_name), "__zone_state_%s", input_name);
                    }
                    input_idx = llvm_class_field_index(cls, field_name);
                    if (input_idx < 0)
                        continue;
                    input_ptr = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
                        (unsigned)input_idx, llvm_tmp_name(ctx));
                    input_val = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                        input_ptr, llvm_tmp_name(ctx));
                    if (state_decl->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL)
                        result = LLVMBuildAnd(ctx->builder, result, input_val, llvm_tmp_name(ctx));
                    else
                        result = LLVMBuildOr(ctx->builder, result, input_val, llvm_tmp_name(ctx));
                }
                return result;
            }
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__zone_state_%s", name);
            field_idx = llvm_class_field_index(cls, field_name);
        } else if (llvm_world_has_zone_slot(world_decl, name)) {
            char field_name[256];
            snprintf(field_name, sizeof(field_name), "__zone_active_%s", name);
            field_idx = llvm_class_field_index(cls, field_name);
        }
        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        base_ptr = llvm_current_self_base_ptr(ctx, cls);
        if (base_ptr == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    }

    if ((strcmp(callee_name, "HasZoneProjection") == 0
         || strcmp(callee_name, "HasZoneLayer") == 0
         || strcmp(callee_name, "HasZoneState") == 0)
        && node->data.call.arg_count == 2) {
        ASTNode *world_decl = llvm_find_named_domain_decl(ctx, AST_WORLD_DECL,
            ctx->current_class_name);
        LLVMClassTypeEntry *world_cls = llvm_lookup_class(ctx, ctx->current_class_name);
        const char *zone_name = NULL;
        const char *detail_name = NULL;
        ASTNode *zone_decl;
        LLVMClassTypeEntry *zone_cls;
        int zone_idx;
        int field_idx = -1;
        LLVMValueRef world_ptr;
        LLVMValueRef zone_ptr;
        LLVMValueRef gep;

        if (world_decl == NULL || world_cls == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        if (node->data.call.arguments[0]->type == AST_IDENTIFIER)
            zone_name = node->data.call.arguments[0]->data.identifier.name;
        else if (node->data.call.arguments[0]->type == AST_STRING)
            zone_name = node->data.call.arguments[0]->data.string.value;
        if (node->data.call.arguments[1]->type == AST_IDENTIFIER)
            detail_name = node->data.call.arguments[1]->data.identifier.name;
        else if (node->data.call.arguments[1]->type == AST_STRING)
            detail_name = node->data.call.arguments[1]->data.string.value;
        if (zone_name == NULL || detail_name == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);

        zone_decl = llvm_resolve_world_zone_decl(ctx, world_decl, zone_name);
        zone_cls = zone_decl != NULL && zone_decl->data.zone_decl.name != NULL
            ? llvm_lookup_class(ctx, zone_decl->data.zone_decl.name)
            : NULL;
        zone_idx = llvm_class_field_index(world_cls, zone_name);
        if (zone_decl == NULL || zone_cls == NULL || zone_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);

        if (strcmp(callee_name, "HasZoneProjection") == 0) {
            ASTNode *slot = llvm_find_zone_domain_slot_decl(zone_decl, detail_name);
            if (slot != NULL && !slot->data.domain_slot.is_subject) {
                char field_name[256];
                snprintf(field_name, sizeof(field_name), "__projection_ready_%s", detail_name);
                field_idx = llvm_class_field_index(zone_cls, field_name);
            }
        } else if (strcmp(callee_name, "HasZoneLayer") == 0) {
            if (llvm_find_zone_layer_slot_decl(zone_decl, detail_name) != NULL) {
                char field_name[256];
                snprintf(field_name, sizeof(field_name), "__layer_active_%s", detail_name);
                field_idx = llvm_class_field_index(zone_cls, field_name);
            }
        } else {
            if (llvm_find_zone_state_decl(ctx, zone_decl, detail_name) != NULL) {
                char field_name[256];
                snprintf(field_name, sizeof(field_name), "__state_%s", detail_name);
                field_idx = llvm_class_field_index(zone_cls, field_name);
            }
        }

        if (field_idx < 0)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        world_ptr = llvm_current_self_base_ptr(ctx, world_cls);
        if (world_ptr == NULL)
            return LLVMConstInt(ctx->type_i1, 0, 0);
        zone_ptr = LLVMBuildStructGEP2(ctx->builder, world_cls->struct_type, world_ptr,
            (unsigned)zone_idx, llvm_tmp_name(ctx));
        gep = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type, zone_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        return LLVMBuildLoad2(ctx->builder, ctx->type_i1, gep, llvm_tmp_name(ctx));
    }

    /* Built-in: Log */
    if (strcmp(callee_name, "Log") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef arg = llvm_emit_expression(
            node->data.call.arguments[0], ctx);
        if (arg == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        /* Select the right log function based on arg type */
        LLVMTypeRef arg_type = LLVMTypeOf(arg);
        const char *log_fn_name = "pgy_log_int";

        if (arg_type == ctx->type_i64)        log_fn_name = "pgy_log_long";
        else if (arg_type == ctx->type_f32)   log_fn_name = "pgy_log_float";
        else if (arg_type == ctx->type_f64)   log_fn_name = "pgy_log_double";
        else if (arg_type == ctx->type_i1)    log_fn_name = "pgy_log_bool";
        else if (arg_type == ctx->type_i8ptr) log_fn_name = "pgy_log_string";

        LLVMFuncEntry *log_fn = llvm_lookup_function(ctx, log_fn_name);
        if (log_fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { arg };
        LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn,
                       args, 1, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: ClaimSlot<T>() — handled mostly in let_decl, but standalone */
    if (strcmp(callee_name, "ClaimSlot") == 0
        || strcmp(callee_name, "ClaimSecureSlot") == 0) {
        if (strcmp(callee_name, "ClaimSecureSlot") == 0) {
            LLVMTypeRef slot_ty = llvm_secure_slot_struct_type(ctx, "Int");
            LLVMTypeRef token_ty = llvm_secure_token_type(ctx, "Int");
            LLVMValueRef slot_tmp = llvm_create_entry_alloca(ctx, slot_ty, llvm_tmp_name(ctx));
            LLVMValueRef token_tmp = llvm_create_entry_alloca(ctx, token_ty, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), slot_tmp);
            LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_tmp);

            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, slot_tmp, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

            LLVMValueRef slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder,
                slot_tmp, ctx->type_i64, llvm_tmp_name(ctx));
            LLVMValueRef token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
                LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
                llvm_tmp_name(ctx));
            LLVMValueRef slot_token_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, slot_tmp, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);
            LLVMValueRef token_id_ptr = LLVMBuildStructGEP2(ctx->builder,
                token_ty, token_tmp, 0, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, token_id, token_id_ptr);
            LLVMValueRef token_write_ptr = LLVMBuildStructGEP2(ctx->builder,
                token_ty, token_tmp, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                token_write_ptr);
            LLVMValueRef token_read_ptr = LLVMBuildStructGEP2(ctx->builder,
                token_ty, token_tmp, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                token_read_ptr);
            return LLVMBuildLoad2(ctx->builder, slot_ty, slot_tmp, llvm_tmp_name(ctx));
        } else {
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name), "pgy_claim_Int");
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                   NULL, 0, llvm_tmp_name(ctx));
        }
    }

    if (strcmp(callee_name, "ClaimDeviceSlot") == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_claim_device_Int");
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                              NULL, 0, llvm_tmp_name(ctx));
    }

    /* Built-in: Write(slot, value) */
    if (strcmp(callee_name, "Write") == 0) {
        if (node->data.call.arg_count < 2)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        /* Resolve slot inner type */
        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_write_%s" : "pgy_write_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL) {
            if (is_secure)
                llvm_direct_secure_slot_write(ctx, slot_var, val);
            else
                llvm_direct_slot_write(ctx, slot_var, val);
            return LLVMConstInt(ctx->type_i32, 0, 0);
        }

        if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            LLVMValueRef args[] = { slot_var->alloca, val, token_var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        } else {
            LLVMValueRef args[] = { slot_var->alloca, val };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: Read(slot) */
    if (strcmp(callee_name, "Read") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_read_%s" : "pgy_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL) {
            if (is_secure)
                return llvm_direct_secure_slot_read(ctx, slot_var, inner);
            return llvm_direct_slot_read(ctx, slot_var, inner);
        }

        if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                   args, 2, llvm_tmp_name(ctx));
        } else {
            LLVMValueRef args[] = { slot_var->alloca };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                   args, 1, llvm_tmp_name(ctx));
        }
    }

    /* Built-in: Release(slot) */
    if (strcmp(callee_name, "Release") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_release_%s" : "pgy_release_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL && is_secure)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        if (fn == NULL) {
            if (is_secure)
                llvm_direct_secure_slot_release(ctx, slot_var);
            else
                llvm_direct_slot_release(ctx, slot_var);
        } else if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            LLVMValueRef args[] = { slot_var->alloca, token_var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        } else {
            LLVMValueRef args[] = { slot_var->alloca };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }

        /* Mark slot as explicitly released */
        if (source_name != NULL) {
            const char *sname = source_name;
            for (int ri = 0; ri < ctx->slot_var_count; ri++) {
                if (strcmp(ctx->slot_vars[ri].var_name, sname) == 0) {
                    ctx->slot_vars[ri].released = true;
                    break;
                }
            }
        }

        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "DeviceWrite") == 0) {
        if (node->data.call.arg_count < 2)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL) inner = "Int";
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_device_write_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (fn == NULL || val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca, val };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "DeviceRead") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL) inner = "Int";
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_device_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                              args, 1, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "ReleaseDeviceSlot") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL) inner = "Int";
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_release_device_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        if (slot_arg->type == AST_IDENTIFIER)
            llvm_mark_device_slot_released(ctx, slot_arg->data.identifier.name);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "SubmitDeviceRead") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL) inner = "Int";
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_submit_device_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                              args, 1, llvm_tmp_name(ctx));
    }

    /* Event invocation: OnHit(42) → OnHit_INVOKE(&OnHit, 42) */
    {
        LLVMEventTypeEntry *evt = llvm_lookup_event(ctx, callee_name);
        if (evt != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", callee_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMValueRef ev_ptr = LLVMGetNamedGlobal(ctx->module, callee_name);
            if (ev_ptr == NULL) {
                LLVMVarEntry *ev = llvm_scope_lookup(ctx, callee_name);
                if (ev != NULL) ev_ptr = ev->alloca;
            }
            if (fn != NULL && ev_ptr != NULL) {
                size_t ac = node->data.call.arg_count;
                LLVMValueRef *args = calloc(ac + 1, sizeof(LLVMValueRef));
                args[0] = ev_ptr;
                for (size_t j = 0; j < ac; j++)
                    args[j + 1] = llvm_emit_expression(
                        node->data.call.arguments[j], ctx);
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, (unsigned)(ac + 1), "");
                free(args);
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
        }
    }

    /* Built-in: Abs(x) → select(x < 0, -x, x) */
    if (strcmp(callee_name, "Abs") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef x = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef zero = LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef neg = LLVMBuildNeg(ctx->builder, x, llvm_tmp_name(ctx));
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, x, zero,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, neg, x, llvm_tmp_name(ctx));
    }

    /* Built-in: Min(a, b) → select(a < b, a, b) */
    if (strcmp(callee_name, "Min") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef a = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef b = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, a, b,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
    }

    /* Built-in: Max(a, b) → select(a > b, a, b) */
    if (strcmp(callee_name, "Max") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef a = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef b = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, a, b,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
    }

    /* Built-in: ArrayLength(arr) */
    if (strcmp(callee_name, "ArrayLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef arr = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (arr != NULL && LLVMGetTypeKind(LLVMTypeOf(arr)) == LLVMStructTypeKind) {
            LLVMValueRef len = llvm_array_length_i64(ctx, arr);
            return LLVMBuildTrunc(ctx->builder, len, ctx->type_i32, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ArrayPush") == 0 && node->data.call.arg_count == 2) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, arr_arg->data.identifier.name);
        if (arr_var == NULL || entry == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
        if (suffix == NULL || strcmp(suffix, "Unknown") == 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        if (LLVMTypeOf(value) != entry->elem_type) {
            if ((entry->elem_type == ctx->type_i32 || entry->elem_type == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
            else if ((entry->elem_type == ctx->type_f32 || entry->elem_type == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
        }

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_array_push_%s", suffix);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn != NULL) {
            LLVMValueRef args[] = { arr_var->alloca, value };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ArraySet") == 0 && node->data.call.arg_count == 3) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, arr_arg->data.identifier.name);
        LLVMValueRef idx = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[2], ctx);
        if (arr_var == NULL || entry == NULL || idx == NULL || value == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef arr = LLVMBuildLoad2(ctx->builder, arr_var->type, arr_var->alloca,
            llvm_tmp_name(ctx));
        LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
        if (LLVMTypeOf(value) != entry->elem_type) {
            if ((entry->elem_type == ctx->type_i32 || entry->elem_type == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
            else if ((entry->elem_type == ctx->type_f32 || entry->elem_type == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, entry->elem_type, llvm_tmp_name(ctx));
        }
        LLVMValueRef gep = LLVMBuildGEP2(ctx->builder, entry->elem_type, data_ptr, &idx, 1,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, gep);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if (strcmp(callee_name, "ArrayPop") == 0 && node->data.call.arg_count == 1) {
        ASTNode *arr_arg = node->data.call.arguments[0];
        if (arr_arg == NULL || arr_arg->type != AST_IDENTIFIER)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, arr_arg->data.identifier.name);
        if (arr_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef arr = LLVMBuildLoad2(ctx->builder, arr_var->type, arr_var->alloca,
            llvm_tmp_name(ctx));
        LLVMValueRef len = llvm_array_length_i64(ctx, arr);
        LLVMValueRef has_any = LLVMBuildICmp(ctx->builder, LLVMIntUGT, len,
            LLVMConstInt(ctx->type_i64, 0, 0), llvm_tmp_name(ctx));
        LLVMValueRef dec = LLVMBuildSub(ctx->builder, len,
            LLVMConstInt(ctx->type_i64, 1, 0), llvm_tmp_name(ctx));
        LLVMValueRef next_len = LLVMBuildSelect(ctx->builder, has_any, dec, len,
            llvm_tmp_name(ctx));
        arr = LLVMBuildInsertValue(ctx->builder, arr, next_len, 1, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, arr, arr_var->alloca);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    if ((strcmp(callee_name, "ViewRead") == 0
         || strcmp(callee_name, "ViewWrite") == 0
         || strcmp(callee_name, "Move") == 0)
        && node->data.call.arg_count == 1) {
        return llvm_emit_expression(node->data.call.arguments[0], ctx);
    }

    /* Built-in: StringLength(s) → call strlen */
    if (strcmp(callee_name, "StringLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef s = llvm_emit_expression(node->data.call.arguments[0], ctx);
        /* Declare strlen if not already */
        LLVMFuncEntry *strlen_fn = llvm_lookup_function(ctx, "strlen");
        if (strlen_fn == NULL) {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i64, params, 1, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, "strlen", ft);
            llvm_register_function(ctx, "strlen", fn, ft, ctx->type_i64);
            strlen_fn = llvm_lookup_function(ctx, "strlen");
        }
        LLVMValueRef args[] = { s };
        LLVMValueRef len = LLVMBuildCall2(ctx->builder, strlen_fn->fn_type,
            strlen_fn->fn, args, 1, llvm_tmp_name(ctx));
        return LLVMBuildTrunc(ctx->builder, len, ctx->type_i32, llvm_tmp_name(ctx));
    }

    if ((strcmp(callee_name, "Contains") == 0
         || strcmp(callee_name, "StringContains") == 0)
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringContains");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if ((strcmp(callee_name, "Replace") == 0
         || strcmp(callee_name, "StringReplace") == 0)
        && node->data.call.arg_count == 3) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringReplace");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 3);
    }

    if (strcmp(callee_name, "Substring") == 0
        && node->data.call.arg_count == 3) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "Substring");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 3);
    }

    if ((strcmp(callee_name, "Trim") == 0
         || strcmp(callee_name, "StringTrim") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringTrim");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if ((strcmp(callee_name, "Upper") == 0
         || strcmp(callee_name, "ToUpper") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToUpper");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if ((strcmp(callee_name, "Lower") == 0
         || strcmp(callee_name, "ToLower") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToLower");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if ((strcmp(callee_name, "Concat") == 0
         || strcmp(callee_name, "StringConcat") == 0)
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringConcat");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "ReadFile") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_read_file");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "ToString") == 0
        && node->data.call.arg_count == 1) {
        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return llvm_coerce_value_to_string(value, ctx);
    }

    if (strcmp(callee_name, "ToInt") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToInt");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "ToFloat") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToFloat");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "Random") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "Random");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "WriteFile") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_write_file");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "Input") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_input");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "SeedRandom") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "SeedRandom");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "FileOpen") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_open");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "FileRead") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_read");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    if (strcmp(callee_name, "FileWrite") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_write");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
    }

    if (strcmp(callee_name, "FileClose") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_close");
        if (fn != NULL)
            return llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
    }

    /* Built-in: Print(s) → printf("%s", s) */
    if (strcmp(callee_name, "Print") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef vt = LLVMTypeOf(val);
        LLVMFuncEntry *pf = llvm_lookup_function(ctx, "printf");
        if (pf != NULL) {
            if (vt == ctx->type_i8ptr) {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder,
                    "%s", ".fmt_s");
                LLVMValueRef args[] = { fmt, val };
                LLVMBuildCall2(ctx->builder, pf->fn_type, pf->fn, args, 2, "");
            } else {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder,
                    "%d", ".fmt_d");
                LLVMValueRef args[] = { fmt, val };
                LLVMBuildCall2(ctx->builder, pf->fn_type, pf->fn, args, 2, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: Ok(value) → { .ok=true, .value=value } */
    if (strcmp(callee_name, "Ok") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, LLVMTypeOf(val), ctx->type_i8ptr }, 3, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r, val, 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: Err(value) → { .ok=false, .value=value } */
    if (strcmp(callee_name, "Err") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 3, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        if (LLVMTypeOf(val) != ctx->type_i8ptr) {
            if (LLVMGetTypeKind(LLVMTypeOf(val)) == LLVMPointerTypeKind) {
                val = LLVMBuildBitCast(ctx->builder, val, ctx->type_i8ptr, llvm_tmp_name(ctx));
            } else {
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
        }
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r, val, 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: IsOk(result) → extract ok field */
    if (strcmp(callee_name, "IsOk") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: IsErr(result) → !ok */
    if (strcmp(callee_name, "IsErr") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: Unwrap(result) → extract value field */
    if (strcmp(callee_name, "Unwrap") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOr(result, default) → ok ? value : default */
    if (strcmp(callee_name, "UnwrapOr") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef def = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        LLVMValueRef ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, ok, val, def, llvm_tmp_name(ctx));
    }

    /* Built-in: Some(value) → { .tag=PgyOptionSome, .value=value } */
    if (strcmp(callee_name, "Some") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, LLVMTypeOf(val) }, 2, 0);
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o, val, 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: None() → { .tag=PgyOptionNone, .value=zero } */
    if (strcmp(callee_name, "None") == 0 && node->data.call.arg_count == 0) {
        LLVMTypeRef value_ty = ctx->type_i32;
        if (LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind
            && LLVMCountStructElementTypes(ctx->current_ret_type) == 2) {
            LLVMTypeRef fields[2];
            LLVMGetStructElementTypes(ctx->current_ret_type, fields);
            if (fields[0] == ctx->type_i32)
                value_ty = fields[1];
        }
        LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, value_ty }, 2, 0);
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstNull(value_ty), 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: IsSome(option) / IsNone(option) */
    if ((strcmp(callee_name, "IsSome") == 0 || strcmp(callee_name, "IsNone") == 0)
        && node->data.call.arg_count == 1) {
        LLVMValueRef o = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, o, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32,
                strcmp(callee_name, "IsSome") == 0 ? 0 : 1, 0),
            llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOption(option) → extract value field */
    if (strcmp(callee_name, "UnwrapOption") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef o = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return LLVMBuildExtractValue(ctx->builder, o, 1, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "Cancel") == 0 && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_task_cancel_export");
        LLVMValueRef task = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (fn != NULL) {
            LLVMValueRef args[] = { task };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                args, 1, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "IsCancelled") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_task_is_cancelled_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                NULL, 0, llvm_tmp_name(ctx));
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "TrySend") == 0 && node->data.call.arg_count == 2) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                snprintf(fname, sizeof(fname), "pgy_channel_try_send_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, val };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, llvm_tmp_name(ctx));
                }
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "TrySendStatus") == 0 && node->data.call.arg_count == 2) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char closed_name[128];
                char send_name[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                snprintf(closed_name, sizeof(closed_name), "pgy_channel_closed_%s",
                    inner != NULL ? inner : "Int");
                snprintf(send_name, sizeof(send_name), "pgy_channel_try_send_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *closed_fn = llvm_lookup_function(ctx, closed_name);
                LLVMFuncEntry *send_fn = llvm_lookup_function(ctx, send_name);
                if (closed_fn != NULL && send_fn != NULL) {
                    LLVMValueRef closed_args[] = { ch_var->alloca };
                    LLVMValueRef send_args[] = { ch_var->alloca, val };
                    LLVMValueRef closed = LLVMBuildCall2(ctx->builder, closed_fn->fn_type,
                        closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, send_fn->fn_type,
                        send_fn->fn, send_args, 2, llvm_tmp_name(ctx));
                    LLVMValueRef has_value = LLVMBuildOr(ctx->builder, closed, ok,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, ctx->type_i1, has_value, ok);
                }
            }
        }
        return llvm_build_option_value(ctx, ctx->type_i1,
            LLVMConstInt(ctx->type_i1, 0, 0), LLVMConstInt(ctx->type_i1, 0, 0));
    }

    if (strcmp(callee_name, "SendTimeout") == 0 && node->data.call.arg_count == 3) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[2], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(fname, sizeof(fname), "pgy_channel_send_timeout_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, val, timeout };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 3, llvm_tmp_name(ctx));
                }
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "SendTimeoutStatus") == 0 && node->data.call.arg_count == 3) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char closed_name[128];
                char send_name[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[2], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(closed_name, sizeof(closed_name), "pgy_channel_closed_%s",
                    inner != NULL ? inner : "Int");
                snprintf(send_name, sizeof(send_name), "pgy_channel_send_timeout_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *closed_fn = llvm_lookup_function(ctx, closed_name);
                LLVMFuncEntry *send_fn = llvm_lookup_function(ctx, send_name);
                if (closed_fn != NULL && send_fn != NULL) {
                    LLVMValueRef closed_args[] = { ch_var->alloca };
                    LLVMValueRef send_args[] = { ch_var->alloca, val, timeout };
                    LLVMValueRef closed_before = LLVMBuildCall2(ctx->builder,
                        closed_fn->fn_type, closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, send_fn->fn_type,
                        send_fn->fn, send_args, 3, llvm_tmp_name(ctx));
                    LLVMValueRef closed_after = LLVMBuildCall2(ctx->builder,
                        closed_fn->fn_type, closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
                    LLVMValueRef failed = LLVMBuildNot(ctx->builder, ok, llvm_tmp_name(ctx));
                    LLVMValueRef failed_and_closed = LLVMBuildAnd(ctx->builder, failed,
                        closed_after, llvm_tmp_name(ctx));
                    LLVMValueRef closed = LLVMBuildOr(ctx->builder, closed_before,
                        failed_and_closed, llvm_tmp_name(ctx));
                    LLVMValueRef has_value = LLVMBuildOr(ctx->builder, closed, ok,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, ctx->type_i1, has_value, ok);
                }
            }
        }
        return llvm_build_option_value(ctx, ctx->type_i1,
            LLVMConstInt(ctx->type_i1, 0, 0), LLVMConstInt(ctx->type_i1, 0, 0));
    }

    if ((strcmp(callee_name, "TryRecv") == 0 && node->data.call.arg_count == 1)
        || (strcmp(callee_name, "RecvTimeout") == 0 && node->data.call.arg_count == 2)) {
        ASTNode *channel = node->data.call.arguments[0];
        const char *inner = "Int";
        LLVMVarEntry *ch_var = NULL;
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            ch_var = llvm_scope_lookup(ctx, name);
            {
                const char *lookup_inner = llvm_lookup_channel_inner(ctx, name);
                if (lookup_inner != NULL)
                    inner = lookup_inner;
            }
        }

        LLVMTypeRef value_ty = pergyra_type_to_llvm(ctx, inner);
        if (ch_var != NULL) {
            LLVMValueRef tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);

            char fname[128];
            if (strcmp(callee_name, "TryRecv") == 0) {
                snprintf(fname, sizeof(fname), "pgy_channel_try_recv_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, tmp };
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, llvm_tmp_name(ctx));
                    LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, value_ty, ok, value);
                }
            } else {
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[1], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(fname, sizeof(fname), "pgy_channel_recv_timeout_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, tmp, timeout };
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 3, llvm_tmp_name(ctx));
                    LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, value_ty, ok, value);
                }
            }
        }

        return llvm_build_option_value(ctx, value_ty,
            LLVMConstInt(ctx->type_i1, 0, 0), LLVMConstNull(value_ty));
    }

    if ((strcmp(callee_name, "ChannelReady") == 0
         || strcmp(callee_name, "ChannelLength") == 0
         || strcmp(callee_name, "ChannelCapacity") == 0
         || strcmp(callee_name, "ChannelSpace") == 0
         || strcmp(callee_name, "ChannelFull") == 0
         || strcmp(callee_name, "ChannelClosed") == 0)
        && node->data.call.arg_count == 1) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                snprintf(fname, sizeof(fname), "pgy_channel_%s_%s",
                    strcmp(callee_name, "ChannelReady") == 0 ? "ready" :
                    strcmp(callee_name, "ChannelLength") == 0 ? "length" :
                    strcmp(callee_name, "ChannelCapacity") == 0 ? "capacity" :
                    strcmp(callee_name, "ChannelSpace") == 0 ? "space" :
                    strcmp(callee_name, "ChannelFull") == 0 ? "full" :
                    "closed",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 1, llvm_tmp_name(ctx));
                }
            }
        }

        if (strcmp(callee_name, "ChannelLength") == 0
            || strcmp(callee_name, "ChannelCapacity") == 0
            || strcmp(callee_name, "ChannelSpace") == 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (node->data.call.callee->type == AST_IDENTIFIER) {
        const char *host_name = ctx->current_class_name;
        ASTNode *host_method = llvm_current_host_method_decl(ctx, callee_name);
        if (host_name != NULL && host_method != NULL) {
            char full_name[256];
            snprintf(full_name, sizeof(full_name), "%s_%s", host_name, callee_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
            if (fn != NULL) {
                size_t argc = node->data.call.arg_count;
                LLVMValueRef *args = calloc(argc + 1, sizeof(LLVMValueRef));
                if (args == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                args[0] = llvm_current_self_call_arg(ctx);
                for (size_t i = 0; i < argc; i++) {
                    LLVMValueRef arg_val = llvm_emit_expression(node->data.call.arguments[i], ctx);
                    size_t logical_idx = 0;
                    for (size_t pk = 0; pk < host_method->data.func_decl.param_count; pk++) {
                        FuncParam *p = host_method->data.func_decl.params[pk];
                        const char *ptn = NULL;
                        LLVMClassTypeEntry *param_cls = NULL;
                        if (p->type == NULL && strcmp(p->name, "self") == 0)
                            continue;
                        if (logical_idx == i) {
                            if (p->type != NULL && p->type->type == AST_TYPE)
                                ptn = p->type->data.type.name;
                            param_cls = ptn != NULL ? llvm_lookup_class(ctx, ptn) : NULL;
                            if (param_cls != NULL && param_cls->is_pointer_self_host
                                && node->data.call.arguments[i] != NULL
                                && node->data.call.arguments[i]->type == AST_IDENTIFIER) {
                                const char *arg_name =
                                    node->data.call.arguments[i]->data.identifier.name;
                                LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                                if (arg_var != NULL) {
                                    if (arg_var->type == LLVMPointerType(param_cls->struct_type, 0))
                                        arg_val = LLVMBuildLoad2(ctx->builder,
                                            arg_var->type, arg_var->alloca, llvm_tmp_name(ctx));
                                    else
                                        arg_val = arg_var->alloca;
                                }
                            }
                            break;
                        }
                        logical_idx++;
                    }
                    args[i + 1] = arg_val;
                }
                {
                    LLVMValueRef result;
                    if (fn->ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, (unsigned)(argc + 1), "");
                        result = LLVMConstInt(ctx->type_i32, 0, 0);
                    } else {
                        result = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, (unsigned)(argc + 1), llvm_tmp_name(ctx));
                    }
                    free(args);
                    return result;
                }
            }
        }
    }

    size_t argc = node->data.call.arg_count;
    ASTNode *decl = llvm_find_function_decl(ctx, callee_name);
    unsigned emitted_argc = 0;
    LLVMValueRef *args = NULL;

    if (decl != NULL)
        args = llvm_build_boundary_call_args(ctx, decl, node->data.call.arguments,
            argc, &emitted_argc);
    if (args == NULL) {
        args = calloc(argc > 0 ? argc : 1, sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(node->data.call.arguments[i], ctx);
        emitted_argc = (unsigned)argc;
    }

    LLVMFuncEntry *func = llvm_resolve_callee_entry(ctx, callee_name, args, argc);
    if (func == NULL) {
        fprintf(stderr, "[llvm] warning: unknown function '%s'\n", callee_name);
        free(args);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    for (size_t i = 0; decl == NULL && i < argc; i++) {
        ASTNode *arg_node = node->data.call.arguments[i];
        unsigned param_count = LLVMCountParams(func->fn);
        LLVMTypeRef param_ty = (i < param_count)
            ? LLVMTypeOf(LLVMGetParam(func->fn, (unsigned)i))
            : NULL;
        if (param_ty != NULL
            && LLVMGetTypeKind(param_ty) == LLVMPointerTypeKind
            && arg_node->type == AST_IDENTIFIER) {
            LLVMVarEntry *v = llvm_scope_lookup(ctx,
                arg_node->data.identifier.name);
            if (v != NULL)
                args[i] = v->alloca;
        }
    }

    LLVMValueRef result;
    if (func->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                       args, emitted_argc, "");
        result = LLVMConstInt(ctx->type_i32, 0, 0);
    } else {
        result = LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                                args, emitted_argc, llvm_tmp_name(ctx));
    }

    free(args);
    return result;
}

static LLVMValueRef
llvm_emit_member_lvalue_ptr(ASTNode *node, LLVMGenCtx *ctx, LLVMTypeRef *out_field_type)
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

    obj_node = node->data.member.object;
    field_name = node->data.member.name;
    if (obj_node == NULL || field_name == NULL)
        return NULL;

    class_name = llvm_expr_custom_type_name(obj_node, ctx);
    if (class_name == NULL)
        return NULL;

    cls = llvm_lookup_class(ctx, class_name);
    if (cls == NULL)
        return NULL;

    if (obj_node->type == AST_IDENTIFIER) {
        const char *var_name = obj_node->data.identifier.name;
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

static LLVMValueRef
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
                LLVMValueRef arr = LLVMBuildLoad2(ctx->builder, arr_var->type,
                    arr_var->alloca, llvm_tmp_name(ctx));
                LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
                LLVMValueRef gep = LLVMBuildGEP2(ctx->builder, entry->elem_type,
                    data_ptr, &idx, 1, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, val, gep);
                return val;
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Member assignment: obj.field = value */
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
        return val;
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    const char *name = NULL;
    if (node->data.assignment.target->type == AST_IDENTIFIER)
        name = node->data.assignment.target->data.identifier.name;

    if (name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
    if (var == NULL && ctx->current_class_name != NULL) {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, ctx->current_class_name);
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
                if (llvm_nominal_uses_pointer_self(ctx, cls->class_name))
                    base_ptr = LLVMBuildLoad2(ctx->builder, self_var->type,
                        self_var->alloca, llvm_tmp_name(ctx));
                gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
                    (unsigned)field_idx, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, val, gep);
                return val;
            }
        }
    }
    if (var == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Slot sugar: x = 5 → pgy_write_T(&x, 5) */
    const char *slot_inner = llvm_lookup_slot_inner(ctx, name);
    if (slot_inner != NULL) {
        LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", slot_inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn != NULL) {
            LLVMValueRef args[] = { var->alloca, val };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        } else {
            if (llvm_lookup_slot_is_secure(ctx, name))
                llvm_direct_secure_slot_write(ctx, var, val);
            else
                llvm_direct_slot_write(ctx, var, val);
        }
        return val;
    }

    LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
    if (val == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMBuildStore(ctx->builder, val, var->alloca);
    return val;
}

static LLVMValueRef
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

    const char *class_name = llvm_expr_custom_type_name(obj_node, ctx);
    if (class_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, class_name);
    if (cls == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    int field_idx = llvm_class_field_index(cls, field_name);
    if (field_idx < 0)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (obj_node->type == AST_IDENTIFIER) {
        const char *var_name = obj_node->data.identifier.name;
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

    return LLVMConstInt(ctx->type_i32, 0, 0);
}

LLVMValueRef
llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return NULL;

    switch (node->type) {
    case AST_NUMBER:        return llvm_emit_number(node, ctx);
    case AST_STRING:        return llvm_emit_string(node, ctx);
    case AST_BOOLEAN:       return llvm_emit_boolean(node, ctx);
    case AST_IDENTIFIER:    return llvm_emit_identifier(node, ctx);
    case AST_BINARY:        return llvm_emit_binary(node, ctx);
    case AST_UNARY:         return llvm_emit_unary(node, ctx);
    case AST_CALL:          return llvm_emit_call(node, ctx);
    case AST_ASSIGNMENT:    return llvm_emit_assignment(node, ctx);
    case AST_MEMBER_ACCESS: return llvm_emit_member_access(node, ctx);
    case AST_ARRAY_LITERAL: {
        size_t count = node->data.array_literal.count;
        const char *inner_name = "Int";
        LLVMTypeRef elem_type = ctx->type_i32;
        if (count > 0) {
            LLVMValueRef first = llvm_emit_expression(node->data.array_literal.elements[0], ctx);
            if (first != NULL) {
                elem_type = LLVMTypeOf(first);
                const char *suffix = llvm_type_to_suffix(ctx, elem_type);
                if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
                    inner_name = suffix;
            }
        }

        LLVMTypeRef array_type = llvm_array_struct_type(ctx, inner_name);
        LLVMValueRef tmp = llvm_create_entry_alloca(ctx, array_type, llvm_tmp_name(ctx));
        char new_fn_name[64];
        char push_fn_name[64];
        snprintf(new_fn_name, sizeof(new_fn_name), "pgy_array_new_%s", inner_name);
        snprintf(push_fn_name, sizeof(push_fn_name), "pgy_array_push_%s", inner_name);
        LLVMFuncEntry *new_fn = llvm_lookup_function(ctx, new_fn_name);
        LLVMFuncEntry *push_fn = llvm_lookup_function(ctx, push_fn_name);
        if (new_fn != NULL) {
            LLVMValueRef args[] = {
                LLVMConstInt(ctx->type_i64, (unsigned long long)count, 0)
            };
            LLVMValueRef arr_val = LLVMBuildCall2(ctx->builder, new_fn->fn_type,
                new_fn->fn, args, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, arr_val, tmp);
        }
        for (size_t i = 0; i < count; i++) {
            LLVMValueRef elem = llvm_emit_expression(node->data.array_literal.elements[i], ctx);
            if (push_fn != NULL && elem != NULL) {
                LLVMValueRef args[] = { tmp, elem };
                LLVMBuildCall2(ctx->builder, push_fn->fn_type, push_fn->fn, args, 2, "");
            }
        }
        return LLVMBuildLoad2(ctx->builder, array_type, tmp, llvm_tmp_name(ctx));
    }

    case AST_ARRAY_ACCESS: {
        /* arr[idx] → GEP + load */
        ASTNode *array_node = node->data.array_access.array;
        LLVMValueRef arr = llvm_emit_expression(array_node, ctx);
        LLVMValueRef idx = llvm_emit_expression(
            node->data.array_access.index, ctx);
        if (arr == NULL || idx == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMTypeRef arr_ty = LLVMTypeOf(arr);
        if (arr_ty == ctx->type_i8ptr) {
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                LLVMInt8TypeInContext(ctx->context),
                arr, &idx, 1, llvm_tmp_name(ctx));
            return LLVMBuildLoad2(ctx->builder,
                LLVMInt8TypeInContext(ctx->context),
                gep, llvm_tmp_name(ctx));
        }

        if (LLVMGetTypeKind(arr_ty) == LLVMPointerTypeKind) {
            LLVMTypeRef elem_ty = LLVMGetElementType(arr_ty);
            if (elem_ty != NULL) {
                LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                    elem_ty, arr, &idx, 1, llvm_tmp_name(ctx));
                return LLVMBuildLoad2(ctx->builder, elem_ty,
                    gep, llvm_tmp_name(ctx));
            }
        }

        if (LLVMGetTypeKind(arr_ty) == LLVMStructTypeKind) {
            LLVMValueRef data_ptr = llvm_array_data_ptr(ctx, arr);
            LLVMTypeRef elem_ty = ctx->type_i32;
            if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
                LLVMArrayVarEntry *entry = llvm_lookup_array_var(
                    ctx, array_node->data.identifier.name);
                if (entry != NULL)
                    elem_ty = entry->elem_type;
            }
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                elem_ty, data_ptr, &idx, 1, llvm_tmp_name(ctx));
            return LLVMBuildLoad2(ctx->builder, elem_ty,
                gep, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_CONTEXT_ACCESS: {
        /* context.GetRole("slotName") → load role slot from self (i8*)
         * self is in scope as the party/systemic method's first param */
        LLVMVarEntry *self_var = llvm_scope_lookup(ctx, "self");
        if (self_var == NULL)
            return LLVMConstNull(ctx->type_i8ptr);

        /* For now: return the self pointer cast — the role slot is
         * accessed through the party struct, which self points to */
        LLVMValueRef self_val = LLVMBuildLoad2(ctx->builder,
            ctx->type_i8ptr, self_var->alloca, llvm_tmp_name(ctx));
        return self_val;
    }

    case AST_PARTY_INSTANCE: {
        /* PartyType { slot1: val1, slot2: val2 }
         * → alloca struct, store fields, return value */
        const char *pty = node->data.party_instance.party_type;
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, pty);
        if (cls == NULL)
            return LLVMConstNull(ctx->type_i8ptr);

        LLVMValueRef alloca = llvm_create_entry_alloca(ctx,
            cls->struct_type, llvm_tmp_name(ctx));

        /* Zero-initialize */
        LLVMValueRef zero = LLVMConstNull(cls->struct_type);
        LLVMBuildStore(ctx->builder, zero, alloca);

        /* Store each assignment */
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            const char *slot_name = node->data.party_instance.assignments[i].slot_name;
            ASTNode *val_node = node->data.party_instance.assignments[i].value;

            /* Find field index */
            for (int f = 0; f < cls->field_count; f++) {
                if (strcmp(cls->fields[f].field_name, slot_name) == 0) {
                    LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                        ctx->builder, cls->struct_type, alloca,
                        (unsigned)cls->fields[f].index,
                        llvm_tmp_name(ctx));
                    LLVMValueRef val = llvm_emit_expression(val_node, ctx);
                    if (val != NULL)
                        LLVMBuildStore(ctx->builder, val, field_ptr);
                    break;
                }
            }
        }

        return LLVMBuildLoad2(ctx->builder, cls->struct_type,
            alloca, llvm_tmp_name(ctx));
    }

    case AST_TASK_GROUP: {
        /* TaskGroup { tasks... } → emit tasks sequentially (MVP) */
        for (size_t i = 0; i < node->data.task_group.task_count; i++) {
            if (node->data.task_group.tasks[i] != NULL)
                llvm_emit_expression(node->data.task_group.tasks[i], ctx);
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_CHANNEL_SEND: {
        /* ch <- value → pgy_channel_send_T(&ch, value) */
        LLVMVarEntry *ch_var = NULL;
        const char *suffix = "Int";
        if (node->data.channel_send.channel != NULL
            && node->data.channel_send.channel->type == AST_IDENTIFIER) {
            const char *name = node->data.channel_send.channel->data.identifier.name;
            ch_var = llvm_scope_lookup(ctx, name);
            {
                const char *inner = llvm_lookup_channel_inner(ctx, name);
                if (inner != NULL)
                    suffix = inner;
            }
        }
        if (ch_var != NULL) {
            LLVMValueRef val = llvm_emit_expression(
                node->data.channel_send.value, ctx);
            char fname[128];
            snprintf(fname, sizeof(fname), "pgy_channel_send_%s", suffix);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            if (fn != NULL && val != NULL) {
                LLVMValueRef args[] = { ch_var->alloca, val };
                return LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 2, llvm_tmp_name(ctx));
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    case AST_CHANNEL_RECV: {
        /* <- ch → pgy_channel_recv_val_T(&ch) */
        LLVMVarEntry *ch_var = NULL;
        const char *suffix = "Int";
        if (node->data.channel_recv.channel != NULL
            && node->data.channel_recv.channel->type == AST_IDENTIFIER) {
            const char *name = node->data.channel_recv.channel->data.identifier.name;
            ch_var = llvm_scope_lookup(ctx, name);
            {
                const char *inner = llvm_lookup_channel_inner(ctx, name);
                if (inner != NULL)
                    suffix = inner;
            }
        }
        if (ch_var != NULL) {
            char fname[128];
            snprintf(fname, sizeof(fname), "pgy_channel_recv_val_%s", suffix);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            if (fn != NULL) {
                LLVMValueRef args[] = { ch_var->alloca };
                return LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 1, llvm_tmp_name(ctx));
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_SPAWN_EXPR:
        return llvm_emit_spawn_expr(node, ctx);

    case AST_AWAIT_EXPR:
        if (node->data.await_expr.expression != NULL) {
            ASTNode *inner_expr = node->data.await_expr.expression;
            const char *inner = NULL;
            bool is_remote = false;
            if (inner_expr->type == AST_IDENTIFIER)
                inner = llvm_lookup_future_inner(ctx, inner_expr->data.identifier.name);
            if (inner_expr->type == AST_IDENTIFIER)
                is_remote = llvm_lookup_future_is_remote(ctx, inner_expr->data.identifier.name);
            if (inner != NULL) {
                LLVMValueRef task = llvm_emit_expression(inner_expr, ctx);
                return llvm_await_task_handle(ctx, task, inner, is_remote);
            }
            return llvm_emit_expression(inner_expr, ctx);
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    case AST_LAMBDA_EXPR: {
        /* Generate a static LLVM function and return its pointer */
        int lid = ctx->lambda_counter++;
        int pc = (int)node->data.lambda_expr.param_count;

        /* Determine return type */
        LLVMTypeRef ret_type = ctx->type_i32;
        if (node->data.lambda_expr.return_type != NULL)
            ret_type = ast_type_to_llvm(ctx, node->data.lambda_expr.return_type);
        else if (node->data.lambda_expr.body != NULL
                 && node->data.lambda_expr.body->type == AST_BLOCK)
            ret_type = ctx->type_void;

        /* Parameter types (default i32) */
        LLVMTypeRef lparams[8];
        for (int j = 0; j < pc && j < 8; j++) {
            ASTNode *p = node->data.lambda_expr.params[j];
            if (p->type == AST_LET_DECL && p->data.let_decl.type != NULL)
                lparams[j] = ast_type_to_llvm(ctx, p->data.let_decl.type);
            else
                lparams[j] = ctx->type_i32;
        }

        char lname[128];
        snprintf(lname, sizeof(lname), "pgy_lambda_%d", lid);
        LLVMTypeRef lft = LLVMFunctionType(ret_type,
            lparams, (unsigned)pc, 0);
        LLVMValueRef lfn = LLVMAddFunction(ctx->module, lname, lft);
        llvm_register_function(ctx, LLVMGetValueName(lfn),
            lfn, lft, ret_type);

        /* Save current builder state */
        LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
        LLVMValueRef saved_fn = ctx->current_function;
        LLVMTypeRef saved_ret = ctx->current_ret_type;

        ctx->current_function = lfn;
        ctx->current_ret_type = ret_type;

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, lfn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        llvm_scope_push(ctx);
        for (int j = 0; j < pc; j++) {
            ASTNode *p = node->data.lambda_expr.params[j];
            const char *pname = (p->type == AST_IDENTIFIER)
                ? p->data.identifier.name : p->data.let_decl.name;
            LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder,
                lparams[j], pname);
            LLVMBuildStore(ctx->builder, LLVMGetParam(lfn, (unsigned)j),
                alloca);
            llvm_scope_declare(ctx, pname, alloca, lparams[j]);
        }

        if (node->data.lambda_expr.body != NULL) {
            if (node->data.lambda_expr.body->type == AST_BLOCK) {
                llvm_emit_block(node->data.lambda_expr.body, ctx);
            } else {
                LLVMValueRef val = llvm_emit_expression(
                    node->data.lambda_expr.body, ctx);
                if (ret_type != ctx->type_void)
                    LLVMBuildRet(ctx->builder, val);
                else
                    LLVMBuildRetVoid(ctx->builder);
            }
        }

        /* Ensure terminator exists */
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) == NULL) {
            if (ret_type == ctx->type_void)
                LLVMBuildRetVoid(ctx->builder);
            else
                LLVMBuildRet(ctx->builder,
                    LLVMConstInt(ret_type, 0, 0));
        }

        llvm_scope_pop(ctx);

        /* Restore builder state */
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

        return lfn;
    }

    case AST_EVENT_SUBSCRIBE: {
        /* event += handler → EventName_SUBSCRIBE(&event, handler) */
        ASTNode *evt = node->data.event_op.event;
        ASTNode *handler = node->data.event_op.handler;

        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_SUBSCRIBE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);
            LLVMValueRef hval = llvm_emit_expression(handler, ctx);

            if (fn != NULL && ev_ptr != NULL) {
                LLVMValueRef args[] = { ev_ptr, hval };
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 2, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_EVENT_UNSUBSCRIBE: {
        /* event -= handler → EventName_UNSUBSCRIBE(&event, handler) */
        ASTNode *evt = node->data.event_op.event;
        ASTNode *handler = node->data.event_op.handler;

        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_UNSUBSCRIBE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);
            LLVMValueRef hval = llvm_emit_expression(handler, ctx);

            if (fn != NULL && ev_ptr != NULL) {
                LLVMValueRef args[] = { ev_ptr, hval };
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 2, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_EVENT_INVOKE: {
        /* Emit(event, args...) → EventName_INVOKE(&event, args...) */
        ASTNode *evt = node->data.event_invoke.event;
        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);

            if (fn != NULL && ev_ptr != NULL) {
                size_t ac = node->data.event_invoke.arg_count;
                LLVMValueRef *args = calloc(ac + 1, sizeof(LLVMValueRef));
                args[0] = ev_ptr;
                for (size_t j = 0; j < ac; j++)
                    args[j + 1] = llvm_emit_expression(
                        node->data.event_invoke.arguments[j], ctx);
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, (unsigned)(ac + 1), "");
                free(args);
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    default:
        fprintf(stderr, "[llvm] warning: unhandled expression AST type %d\n",
                (int)node->type);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }
}


#endif /* PGY_LLVM_ENABLED */
