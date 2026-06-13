/*
 * Copyright (c) 2026 Pergyra Language Project
 * Transitive frontier propagation dependency graph and schedule.
 *
 * Nodes are domain state slots (zone states, layers, world-derived states).
 * A directed edge from -> to means "to" must be recomputed after "from"
 * changes (refresh source->object, world state input->state, zone effect
 * source->target, links). The schedule drives propagation in dependency
 * order, replacing the count-bounded "re-run all clauses N times" policy.
 */
#ifndef PERGYRA_PROPAGATION_GRAPH_H
#define PERGYRA_PROPAGATION_GRAPH_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t from;
    size_t to;
} PropEdge;

typedef struct {
    char    **node_names;     /* owned; node_names[i] is node i */
    size_t    node_count;
    PropEdge *edges;          /* owned */
    size_t    edge_count;
    size_t    edge_capacity;

    /* Computed by propagation_graph_schedule. */
    size_t   *order;          /* owned; topo (condensation) order of nodes */
    size_t    order_count;
    size_t   *scc_of;         /* owned; scc_of[i] = SCC id of node i */
    size_t    scc_count;
    size_t    max_scc_size;   /* largest cyclic cluster (fixpoint width) */
    size_t    chain_depth;    /* longest dependency chain in the condensation */
    size_t    pass_limit;     /* graph-derived bound (replaces count bound) */
    bool      has_cycle;
} PropagationGraph;

PropagationGraph *propagation_graph_create(void);
void   propagation_graph_destroy(PropagationGraph *g);
/* Returns the node index for name, creating it on first use. */
size_t propagation_graph_intern_node(PropagationGraph *g, const char *name);
/* Records edge "from affects to" (recompute to after from changes). */
bool   propagation_graph_add_edge(PropagationGraph *g, size_t from, size_t to);
/* Computes SCCs, condensation topo order, chain depth, and pass limit. */
bool   propagation_graph_schedule(PropagationGraph *g);
/* Human-readable dependency/schedule trace for dogfood debugging. */
void   propagation_graph_dump(const PropagationGraph *g, void *out_file,
                            const char *label);

#endif /* PERGYRA_PROPAGATION_GRAPH_H */
