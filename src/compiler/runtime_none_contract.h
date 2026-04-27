/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_RUNTIME_NONE_CONTRACT_H
#define PGY_RUNTIME_NONE_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>

#include "parser/ast.h"

bool runtime_none_validate_ast(const ASTNode *ast, char *message, size_t message_size);

#endif /* PGY_RUNTIME_NONE_CONTRACT_H */
