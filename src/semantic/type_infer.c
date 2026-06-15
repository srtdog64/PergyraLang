/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Lightweight expression type inference utility.  Full semantic expression
 * typing remains in the type checker; this owner keeps the Type core focused
 * on construction, equality, assignability, and generic instantiation.
 */

#include "type_system.h"
#include "../parser/ast_api.h"

#include <stdlib.h>
#include <string.h>

Type *
type_infer_expression(const ASTNode *expr, TypeEnv *env)
{
    if (expr == NULL)
        return TYPE_UNKNOWN;

    switch (expr->type) {
    case AST_NUMBER:
        if (ast_number_is_long(expr))
            return TYPE_LONG;
        if (ast_number_is_float(expr))
            return TYPE_FLOAT;
        return ast_number_value(expr) == (int64_t)ast_number_value(expr)
            ? TYPE_INT
            : TYPE_FLOAT;

    case AST_STRING:
        return TYPE_STRING;

    case AST_BOOLEAN:
        return TYPE_BOOL;

    case AST_IDENTIFIER: {
        const char *expr_name = ast_identifier_name((ASTNode *)expr);
        Type *var_type = type_env_lookup_variable(env, expr_name);
        if (var_type != NULL)
            return var_type;

        Type *named_type = type_env_lookup_type(env, expr_name);
        if (named_type != NULL)
            return named_type;

        return TYPE_UNKNOWN;
    }

    case AST_TYPE: {
        Type *named_type = type_env_lookup_type(env, ast_type_name((ASTNode *)expr));
        return named_type != NULL ? named_type : TYPE_UNKNOWN;
    }

    case AST_MEMBER_ACCESS: {
        Type *object_type = type_infer_expression(ast_member_object(expr), env);
        if (object_type != NULL
            && strcmp(ast_member_name(expr), "Length") == 0
            && (type_equals(type_constructed_constructor(object_type), TYPE_ARRAY)
                || type_equals(type_constructed_constructor(object_type), TYPE_SLICE))) {
            return TYPE_INT;
        }
        return TYPE_UNKNOWN;
    }

    case AST_ARRAY_ACCESS: {
        Type *array_type = type_infer_expression(ast_array_access_array(expr), env);
        if (array_type != NULL
            && type_constructed_arg_count(array_type) >= 1) {
            return type_constructed_arg(array_type, 0);
        }
        return TYPE_UNKNOWN;
    }

    case AST_ASSIGNMENT:
        return type_infer_expression(ast_assignment_target(expr), env);

    case AST_BINARY: {
        Type *left = type_infer_expression(ast_binary_left(expr), env);
        Type *right = type_infer_expression(ast_binary_right(expr), env);
        PgyTokenType op = ast_binary_operator(expr).type;

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
        Type *operand = type_infer_expression(ast_unary_operand(expr), env);
        if (ast_unary_operator(expr).type == TOKEN_NOT)
            return TYPE_BOOL;
        return operand != NULL ? operand : TYPE_UNKNOWN;
    }

    case AST_CALL:
        if (ast_call_callee(expr) != NULL
            && ast_call_callee(expr)->type == AST_MEMBER_ACCESS
            && ast_member_object(ast_call_callee(expr)) != NULL
            && ast_member_name(ast_call_callee(expr)) != NULL
            && strcmp(ast_member_name(ast_call_callee(expr)), "Slice") == 0) {
            Type *receiver = type_infer_expression(
                ast_member_object(ast_call_callee(expr)), env);
            if (receiver != NULL
                && type_constructed_arg_count(receiver) >= 1
                && (type_equals(type_constructed_constructor(receiver), TYPE_ARRAY)
                    || type_equals(type_constructed_constructor(receiver), TYPE_SLICE))) {
                Type *slice_args[1] = { type_constructed_arg(receiver, 0) };
                return type_create_constructed(TYPE_SLICE, slice_args, 1);
            }
        }
        if (ast_call_callee(expr) != NULL
            && ast_call_callee(expr)->type == AST_IDENTIFIER) {
            const char *callee = ast_identifier_name(ast_call_callee(expr));
            ASTNode *first_arg = ast_call_argument(expr, 0);
            size_t arg_count = ast_call_arg_count(expr);

            if (strcmp(callee, "Read") == 0 && arg_count >= 1) {
                Type *slot_type = type_infer_expression(first_arg, env);
                if (slot_type != NULL && slot_type->kind == TYPE_KIND_SLOT)
                    return type_slot_inner_type(slot_type);
            }
            if ((strcmp(callee, "ClaimSlot") == 0 || strcmp(callee, "ClaimSecureSlot") == 0)
                && arg_count >= 1) {
                Type *inner = type_infer_expression(first_arg, env);
                if (inner != NULL && inner != TYPE_UNKNOWN)
                    return type_create_slot(inner, strcmp(callee, "ClaimSecureSlot") == 0);
            }
            if ((strcmp(callee, "Clone") == 0 || strcmp(callee, "RcClone") == 0)
                && arg_count >= 1)
                return type_infer_expression(first_arg, env);
            if (strcmp(callee, "RcDowngrade") == 0 && arg_count >= 1) {
                Type *rc_type = type_infer_expression(first_arg, env);
                if (type_constructed_is(rc_type, TYPE_RC, 1)) {
                    Type *weak_args[1] = { type_constructed_arg(rc_type, 0) };
                    return type_create_constructed(TYPE_WEAK, weak_args, 1);
                }
            }
            if (strcmp(callee, "WeakUpgrade") == 0 && arg_count >= 1) {
                Type *weak_type = type_infer_expression(first_arg, env);
                if (type_constructed_is(weak_type, TYPE_WEAK, 1)) {
                    Type *rc_args[1] = { type_constructed_arg(weak_type, 0) };
                    return type_create_constructed(TYPE_RC, rc_args, 1);
                }
            }
            if (strcmp(callee, "AllocatorSystem") == 0
                || strcmp(callee, "AllocatorTracing") == 0
                || strcmp(callee, "AllocatorDebug") == 0
                || strcmp(callee, "AllocatorScratch") == 0
                || strcmp(callee, "AllocatorResult") == 0
                || strcmp(callee, "AllocatorPersistent") == 0
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
        Type *inner = type_infer_expression(ast_await_expression(expr), env);
        if (inner != NULL
            && type_constructed_arg_count(inner) == 1) {
            return type_constructed_arg(inner, 0);
        }
        return TYPE_UNKNOWN;
    }

    case AST_SPAWN_EXPR: {
        Type *inner = type_infer_expression(ast_spawn_function(expr), env);
        Type *args[1] = { inner != NULL ? inner : TYPE_UNKNOWN };
        return type_create_constructed(TYPE_FUTURE, args, 1);
    }

    case AST_CHANNEL_RECV: {
        Type *channel_type = type_infer_expression(ast_channel_recv_channel(expr), env);
        if (channel_type != NULL
            && type_constructed_arg_count(channel_type) == 1) {
            return type_constructed_arg(channel_type, 0);
        }
        return TYPE_UNKNOWN;
    }

    case AST_CHANNEL_SEND:
    case AST_EVENT_INVOKE:
        return TYPE_VOID;

    case AST_LAMBDA_EXPR: {
        size_t param_count = ast_lambda_param_count(expr);
        Type **params = calloc(param_count == 0 ? 1 : param_count, sizeof(Type *));
        if (params == NULL)
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < param_count; i++) {
            params[i] = TYPE_UNKNOWN;
        }
        Type *return_type = ast_lambda_return_type(expr) != NULL
            ? type_infer_expression(ast_lambda_return_type(expr), env)
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
