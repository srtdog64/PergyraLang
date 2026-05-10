#ifndef PGY_MODULE_NORMALIZER_INTERNAL_H
#define PGY_MODULE_NORMALIZER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"

typedef struct
{
    char *old_name;
    char *new_name;
} ModuleRenameEntry;

typedef struct ModuleRenameScope
{
    struct ModuleRenameScope *parent;
    ModuleRenameEntry        *entries;
    size_t                    count;
    size_t                    capacity;
} ModuleRenameScope;

typedef struct
{
    char  **names;
    size_t  count;
    size_t  capacity;
} ModuleShadowNames;

bool module_rename_scope_add(ModuleRenameScope *scope,
                             const char *old_name,
                             const char *new_name);
void module_rename_scope_destroy(ModuleRenameScope *scope);
bool module_shadow_push(ModuleShadowNames *shadow, const char *name);
bool module_shadow_contains(const ModuleShadowNames *shadow, const char *name);
void module_shadow_pop_to(ModuleShadowNames *shadow, size_t saved_count);
void module_shadow_destroy(ModuleShadowNames *shadow);
void module_normalizer_normalize_node_refs(ASTNode *node,
                                           ModuleRenameScope *scope,
                                           ModuleShadowNames *shadow);

#endif /* PGY_MODULE_NORMALIZER_INTERNAL_H */
