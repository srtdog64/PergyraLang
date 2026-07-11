/*
 * Copyright (c) 2025 Pergyra Language Project
 * Module-boundary payload rows carried by ASTNode.
 */

#ifndef PERGYRA_AST_MODULE_DATA_H
#define PERGYRA_AST_MODULE_DATA_H

#include "ast_types.h"

typedef struct {
    char *path;
} ASTImportDeclData;

typedef struct {
    char *module_name;
} ASTUseDeclData;

typedef struct {
    char    *name;
    ASTNode **statements;
    size_t   count;
    size_t   capacity;
} ASTNamespaceDeclData;

#endif /* PERGYRA_AST_MODULE_DATA_H */
