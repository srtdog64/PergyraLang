/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_IMPORT_RESOLVER_H
#define PGY_IMPORT_RESOLVER_H

#include "../parser/ast.h"

typedef struct {
    char *canonical_path;
    char *text;
} ImportSourceInput;

typedef struct {
    ImportSourceInput *rows;
    size_t count;
    size_t capacity;
} ImportSourceSnapshot;

ASTNode *import_resolver_load_program(const char *source_path, char **error_message);
/* Retain the actual parsed buffers, including modules with no declarations.
 * A successful load transfers them to the initially empty snapshot; failure
 * destroys the partial snapshot. NULL keeps the ordinary load's lifetime. */
ASTNode *import_resolver_load_program_with_snapshot(const char *source_path,
    char **error_message, ImportSourceSnapshot *snapshot);
/* Recompose one program from the exact buffers retained by the initial load.
 * Every retained input must be consumed exactly once; no filesystem fallback
 * is permitted for a missing path. */
ASTNode *import_resolver_load_program_from_snapshot(const char *source_path,
    const ImportSourceSnapshot *snapshot, char **error_message);
void import_resolver_destroy_source_snapshot(ImportSourceSnapshot *snapshot);

#endif /* PGY_IMPORT_RESOLVER_H */
