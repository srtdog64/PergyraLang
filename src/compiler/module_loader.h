/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Module loader — parse source files, normalize namespace/export names,
 * and inline imported modules into a single AST_PROGRAM.
 */

#ifndef PGY_MODULE_LOADER_H
#define PGY_MODULE_LOADER_H

#include "../parser/ast.h"

ASTNode *module_loader_load_program(const char *source_path, char **error_message);

#endif /* PGY_MODULE_LOADER_H */
