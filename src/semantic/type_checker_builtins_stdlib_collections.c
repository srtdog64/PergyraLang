#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "type_checker_collection_policy.h"
#include "diag_codes.h"

typedef enum StdlibCollectionBuiltinKind {
    STDLIB_COLLECTION_UNKNOWN = 0,
    STDLIB_COLLECTION_LIST_NEW,
    STDLIB_COLLECTION_LIST_PUSH,
    STDLIB_COLLECTION_LIST_GET,
    STDLIB_COLLECTION_LIST_SET,
    STDLIB_COLLECTION_LIST_SIZE,
    STDLIB_COLLECTION_LIST_REMOVE,
    STDLIB_COLLECTION_SET_NEW,
    STDLIB_COLLECTION_SET_ADD,
    STDLIB_COLLECTION_SET_REMOVE,
    STDLIB_COLLECTION_SET_HAS,
    STDLIB_COLLECTION_SET_SIZE,
    STDLIB_COLLECTION_SET_VALUES,
    STDLIB_COLLECTION_QUEUE_NEW,
    STDLIB_COLLECTION_QUEUE_PUSH,
    STDLIB_COLLECTION_QUEUE_POP,
    STDLIB_COLLECTION_QUEUE_SIZE,
    STDLIB_COLLECTION_QUEUE_EMPTY,
    STDLIB_COLLECTION_ARRAY_LENGTH,
    STDLIB_COLLECTION_ARRAY_PUSH,
    STDLIB_COLLECTION_ARRAY_SET,
    STDLIB_COLLECTION_ARRAY_POP,
    STDLIB_COLLECTION_ARRAY_SORT,
    STDLIB_COLLECTION_ARRAY_REVERSE,
    STDLIB_COLLECTION_ARRAY_MAP,
    STDLIB_COLLECTION_ARRAY_FILTER,
    STDLIB_COLLECTION_SLICE_COPY
} StdlibCollectionBuiltinKind;

typedef struct StdlibCollectionBuiltinSpec {
    const char *name;
    StdlibCollectionBuiltinKind kind;
} StdlibCollectionBuiltinSpec;

static int
stdlib_collection_builtin_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const StdlibCollectionBuiltinSpec *spec =
        (const StdlibCollectionBuiltinSpec *)entry;

    return strcmp(name, spec->name);
}

static Type *
stdlib_collection_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static StdlibCollectionBuiltinKind
stdlib_collection_builtin_kind(const char *name)
{
    static const StdlibCollectionBuiltinSpec specs[] = {
        { "ArrayFilter", STDLIB_COLLECTION_ARRAY_FILTER },
        { "ArrayLength", STDLIB_COLLECTION_ARRAY_LENGTH },
        { "ArrayMap", STDLIB_COLLECTION_ARRAY_MAP },
        { "ArrayPop", STDLIB_COLLECTION_ARRAY_POP },
        { "ArrayPush", STDLIB_COLLECTION_ARRAY_PUSH },
        { "ArrayReverse", STDLIB_COLLECTION_ARRAY_REVERSE },
        { "ArraySet", STDLIB_COLLECTION_ARRAY_SET },
        { "ArraySort", STDLIB_COLLECTION_ARRAY_SORT },
        { "ListGet", STDLIB_COLLECTION_LIST_GET },
        { "ListNew", STDLIB_COLLECTION_LIST_NEW },
        { "ListPush", STDLIB_COLLECTION_LIST_PUSH },
        { "ListRemove", STDLIB_COLLECTION_LIST_REMOVE },
        { "ListSet", STDLIB_COLLECTION_LIST_SET },
        { "ListSize", STDLIB_COLLECTION_LIST_SIZE },
        { "QueueEmpty", STDLIB_COLLECTION_QUEUE_EMPTY },
        { "QueueNew", STDLIB_COLLECTION_QUEUE_NEW },
        { "QueuePop", STDLIB_COLLECTION_QUEUE_POP },
        { "QueuePush", STDLIB_COLLECTION_QUEUE_PUSH },
        { "QueueSize", STDLIB_COLLECTION_QUEUE_SIZE },
        { "SetAdd", STDLIB_COLLECTION_SET_ADD },
        { "SetHas", STDLIB_COLLECTION_SET_HAS },
        { "SetNew", STDLIB_COLLECTION_SET_NEW },
        { "SetRemove", STDLIB_COLLECTION_SET_REMOVE },
        { "SetSize", STDLIB_COLLECTION_SET_SIZE },
        { "SetValues", STDLIB_COLLECTION_SET_VALUES },
        { "SliceCopy", STDLIB_COLLECTION_SLICE_COPY }
    };
    const StdlibCollectionBuiltinSpec *match;

    if (name == NULL)
        return STDLIB_COLLECTION_UNKNOWN;
    match = (const StdlibCollectionBuiltinSpec *)bsearch(
        &name, specs, sizeof(specs) / sizeof(specs[0]), sizeof(specs[0]),
        stdlib_collection_builtin_compare);
    return match != NULL ? match->kind : STDLIB_COLLECTION_UNKNOWN;
}

static bool
stdlib_collection_builtin_mutates_storage(StdlibCollectionBuiltinKind kind)
{
    return kind == STDLIB_COLLECTION_LIST_PUSH
        || kind == STDLIB_COLLECTION_LIST_SET
        || kind == STDLIB_COLLECTION_LIST_REMOVE
        || kind == STDLIB_COLLECTION_SET_ADD
        || kind == STDLIB_COLLECTION_SET_REMOVE
        || kind == STDLIB_COLLECTION_QUEUE_PUSH
        || kind == STDLIB_COLLECTION_QUEUE_POP
        || kind == STDLIB_COLLECTION_ARRAY_PUSH
        || kind == STDLIB_COLLECTION_ARRAY_SET
        || kind == STDLIB_COLLECTION_ARRAY_POP
        || kind == STDLIB_COLLECTION_ARRAY_SORT
        || kind == STDLIB_COLLECTION_ARRAY_REVERSE;
}

static bool
stdlib_collection_reject_parallel_mutation(ASTNode *expr,
                                           const char *name,
                                           SemanticContext *ctx,
                                           StdlibCollectionBuiltinKind kind)
{
    if (ctx == NULL || !ctx->in_parallel)
        return false;
    if (!stdlib_collection_builtin_mutates_storage(kind))
        return false;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SLOT_CONFLICT,
        PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT,
        PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
        "Parallel context does not permit collection mutator '%s'.\n"
        "Reason:\n"
        "- growable collection storage may reallocate, rehash, or alias while another task observes it\n"
        "- generated C/LLVM workers cannot safely share mutable collection storage by raw pointer\n"
        "Fix:\n"
        "- mutate the collection before or after parallel\n"
        "- or send immutable values through a channel/result boundary",
        name != NULL ? name : "<collection builtin>");
    return true;
}

Type *
type_check_stdlib_collection_call(ASTNode *expr,
                                  const char *name,
                                  SemanticContext *ctx,
                                  bool *handled_out)
{
    StdlibCollectionBuiltinKind kind = stdlib_collection_builtin_kind(name);

    if (kind == STDLIB_COLLECTION_UNKNOWN) {
        if (handled_out != NULL)
            *handled_out = false;
        return NULL;
    }

    if (handled_out != NULL)
        *handled_out = true;

    if (stdlib_collection_reject_parallel_mutation(expr, name, ctx, kind))
        return TYPE_UNKNOWN;

    ASTNode *arg0 = ast_call_argument(expr, 0);
    ASTNode *arg1 = ast_call_argument(expr, 1);
    ASTNode *arg2 = ast_call_argument(expr, 2);

    if (kind == STDLIB_COLLECTION_LIST_NEW) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }
    if (kind == STDLIB_COLLECTION_LIST_PUSH) {
        Type *list_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        list_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        value_type = stdlib_collection_normalize_type(
            type_check_expression(arg1, ctx));
        reject_borrowed_boundary_container_store(
            arg1, value_type, "list", "ListPush", ctx);
        if (type_is_constructed_named(list_type, "List")
            && type_constructed_arg_count(list_type) == 1) {
            require_assignable(value_type,
                type_constructed_arg(list_type, 0),
                arg1, ctx);
        } else if (list_type != NULL && list_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ListPush expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_LIST_GET) {
        Type *list_type;
        Type *index_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        list_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        index_type = stdlib_collection_normalize_type(
            type_check_expression(arg1, ctx));
        require_assignable(index_type, TYPE_INT, arg1, ctx);
        if (type_is_constructed_named(list_type, "List")
            && type_constructed_arg_count(list_type) == 1) {
            return stdlib_collection_normalize_type(
                type_constructed_arg(list_type, 0));
        }
        if (list_type != NULL && list_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ListGet expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (kind == STDLIB_COLLECTION_LIST_SET) {
        Type *list_type;
        Type *index_type;
        Type *value_type;
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        list_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        index_type = stdlib_collection_normalize_type(
            type_check_expression(arg1, ctx));
        value_type = stdlib_collection_normalize_type(
            type_check_expression(arg2, ctx));
        reject_borrowed_boundary_container_store(
            arg2, value_type, "list", "ListSet", ctx);
        require_assignable(index_type, TYPE_INT, arg1, ctx);
        if (type_is_constructed_named(list_type, "List")
            && type_constructed_arg_count(list_type) == 1) {
            require_assignable(value_type,
                type_constructed_arg(list_type, 0),
                arg2, ctx);
        } else if (list_type != NULL && list_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ListSet expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_LIST_SIZE) {
        Type *list_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        list_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        if (list_type != NULL && list_type != TYPE_UNKNOWN
            && !type_is_constructed_named(list_type, "List")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ListSize expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (kind == STDLIB_COLLECTION_LIST_REMOVE) {
        Type *list_type;
        Type *index_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        list_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        index_type = stdlib_collection_normalize_type(
            type_check_expression(arg1, ctx));
        require_assignable(index_type, TYPE_INT, arg1, ctx);
        if (list_type != NULL && list_type != TYPE_UNKNOWN
            && !type_is_constructed_named(list_type, "List")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "ListRemove expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (kind == STDLIB_COLLECTION_SET_NEW) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }
    if (kind == STDLIB_COLLECTION_SET_ADD
        || kind == STDLIB_COLLECTION_SET_REMOVE) {
        Type *set_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        set_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        value_type = stdlib_collection_normalize_type(
            type_check_expression(arg1, ctx));
        if (kind == STDLIB_COLLECTION_SET_ADD) {
            reject_borrowed_boundary_container_store(
                arg1, value_type, "set", "SetAdd", ctx);
        }
        if (type_is_constructed_named(set_type, "Set")
            && type_constructed_arg_count(set_type) == 1) {
            require_assignable(value_type,
                type_constructed_arg(set_type, 0),
                arg1, ctx);
        } else if (set_type != NULL && set_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "%s expects Set<T> as first argument, got '%s'",
                name,
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_SET_HAS) {
        Type *set_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        set_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        value_type = stdlib_collection_normalize_type(
            type_check_expression(arg1, ctx));
        if (type_is_constructed_named(set_type, "Set")
            && type_constructed_arg_count(set_type) == 1) {
            require_assignable(value_type,
                type_constructed_arg(set_type, 0),
                arg1, ctx);
        } else if (set_type != NULL && set_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "SetHas expects Set<T> as first argument, got '%s'",
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_BOOL;
    }
    if (kind == STDLIB_COLLECTION_SET_SIZE) {
        Type *set_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        set_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        if (set_type != NULL && set_type != TYPE_UNKNOWN
            && !type_is_constructed_named(set_type, "Set")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "SetSize expects Set<T> as first argument, got '%s'",
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (kind == STDLIB_COLLECTION_SET_VALUES) {
        Type *set_type;
        Type *inner_type;
        Type *args[1];
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        set_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        if (type_is_constructed_named(set_type, "Set")
            && type_constructed_arg_count(set_type) == 1) {
            inner_type = type_constructed_arg(set_type, 0);
            if (!type_checker_ordered_collection_key_supported(inner_type)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                    PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                    PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                    "SetValues currently supports only Set<String>, Set<Int>, Set<Long>, or Set<Bool>, got '%s'",
                    set_type->name != NULL ? set_type->name : "<type>");
            }
            args[0] = inner_type != NULL ? inner_type : TYPE_UNKNOWN;
            return type_create_constructed(TYPE_ARRAY, args, 1);
        }
        if (set_type != NULL && set_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "SetValues expects Set<T> as first argument, got '%s'",
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_UNKNOWN;
    }
    if (kind == STDLIB_COLLECTION_QUEUE_NEW) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }
    if (kind == STDLIB_COLLECTION_QUEUE_PUSH) {
        Type *queue_type;
        Type *value_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        value_type = stdlib_collection_normalize_type(
            type_check_expression(arg1, ctx));
        reject_borrowed_boundary_container_store(
            arg1, value_type, "queue", "QueuePush", ctx);
        if (type_is_constructed_named(queue_type, "Queue")
            && type_constructed_arg_count(queue_type) == 1) {
            require_assignable(value_type,
                type_constructed_arg(queue_type, 0),
                arg1, ctx);
        } else if (queue_type != NULL && queue_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "QueuePush expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return TYPE_VOID;
    }
    if (kind == STDLIB_COLLECTION_QUEUE_POP) {
        Type *queue_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        if (type_is_constructed_named(queue_type, "Queue")
            && type_constructed_arg_count(queue_type) == 1) {
            return stdlib_collection_normalize_type(
                type_constructed_arg(queue_type, 0));
        }
        if (queue_type != NULL && queue_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "QueuePop expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (kind == STDLIB_COLLECTION_QUEUE_SIZE
        || kind == STDLIB_COLLECTION_QUEUE_EMPTY) {
        Type *queue_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        if (queue_type != NULL && queue_type != TYPE_UNKNOWN
            && !type_is_constructed_named(queue_type, "Queue")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "%s expects Queue<T> as first argument, got '%s'",
                name,
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return kind == STDLIB_COLLECTION_QUEUE_EMPTY ? TYPE_BOOL : TYPE_INT;
    }
    if (kind == STDLIB_COLLECTION_ARRAY_LENGTH) {
        Type *arg;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arg = stdlib_collection_normalize_type(
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
        arr = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        val = stdlib_collection_normalize_type(
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
        arr = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        require_assignable(
            type_check_expression(arg1, ctx),
            TYPE_INT, arg1, ctx);
        val = stdlib_collection_normalize_type(
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
        arr = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
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
        arr = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
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
        arr = stdlib_collection_normalize_type(
            type_check_expression(arg0, ctx));
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, arg0,
                "%s requires Array<T> as first argument, got '%s'",
                name, type_name_or_unknown(arr));
        /* Effect polymorphism: the mapped/filtered function's effects flow
         * into the caller's derived set, so passing an IO/authority-bearing
         * function to ArrayMap/ArrayFilter carries that obligation outward. */
        semantic_record_effect(ctx,
            type_function_effects(stdlib_collection_normalize_type(
                type_check_expression(arg1, ctx))));
        return arr;
    }
    if (kind == STDLIB_COLLECTION_SLICE_COPY) {
        Type *slice;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        slice = stdlib_collection_normalize_type(
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
