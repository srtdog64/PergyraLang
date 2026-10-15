/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker — HashMap stdlib builtin dispatch.
 */

#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "type_checker_collection_policy.h"
#include "diag_codes.h"
#include "typed_map_query_fact_owner.h"
#include "function_param_publication_owner.h"

typedef enum StdlibMapBuiltinKind {
    STDLIB_MAP_BUILTIN_UNKNOWN = 0,
    STDLIB_MAP_BUILTIN_NEW,
    STDLIB_MAP_BUILTIN_SET,
    STDLIB_MAP_BUILTIN_GET,
    STDLIB_MAP_BUILTIN_HAS,
    STDLIB_MAP_BUILTIN_REMOVE,
    STDLIB_MAP_BUILTIN_SIZE,
    STDLIB_MAP_BUILTIN_KEYS
} StdlibMapBuiltinKind;

typedef struct StdlibMapBuiltinSpec {
    const char *name;
    StdlibMapBuiltinKind kind;
} StdlibMapBuiltinSpec;

static int
stdlib_map_builtin_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const StdlibMapBuiltinSpec *spec = (const StdlibMapBuiltinSpec *)entry;

    return strcmp(name, spec->name);
}

static Type *
stdlib_map_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static ASTNode *
stdlib_map_arg(ASTNode *expr, size_t index)
{
    return ast_call_argument(expr, index);
}

static StdlibMapBuiltinKind
stdlib_map_builtin_kind(const char *name)
{
    static const StdlibMapBuiltinSpec specs[] = {
        { "MapGet", STDLIB_MAP_BUILTIN_GET },
        { "MapHas", STDLIB_MAP_BUILTIN_HAS },
        { "MapKeys", STDLIB_MAP_BUILTIN_KEYS },
        { "MapNew", STDLIB_MAP_BUILTIN_NEW },
        { "MapRemove", STDLIB_MAP_BUILTIN_REMOVE },
        { "MapSet", STDLIB_MAP_BUILTIN_SET },
        { "MapSize", STDLIB_MAP_BUILTIN_SIZE }
    };
    const StdlibMapBuiltinSpec *match;

    if (name == NULL)
        return STDLIB_MAP_BUILTIN_UNKNOWN;
    match = (const StdlibMapBuiltinSpec *)bsearch(
        &name, specs, sizeof(specs) / sizeof(specs[0]), sizeof(specs[0]),
        stdlib_map_builtin_compare);
    return match != NULL ? match->kind : STDLIB_MAP_BUILTIN_UNKNOWN;
}

static bool
stdlib_map_builtin_mutates_storage(StdlibMapBuiltinKind kind)
{
    return kind == STDLIB_MAP_BUILTIN_SET
        || kind == STDLIB_MAP_BUILTIN_REMOVE;
}

static bool
stdlib_map_reject_parallel_mutation(ASTNode *expr,
                                    const char *name,
                                    SemanticContext *ctx,
                                    StdlibMapBuiltinKind kind)
{
    if (ctx == NULL || !ctx->in_parallel)
        return false;
    if (!stdlib_map_builtin_mutates_storage(kind))
        return false;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SLOT_CONFLICT,
        PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT,
        PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
        "Parallel context does not permit map mutator '%s'.\n"
        "Reason:\n"
        "- HashMap storage may rehash while another task observes it\n"
        "- generated C/LLVM workers cannot safely share mutable map storage by raw pointer\n"
        "Fix:\n"
        "- mutate the map before or after parallel\n"
        "- or send immutable values through a channel/result boundary",
        name != NULL ? name : "<map builtin>");
    return true;
}

static void
report_unsupported_map_key(ASTNode *expr, const char *name, Type *map_type,
                           SemanticContext *ctx)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
        stdlib_map_arg(expr, 0),
        "%s currently supports only %s, got '%s'",
        name,
        type_checker_hashmap_type_policy_text(),
        map_type->name != NULL ? map_type->name : "<type>");
}

static bool
stdlib_map_key_supported(Type *key_type)
{
    return type_checker_hashmap_key_supported(key_type);
}

static void
report_expected_hashmap(ASTNode *expr, const char *name, Type *map_type,
                        SemanticContext *ctx)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
        stdlib_map_arg(expr, 0),
        "%s expects HashMap<K, T> as first argument, got '%s'",
        name, map_type->name != NULL ? map_type->name : "<type>");
}

Type *
type_check_stdlib_map_call(ASTNode *expr, const char *name,
                           SemanticContext *ctx, bool *handled_out)
{
    StdlibMapBuiltinKind kind = stdlib_map_builtin_kind(name);

    if (kind == STDLIB_MAP_BUILTIN_UNKNOWN) {
        if (handled_out != NULL)
            *handled_out = false;
        return TYPE_UNKNOWN;
    }

    if (handled_out != NULL)
        *handled_out = true;

    /* The existing AST slot owns result text through lowering. A failed
     * recheck (including arity/receiver/key failure) cannot retain old V. */
    if (kind == STDLIB_MAP_BUILTIN_GET
        && !ast_call_clear_semantic_value_type_name(expr)) {
        semantic_error(ctx, expr, "Typed MapGet result snapshot reset failed");
        return TYPE_UNKNOWN;
    }

    if (stdlib_map_reject_parallel_mutation(expr, name, ctx, kind))
        return TYPE_UNKNOWN;

    semantic_record_builtin_effect(ctx, expr, name);

    if (kind == STDLIB_MAP_BUILTIN_NEW) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN; /* type resolved from let annotation */
    }
    if (kind == STDLIB_MAP_BUILTIN_SET) {
        Type *map_type;
        Type *key_type;
        Type *value_type;
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        map_type = stdlib_map_normalize_type(
            type_check_expression(stdlib_map_arg(expr, 0), ctx));
        if (reject_non_inout_param_collection_mutator_receiver(
                stdlib_map_arg(expr, 0), map_type, "MapSet", "map", ctx))
            return TYPE_UNKNOWN;
        key_type = stdlib_map_normalize_type(
            type_check_expression(stdlib_map_arg(expr, 1), ctx));
        value_type = stdlib_map_normalize_type(
            type_check_expression(stdlib_map_arg(expr, 2), ctx));
        reject_borrowed_boundary_container_store(
            stdlib_map_arg(expr, 2), value_type, "map", "MapSet", ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && type_constructed_arg_count(map_type) == 2) {
            Type *expected_key = type_constructed_arg(map_type, 0);
            require_assignable(key_type, expected_key,
                stdlib_map_arg(expr, 1), ctx);
            require_assignable(value_type, type_constructed_arg(map_type, 1),
                stdlib_map_arg(expr, 2), ctx);
            if (!stdlib_map_key_supported(expected_key))
                report_unsupported_map_key(expr, name, map_type, ctx);
        } else if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            report_expected_hashmap(expr, name, map_type, ctx);
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_MAP_BUILTIN_GET || kind == STDLIB_MAP_BUILTIN_HAS
        || kind == STDLIB_MAP_BUILTIN_REMOVE) {
        Type *map_type;
        Type *key_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        map_type = stdlib_map_normalize_type(
            type_check_expression(stdlib_map_arg(expr, 0), ctx));
        if (kind == STDLIB_MAP_BUILTIN_REMOVE
            && reject_non_inout_param_collection_mutator_receiver(
                stdlib_map_arg(expr, 0), map_type, "MapRemove", "map", ctx))
            return TYPE_UNKNOWN;
        key_type = stdlib_map_normalize_type(
            type_check_expression(stdlib_map_arg(expr, 1), ctx));
        if (type_is_constructed_named(map_type, "HashMap")
            && type_constructed_arg_count(map_type) == 2) {
            Type *expected_key = type_constructed_arg(map_type, 0);
            require_assignable(key_type, expected_key,
                stdlib_map_arg(expr, 1), ctx);
            if (!stdlib_map_key_supported(expected_key))
                report_unsupported_map_key(expr, name, map_type, ctx);
            if (kind == STDLIB_MAP_BUILTIN_GET) {
                Type *result = stdlib_map_normalize_type(type_constructed_arg(map_type, 1));
                if (result == NULL || result == TYPE_UNKNOWN ||
                    (result->name != NULL &&
                     (strcmp(result->name, "<unknown>") == 0 || strcmp(result->name, "Unknown") == 0))) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                        expr, "MapGet requires a concrete value type; Unknown cannot establish result ownership");
                    return TYPE_UNKNOWN;
                }
                if (semantic_param_publication_pending(ctx) && !ctx->has_error && result == TYPE_INT && stdlib_map_key_supported(expected_key) &&
                    !semantic_typed_map_query_record(ctx, expr, PGY_TYPED_MAP_QUERY_INT, map_type, result))
                    semantic_error(ctx, expr, "Typed MapGet query fact identity/allocation failed");
                if (!ctx->has_error) {
                    if (result->name == NULL || result->name[0] == '\0') {
                        semantic_error(ctx, expr,
                            "Typed MapGet result snapshot requires checked value type metadata");
                    } else if (!ast_call_set_semantic_value_type_name_copy(expr, result->name)) {
                        semantic_error(ctx, expr, "Typed MapGet result snapshot allocation failed");
                    }
                }
                if (!ctx->has_error && result == TYPE_STRING && semantic_param_publication_pending(ctx) &&
                    !semantic_param_publication_record_checked_result_relation(ctx, expr, map_type, result, 0, 0))
                    semantic_error(ctx, expr, "Checked String MapGet result relation publication failed");
                return result;
            }
        } else if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            report_expected_hashmap(expr, name, map_type, ctx);
        }
        if (kind == STDLIB_MAP_BUILTIN_GET) {
            if (map_type == TYPE_UNKNOWN)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                    PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                    expr, "MapGet requires a checked HashMap receiver and value type; missing type facts cannot establish result ownership");
            return TYPE_UNKNOWN; /* Failed signature, never a contextual grant. */
        }
        return kind == STDLIB_MAP_BUILTIN_HAS ? TYPE_BOOL : TYPE_VOID;
    }
    if (kind == STDLIB_MAP_BUILTIN_SIZE) {
        Type *map_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        map_type = stdlib_map_normalize_type(
            type_check_expression(stdlib_map_arg(expr, 0), ctx));
        if (map_type != NULL && map_type != TYPE_UNKNOWN
            && !type_is_constructed_named(map_type, "HashMap"))
            report_expected_hashmap(expr, name, map_type, ctx);
        return TYPE_INT;
    }
    if (kind == STDLIB_MAP_BUILTIN_KEYS) {
        Type *map_type;
        Type *key_type;
        Type *args[1];
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        map_type = stdlib_map_normalize_type(
            type_check_expression(stdlib_map_arg(expr, 0), ctx));
        if (type_is_constructed_named(map_type, "HashMap")
            && type_constructed_arg_count(map_type) == 2) {
            key_type = type_constructed_arg(map_type, 0);
            if (!stdlib_map_key_supported(key_type))
                report_unsupported_map_key(expr, name, map_type, ctx);
            args[0] = key_type != NULL ? key_type : TYPE_UNKNOWN;
            Type *result = type_create_constructed(TYPE_ARRAY, args, 1);
            if (semantic_param_publication_pending(ctx) && !ctx->has_error && stdlib_map_key_supported(key_type) &&
                !semantic_typed_map_query_record(ctx, expr, PGY_TYPED_MAP_QUERY_KEYS, map_type, result))
                semantic_error(ctx, expr, "Typed MapKeys query fact identity/allocation failed");
            /* AST owns this checked result snapshot through HIR/MIR lowering. */
            if (!ctx->has_error && result != NULL && result != TYPE_UNKNOWN
                && result->name != NULL && result->name[0] != '\0'
                && !ast_call_set_semantic_value_type_name_copy(expr, result->name))
                semantic_error(ctx, expr, "Typed MapKeys result snapshot allocation failed");
            return result;
        }
        if (map_type != NULL && map_type != TYPE_UNKNOWN)
            report_expected_hashmap(expr, name, map_type, ctx);
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}
