#include <stdlib.h>
#include <string.h>

#include "type_system.h"

bool
type_equals(const Type *a, const Type *b)
{
    if (a == b)
        return true;
    if (a == NULL || b == NULL)
        return false;
    if (a->kind != b->kind)
        return false;

    if (a->kind == TYPE_KIND_PRIMITIVE)
        return strcmp(a->name, b->name) == 0;

    if (a->kind == TYPE_KIND_SLOT) {
        return a->data.slot.is_secure == b->data.slot.is_secure
            && a->data.slot.access_mode == b->data.slot.access_mode
            && type_equals(a->data.slot.inner_type,
                           b->data.slot.inner_type);
    }

    if (a->kind == TYPE_KIND_CONSTRUCTED) {
        if (a->data.constructed.arg_count != b->data.constructed.arg_count)
            return false;
        if (!type_equals(a->data.constructed.constructor,
                         b->data.constructed.constructor))
            return false;
        for (size_t i = 0; i < a->data.constructed.arg_count; i++) {
            if (!type_equals(a->data.constructed.args[i],
                             b->data.constructed.args[i]))
                return false;
        }
        return true;
    }

    if (a->kind == TYPE_KIND_GENERIC)
        return strcmp(a->data.generic.param_name,
                      b->data.generic.param_name) == 0;

    if (a->kind == TYPE_KIND_TUPLE) {
        if (a->data.tuple.element_count != b->data.tuple.element_count)
            return false;
        for (size_t i = 0; i < a->data.tuple.element_count; i++) {
            if (!type_equals(a->data.tuple.elements[i],
                             b->data.tuple.elements[i]))
                return false;
        }
        return true;
    }

    if (a->kind == TYPE_KIND_FUNCTION) {
        if (a->data.function.param_count != b->data.function.param_count)
            return false;
        if (!type_equals(a->data.function.return_type,
                         b->data.function.return_type))
            return false;
        for (size_t i = 0; i < a->data.function.param_count; i++) {
            if (!type_equals(a->data.function.param_types[i],
                             b->data.function.param_types[i]))
                return false;
        }
        return true;
    }

    return strcmp(a->name, b->name) == 0;
}

bool
type_is_assignable(const Type *from, const Type *to)
{
    if (type_equals(from, to))
        return true;

    if (from == TYPE_UNKNOWN || to == TYPE_UNKNOWN)
        return true;

    if (from->kind == TYPE_KIND_CONSTRUCTED && to->kind == TYPE_KIND_CONSTRUCTED
        && from->data.constructed.constructor != NULL && to->data.constructed.constructor != NULL
        && from->data.constructed.constructor->name != NULL && to->data.constructed.constructor->name != NULL
        && strcmp(from->data.constructed.constructor->name, "Option") == 0
        && strcmp(to->data.constructed.constructor->name, "Option") == 0) {
        if (from->data.constructed.arg_count > 0 && from->data.constructed.args[0] == TYPE_UNKNOWN)
            return true;
        if (to->data.constructed.arg_count > 0 && to->data.constructed.args[0] == TYPE_UNKNOWN)
            return true;
    }

    if (from->kind == TYPE_KIND_PRIMITIVE
        && to->kind == TYPE_KIND_PRIMITIVE) {
        if (strcmp(from->name, "Int") == 0
            && strcmp(to->name, "Long") == 0)
            return true;
        if (strcmp(from->name, "Float") == 0
            && strcmp(to->name, "Double") == 0)
            return true;
    }

    if (from->kind == TYPE_KIND_ENUM && to->kind == TYPE_KIND_PRIMITIVE
        && strcmp(to->name, "Int") == 0)
        return true;

    if (from->kind == TYPE_KIND_CLASS
        && to->kind == TYPE_KIND_CONSTRUCTED
        && to->data.constructed.constructor != NULL
        && to->data.constructed.constructor->kind == TYPE_KIND_CLASS
        && from->name != NULL && to->data.constructed.constructor->name != NULL
        && strcmp(from->name, to->data.constructed.constructor->name) == 0)
        return true;

    return false;
}

bool
type_satisfies_constraint(const Type *type, const Type *constraint)
{
    if (type == NULL || constraint == NULL)
        return false;

    if (type_equals(type, constraint))
        return true;

    if (constraint->kind == TYPE_KIND_CLASS)
        return type_equals(type, constraint);

    return false;
}

Type *
type_instantiate(Type *generic_type, Type **type_args, size_t arg_count)
{
    if (generic_type == NULL || arg_count == 0)
        return generic_type;

    if (generic_type->kind == TYPE_KIND_GENERIC && arg_count >= 1)
        return type_args[0];

    if (generic_type->kind == TYPE_KIND_CONSTRUCTED) {
        size_t n = generic_type->data.constructed.arg_count;
        Type **new_args = malloc(n * sizeof(Type *));
        Type *result;
        if (new_args == NULL)
            return NULL;

        for (size_t i = 0; i < n; i++) {
            new_args[i] = type_instantiate(
                generic_type->data.constructed.args[i],
                type_args, arg_count);
        }
        result = type_create_constructed(
            generic_type->data.constructed.constructor,
            new_args, n);
        free(new_args);
        return result;
    }

    return generic_type;
}
