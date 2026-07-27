/*
 * Copyright (c) 2026 Pergyra Language Project
 * Codegen-side graph-derived frontier pass limits.
 *
 * domain_frontier_policy stays owner-neutral (counts in, limits out). World
 * migration still has an AST/header bridge; zone topology is carried from its
 * DIR owner through MIR and must not be reconstructed in a backend. Each
 * helper returns max(graph SCC-weighted longest dependency chain, count-based
 * floor) so a deep transitive chain is never under-iterated, while simple cases
 * keep the floor.
 */
#ifndef PERGYRA_DOMAIN_FRONTIER_GRAPH_H
#define PERGYRA_DOMAIN_FRONTIER_GRAPH_H

#include <stddef.h>

#include "../compiler/mir.h"
#include "../parser/ast.h"

size_t pgy_codegen_world_frontier_graph_pass_limit(const ASTNode *world,
                                                   const char *world_name,
                                                   size_t count_floor);
size_t pgy_codegen_world_frontier_graph_pass_limit_from_header(
    const MIRDeclHeader *header,
    const char *world_name,
    size_t count_floor);
bool pgy_codegen_zone_frontier_graph_pass_limit_from_mir(
    const MIRProgram *mir,
    const char *zone_name,
    size_t count_floor,
    size_t *limit_out);

#endif /* PERGYRA_DOMAIN_FRONTIER_GRAPH_H */
