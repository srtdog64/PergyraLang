/*
 * Copyright (c) 2026 Pergyra Language Project
 * Build the transitive propagation dependency graph from owner facts.
 *
 * Edge direction is "from must be recomputed before to" (from -> to means
 * to depends on from): refresh source -> object, zone effect source -> target,
 * link endpoints -> relation slot, and world-state inputs -> derived state.
 */
#include "propagation_graph.h"
#include "propagation_graph_build.h"
#include "mir_decl_headers.h"

#include "../parser/ast_domain_api.h"

#include <string.h>

static bool
prop_add_named_edge(PropagationGraph *g, const char *from, const char *to)
{
    size_t a;
    size_t b;

    if (g == NULL || from == NULL || to == NULL)
        return false;
    a = propagation_graph_intern_node(g, from);
    b = propagation_graph_intern_node(g, to);
    if (a == (size_t)-1 || b == (size_t)-1)
        return false;
    return propagation_graph_add_edge(g, a, b);
}

bool
propagation_graph_build_from_zone_mir(PropagationGraph *g,
                                      const MIRProgram *mir,
                                      const char *zone_name)
{
    if (g == NULL || mir == NULL || zone_name == NULL
        || !mir->has_domain_topology) {
        return false;
    }
    for (size_t i = 0; i < mir->domain_topology_row_count; i++) {
        const MIRDomainTopologyRow *row = &mir->domain_topology_rows[i];
        if (row->owner_name == NULL
            || strcmp(row->owner_name, zone_name) != 0) {
            continue;
        }
        switch (row->kind) {
        case MIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH:
        case MIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH:
        case MIR_DOMAIN_TOPOLOGY_PROJECTION_BIND:
            if (!prop_add_named_edge(g,
                    row->source_slot_name,
                    row->projection_slot_name)) {
                return false;
            }
            break;
        case MIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT:
            if (!prop_add_named_edge(g,
                    row->layer_slot_name,
                    row->target_slot_name)) {
                return false;
            }
            break;
        case MIR_DOMAIN_TOPOLOGY_LINK_RELATION:
            if (!prop_add_named_edge(g,
                    row->left_slot_name,
                    row->layer_slot_name)
                || !prop_add_named_edge(g,
                    row->right_slot_name,
                    row->layer_slot_name)) {
                return false;
            }
            break;
        default:
            return false;
        }
    }
    return true;
}

bool
propagation_graph_build_from_world(PropagationGraph *g, const ASTNode *world)
{
    ASTNode **states;
    size_t count;

    if (g == NULL || world == NULL)
        return false;

    count = 0;
    states = ast_world_states(world, &count);
    for (size_t i = 0; states != NULL && i < count; i++) {
        const char *derived = ast_world_state_name(states[i]);
        size_t input_count = ast_world_state_input_count(states[i]);
        for (size_t k = 0; k < input_count; k++) {
            if (!prop_add_named_edge(g,
                    ast_world_state_input_name(states[i], k), derived)) {
                return false;
            }
        }
    }
    return true;
}

bool
propagation_graph_build_from_world_header(PropagationGraph *g,
                                          const MIRDeclHeader *header)
{
    if (g == NULL || header == NULL)
        return false;

    for (size_t i = 0; i < mir_decl_header_world_state_count(header); i++) {
        const MIRDeclWorldState *state =
            mir_decl_header_world_state(header, i);
        const char *derived = mir_decl_world_state_name(state);
        size_t input_count = mir_decl_world_state_input_count(state);

        for (size_t k = 0; k < input_count; k++) {
            if (!prop_add_named_edge(g,
                    mir_decl_world_state_input_name(state, k), derived)) {
                return false;
            }
        }
    }
    return true;
}
