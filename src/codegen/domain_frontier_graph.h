/*
 * Copyright (c) 2026 Pergyra Language Project
 * Codegen-side graph-derived frontier pass limits.
 *
 * domain_frontier_policy stays AST-free (counts in, limits out). The AST
 * dependency for building the transitive propagation graph lives here, in the
 * codegen layer, so the policy contract gate is preserved. Each helper returns
 * max(graph SCC-weighted longest dependency chain, count-based floor) so a deep
 * transitive chain is never under-iterated, while simple cases keep the floor.
 */
#ifndef PERGYRA_DOMAIN_FRONTIER_GRAPH_H
#define PERGYRA_DOMAIN_FRONTIER_GRAPH_H

#include <stddef.h>

#include "../parser/ast.h"

size_t pgy_codegen_world_frontier_graph_pass_limit(const ASTNode *world,
                                                   const char *world_name,
                                                   size_t count_floor);
size_t pgy_codegen_zone_frontier_graph_pass_limit(const ASTNode *zone,
                                                  const char *zone_name,
                                                  size_t count_floor);

#endif /* PERGYRA_DOMAIN_FRONTIER_GRAPH_H */
