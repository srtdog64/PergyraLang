#ifndef PGY_IMPORT_RESOLVER_INTERNAL_H
#define PGY_IMPORT_RESOLVER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    char  **paths;
    size_t  count;
    size_t  capacity;
} ImportStack;

bool import_stack_push(ImportStack *stack, const char *path);
bool import_stack_contains(const ImportStack *stack, const char *path);
void import_stack_pop(ImportStack *stack);
void import_stack_destroy(ImportStack *stack);

char *import_resolver_canonicalize_path_dup(const char *path);
/* Existing-file identity only: no lexical or unresolved-path fallback. */
char *import_resolver_existing_final_identity_path_dup(const char *path);
char *import_resolver_resolve_stdlib_module_path(const char *source_path,
                                                 const char *module_name);

#endif /* PGY_IMPORT_RESOLVER_INTERNAL_H */
