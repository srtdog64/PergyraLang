#include "module_normalizer_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

bool
module_rename_scope_add(ModuleRenameScope *scope,
                        const char *old_name,
                        const char *new_name)
{
    char *owned_old_name;
    char *owned_new_name;

    if (scope == NULL || old_name == NULL || new_name == NULL)
        return false;

    if (scope->count == scope->capacity) {
        size_t next = 8;
        if (scope->capacity != 0) {
            if (scope->capacity > SIZE_MAX / 2)
                return false;
            next = scope->capacity * 2;
        }
        if (next > SIZE_MAX / sizeof(ModuleRenameEntry))
            return false;
        ModuleRenameEntry *grown =
            realloc(scope->entries, next * sizeof(ModuleRenameEntry));
        if (grown == NULL)
            return false;
        scope->entries = grown;
        scope->capacity = next;
    }
    owned_old_name = pergyra_strdup(old_name);
    owned_new_name = pergyra_strdup(new_name);
    if (owned_old_name == NULL || owned_new_name == NULL) {
        free(owned_old_name);
        free(owned_new_name);
        return false;
    }
    scope->entries[scope->count].old_name = owned_old_name;
    scope->entries[scope->count].new_name = owned_new_name;
    scope->count++;
    return true;
}

void
module_rename_scope_destroy(ModuleRenameScope *scope)
{
    if (scope == NULL)
        return;
    for (size_t i = 0; i < scope->count; i++) {
        free(scope->entries[i].old_name);
        free(scope->entries[i].new_name);
    }
    free(scope->entries);
    scope->entries = NULL;
    scope->count = 0;
    scope->capacity = 0;
}
