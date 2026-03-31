/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_MODULE_NORMALIZER_H
#define PGY_MODULE_NORMALIZER_H

#include <stdbool.h>

#include "../parser/ast.h"

bool module_normalize_ast(ASTNode *program, bool imported, const char *private_prefix);

#endif /* PGY_MODULE_NORMALIZER_H */
