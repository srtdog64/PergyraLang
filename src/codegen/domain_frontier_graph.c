/*
 * Copyright (c) 2026 Pergyra Language Project
 * Codegen-side graph-derived frontier pass limits.
 */
#include "domain_frontier_graph.h"

#include "../compiler/propagation_graph_build.h"

#include <stdio.h>
#include <stdlib.h>

size_t
pgy_codegen_world_frontier_graph_pass_limit(const ASTNode *world,
                                            const char *world_name,
                                            size_t count_floor)
{
    PropagationGraph *g = propagation_graph_create();
    size_t limit = count_floor;

    if (g != NULL
        && propagation_graph_build_from_world(g, world)
        && propagation_graph_schedule(g)
        && g->pass_limit > limit) {
        limit = g->pass_limit;
    }
    if (g != NULL && getenv("PGY_DUMP_PROPAGATION") != NULL)
        propagation_graph_dump(g, stderr, world_name);
    propagation_graph_destroy(g);
    return limit;
}

size_t
pgy_codegen_world_frontier_graph_pass_limit_from_header(
    const MIRDeclHeader *header,
    const char *world_name,
    size_t count_floor)
{
    PropagationGraph *g = propagation_graph_create();
    size_t limit = count_floor;

    if (g != NULL
        && propagation_graph_build_from_world_header(g, header)
        && propagation_graph_schedule(g)
        && g->pass_limit > limit) {
        limit = g->pass_limit;
    }
    if (g != NULL && getenv("PGY_DUMP_PROPAGATION") != NULL)
        propagation_graph_dump(g, stderr, world_name);
    propagation_graph_destroy(g);
    return limit;
}

bool
pgy_codegen_zone_frontier_graph_pass_limit_from_mir(
    const MIRProgram *mir,
    const char *zone_name,
    size_t count_floor,
    size_t *limit_out)
{
    PropagationGraph *g;
    size_t limit = count_floor;
    bool ok;

    if (limit_out == NULL)
        return false;
    g = propagation_graph_create();
    if (g == NULL)
        return false;
    ok = propagation_graph_build_from_zone_mir(g, mir, zone_name)
        && propagation_graph_schedule(g);
    if (ok && g->pass_limit > limit) {
        limit = g->pass_limit;
    }
    if (ok && getenv("PGY_DUMP_PROPAGATION") != NULL)
        propagation_graph_dump(g, stderr, zone_name);
    propagation_graph_destroy(g);
    if (!ok)
        return false;
    *limit_out = limit;
    return true;
}
