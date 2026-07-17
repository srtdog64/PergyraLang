/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "module_loader.h"

#include "import_resolver.h"
#include "import_resolver_internal.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

static uint64_t
module_loader_fingerprint(const char *text)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    const unsigned char *p = (const unsigned char *)(text != NULL ? text : "");
    while (*p != '\0') {
        hash ^= (uint64_t)*p++;
        hash *= UINT64_C(1099511628211);
    }
    return hash != 0 ? hash : UINT64_C(1);
}

static bool
module_loader_graph_add(PgySourceModuleGraph *graph,
                        const char *path,
                        uint32_t syntax_id)
{
    if (graph == NULL || path == NULL || path[0] == '\0')
        return false;
    for (size_t i = 0; i < graph->row_count; i++) {
        PgySourceModuleRow *row = &graph->rows[i];
        if (strcmp(row->canonical_path, path) == 0) {
            row->statement_count++;
            if (row->first_syntax_id == 0)
                row->first_syntax_id = syntax_id;
            return true;
        }
    }
    if (graph->row_count == graph->row_capacity) {
        size_t capacity = graph->row_capacity == 0 ? 4 : graph->row_capacity * 2;
        PgySourceModuleRow *rows = realloc(
            graph->rows, capacity * sizeof(*rows));
        if (rows == NULL)
            return false;
        graph->rows = rows;
        graph->row_capacity = capacity;
    }
    PgySourceModuleRow *row = &graph->rows[graph->row_count++];
    memset(row, 0, sizeof(*row));
    row->canonical_path = pergyra_strdup(path);
    if (row->canonical_path == NULL) {
        graph->row_count--;
        return false;
    }
    row->module_id = module_loader_fingerprint(path);
    row->first_syntax_id = syntax_id;
    row->statement_count = 1;
    graph->graph_id ^= row->module_id + UINT64_C(0x9e3779b97f4a7c15)
        + (graph->graph_id << 6) + (graph->graph_id >> 2);
    return true;
}

static PgySourceModuleGraph *
module_loader_build_graph(const char *source_path, const ASTNode *program)
{
    char *canonical_source = NULL;
    PgySourceModuleGraph *graph = calloc(1, sizeof(*graph));
    if (graph == NULL || source_path == NULL || program == NULL)
        goto fail;
    canonical_source = import_resolver_canonicalize_path_dup(source_path);
    if (canonical_source == NULL)
        goto fail;
    graph->graph_id = module_loader_fingerprint(canonical_source);
    if (!module_loader_graph_add(graph, canonical_source, program->stable_id))
        goto fail;
    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        const ASTNode *statement = ast_program_statement(program, i);
        const char *path = statement != NULL && statement->origin_path != NULL
            ? statement->origin_path : source_path;
        if (!module_loader_graph_add(graph, path,
                                     statement != NULL ? statement->stable_id : 0))
            goto fail;
    }
    if (graph->graph_id == 0)
        graph->graph_id = UINT64_C(1);
    free(canonical_source);
    return graph;
fail:
    free(canonical_source);
    module_loader_destroy_graph(graph);
    return NULL;
}

ASTNode *
module_loader_load_program(const char *source_path, char **error_message)
{
    PgySourceModuleGraph *graph = NULL;
    ASTNode *program = module_loader_load_program_with_graph(
        source_path, &graph, error_message);
    module_loader_destroy_graph(graph);
    return program;
}

ASTNode *
module_loader_load_program_with_graph(const char *source_path,
                                      PgySourceModuleGraph **graph_out,
                                      char **error_message)
{
    ASTNode *program;
    if (graph_out != NULL)
        *graph_out = NULL;
    program = import_resolver_load_program(source_path, error_message);
    if (program == NULL)
        return NULL;
    if (graph_out != NULL) {
        *graph_out = module_loader_build_graph(source_path, program);
        if (*graph_out == NULL) {
            ast_destroy(program);
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "module loader could not anchor the source module graph");
            return NULL;
        }
    }
    return program;
}

bool
module_loader_validate_graph(const PgySourceModuleGraph *graph,
                             char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (graph == NULL || graph->graph_id == 0 || graph->row_count == 0
        || graph->rows == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "module graph is missing its stable source anchor");
        return false;
    }
    for (size_t i = 0; i < graph->row_count; i++) {
        const PgySourceModuleRow *row = &graph->rows[i];
        if (row->module_id == 0 || row->first_syntax_id == 0
            || row->canonical_path == NULL
            || row->canonical_path[0] == '\0' || row->statement_count == 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "module graph contains an unanchored source row");
            return false;
        }
        for (size_t j = i + 1; j < graph->row_count; j++) {
            if (graph->rows[j].module_id == row->module_id
                || strcmp(graph->rows[j].canonical_path,
                          row->canonical_path) == 0) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "module graph contains duplicate source identity");
                return false;
            }
        }
    }
    return true;
}

void
module_loader_destroy_graph(PgySourceModuleGraph *graph)
{
    if (graph == NULL)
        return;
    for (size_t i = 0; i < graph->row_count; i++)
        free(graph->rows[i].canonical_path);
    free(graph->rows);
    free(graph);
}
