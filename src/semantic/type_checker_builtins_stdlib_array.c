/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Array and Slice stdlib builtin type contracts.
 */

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
#include "type_checker_collection_policy.h"
#include "diag_codes.h"

static Type *
stdlib_array_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

Type *
type_check_stdlib_array_call(ASTNode *expr,
                             const char *name,
                             StdlibCollectionBuiltinKind kind,
                             SemanticContext *ctx)
{
    ASTNode *arg0 = ast_call_argument(expr, 0);
    ASTNode *arg1 = ast_call_argument(expr, 1);
    ASTNode *arg2 = ast_call_argument(expr, 2);

    if (kind == STDLIB_COLLECTION_ARRAY_LENGTH) {
        Type *arg;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arg = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (!type_is_constructed_named(arg, "Array")
            && !type_is_constructed_named(arg, "Slice")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ArrayLength requires Array<T> or Slice<T>, got '%s'",
                type_name_or_unknown(arg));
        }
        return TYPE_INT;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_PUSH) {
        Type *arr;
        Type *val;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        arr = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (reject_non_inout_param_collection_mutator_receiver(
                arg0, arr, "ArrayPush", "array", ctx))
            return TYPE_UNKNOWN;
        val = stdlib_array_normalize_type(
            type_check_expression(arg1, ctx));
        reject_borrowed_boundary_container_store(
            arg1, val, "array", "ArrayPush", ctx);
        if (!type_is_constructed_named(arr, "Array")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ArrayPush requires Array<T>, got '%s'",
                type_name_or_unknown(arr));
        } else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL)
                require_assignable(val, inner, arg1, ctx);
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_SET) {
        Type *arr;
        Type *val;
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        arr = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (reject_non_inout_param_collection_mutator_receiver(
                arg0, arr, "ArraySet", "array", ctx))
            return TYPE_UNKNOWN;
        require_assignable(
            type_check_expression(arg1, ctx),
            TYPE_INT, arg1, ctx);
        val = stdlib_array_normalize_type(
            type_check_expression(arg2, ctx));
        reject_borrowed_boundary_container_store(
            arg2, val, "array", "ArraySet", ctx);
        if (!type_is_constructed_named(arr, "Array")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ArraySet requires Array<T>, got '%s'",
                type_name_or_unknown(arr));
        } else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL)
                require_assignable(val, inner, arg2, ctx);
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_POP) {
        Type *arr;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arr = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (reject_non_inout_param_collection_mutator_receiver(
                arg0, arr, "ArrayPop", "array", ctx))
            return TYPE_UNKNOWN;
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ArrayPop requires Array<T>, got '%s'",
                type_name_or_unknown(arr));
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_SORT
        || kind == STDLIB_COLLECTION_ARRAY_REVERSE) {
        Type *arr;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arr = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (reject_non_inout_param_collection_mutator_receiver(
                arg0, arr, name, "array", ctx))
            return TYPE_UNKNOWN;
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "%s requires Array<T>, got '%s'", name,
                type_name_or_unknown(arr));
        return arr;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_MAP
        || kind == STDLIB_COLLECTION_ARRAY_FILTER) {
        Type *arr;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        arr = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "%s requires Array<T> as first argument, got '%s'",
                name, type_name_or_unknown(arr));
        semantic_record_effect(ctx, type_function_effects(stdlib_array_normalize_type(type_check_expression(arg1, ctx))));
        return arr;
    }
    if (kind == STDLIB_COLLECTION_SLICE_COPY) {
        Type *slice;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        slice = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (!type_is_constructed_named(slice, "Slice")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "SliceCopy requires Slice<T>, got '%s'",
                type_name_or_unknown(slice));
            return TYPE_UNKNOWN;
        }
        Type *inner = type_get_constructed_arg(slice, 0);
        if (inner == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE, arg0,
                "SliceCopy requires concrete Slice<T>, got '%s'",
                type_name_or_unknown(slice));
            return TYPE_UNKNOWN;
        }
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_ARRAY, args, 1);
    }

    return NULL;
}
