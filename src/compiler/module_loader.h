/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Module loader — parse source files, normalize namespace/export names,
 * and inline imported modules into a single AST_PROGRAM.
 */

#ifndef PGY_MODULE_LOADER_H
#define PGY_MODULE_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../parser/ast.h"

typedef struct
{
    uint64_t module_id;
    char *canonical_path;
    uint32_t first_syntax_id;
    size_t statement_count;
} PgySourceModuleRow;

typedef struct
{
    uint64_t graph_id;
    PgySourceModuleRow *rows;
    size_t row_count;
    size_t row_capacity;
} PgySourceModuleGraph;

ASTNode *module_loader_load_program_with_graph(const char *source_path,
                                               PgySourceModuleGraph **graph_out,
                                               char **error_message);
bool module_loader_validate_graph(const PgySourceModuleGraph *graph,
                                  char **error_message);
void module_loader_destroy_graph(PgySourceModuleGraph *graph);

#endif /* PGY_MODULE_LOADER_H */
