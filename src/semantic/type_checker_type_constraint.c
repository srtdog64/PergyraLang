#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
type_constraint_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

char *
format_type_constraint_bounds(TypeConstraint *tc)
{
    char *result = NULL;

    if (tc == NULL || tc->bound_count == 0)
        return type_constraint_strdup_fmt("<constraint>");

    for (size_t i = 0; i < tc->bound_count; i++) {
        ASTNode *bound = tc->bounds[i];
        const char *bound_name =
            (bound != NULL
             && bound->type == AST_TYPE
             && bound->data.type.name != NULL)
                ? bound->data.type.name
                : "<constraint>";
        char *next;

        if (result == NULL) {
            result = type_constraint_strdup_fmt("%s", bound_name);
        } else {
            next = type_constraint_strdup_fmt("%s + %s", result, bound_name);
            free(result);
            result = next;
        }

        if (result == NULL)
            return type_constraint_strdup_fmt("<constraint>");
    }

    return result;
}
