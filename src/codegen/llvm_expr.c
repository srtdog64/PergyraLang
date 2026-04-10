/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — expression emission
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static LLVMValueRef llvm_emit_member_lvalue_ptr(ASTNode *node, LLVMGenCtx *ctx,
                                                LLVMTypeRef *out_field_type);
static LLVMTypeRef llvm_function_signature_from_event_type(LLVMGenCtx *ctx,
                                                           ASTNode *type_node);
LLVMValueRef llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx);
#include "llvm_expr_helpers.inc"
#include "llvm_expr_values.inc"

static LLVMTypeRef
llvm_function_signature_from_event_type(LLVMGenCtx *ctx, ASTNode *type_node)
{
    size_t param_count;
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef ret_type = ctx->type_void;
    LLVMTypeRef fn_type;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_EVENT_HANDLER_TYPE)
        return NULL;

    param_count = type_node->data.event_handler_type.param_count;
    if (type_node->data.event_handler_type.return_type != NULL)
        ret_type = ast_type_to_llvm(ctx, type_node->data.event_handler_type.return_type);

    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(LLVMTypeRef));
        if (param_types == NULL)
            return LLVMFunctionType(ret_type, NULL, 0, 0);
        for (size_t i = 0; i < param_count; i++)
            param_types[i] = ast_type_to_llvm(ctx,
                type_node->data.event_handler_type.param_types[i]);
    }

    fn_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
    free(param_types);
    return fn_type;
}
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
            LLVMValueRef true_str = LLVMBuildGlobalStringPtr(
                ctx->builder, "true", llvm_tmp_name(ctx));
            LLVMValueRef false_str = LLVMBuildGlobalStringPtr(
                ctx->builder, "false", llvm_tmp_name(ctx));
            return LLVMBuildSelect(ctx->builder, value, true_str, false_str,
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
            size_t fn_len = strlen("operator_") + strlen(suffix) + 1 + strlen(type_name) + 1;
            char *fn_name = malloc(fn_len);
            if (fn_name == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            snprintf(fn_name, fn_len, "operator_%s_%s", suffix, type_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            free(fn_name);
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

    if ((node->data.binary.op.type == TOKEN_EQUAL
         || node->data.binary.op.type == TOKEN_NOT_EQUAL)
        && (left_type == ctx->type_i8ptr || right_type == ctx->type_i8ptr)) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_string_equals");
        if (left_type != ctx->type_i8ptr)
            left = llvm_coerce_value_to_string(left, ctx);
        if (right_type != ctx->type_i8ptr)
            right = llvm_coerce_value_to_string(right, ctx);
        if (fn != NULL && left != NULL && right != NULL
            && LLVMTypeOf(left) == ctx->type_i8ptr
            && LLVMTypeOf(right) == ctx->type_i8ptr) {
            LLVMValueRef args[] = { left, right };
            LLVMValueRef eq = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                             args, 2, llvm_tmp_name(ctx));
            if (node->data.binary.op.type == TOKEN_EQUAL)
                return eq;
            return LLVMBuildNot(ctx->builder, eq, llvm_tmp_name(ctx));
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

#include "llvm_expr_calls.inc"

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
         * self is in scope as the party/roster method's first param */
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
