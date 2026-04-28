/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Lightweight expression type inference fallback.  Full semantic expression
 * typing remains in the type checker; this owner keeps the Type core focused
 * on construction, equality, assignability, and generic instantiation.
 */

#include "type_system.h"

#include <stdlib.h>
#include <string.h>

Type *
type_infer_expression(const ASTNode *expr, TypeEnv *env)
{
    if (expr == NULL)
        return TYPE_UNKNOWN;

    switch (expr->type) {
    case AST_NUMBER:
        if (expr->data.number.is_long)
            return TYPE_LONG;
        return expr->data.number.value == (int64_t)expr->data.number.value
            ? TYPE_INT
            : TYPE_FLOAT;

    case AST_STRING:
        return TYPE_STRING;

    case AST_BOOLEAN:
        return TYPE_BOOL;

    case AST_IDENTIFIER: {
        Type *var_type = type_env_lookup_variable(env, expr->data.identifier.name);
        if (var_type != NULL)
            return var_type;

        Type *named_type = type_env_lookup_type(env, expr->data.identifier.name);
        if (named_type != NULL)
            return named_type;

        return TYPE_UNKNOWN;
    }

    case AST_TYPE: {
        Type *named_type = type_env_lookup_type(env, expr->data.type.name);
        return named_type != NULL ? named_type : TYPE_UNKNOWN;
    }

    case AST_MEMBER_ACCESS: {
        Type *object_type = type_infer_expression(expr->data.member.object, env);
        if (object_type != NULL
            && object_type->kind == TYPE_KIND_CONSTRUCTED
            && strcmp(expr->data.member.name, "Length") == 0
            && (type_equals(object_type->data.constructed.constructor, TYPE_ARRAY)
                || type_equals(object_type->data.constructed.constructor, TYPE_SLICE))) {
            return TYPE_INT;
        }
        return TYPE_UNKNOWN;
    }

    case AST_ARRAY_ACCESS: {
        Type *array_type = type_infer_expression(expr->data.array_access.array, env);
        if (array_type != NULL
            && array_type->kind == TYPE_KIND_CONSTRUCTED
            && array_type->data.constructed.arg_count >= 1) {
            return array_type->data.constructed.args[0];
        }
        return TYPE_UNKNOWN;
    }

    case AST_ASSIGNMENT:
        return type_infer_expression(expr->data.assignment.target, env);

    case AST_BINARY: {
        Type *left = type_infer_expression(expr->data.binary.left, env);
        Type *right = type_infer_expression(expr->data.binary.right, env);
        PgyTokenType op = expr->data.binary.op.type;

        switch (op) {
        case TOKEN_AND:
        case TOKEN_OR:
        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
            return TYPE_BOOL;

        case TOKEN_PLUS:
            if (left == TYPE_STRING || right == TYPE_STRING)
                return TYPE_STRING;
            break;

        default:
            break;
        }

        if (left == TYPE_DOUBLE || right == TYPE_DOUBLE)
            return TYPE_DOUBLE;
        if (left == TYPE_FLOAT || right == TYPE_FLOAT)
            return TYPE_FLOAT;
        if (left == TYPE_LONG || right == TYPE_LONG)
            return TYPE_LONG;
        if (left != NULL && left != TYPE_UNKNOWN)
            return left;
        if (right != NULL && right != TYPE_UNKNOWN)
            return right;
        return TYPE_UNKNOWN;
    }

    case AST_UNARY: {
        Type *operand = type_infer_expression(expr->data.unary.operand, env);
        if (expr->data.unary.op.type == TOKEN_NOT)
            return TYPE_BOOL;
        return operand != NULL ? operand : TYPE_UNKNOWN;
    }

    case AST_CALL:
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_MEMBER_ACCESS
            && expr->data.call.callee->data.member.object != NULL
            && expr->data.call.callee->data.member.name != NULL
            && strcmp(expr->data.call.callee->data.member.name, "Slice") == 0) {
            Type *receiver = type_infer_expression(
                expr->data.call.callee->data.member.object, env);
            if (receiver != NULL
                && receiver->kind == TYPE_KIND_CONSTRUCTED
                && receiver->data.constructed.arg_count >= 1
                && (type_equals(receiver->data.constructed.constructor, TYPE_ARRAY)
                    || type_equals(receiver->data.constructed.constructor, TYPE_SLICE))) {
                return type_create_constructed(TYPE_SLICE,
                    receiver->data.constructed.args, 1);
            }
        }
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee = expr->data.call.callee->data.identifier.name;

            if (strcmp(callee, "Read") == 0 && expr->data.call.arg_count >= 1) {
                Type *slot_type = type_infer_expression(expr->data.call.arguments[0], env);
                if (slot_type != NULL && slot_type->kind == TYPE_KIND_SLOT)
                    return slot_type->data.slot.inner_type;
            }
            if ((strcmp(callee, "ClaimSlot") == 0 || strcmp(callee, "ClaimSecureSlot") == 0)
                && expr->data.call.arg_count >= 1) {
                Type *inner = type_infer_expression(expr->data.call.arguments[0], env);
                if (inner != NULL && inner != TYPE_UNKNOWN)
                    return type_create_slot(inner, strcmp(callee, "ClaimSecureSlot") == 0);
            }
            if ((strcmp(callee, "Clone") == 0 || strcmp(callee, "RcClone") == 0)
                && expr->data.call.arg_count >= 1)
                return type_infer_expression(expr->data.call.arguments[0], env);
            if (strcmp(callee, "RcDowngrade") == 0 && expr->data.call.arg_count >= 1) {
                Type *rc_type = type_infer_expression(expr->data.call.arguments[0], env);
                if (rc_type != NULL
                    && rc_type->kind == TYPE_KIND_CONSTRUCTED
                    && type_equals(rc_type->data.constructed.constructor, TYPE_RC)
                    && rc_type->data.constructed.arg_count == 1) {
                    return type_create_constructed(TYPE_WEAK,
                        rc_type->data.constructed.args,
                        rc_type->data.constructed.arg_count);
                }
            }
            if (strcmp(callee, "WeakUpgrade") == 0 && expr->data.call.arg_count >= 1) {
                Type *weak_type = type_infer_expression(expr->data.call.arguments[0], env);
                if (weak_type != NULL
                    && weak_type->kind == TYPE_KIND_CONSTRUCTED
                    && type_equals(weak_type->data.constructed.constructor, TYPE_WEAK)
                    && weak_type->data.constructed.arg_count == 1) {
                    return type_create_constructed(TYPE_RC,
                        weak_type->data.constructed.args,
                        weak_type->data.constructed.arg_count);
                }
            }
            if (strcmp(callee, "AllocatorSystem") == 0
                || strcmp(callee, "AllocatorTracing") == 0
                || strcmp(callee, "AllocatorDebug") == 0
                || strcmp(callee, "AllocatorPool") == 0) {
                return TYPE_ALLOCATOR;
            }
            if (strcmp(callee, "ClaimQubit") == 0)
                return TYPE_QUBIT;
            if (strcmp(callee, "Measure") == 0
                || strcmp(callee, "QubitState") == 0)
                return TYPE_INT;
            if (strcmp(callee, "IsCollapsed") == 0
                || strcmp(callee, "IntoClassical") == 0)
                return TYPE_BOOL;
        }

        return TYPE_UNKNOWN;

    case AST_AWAIT_EXPR: {
        Type *inner = type_infer_expression(expr->data.await_expr.expression, env);
        if (inner != NULL
            && inner->kind == TYPE_KIND_CONSTRUCTED
            && inner->data.constructed.arg_count == 1) {
            return inner->data.constructed.args[0];
        }
        return TYPE_UNKNOWN;
    }

    case AST_SPAWN_EXPR: {
        Type *inner = type_infer_expression(expr->data.spawn_expr.function, env);
        Type *args[1] = { inner != NULL ? inner : TYPE_UNKNOWN };
        return type_create_constructed(TYPE_FUTURE, args, 1);
    }

    case AST_CHANNEL_RECV: {
        Type *channel_type = type_infer_expression(expr->data.channel_recv.channel, env);
        if (channel_type != NULL
            && channel_type->kind == TYPE_KIND_CONSTRUCTED
            && channel_type->data.constructed.arg_count == 1) {
            return channel_type->data.constructed.args[0];
        }
        return TYPE_UNKNOWN;
    }

    case AST_CHANNEL_SEND:
    case AST_EVENT_INVOKE:
        return TYPE_VOID;

    case AST_LAMBDA_EXPR: {
        size_t param_count = expr->data.lambda_expr.param_count;
        Type **params = calloc(param_count == 0 ? 1 : param_count, sizeof(Type *));
        if (params == NULL)
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < param_count; i++) {
            params[i] = TYPE_UNKNOWN;
        }
        Type *return_type = expr->data.lambda_expr.return_type != NULL
            ? type_infer_expression(expr->data.lambda_expr.return_type, env)
            : TYPE_UNKNOWN;
        Type *fn_type = type_create_function(params, param_count, return_type);
        free(params);
        return fn_type != NULL ? fn_type : TYPE_UNKNOWN;
    }

    default:
        return TYPE_UNKNOWN;
    }
}

bool
type_unify(Type *a, Type *b, TypeEnv *env)
{
    (void)env;
    return type_equals(a, b);
}

