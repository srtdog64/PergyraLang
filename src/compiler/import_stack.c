#include "import_resolver_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

bool
import_stack_push(ImportStack *stack, const char *path)
{
    if (stack->count == stack->capacity) {
        size_t next = 8;
        if (stack->capacity != 0) {
            if (stack->capacity > SIZE_MAX / 2)
                return false;
            next = stack->capacity * 2;
        }
        if (next > SIZE_MAX / sizeof(char *))
            return false;
        char **grown = realloc(stack->paths, next * sizeof(char *));
        if (grown == NULL)
            return false;
        stack->paths = grown;
        stack->capacity = next;
    }
    stack->paths[stack->count] = pergyra_strdup(path);
    if (stack->paths[stack->count] == NULL)
        return false;
    stack->count++;
    return true;
}

bool
import_stack_contains(const ImportStack *stack, const char *path)
{
    for (size_t i = 0; i < stack->count; i++) {
        if (strcmp(stack->paths[i], path) == 0)
            return true;
    }
    return false;
}

void
import_stack_pop(ImportStack *stack)
{
    if (stack->count == 0)
        return;
    free(stack->paths[stack->count - 1]);
    stack->count--;
}

void
import_stack_destroy(ImportStack *stack)
{
    while (stack->count > 0)
        import_stack_pop(stack);
    free(stack->paths);
    stack->paths = NULL;
    stack->capacity = 0;
}
