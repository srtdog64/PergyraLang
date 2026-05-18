#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "type_system.h"

Type *
type_create_tuple(Type **elements, size_t element_count)
{
    Type *t = calloc(1, sizeof(Type));
    if (t == NULL)
        return NULL;

    t->kind = TYPE_KIND_TUPLE;
    if (element_count > 0 && elements == NULL) {
        free(t);
        return NULL;
    }

    /* Name: "(T0, T1, T2)" */
    size_t name_len = 3; /* "()" + '\0' */
    for (size_t i = 0; i < element_count; i++) {
        const char *en = (elements[i] != NULL && elements[i]->name != NULL)
                            ? elements[i]->name : "?";
        size_t elem_len = strlen(en);
        if (name_len > SIZE_MAX - elem_len) {
            free(t);
            return NULL;
        }
        name_len += elem_len;
        if (i + 1 < element_count) {
            if (name_len > SIZE_MAX - 2) {
                free(t);
                return NULL;
            }
            name_len += 2; /* ", " */
        }
    }

    t->name = malloc(name_len);
    if (t->name == NULL) {
        free(t);
        return NULL;
    }
    {
        size_t offset = 0;
        t->name[offset++] = '(';
        for (size_t i = 0; i < element_count; i++) {
            const char *en = (elements[i] != NULL && elements[i]->name != NULL)
                                ? elements[i]->name : "?";
            size_t elen = strlen(en);
            memcpy(t->name + offset, en, elen);
            offset += elen;
            if (i + 1 < element_count) {
                t->name[offset++] = ',';
                t->name[offset++] = ' ';
            }
        }
        t->name[offset++] = ')';
        t->name[offset] = '\0';
    }

    t->data.tuple.element_count = element_count;
    t->data.tuple.elements = (element_count > 0)
        ? calloc(element_count, sizeof(Type *))
        : NULL;
    if (element_count > 0 && t->data.tuple.elements == NULL) {
        free(t->name);
        free(t);
        return NULL;
    }
    if (element_count > 0 && elements != NULL)
        memcpy(t->data.tuple.elements, elements, element_count * sizeof(Type *));
    return t;
}

bool
type_is_tuple(const Type *t)
{
    return t != NULL && t->kind == TYPE_KIND_TUPLE;
}

size_t
type_tuple_arity(const Type *t)
{
    if (!type_is_tuple(t))
        return 0;
    return t->data.tuple.element_count;
}

Type *
type_tuple_get_element(const Type *t, size_t index)
{
    if (!type_is_tuple(t))
        return NULL;
    if (index >= t->data.tuple.element_count)
        return NULL;
    return t->data.tuple.elements[index];
}
