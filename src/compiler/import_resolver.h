/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_IMPORT_RESOLVER_H
#define PGY_IMPORT_RESOLVER_H

#include "../parser/ast.h"

ASTNode *import_resolver_load_program(const char *source_path, char **error_message);

#endif /* PGY_IMPORT_RESOLVER_H */
