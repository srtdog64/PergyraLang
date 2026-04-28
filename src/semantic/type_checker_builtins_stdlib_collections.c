#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "diag_codes.h"

Type *
type_check_stdlib_collection_call(ASTNode *expr,
                                  const char *name,
                                  SemanticContext *ctx,
                                  bool *handled_out)
{
    if (handled_out != NULL)
        *handled_out = true;

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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ListRemove expects List<T> as first argument, got '%s'",
                list_type->name != NULL ? list_type->name : "<type>");
        }
        return TYPE_INT;
    }
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "SetSize expects Set<T> as first argument, got '%s'",
                set_type->name != NULL ? set_type->name : "<type>");
        }
        return TYPE_INT;
    }
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
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
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "QueuePop expects Queue<T> as first argument, got '%s'",
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "QueueSize") == 0 || strcmp(name, "QueueEmpty") == 0) {
        Type *queue_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        queue_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (queue_type != NULL && queue_type != TYPE_UNKNOWN
            && !type_is_constructed_named(queue_type, "Queue")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "%s expects Queue<T> as first argument, got '%s'",
                name,
                queue_type->name != NULL ? queue_type->name : "<type>");
        }
        return strcmp(name, "QueueEmpty") == 0 ? TYPE_BOOL : TYPE_INT;
    }
    if (strcmp(name, "ArrayLength") == 0) {
        Type *arg;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arg = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arg, "Array")
            && !type_is_constructed_named(arg, "Slice")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArrayLength requires Array<T> or Slice<T>, got '%s'",
                arg->name);
        }
        return TYPE_INT;
    }
    if (strcmp(name, "ArrayPush") == 0) {
        Type *arr;
        Type *val;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        arr = type_check_expression(expr->data.call.arguments[0], ctx);
        val = type_check_expression(expr->data.call.arguments[1], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[1], val, "array", "ArrayPush", ctx);
        if (!type_is_constructed_named(arr, "Array")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArrayPush requires Array<T>, got '%s'", arr->name);
        } else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL)
                require_assignable(val, inner, expr->data.call.arguments[1], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ArraySet") == 0) {
        Type *arr;
        Type *val;
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        arr = type_check_expression(expr->data.call.arguments[0], ctx);
        require_assignable(
            type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
        val = type_check_expression(expr->data.call.arguments[2], ctx);
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[2], val, "array", "ArraySet", ctx);
        if (!type_is_constructed_named(arr, "Array")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArraySet requires Array<T>, got '%s'", arr->name);
        } else {
            Type *inner = type_get_constructed_arg(arr, 0);
            if (inner != NULL)
                require_assignable(val, inner, expr->data.call.arguments[2], ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "ArrayPop") == 0) {
        Type *arr;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "ArrayPop requires Array<T>, got '%s'", arr->name);
        return TYPE_VOID;
    }
    if (strcmp(name, "ArraySort") == 0
        || strcmp(name, "ArrayReverse") == 0) {
        Type *arr;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "%s requires Array<T>, got '%s'", name, arr->name);
        return arr;
    }
    if (strcmp(name, "ArrayMap") == 0 || strcmp(name, "ArrayFilter") == 0) {
        Type *arr;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        arr = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr, "Array"))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "%s requires Array<T> as first argument, got '%s'",
                name, arr->name);
        type_check_expression(expr->data.call.arguments[1], ctx);
        return arr;
    }

    if (handled_out != NULL)
        *handled_out = false;
    return NULL;
}
