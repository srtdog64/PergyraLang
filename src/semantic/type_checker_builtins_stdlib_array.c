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
#include "../common/pgy_builtin_type_table.h"

#include <string.h>

static bool
compiler_retire_array_storage_path_matches(const char *path,
                                           const char *suffix)
{
    size_t path_length;
    size_t suffix_length;

    if (path == NULL || suffix == NULL)
        return false;
    path_length = strlen(path);
    suffix_length = strlen(suffix);
    if (path_length < suffix_length)
        return false;
    for (size_t i = 0; i < suffix_length; i++) {
        char actual = path[path_length - suffix_length + i];
        char expected = suffix[i];
        if (actual == '\\')
            actual = '/';
        if (actual != expected)
            return false;
    }
    return path_length == suffix_length
        || path[path_length - suffix_length - 1] == '/'
        || path[path_length - suffix_length - 1] == '\\';
}

static bool
compiler_retire_array_storage_context_ready(SemanticContext *ctx)
{
    static const char routine_owner_path[] =
        "src/self_hosted/mir/routine_build_storage_lifetime_owner.pgy";
    static const char artifact_owner_path[] =
        "src/self_hosted/mir/ast_arena_storage_lifetime_owner.pgy";
    const PgyBuiltinInfo *builtin =
        pgy_builtin_lookup("CompilerRetireArrayStorage");
    ASTNode *function;
    FuncParam *param;
    ASTNode *return_type;
    const char *function_name;
    const char *param_type_name;
    const char *return_type_name;
    bool owner_contract_ready;

    if (ctx == NULL || builtin == NULL
        || (builtin->flags & PGY_BUILTIN_FLAG_COMPILER_INTERNAL) == 0)
        return false;
    function = ctx->current_function_decl;
    if (function == NULL
        || ast_declaration_name(function) == NULL
        || ast_func_param_count(function) != 1)
        return false;
    param = ast_func_param(function, 0);
    return_type = ast_func_return_type(function);
    if (param == NULL || param->mode != PARAM_MODE_OWN
        || param->type == NULL || ast_type_name(param->type) == NULL
        || return_type == NULL || ast_type_name(return_type) == NULL)
        return false;
    function_name = ast_declaration_name(function);
    param_type_name = ast_type_name(param->type);
    return_type_name = ast_type_name(return_type);
    owner_contract_ready =
        (strcmp(function_name,
                 "SelfMirRoutineBuildStorageRetireAfterLastConsumer") == 0
         && strcmp(param_type_name, "SelfMirRoutineBuild") == 0
         && strcmp(return_type_name, "Void") == 0
         && compiler_retire_array_storage_path_matches(
             ctx->current_module_path, routine_owner_path))
        ||
        (strcmp(function_name,
                "SelfMirAstArenaNonTraversalStorageRetireAfterDomainProjection") == 0
         && strcmp(param_type_name, "AstTreeArtifact") == 0
         && strcmp(return_type_name, "AstTreeArtifact") == 0
         && compiler_retire_array_storage_path_matches(
             ctx->current_module_path, artifact_owner_path))
        ||
        (strcmp(function_name,
                "SelfMirAstArenaTraversalStorageRetireAfterRoutineFacts") == 0
         && strcmp(param_type_name, "AstTreeArtifact") == 0
         && strcmp(return_type_name, "Void") == 0
         && compiler_retire_array_storage_path_matches(
             ctx->current_module_path, artifact_owner_path));
    return owner_contract_ready;
}

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
    if (kind == STDLIB_COLLECTION_ARRAY_PUSH ||
        kind == STDLIB_COLLECTION_ARRAY_PUSH_OWNED_STRING) {
        Type *arr;
        Type *val;
        const char *op_name = kind == STDLIB_COLLECTION_ARRAY_PUSH_OWNED_STRING
            ? "ArrayPushOwnedString" : "ArrayPush";
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        arr = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (reject_non_inout_param_collection_mutator_receiver(
                arg0, arr, op_name, "array", ctx))
            return TYPE_UNKNOWN;
        val = stdlib_array_normalize_type(
            type_check_expression(arg1, ctx));
        reject_borrowed_boundary_container_store(
            arg1, val, "array", op_name, ctx);
        if (!type_is_constructed_named(arr, "Array")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "%s requires Array<T>, got '%s'", op_name,
                type_name_or_unknown(arr));
        } else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (kind == STDLIB_COLLECTION_ARRAY_PUSH_OWNED_STRING &&
                (inner == NULL || !type_equals(inner, TYPE_STRING))) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                    PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                    PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                    "ArrayPushOwnedString requires Array<String>, got '%s'",
                    type_name_or_unknown(arr));
            } else if (inner != NULL)
                require_assignable(val, inner, arg1, ctx);
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_DROP_OWNED_STRINGS) {
        Type *arr;
        Type *inner;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arr = stdlib_array_normalize_type(type_check_expression(arg0, ctx));
        if (reject_non_inout_param_collection_mutator_receiver(
                arg0, arr, "ArrayDropOwnedStrings", "array", ctx))
            return TYPE_UNKNOWN;
        inner = type_is_constructed_named(arr, "Array")
            ? type_get_constructed_arg(arr, 0) : NULL;
        if (inner == NULL || !type_equals(inner, TYPE_STRING)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ArrayDropOwnedStrings requires Array<String>, got '%s'",
                type_name_or_unknown(arr));
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_COMPILER_RETIRE_ARRAY_STORAGE) {
        Type *arr;
        if (!compiler_retire_array_storage_context_ready(ctx)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                "CompilerRetireArrayStorage is restricted to self-host storage lifetime owners");
            return TYPE_UNKNOWN;
        }
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        if (arg0 == NULL || arg0->type != AST_IDENTIFIER) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE, arg0,
                "CompilerRetireArrayStorage requires one named owned Array<T> binding");
            return TYPE_UNKNOWN;
        }
        arr = stdlib_array_normalize_type(type_check_expression(arg0, ctx));
        if (reject_non_inout_param_collection_mutator_receiver(
                arg0, arr, "CompilerRetireArrayStorage", "array", ctx))
            return TYPE_UNKNOWN;
        if (!type_is_constructed_named(arr, "Array")) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "CompilerRetireArrayStorage requires Array<T>, got '%s'",
                type_name_or_unknown(arr));
            return TYPE_UNKNOWN;
        }
        if (!consume_array_storage_binding(arg0, ctx))
            return TYPE_UNKNOWN;
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
