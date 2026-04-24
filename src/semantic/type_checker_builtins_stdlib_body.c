/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker — stdlib builtin dispatch body.
 * Extracted from the legacy type_checker_builtins_stdlib_body.inc fragment
 * that was chained together with slotops.inc inside type_checker_builtins.c.
 * Cross-TU helpers live in type_checker_builtins_internal.h.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_channel_transport_internal.h"
#include "diag_codes.h"

Type *
type_check_stdlib_call(ASTNode *expr, const char *name, SemanticContext *ctx)
{
    if (strcmp(name, "Abs") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return type_check_expression(expr->data.call.arguments[0], ctx);
    }
    if (strcmp(name, "Min") == 0 || strcmp(name, "Max") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *a = type_check_expression(expr->data.call.arguments[0], ctx);
        Type *b = type_check_expression(expr->data.call.arguments[1], ctx);
        require_assignable(b, a, expr->data.call.arguments[1], ctx);
        return a;
    }
    if (strcmp(name, "StringLength") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "Contains") == 0 || strcmp(name, "StringContains") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "Replace") == 0 || strcmp(name, "StringReplace") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < 3; i++) {
            require_assignable(type_check_expression(expr->data.call.arguments[i], ctx),
                TYPE_STRING, expr->data.call.arguments[i], ctx);
        }
        return TYPE_STRING;
    }
    if (strcmp(name, "Substring") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[2], ctx),
            TYPE_INT, expr->data.call.arguments[2], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Trim") == 0 || strcmp(name, "StringTrim") == 0
        || strcmp(name, "Upper") == 0 || strcmp(name, "ToUpper") == 0
        || strcmp(name, "Lower") == 0 || strcmp(name, "ToLower") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Concat") == 0 || strcmp(name, "StringConcat") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "StringSplit") == 0 || strcmp(name, "Split") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        Type *args[1] = { TYPE_STRING };
        return type_create_constructed(TYPE_ARRAY, args, 1);
    }
    if (strcmp(name, "StringJoin") == 0 || strcmp(name, "Join") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr_type, "Array")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "StringJoin requires Array<String> as first argument");
        }
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "ToInt") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "ToFloat") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_FLOAT;
    }
    /* Math builtins */
    if (strcmp(name, "Sqrt") == 0 || strcmp(name, "Floor") == 0
        || strcmp(name, "Ceil") == 0
        || strcmp(name, "Sin") == 0 || strcmp(name, "Cos") == 0
        || strcmp(name, "Tan") == 0 || strcmp(name, "Asin") == 0
        || strcmp(name, "Acos") == 0 || strcmp(name, "Atan") == 0
        || strcmp(name, "Exp") == 0 || strcmp(name, "MathLog") == 0
        || strcmp(name, "Log10") == 0 || strcmp(name, "Log2") == 0
        || strcmp(name, "Round") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Atan2") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Clamp") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        Type *val = type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        type_check_expression(expr->data.call.arguments[2], ctx);
        return val;
    }
    if (strcmp(name, "PI") == 0 || strcmp(name, "E") == 0) {
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Pow") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Random") == 0) {
        if (expr->data.call.arg_count > 0)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "SeedRandom") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_INT, expr->data.call.arguments[0], ctx);
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_VOID;
    }
    /* HashMap builtins */
    if (strcmp(name, "MapNew") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN; /* type resolved from let annotation */
    }
    if (strcmp(name, "MapSet") == 0) {
        Type *map_type;
        Type *key_type;
        Type *value_type;
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        key_type = type_check_expression(expr->data.call.arguments[1], ctx);
        value_type = type_check_expression(expr->data.call.arguments[2], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[2], value_type, "map", "MapSet", ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            require_assignable(key_type,
                map_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
            require_assignable(value_type,
                map_type->data.constructed.args[1],
                expr->data.call.arguments[2], ctx);
            if (map_type->data.constructed.args[0] != NULL
                && map_type->data.constructed.args[0]->name != NULL
                && strcmp(map_type->data.constructed.args[0]->name, "String") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Int") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Long") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Bool") != 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                    "MapSet currently supports only HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, and HashMap<Bool, T>, got '%s'",
                    map_type->name != NULL ? map_type->name : "<type>");
            }
        } else if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "MapSet expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "MapGet") == 0) {
        Type *map_type;
        Type *key_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        key_type = type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            require_assignable(key_type,
                map_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
            if (map_type->data.constructed.args[0] != NULL
                && map_type->data.constructed.args[0]->name != NULL
                && strcmp(map_type->data.constructed.args[0]->name, "String") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Int") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Long") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Bool") != 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                    "MapGet currently supports only HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, and HashMap<Bool, T>, got '%s'",
                    map_type->name != NULL ? map_type->name : "<type>");
            }
            return map_type->data.constructed.args[1];
        }
        if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "MapGet expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_UNKNOWN; /* resolved from context */
    }
    if (strcmp(name, "MapHas") == 0) {
        Type *map_type;
        Type *key_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        key_type = type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            require_assignable(key_type,
                map_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
            if (map_type->data.constructed.args[0] != NULL
                && map_type->data.constructed.args[0]->name != NULL
                && strcmp(map_type->data.constructed.args[0]->name, "String") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Int") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Long") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Bool") != 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                    "MapHas currently supports only HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, and HashMap<Bool, T>, got '%s'",
                    map_type->name != NULL ? map_type->name : "<type>");
            }
        } else if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "MapHas expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "MapRemove") == 0) {
        Type *map_type;
        Type *key_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        key_type = type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            require_assignable(key_type,
                map_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
            if (map_type->data.constructed.args[0] != NULL
                && map_type->data.constructed.args[0]->name != NULL
                && strcmp(map_type->data.constructed.args[0]->name, "String") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Int") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Long") != 0
                && strcmp(map_type->data.constructed.args[0]->name, "Bool") != 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                    "MapRemove currently supports only HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, and HashMap<Bool, T>, got '%s'",
                    map_type->name != NULL ? map_type->name : "<type>");
            }
        } else if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "MapRemove expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "MapSize") == 0) {
        Type *map_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (map_type != NULL && map_type != TYPE_UNKNOWN
            && !type_is_constructed_named(map_type, "HashMap")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "MapSize expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "MapKeys") == 0) {
        Type *map_type;
        Type *key_type;
        Type *args[1];
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        map_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            key_type = map_type->data.constructed.args[0];
            if (key_type != NULL
                && key_type->name != NULL
                && strcmp(key_type->name, "String") != 0
                && strcmp(key_type->name, "Int") != 0
                && strcmp(key_type->name, "Long") != 0
                && strcmp(key_type->name, "Bool") != 0) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                    "MapKeys currently supports only HashMap<String, T>, HashMap<Int, T>, HashMap<Long, T>, and HashMap<Bool, T>, got '%s'",
                    map_type->name != NULL ? map_type->name : "<type>");
            }
            args[0] = key_type != NULL ? key_type : TYPE_UNKNOWN;
            return type_create_constructed(TYPE_ARRAY, args, 1);
        }
        if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "MapKeys expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_UNKNOWN;
    }
    /* List builtins */
    if (strcmp(name, "ListNew") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "ListPush") == 0) {
        Type *list_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        list_type = type_check_expression(expr->data.call.arguments[0], ctx);
        value_type = type_check_expression(expr->data.call.arguments[1], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[1], value_type, "list", "ListPush", ctx);
        if (type_is_constructed_named(list_type, "List")
            && list_type->data.constructed.arg_count == 1) {
            require_assignable(value_type,
                list_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
        } else if (list_type != NULL && list_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ListPush expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ListGet") == 0) {
        Type *list_type;
        Type *index_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        list_type = type_check_expression(expr->data.call.arguments[0], ctx);
        index_type = type_check_expression(expr->data.call.arguments[1], ctx);
        require_assignable(index_type, TYPE_INT, expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(list_type, "List")
            && list_type->data.constructed.arg_count == 1) {
            return list_type->data.constructed.args[0];
        }
        if (list_type != NULL && list_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ListGet expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "ListSet") == 0) {
        Type *list_type;
        Type *index_type;
        Type *value_type;
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        list_type = type_check_expression(expr->data.call.arguments[0], ctx);
        index_type = type_check_expression(expr->data.call.arguments[1], ctx);
        value_type = type_check_expression(expr->data.call.arguments[2], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[2], value_type, "list", "ListSet", ctx);
        require_assignable(index_type, TYPE_INT, expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(list_type, "List")
            && list_type->data.constructed.arg_count == 1) {
            require_assignable(value_type,
                list_type->data.constructed.args[0],
                expr->data.call.arguments[2], ctx);
        } else if (list_type != NULL && list_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ListSet expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ListSize") == 0) {
        Type *list_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        list_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (list_type != NULL && list_type != TYPE_UNKNOWN
            && !type_is_constructed_named(list_type, "List")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ListSize expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "ListRemove") == 0) {
        Type *list_type;
        Type *index_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        list_type = type_check_expression(expr->data.call.arguments[0], ctx);
        index_type = type_check_expression(expr->data.call.arguments[1], ctx);
        require_assignable(index_type, TYPE_INT, expr->data.call.arguments[1], ctx);
        if (list_type != NULL && list_type != TYPE_UNKNOWN
            && !type_is_constructed_named(list_type, "List")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ListRemove expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_INT;
    }
    /* Set builtins */
    if (strcmp(name, "SetNew") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "SetAdd") == 0 || strcmp(name, "SetRemove") == 0) {
        Type *set_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        set_type = type_check_expression(expr->data.call.arguments[0], ctx);
        value_type = type_check_expression(expr->data.call.arguments[1], ctx);
        if (strcmp(name, "SetAdd") == 0) {
            reject_borrowed_boundary_container_store(
                expr->data.call.arguments[1], value_type, "set", "SetAdd", ctx);
        }
        if (type_is_constructed_named(set_type, "Set")
            && set_type->data.constructed.arg_count == 1) {
            require_assignable(value_type,
                set_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
        } else if (set_type != NULL && set_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "%s expects Set<T> as first argument, got '%s'",
                name,
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "SetHas") == 0) {
        Type *set_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        set_type = type_check_expression(expr->data.call.arguments[0], ctx);
        value_type = type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(set_type, "Set")
            && set_type->data.constructed.arg_count == 1) {
            require_assignable(value_type,
                set_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
        } else if (set_type != NULL && set_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "SetHas expects Set<T> as first argument, got '%s'",
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "SetSize") == 0) {
        Type *set_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        set_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (set_type != NULL && set_type != TYPE_UNKNOWN
            && !type_is_constructed_named(set_type, "Set")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "SetSize expects Set<T> as first argument, got '%s'",
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_INT;
    }
    /* Queue builtins */
    if (strcmp(name, "QueueNew") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "QueuePush") == 0) {
        Type *queue_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = type_check_expression(expr->data.call.arguments[0], ctx);
        value_type = type_check_expression(expr->data.call.arguments[1], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[1], value_type, "queue", "QueuePush", ctx);
        if (type_is_constructed_named(queue_type, "Queue")
            && queue_type->data.constructed.arg_count == 1) {
            require_assignable(value_type,
                queue_type->data.constructed.args[0],
                expr->data.call.arguments[1], ctx);
        } else if (queue_type != NULL && queue_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "QueuePush expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "QueuePop") == 0) {
        Type *queue_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (type_is_constructed_named(queue_type, "Queue")
            && queue_type->data.constructed.arg_count == 1) {
            return queue_type->data.constructed.args[0];
        }
        if (queue_type != NULL && queue_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "QueuePop expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "QueueSize") == 0) {
        Type *queue_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (queue_type != NULL && queue_type != TYPE_UNKNOWN
            && !type_is_constructed_named(queue_type, "Queue")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "QueueSize expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "QueueEmpty") == 0) {
        Type *queue_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (queue_type != NULL && queue_type != TYPE_UNKNOWN
            && !type_is_constructed_named(queue_type, "Queue")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "QueueEmpty expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return TYPE_BOOL;
    }
    /* FSM builtins */
    if (strcmp(name, "FsmNew") == 0) { return TYPE_UNKNOWN; }
    if (strcmp(name, "FsmAddState") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "FsmTransition") == 0) {
        if (expr->data.call.arg_count >= 4) {
            for (size_t ai = 0; ai < 4; ai++)
                type_check_expression(expr->data.call.arguments[ai], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "FsmStep") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "FsmCurrent") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "FsmCurrentName") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    /* Timer builtins */
    if (strcmp(name, "TimerNew") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "TimerTick") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "TimerRemaining") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "TimerDone") == 0 || strcmp(name, "CooldownReady") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "TimerReset") == 0 || strcmp(name, "CooldownTrigger") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "CooldownNew") == 0) {
        if (expr->data.call.arg_count >= 1)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "CooldownTick") == 0) {
        if (expr->data.call.arg_count >= 2) {
            type_check_expression(expr->data.call.arguments[0], ctx);
            type_check_expression(expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ArrayLength") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arg = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arg, "Array")
            && !type_is_constructed_named(arg, "Slice")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArrayLength requires Array<T> or Slice<T>, got '%s'",
                arg->name);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "ArrayPush") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        Type *val = type_check_expression(expr->data.call.arguments[1], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[1], val, "array", "ArrayPush", ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArrayPush requires Array<T>, got '%s'", arr->name);
        else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL)
                require_assignable(val, inner, expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ArraySet") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        require_assignable(
            type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
        Type *val = type_check_expression(expr->data.call.arguments[2], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[2], val, "array", "ArraySet", ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArraySet requires Array<T>, got '%s'", arr->name);
        else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL)
                require_assignable(val, inner, expr->data.call.arguments[2], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ArrayPop") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArrayPop requires Array<T>, got '%s'", arr->name);
        return TYPE_VOID;
    }
    if (strcmp(name, "ArraySort") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArraySort requires Array<T>, got '%s'", arr->name);
        return arr;
    }
    if (strcmp(name, "ArrayMap") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArrayMap requires Array<T> as first argument, got '%s'", arr->name);
        /* Second arg is a function — type-check it but allow any callable */
        type_check_expression(expr->data.call.arguments[1], ctx);
        /* Return type: same Array<T> (element type preserved for now) */
        return arr;
    }
    if (strcmp(name, "ArrayFilter") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArrayFilter requires Array<T> as first argument, got '%s'", arr->name);
        /* Second arg is a predicate function */
        type_check_expression(expr->data.call.arguments[1], ctx);
        return arr;
    }
    if (strcmp(name, "ArrayReverse") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArrayReverse requires Array<T>, got '%s'", arr->name);
        return arr;
    }
    if (strcmp(name, "ToString") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Print") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "ReadLine") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_STRING;
    }
    if (strcmp(name, "Now") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_INT;
    }
    if (strcmp(name, "Sleep") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_INT, expr->data.call.arguments[0], ctx);
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_VOID;
    }

    if (strcmp(name, "ClaimQubit") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        /* Qubit starts in SUPERPOSITION state (uncollapsed). */
        return TYPE_QUBIT;
    }
    if (strcmp(name, "ClaimDeviceSlot") == 0) {
        return type_check_claim_device_slot(expr, ctx);
    }
    if (strcmp(name, "DeviceRead") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return type_get_constructed_arg(
            type_check_device_handle_arg(expr->data.call.arguments[0], ctx, name, false), 0);
    }
    if (strcmp(name, "DeviceWrite") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *slot_type = type_check_device_handle_arg(
            expr->data.call.arguments[0], ctx, name, false);
        Type *inner = type_get_constructed_arg(slot_type, 0);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            inner, expr->data.call.arguments[1], ctx);
        return TYPE_VOID;
    }
    if (strcmp(name, "ReleaseDeviceSlot") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ASTNode *slot_arg = expr->data.call.arguments[0];
        Type *slot_type = type_check_device_handle_arg(slot_arg, ctx, name, true);
        if (slot_type == TYPE_UNKNOWN)
            return TYPE_UNKNOWN;
        if (slot_arg->type != AST_IDENTIFIER) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
                "ReleaseDeviceSlot requires a DeviceSlot identifier");
            return TYPE_UNKNOWN;
        }
        {
            Symbol *sym = scope_lookup(ctx->scope, slot_arg->data.identifier.name);
            if (sym != NULL && sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, slot_arg,
                    "DeviceSlot '%s' has already been released",
                    slot_arg->data.identifier.name);
                return TYPE_UNKNOWN;
            }
        }
        scope_release_slot(ctx->scope, slot_arg->data.identifier.name);
        return TYPE_VOID;
    }
    if (strcmp(name, "SubmitDeviceRead") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *slot_type = type_check_device_handle_arg(
            expr->data.call.arguments[0], ctx, name, false);
        return wrap_constructed(TYPE_REMOTE_FUTURE,
            type_get_constructed_arg(slot_type, 0));
    }

    /* ---- Clone: explicit copy of Slot ---- */
    if (strcmp(name, "Clone") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *arg_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (arg_type == NULL)
            return TYPE_UNKNOWN;
        /* Clone returns the same type — a fresh independent copy */
        return arg_type;
    }

    /* ---- Result builtins ---- */
    if (strcmp(name, "IsOk") == 0 || strcmp(name, "IsErr") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "Some") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return wrap_constructed(TYPE_OPTION,
            type_check_expression(expr->data.call.arguments[0], ctx));
    }
    if (strcmp(name, "None") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return wrap_constructed(TYPE_OPTION, TYPE_UNKNOWN);
    }
    if (strcmp(name, "IsSome") == 0 || strcmp(name, "IsNone") == 0) {
        Type *ot;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ot = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ot, "Option")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "%s requires Option<T>, got '%s'", name,
                ot != NULL ? ot->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "UnwrapOption") == 0) {
        Type *ot;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ot = type_check_expression(expr->data.call.arguments[0], ctx);
        if (type_is_constructed_named(ot, "Option"))
            return type_get_constructed_arg(ot, 0);
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
            "UnwrapOption requires Option<T>, got '%s'",
            ot != NULL ? ot->name : "<null>");
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "Unwrap") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        Type *rt = type_check_expression(expr->data.call.arguments[0], ctx);
        if (type_is_constructed_named(rt, "Result"))
            return type_get_constructed_arg(rt, 0);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "UnwrapOr") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        Type *rt = type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(rt, "Result"))
            return type_get_constructed_arg(rt, 0);
        return TYPE_UNKNOWN;
    }

    /* ---- Channel builtins ---- */
    if (strcmp(name, "TryRecv") == 0) {
        return type_check_channel_recv_builtin(expr, name, false, ctx);
    }
    if (strcmp(name, "RecvTimeout") == 0) {
        return type_check_channel_recv_builtin(expr, name, true, ctx);
    }
    if (strcmp(name, "TrySend") == 0) {
        return type_check_channel_send_builtin(expr, name, false, false, ctx);
    }
    if (strcmp(name, "SendTimeout") == 0) {
        return type_check_channel_send_builtin(expr, name, true, false, ctx);
    }
    if (strcmp(name, "TrySendStatus") == 0) {
        return type_check_channel_send_builtin(expr, name, false, true, ctx);
    }
    if (strcmp(name, "SendTimeoutStatus") == 0) {
        return type_check_channel_send_builtin(expr, name, true, true, ctx);
    }
    if (strcmp(name, "ChannelLength") == 0
        || strcmp(name, "ChannelCapacity") == 0
        || strcmp(name, "ChannelSpace") == 0
        || strcmp(name, "ChannelFull") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "%s requires Channel<T>, got '%s'",
                name,
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return strcmp(name, "ChannelFull") == 0 ? TYPE_BOOL : TYPE_INT;
    }
    if (strcmp(name, "ChannelClosed") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ChannelClosed requires Channel<T>, got '%s'",
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "ChannelClose") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ChannelClose requires Channel<T>, got '%s'",
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ChannelReady") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *ch_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(ch_type, "Channel")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ChannelReady requires Channel<T>, got '%s'",
                ch_type != NULL ? ch_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "Cancel") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        Type *task_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_future_like(task_type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "Cancel requires Future<T> or RemoteFuture<T>, got '%s'",
                task_type != NULL ? task_type->name : "<null>");
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    if (strcmp(name, "IsCancelled") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_REMOTE);
        return TYPE_BOOL;
    }

    if (strcmp(name, "Measure") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC | EFFECT_COLLAPSE);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: CLASSICAL qubits cannot be measured */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs == QUBIT_STATE_CLASSICAL)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "Cannot Measure() a qubit in CLASSICAL state "
                    "(already converted via IntoClassical)");
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_COLLAPSED);
        /* Propagate collapse to all qubits in the same entanglement pool */
        {
            int32_t pool = get_qubit_entangle_pool(
                expr->data.call.arguments[0], ctx);
            if (pool >= 0)
                propagate_collapse_to_pool(ctx, pool);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "Entangle") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_qubit_use(expr->data.call.arguments[1], ctx),
            TYPE_QUBIT, expr->data.call.arguments[1], ctx);
        /* State validation: only SUPERPOSITION/NONE qubits can be entangled */
        {
            QubitSemanticState sa = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            QubitSemanticState sb = get_qubit_semantic_state(
                expr->data.call.arguments[1], ctx);
            if (sa == QUBIT_STATE_COLLAPSED || sa == QUBIT_STATE_CLASSICAL)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "Cannot Entangle() a qubit in %s state",
                    qubit_state_name(sa));
            if (sb == QUBIT_STATE_COLLAPSED || sb == QUBIT_STATE_CLASSICAL)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "Cannot Entangle() a qubit in %s state",
                    qubit_state_name(sb));
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_ENTANGLED);
        set_qubit_semantic_state(expr->data.call.arguments[1], ctx,
                                 QUBIT_STATE_ENTANGLED);
        /* Compile-time entanglement pool: allocate / merge */
        {
            int32_t pa = get_qubit_entangle_pool(
                expr->data.call.arguments[0], ctx);
            int32_t pb = get_qubit_entangle_pool(
                expr->data.call.arguments[1], ctx);
            if (pa >= 0 && pb >= 0) {
                if (pa != pb)
                    merge_entangle_pools(ctx, pa, pb);
            } else if (pa >= 0) {
                set_qubit_entangle_pool(expr->data.call.arguments[1], ctx, pa);
            } else if (pb >= 0) {
                set_qubit_entangle_pool(expr->data.call.arguments[0], ctx, pb);
            } else {
                int32_t new_pool = alloc_entangle_pool(ctx);
                set_qubit_entangle_pool(expr->data.call.arguments[0], ctx,
                                        new_pool);
                set_qubit_entangle_pool(expr->data.call.arguments[1], ctx,
                                        new_pool);
            }
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "QubitState") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "IsCollapsed") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "ReleaseQubit") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        consume_qubit_value(expr->data.call.arguments[0], ctx, "released");
        return TYPE_VOID;
    }
    if (strcmp(name, "H") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: CLASSICAL qubits cannot receive gate operations */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs == QUBIT_STATE_CLASSICAL)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "Cannot apply H() to a qubit in CLASSICAL state "
                    "(already converted via IntoClassical)");
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_SUPERPOSITION);
        return TYPE_VOID;
    }
    if (strcmp(name, "IntoClassical") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_qubit_use(expr->data.call.arguments[0], ctx),
            TYPE_QUBIT, expr->data.call.arguments[0], ctx);
        /* State validation: only COLLAPSED qubits can be converted.
         * Unmeasured/unknown states (NONE) must be rejected. */
        {
            QubitSemanticState qs = get_qubit_semantic_state(
                expr->data.call.arguments[0], ctx);
            if (qs != QUBIT_STATE_COLLAPSED)
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                    "IntoClassical() requires a COLLAPSED qubit (after Measure) "
                    "got %s", qubit_state_name(qs));
        }
        set_qubit_semantic_state(expr->data.call.arguments[0], ctx,
                                 QUBIT_STATE_CLASSICAL);
        consume_qubit_value(expr->data.call.arguments[0], ctx,
                            "converted to classical");
        return TYPE_BOOL;
    }

    return NULL;
}
