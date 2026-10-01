/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Array and Slice stdlib builtin type contracts.
 */

#include "type_checker_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "array_storage_release_owner.h"
#include "type_checker_builtins_internal.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
#include "type_checker_collection_policy.h"
#include "type_checker_ownership_support_internal.h"
#include "diag_codes.h"
#include "../common/pgy_builtin_type_table.h"

#include <string.h>

static bool
compiler_internal_builtin_path_matches(const char *path,
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
compiler_internal_builtin_type_text_match_at(const ASTNode *type_node,
                                             const char **expected_cursor)
{
    const char *base_name;
    const char *cursor;
    GenericParams *generic_args;
    size_t generic_count;

    if (type_node == NULL || type_node->type != AST_TYPE
        || expected_cursor == NULL || *expected_cursor == NULL)
        return false;
    base_name = ast_type_name(type_node);
    cursor = *expected_cursor;
    if (base_name == NULL || strncmp(cursor, base_name, strlen(base_name)) != 0)
        return false;
    cursor += strlen(base_name);

    generic_args = ast_type_generic_args(type_node);
    generic_count = ast_generic_param_count(generic_args);
    if (generic_count == 0) {
        *expected_cursor = cursor;
        return true;
    }
    if (*cursor != '<')
        return false;
    cursor++;
    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *arg = ast_generic_param_at(generic_args, i);
        ASTNode *arg_type = ast_generic_param_constraint(arg);
        const char *arg_name = ast_generic_param_name(arg);

        if (i > 0) {
            if (*cursor != ',')
                return false;
            cursor++;
            if (*cursor == ' ')
                cursor++;
        }
        if (arg_type != NULL) {
            if (!compiler_internal_builtin_type_text_match_at(
                    arg_type, &cursor))
                return false;
        } else {
            size_t arg_name_length;
            if (arg_name == NULL)
                return false;
            arg_name_length = strlen(arg_name);
            if (strncmp(cursor, arg_name, arg_name_length) != 0)
                return false;
            cursor += arg_name_length;
        }
    }
    if (*cursor != '>')
        return false;
    *expected_cursor = cursor + 1;
    return true;
}

static bool
compiler_internal_builtin_type_matches(const ASTNode *type_node,
                                       const char *expected)
{
    const char *cursor = expected;

    return compiler_internal_builtin_type_text_match_at(type_node, &cursor)
        && cursor != NULL && *cursor == '\0';
}

static bool
compiler_retire_array_storage_context_ready(SemanticContext *ctx)
{
    const PgyBuiltinInfo *builtin =
        pgy_builtin_lookup("CompilerRetireArrayStorage");
    ASTNode *function;
    FuncParam *param;
    ASTNode *return_type;
    const char *function_name;

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
#define PGY_COMPILER_INTERNAL_PARAM_OWN PARAM_MODE_OWN
#define PGY_COMPILER_INTERNAL_BUILTIN_CALLER(                              \
    registry_builtin, module_path, caller_name, parameter_mode,            \
    parameter_type, caller_return_type)                                    \
    if (strcmp(builtin->name, (registry_builtin)) == 0                     \
        && strcmp(function_name, (caller_name)) == 0                       \
        && param->mode == (parameter_mode)                                 \
        && compiler_internal_builtin_type_matches(                         \
            param->type, (parameter_type))                                 \
        && compiler_internal_builtin_type_matches(                         \
            return_type, (caller_return_type))                             \
        && compiler_internal_builtin_path_matches(                         \
            ctx->current_module_path, (module_path)))                      \
        return true;
#include "../common/compiler_internal_builtin_caller_registry.def"
#undef PGY_COMPILER_INTERNAL_BUILTIN_CALLER
#undef PGY_COMPILER_INTERNAL_PARAM_OWN
    return false;
}

static Type *
stdlib_array_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static bool
reject_array_storage_invalidation_with_live_slice(ASTNode *receiver,
                                                  const char *operation,
                                                  SemanticContext *ctx)
{
    Symbol *slice_sym;
    const char *place_path;

    if (receiver == NULL || (receiver->type != AST_IDENTIFIER
            && receiver->type != AST_MEMBER_ACCESS))
        return false;
    slice_sym = semantic_find_active_slice_borrow_for_array_place(
        ctx->scope, receiver, ctx);
    if (slice_sym == NULL)
        return false;
    place_path = semantic_assignment_target_path_scratch(receiver, ctx);
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_REDUCE_SCOPE_OR_RETRY, receiver,
        "%s cannot change Array storage for '%s' while Slice '%s' is live.\n"
        "Reason:\n- Slice<T> is a borrowed write-through view into the Array backing storage\n"
        "- growing, shrinking, retiring, or replacing that storage would invalidate the view\n"
        "Fix:\n- perform the storage-changing operation before creating the Slice\n"
        "- or end the Slice's lexical scope before changing '%s'",
        operation, place_path, slice_sym->name, place_path);
    return true;
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
        if (reject_invalid_array_mutator_receiver(
                arg0, arr, op_name, ctx))
            return TYPE_UNKNOWN;
        if (reject_array_storage_invalidation_with_live_slice(
                arg0, op_name, ctx))
            return TYPE_UNKNOWN;
        if (kind == STDLIB_COLLECTION_ARRAY_PUSH
            && semantic_collection_reject_unsafe_owned_string_mutation(
                arg0, op_name, ctx))
            return TYPE_UNKNOWN;
        if (kind == STDLIB_COLLECTION_ARRAY_PUSH_OWNED_STRING
            && !semantic_collection_admit_owned_string_push(arg0, ctx))
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
            if (inner != NULL && type_equals(inner, TYPE_STRING)) {
                uint32_t owned_result_source = 0;
                PgyCollectionOwnershipEffectKind effect =
                    kind == STDLIB_COLLECTION_ARRAY_PUSH_OWNED_STRING
                        ? PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                        : PGY_COLLECTION_EFFECT_SHALLOW_MUTATION;
                if (kind == STDLIB_COLLECTION_ARRAY_PUSH
                    && semantic_collection_owned_string_call_result(
                        arg1, ctx, &owned_result_source)) {
                    effect = PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH;
                }
                if (!semantic_collection_record_call_effect_from(
                        expr, arg0, effect, owned_result_source, ctx)) {
                semantic_error(ctx, expr,
                    "Could not seal Array<String> ownership transition receipt");
                return TYPE_UNKNOWN;
                }
            }
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_DROP_STORAGE) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = stdlib_array_normalize_type(type_check_expression(arg0, ctx));
        if (arr == TYPE_UNKNOWN || reject_array_storage_invalidation_with_live_slice(
                arg0, "ArrayDrop", ctx)
            || !semantic_array_storage_admit_drop(arg0, arr, ctx)
            || !semantic_collection_owned_element_requirement_record_storage_drop(expr, arg0, ctx))
            return TYPE_UNKNOWN;
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_DROP_OWNED_STRINGS) {
        Type *arr;
        Type *inner;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arr = stdlib_array_normalize_type(type_check_expression(arg0, ctx));
        if (reject_invalid_array_mutator_receiver(
                arg0, arr, "ArrayDropOwnedStrings", ctx))
            return TYPE_UNKNOWN;
        if (reject_array_storage_invalidation_with_live_slice(
                arg0, "ArrayDropOwnedStrings", ctx))
            return TYPE_UNKNOWN;
        if (!semantic_collection_admit_owned_string_drop(arg0, ctx))
            return TYPE_UNKNOWN;
        inner = type_is_constructed_named(arr, "Array")
            ? type_get_constructed_arg(arr, 0) : NULL;
        if (inner == NULL || !type_equals(inner, TYPE_STRING)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ArrayDropOwnedStrings requires Array<String>, got '%s'",
                type_name_or_unknown(arr));
        } else if (!semantic_collection_record_call_effect(
                       expr, arg0, PGY_COLLECTION_EFFECT_DROP, ctx)) {
            semantic_error(ctx, expr,
                "Could not seal Array<String> ownership transition receipt");
            return TYPE_UNKNOWN;
        } else if (!semantic_collection_owned_element_requirement_record_deep_drop(
                       expr, arg0, arr, ctx)) {
            return TYPE_UNKNOWN;
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
        if (reject_array_storage_invalidation_with_live_slice(
                arg0, "CompilerRetireArrayStorage", ctx))
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
        if (reject_invalid_array_mutator_receiver(
                arg0, arr, "ArraySet", ctx))
            return TYPE_UNKNOWN;
        if (semantic_collection_reject_unsafe_owned_string_mutation(
                arg0, "ArraySet", ctx))
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
            if (inner != NULL && type_equals(inner, TYPE_STRING)
                && !semantic_collection_record_call_effect(
                    expr, arg0, PGY_COLLECTION_EFFECT_SHALLOW_MUTATION,
                    ctx)) {
                semantic_error(ctx, expr,
                    "Could not seal Array<String> ownership transition receipt");
                return TYPE_UNKNOWN;
            }
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_POP) {
        Type *arr;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arr = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (reject_invalid_array_mutator_receiver(
                arg0, arr, "ArrayPop", ctx))
            return TYPE_UNKNOWN;
        if (reject_array_storage_invalidation_with_live_slice(
                arg0, "ArrayPop", ctx))
            return TYPE_UNKNOWN;
        if (semantic_collection_reject_unsafe_owned_string_mutation(
                arg0, "ArrayPop", ctx))
            return TYPE_UNKNOWN;
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ArrayPop requires Array<T>, got '%s'",
                type_name_or_unknown(arr));
        else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL && type_equals(inner, TYPE_STRING)
                && !semantic_collection_record_call_effect(
                    expr, arg0, PGY_COLLECTION_EFFECT_SHALLOW_MUTATION,
                    ctx)) {
                semantic_error(ctx, expr,
                    "Could not seal Array<String> ownership transition receipt");
                return TYPE_UNKNOWN;
            }
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_SORT
        || kind == STDLIB_COLLECTION_ARRAY_REVERSE) {
        Type *arr;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arr = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (reject_invalid_array_mutator_receiver(
                arg0, arr, name, ctx))
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
