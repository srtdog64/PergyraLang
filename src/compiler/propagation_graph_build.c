/*
 * Copyright (c) 2026 Pergyra Language Project
 * Build the transitive propagation dependency graph from domain AST.
 *
 * Edge direction is "from must be recomputed before to" (from -> to means
 * to depends on from): refresh source -> object, zone effect source -> target,
 * link endpoints -> relation slot, and world-state inputs -> derived state.
 */
#include "propagation_graph.h"
#include "propagation_graph_build.h"

#include "../parser/ast_domain_api.h"

static void
prop_add_named_edge(PropagationGraph *g, const char *from, const char *to)
{
    size_t a;
    size_t b;

    if (g == NULL || from == NULL || to == NULL)
        return;
    a = propagation_graph_intern_node(g, from);
    b = propagation_graph_intern_node(g, to);
    if (a == (size_t)-1 || b == (size_t)-1)
        return;
    propagation_graph_add_edge(g, a, b);
}

bool
propagation_graph_build_from_zone(PropagationGraph *g, const ASTNode *zone)
{
    ASTNode **rows;
    size_t count;

    if (g == NULL || zone == NULL)
        return false;

    count = 0;
    rows = ast_zone_refreshes(zone, &count);
    for (size_t i = 0; rows != NULL && i < count; i++) {
        prop_add_named_edge(g,
            ast_zone_refresh_source_slot_name(rows[i]),
            ast_zone_refresh_object_slot_name(rows[i]));
    }

    count = 0;
    rows = ast_zone_maintained_effects(zone, &count);
    for (size_t i = 0; rows != NULL && i < count; i++) {
        prop_add_named_edge(g,
            ast_zone_effect_slot_name(rows[i]),
            ast_zone_effect_target_slot_name(rows[i]));
    }

    count = 0;
    rows = ast_zone_links(zone, &count);
    for (size_t i = 0; rows != NULL && i < count; i++) {
        const char *relation = ast_zone_relation_slot_name(rows[i]);
        prop_add_named_edge(g,
            ast_zone_relation_left_slot_name(rows[i]), relation);
        prop_add_named_edge(g,
            ast_zone_relation_right_slot_name(rows[i]), relation);
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
            prop_add_named_edge(g,
                ast_world_state_input_name(states[i], k), derived);
        }
    }
    return true;
}
