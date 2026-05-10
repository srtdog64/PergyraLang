#include "module_normalizer_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

bool
module_shadow_push(ModuleShadowNames *shadow, const char *name)
{
    if (name == NULL)
        return true;
    if (shadow->count == shadow->capacity) {
        size_t next = 8;
        if (shadow->capacity != 0) {
            if (shadow->capacity > SIZE_MAX / 2)
                return false;
            next = shadow->capacity * 2;
        }
        if (next > SIZE_MAX / sizeof(char *))
            return false;
        char **grown = realloc(shadow->names, next * sizeof(char *));
        if (grown == NULL)
            return false;
        shadow->names = grown;
        shadow->capacity = next;
    }
    shadow->names[shadow->count++] = pergyra_strdup(name);
    return shadow->names[shadow->count - 1] != NULL;
}

bool
module_shadow_contains(const ModuleShadowNames *shadow, const char *name)
{
    for (size_t i = shadow->count; i > 0; i--) {
        if (strcmp(shadow->names[i - 1], name) == 0)
            return true;
    }
    return false;
}

void
module_shadow_pop_to(ModuleShadowNames *shadow, size_t saved_count)
{
    while (shadow->count > saved_count) {
        free(shadow->names[shadow->count - 1]);
        shadow->count--;
    }
}

void
module_shadow_destroy(ModuleShadowNames *shadow)
{
    module_shadow_pop_to(shadow, 0);
    free(shadow->names);
    shadow->names = NULL;
    shadow->capacity = 0;
}
