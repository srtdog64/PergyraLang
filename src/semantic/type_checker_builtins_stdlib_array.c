/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Array and Slice stdlib builtin type contracts.
 */

#include "formal_string_store_landing_owner.h"
#include "type_checker_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "array_storage_release_owner.h"
#include "array_storage_defer_release_owner.h"
#include "array_storage_deferred_preservation_owner.h"
#include "indexed_string_borrow_owner.h"
#include "loan_copy_landing_owner.h"
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

/* The ArrayPush call becomes ArrayPushOwnedString, by name and builtin kind. */
static bool
array_push_become_owned_string(ASTNode *expr)
{
    ASTNode *callee = ast_call_callee(expr);
    return callee != NULL && callee->type == AST_IDENTIFIER &&
        ast_replace_identifier_name_copy(callee, "ArrayPushOwnedString") &&
        ast_call_set_semantic_callee_builtin_kind(expr, (uint32_t)BUILTIN_ARRAY_PUSH_OWNED_STRING);
}

/* This is an internal lowering identity, not a source builtin. It lets every
 * backend select the copy-then-retire ABI without exposing cleanup ceremony. */
static bool
array_set_become_owned_string(ASTNode *expr)
{
    ASTNode *callee = ast_call_callee(expr);
    return callee != NULL && callee->type == AST_IDENTIFIER
        && ast_replace_identifier_name_copy(callee, "ArraySetOwnedString")
        && ast_call_set_compiler_builtin(expr)
        && ast_call_set_semantic_callee_builtin_kind(
            expr, (uint32_t)BUILTIN_NOT_BUILTIN);
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
        bool member_value_copy_landed =
            kind == STDLIB_COLLECTION_ARRAY_PUSH
            && semantic_formal_string_store_landing_member_value_copy_selected(
                expr, ctx);
        /* This typed plan owns both parts of one transition: first make the
         * borrowed String independent, then store that copy in the moved
         * member as an owned element.  Neither a value-only copy nor an
         * owned-store-only replay is a sound substitute. */
        if (member_value_copy_landed) {
            /* Keep the exact planned call for post-body revalidation.  The
             * finalizer owns the effect receipt because the current call row
             * is not sealed until this builtin check returns. */
            if (!semantic_formal_string_store_landing_record_member_value_copy_candidate(
                    ctx, expr)
                || !semantic_formal_string_store_landing(
                    expr, 0, 1, ctx)) {
                semantic_error(ctx, expr,
                    "Aggregate member String copy landing could not be recorded");
                return TYPE_UNKNOWN;
            }
        }
        bool owned_push_landed = kind == STDLIB_COLLECTION_ARRAY_PUSH
            && (semantic_formal_string_store_landing_owned_push(expr, ctx)
                || member_value_copy_landed);
        bool owned_push_parameter_receiver = false;
        bool owned_push_parameter_admitted = owned_push_landed
            && semantic_collection_owned_string_parameter_push_admit(
                expr, arg0, ctx, &owned_push_parameter_receiver);
        if (owned_push_landed && owned_push_parameter_receiver
            && !owned_push_parameter_admitted) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, arg0,
                "An owned String push into an Array<String> parameter has no current caller element-ownership proof");
            return TYPE_UNKNOWN;
        }
        if (owned_push_landed) {
            if (!array_push_become_owned_string(expr)) {
                semantic_error(ctx, expr, "Owned String push landing could not be recorded");
                return TYPE_UNKNOWN;
            }
            kind = STDLIB_COLLECTION_ARRAY_PUSH_OWNED_STRING;
            op_name = "ArrayPushOwnedString";
        }
        if (kind == STDLIB_COLLECTION_ARRAY_PUSH
            && !semantic_array_storage_admit_mutation(arg0, ctx))
            return TYPE_UNKNOWN;
        if (kind == STDLIB_COLLECTION_ARRAY_PUSH_OWNED_STRING
            && !semantic_collection_admit_owned_string_push(arg0,
                (owned_push_landed
                    && semantic_formal_string_store_landing_owned_array_plan_current(
                        ctx, expr))
                    || owned_push_parameter_admitted, ctx))
            return TYPE_UNKNOWN;
        if (kind == STDLIB_COLLECTION_ARRAY_PUSH &&
            !semantic_formal_string_store_landing(expr, 0, 1, ctx)) {
            semantic_error(ctx, expr, "Owned String store landing could not be recorded");
            return TYPE_UNKNOWN;
        }
        /* Landing may replace the stored expression with a compiler-owned
         * Clone.  Publish and validate that new call, not the retired child
         * pointer captured before the rewrite. */
        arg1 = ast_call_argument(expr, 1);
        val = stdlib_array_normalize_type(
            type_check_expression(arg1, ctx));
        bool value_copy_landed = false;
        bool copy_landing = false;
        if (kind == STDLIB_COLLECTION_ARRAY_PUSH && type_equals(val, TYPE_STRING)) {
            value_copy_landed =
                semantic_formal_string_store_landing_value_copy(expr, ctx);
            semantic_indexed_string_borrow_push_argument(arg1, expr, ctx, &copy_landing);
        }
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
                uint32_t owned_source = 0;
                bool landed_independent_copy =
                    kind == STDLIB_COLLECTION_ARRAY_PUSH
                    && (semantic_formal_string_store_landing_value_copy(
                            expr, ctx)
                        || semantic_loan_copy_landing_satisfied(ctx, arg1));
                ASTNode *landed_copy_source = landed_independent_copy
                    && arg1 != NULL && arg1->type == AST_CALL
                    && ast_call_arg_count(arg1) == 1
                    ? ast_call_argument(arg1, 0) : NULL;
                bool landed_named_copy = landed_copy_source != NULL
                    && landed_copy_source->type == AST_IDENTIFIER;
                bool landed_named_receiver_admitted = landed_named_copy
                    && arg0->type == AST_IDENTIFIER
                    && semantic_collection_owned_string_push_admissible(
                        arg0, ctx);
                bool source_owned_result =
                    kind == STDLIB_COLLECTION_ARRAY_PUSH
                    && semantic_collection_owned_string_call_result(
                        arg1, ctx, &owned_source)
                    && owned_source != 0;
                uint32_t pending_owned_source = 0;
                bool pending_owned_result =
                    kind == STDLIB_COLLECTION_ARRAY_PUSH
                    && !source_owned_result
                    && semantic_collection_owned_string_result_candidate(
                        arg1, ctx, &pending_owned_source)
                    && pending_owned_source != 0;
                if (kind == STDLIB_COLLECTION_ARRAY_PUSH
                    && (!source_owned_result
                        || (landed_independent_copy
                            && !landed_named_receiver_admitted))
                    && !pending_owned_result
                    && semantic_collection_reject_unsafe_owned_string_mutation(
                        arg0, op_name, ctx)) {
                    /* A fresh owned result transfers through the ordinary
                     * push. A compiler-landed Clone of a direct named local
                     * is the same exact owned producer, so its source receipt
                     * remains the authority instead of refining this call
                     * into a second store contract. A member receiver still
                     * needs its separate exact member proof. */
                    if (!semantic_formal_string_store_landing_select_owned(
                            ctx, expr)) {
                        semantic_error(ctx, expr,
                            "Owned String push landing selection failed");
                    }
                    return TYPE_UNKNOWN;
                }
                if (kind == STDLIB_COLLECTION_ARRAY_PUSH && copy_landing
                    && !value_copy_landed && !pending_owned_result) {
                    /* The push stores an owned copy of a String no owner proves
                     * alive for the receiver. A repeated walk must select the
                     * same landing before changing the builtin effect. */
                    if (ast_call_semantic_collection_effect(
                            expr, NULL, NULL, NULL)) {
                        bool selected =
                            semantic_formal_string_store_landing_select_owned(
                                ctx, expr);
                        if (selected)
                            semantic_loan_copy_landing_error_with_hints(ctx,
                                PGY_CODE_SEM_BORROW_ESCAPE,
                                PGY_CAUSE_BORROW_ESCAPE,
                                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, arg1,
                                "A String an earlier loop walk pushed as borrowed now needs an owned copy");
                        else {
                            semantic_error(ctx, arg1,
                                "Owned String push landing selection failed");
                        }
                        return TYPE_UNKNOWN;
                    }
                    bool landing_parameter_receiver = false;
                    bool landing_parameter_admitted =
                        semantic_collection_owned_string_parameter_push_admit(
                            expr, arg0, ctx, &landing_parameter_receiver);
                    if (landing_parameter_receiver) {
                        if (!landing_parameter_admitted) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_BORROW_ESCAPE,
                                PGY_CAUSE_BORROW_ESCAPE,
                                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, arg0,
                                "An owned String push into an Array<String> parameter has no current caller element-ownership proof");
                            return TYPE_UNKNOWN;
                        }
                        if (!semantic_formal_string_store_landing_select_owned(
                                ctx, expr)) {
                            semantic_error(ctx, expr,
                                "Owned String push landing selection failed");
                            return TYPE_UNKNOWN;
                        }
                        semantic_loan_copy_landing_error_with_hints(ctx,
                            PGY_CODE_SEM_BORROW_ESCAPE,
                            PGY_CAUSE_BORROW_ESCAPE,
                            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, arg1,
                            "A String stored through an Array<String> parameter is replayed as one owned push");
                        return TYPE_UNKNOWN;
                    }
                    if (!semantic_collection_admit_owned_string_push(
                            arg0, false, ctx)) {
                        semantic_formal_string_store_landing_select_value_copy(
                            ctx, expr);
                        return TYPE_UNKNOWN;
                    }
                    if (!array_push_become_owned_string(expr)) {
                        semantic_error(ctx, expr,
                            "Owned String push landing could not be recorded");
                        return TYPE_UNKNOWN;
                    }
                    kind = STDLIB_COLLECTION_ARRAY_PUSH_OWNED_STRING;
                    op_name = "ArrayPushOwnedString";
                }
                bool parameter_receiver = false;
                bool parameter_admitted = false;
                bool member_admitted = false;
                if (kind == STDLIB_COLLECTION_ARRAY_PUSH_OWNED_STRING
                    || source_owned_result) {
                    parameter_admitted =
                        semantic_collection_owned_string_parameter_push_admit(
                            expr, arg0, ctx, &parameter_receiver);
                    if (parameter_receiver && !parameter_admitted) {
                        if (!ctx->has_error) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_BORROW_ESCAPE,
                                PGY_CAUSE_BORROW_ESCAPE,
                                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, arg0,
                                "An owned String push into an Array<String> "
                                "parameter has no current caller "
                                "element-ownership proof");
                        }
                        return TYPE_UNKNOWN;
                    }
                    member_admitted = !parameter_receiver
                        && semantic_collection_owned_element_member_push_provisional(
                            arg0, ctx);
                }
                bool general_owned_result =
                    kind == STDLIB_COLLECTION_ARRAY_PUSH
                    && source_owned_result
                    && (parameter_admitted || member_admitted
                        || (!parameter_receiver
                            && (!landed_independent_copy
                                || landed_named_receiver_admitted)));
                if (pending_owned_result
                    && !semantic_array_storage_record_pending_owned_string_result(
                        ctx, expr, arg0, arg1, pending_owned_source)) {
                    semantic_error(ctx, expr,
                        "Could not defer an exact pending owned String result");
                    return TYPE_UNKNOWN;
                }
                PgyCollectionOwnershipEffectKind effect =
                    kind == STDLIB_COLLECTION_ARRAY_PUSH_OWNED_STRING ||
                        general_owned_result || pending_owned_result
                        ? PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                        : PGY_COLLECTION_EFFECT_SHALLOW_MUTATION;
                bool effect_current = member_value_copy_landed
                    ? true
                    : semantic_collection_record_call_effect_from(
                        expr, arg0, effect,
                        general_owned_result ? owned_source
                            : pending_owned_result ? pending_owned_source : 0,
                        ctx);
                if (!effect_current) {
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
            || !semantic_array_storage_record_defer_release(arg0, false, ctx)
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
        if (!semantic_indexed_string_borrow_owner_transfer_allowed(arg0, ctx)
            || !semantic_collection_admit_owned_string_drop(arg0, ctx)
            || !semantic_array_storage_record_defer_release(arg0, true, ctx))
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
        } else {
            if (!consume_array_storage_binding(arg0, ctx))
                return TYPE_UNKNOWN;
            semantic_indexed_string_borrow_invalidate_deep_drop(arg0, ctx);
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
        /* This compiler-internal boundary retires the same exact Array
         * storage as ArrayDrop.  Publish that release on the own formal
         * before consuming the local symbol so detached-member handoffs can
         * validate the callee's final responsibility from the existing
         * owned-element requirement owner. */
        if (!semantic_collection_owned_element_requirement_record_storage_drop(
                expr, arg0, ctx)
            || !consume_array_storage_binding(arg0, ctx))
            return TYPE_UNKNOWN;
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_SET
        || kind == STDLIB_COLLECTION_ARRAY_SET_OWNED_STRING) {
        Type *arr;
        Type *val;
        bool owned_set = kind ==
            STDLIB_COLLECTION_ARRAY_SET_OWNED_STRING;
        const char *op_name = owned_set
            ? "ArraySetOwnedString" : "ArraySet";
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        if (owned_set && !ast_call_compiler_builtin(expr)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                expr,
                "ArraySetOwnedString is a compiler-owned lowering and cannot be called from source");
            return TYPE_UNKNOWN;
        }
        arr = stdlib_array_normalize_type(
            type_check_expression(arg0, ctx));
        if (reject_invalid_array_mutator_receiver(
                arg0, arr, op_name, ctx))
            return TYPE_UNKNOWN;
        if (!semantic_array_storage_admit_mutation(arg0, ctx))
            return TYPE_UNKNOWN;
        bool inout_set_receiver =
            semantic_formal_string_store_landing_inout_set_receiver_current(
                expr, ctx);
        bool landed_inout_owned_set = inout_set_receiver
            && semantic_formal_string_store_landing_inout_owned_set_current(
                expr, ctx);
        /* Revalidate a prior local owned-set plan even when its receiver
         * premise is temporarily absent. The landing owner can then reselect
         * the exact definition-birth copy on which that plan depends; the
         * fixed-point replay remains fail-closed until the premise returns. */
        bool landed_local_owned_set = !inout_set_receiver
            && semantic_formal_string_store_landing_owned_set(expr, ctx);
        if (!owned_set
            && (landed_inout_owned_set
                || (!inout_set_receiver
                    && (landed_local_owned_set
                        || semantic_collection_owned_string_set_required(
                            arg0, ctx))))) {
            if (!landed_inout_owned_set
                && !landed_local_owned_set) {
                if (!semantic_formal_string_store_landing_select_owned_set(
                        ctx, expr))
                    semantic_error(ctx, expr,
                        "Owned String replacement landing selection failed");
                (void)type_check_expression(arg1, ctx);
                (void)type_check_expression(arg2, ctx);
                return TYPE_UNKNOWN;
            }
            if (!array_set_become_owned_string(expr)) {
                semantic_error(ctx, expr,
                    "Owned String replacement landing could not be recorded");
                return TYPE_UNKNOWN;
            }
            kind = STDLIB_COLLECTION_ARRAY_SET_OWNED_STRING;
            owned_set = true;
            op_name = "ArraySetOwnedString";
        }
        if (!owned_set
            && semantic_collection_reject_unsafe_owned_string_mutation(
                arg0, op_name, ctx)) {
            (void)type_check_expression(arg1, ctx);
            (void)type_check_expression(arg2, ctx);
            return TYPE_UNKNOWN;
        }
        if (owned_set && !landed_inout_owned_set
            && !semantic_collection_admit_owned_string_set(arg0, ctx))
            return TYPE_UNKNOWN;
        require_assignable(
            type_check_expression(arg1, ctx),
            TYPE_INT, arg1, ctx);
        if (!owned_set
            && !semantic_formal_string_store_landing(expr, 0, 2, ctx)) {
            semantic_error(ctx, expr, "Owned String store landing could not be recorded");
            return TYPE_UNKNOWN;
        }
        arg2 = ast_call_argument(expr, 2);
        val = stdlib_array_normalize_type(
            type_check_expression(arg2, ctx));
        bool copy_wanted = false;
        if (!owned_set && type_equals(val, TYPE_STRING))
            semantic_indexed_string_borrow_set_argument(arg2, expr, ctx, &copy_wanted);
        /* A borrowed-element ArraySet still needs a value landing when its
         * input outlives no owner. The owned operation copies internally. */
        if (copy_wanted &&
            !semantic_formal_string_store_landing_select_checked(ctx, expr, 2)) {
            semantic_error(ctx, expr,
                "Owned String store copy landing could not be recorded");
            return TYPE_UNKNOWN;
        }
        if (!owned_set)
            reject_borrowed_boundary_container_store(
                arg2, val, "array", op_name, ctx);
        if (!type_is_constructed_named(arr, "Array")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "%s requires Array<T>, got '%s'", op_name,
                type_name_or_unknown(arr));
        } else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (owned_set
                && (inner == NULL || !type_equals(inner, TYPE_STRING))) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                    PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                    PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                    arg0,
                    "ArraySetOwnedString requires Array<String>, got '%s'",
                    type_name_or_unknown(arr));
            }
            if (inner != NULL)
                require_assignable(val, inner, arg2, ctx);
            if (inner != NULL && type_equals(inner, TYPE_STRING)
                && !semantic_collection_record_call_effect(
                    expr, arg0, owned_set
                        ? PGY_COLLECTION_EFFECT_OWNED_STRING_SET
                        : PGY_COLLECTION_EFFECT_SHALLOW_MUTATION,
                    ctx)) {
                semantic_error(ctx, expr,
                    "Could not seal Array<String> ownership transition receipt");
                return TYPE_UNKNOWN;
            }
            if (owned_set && inner != NULL
                && type_equals(inner, TYPE_STRING))
                semantic_indexed_string_borrow_invalidate_owned_set(
                    arg0, ctx);
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_POP) {
        Type *arr;
        bool owned_string_pop = false;
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
        if (!semantic_array_storage_admit_mutation(arg0, ctx))
            return TYPE_UNKNOWN;
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ArrayPop requires Array<T>, got '%s'",
                type_name_or_unknown(arr));
        else {
            Type *inner = type_get_constructed_arg(arr, 0);
            bool string_elements = inner != NULL
                && type_equals(inner, TYPE_STRING);
            bool parameter_receiver = false;
            bool parameter_admitted = string_elements
                && semantic_collection_owned_string_parameter_pop_admit(
                    expr, arg0, ctx, &parameter_receiver);
            if (string_elements && parameter_receiver
                && !parameter_admitted) {
                if (!ctx->has_error)
                    semantic_error_with_hints(ctx,
                        PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE,
                        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, arg0,
                        "An owned String pop through an Array<String> parameter has no current caller element-ownership proof");
                return TYPE_UNKNOWN;
            }
            bool local_owned_pop = string_elements
                && semantic_collection_owned_string_pop_required(arg0, ctx);
            owned_string_pop = parameter_admitted || local_owned_pop;
            if (owned_string_pop) {
                if (local_owned_pop
                    && !semantic_collection_admit_owned_string_pop(arg0, ctx))
                    return TYPE_UNKNOWN;
            } else if (semantic_collection_reject_unsafe_owned_string_mutation(
                    arg0, "ArrayPop", ctx)) {
                return TYPE_UNKNOWN;
            }
            if (inner != NULL && type_equals(inner, TYPE_STRING)
                && !semantic_collection_record_call_effect(
                    expr, arg0, owned_string_pop
                        ? PGY_COLLECTION_EFFECT_OWNED_STRING_POP
                        : PGY_COLLECTION_EFFECT_SHALLOW_MUTATION,
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
        if (!semantic_array_storage_admit_mutation(arg0, ctx))
            return TYPE_UNKNOWN;
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "%s requires Array<T>, got '%s'", name,
                type_name_or_unknown(arr));
        /* The result is the receiver: its relation reads this type name. */
        else if (arr->name != NULL && !ast_call_set_semantic_value_type_name_copy(expr, arr->name))
            semantic_error(ctx, expr, "Array permutation result type snapshot allocation failed");
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
