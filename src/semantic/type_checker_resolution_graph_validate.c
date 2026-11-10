#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"

static bool
type_resolution_checked_array_count(size_t count, size_t elem_size)
{
    return elem_size > 0 && count <= SIZE_MAX / elem_size;
}

typedef struct {
    size_t *offsets;
    size_t *edge_ordinals;
    unsigned char *edge_keep;
    size_t unique_edge_count;
} TypeResolutionAdjacency;

static void
type_resolution_adjacency_destroy(TypeResolutionAdjacency *adjacency)
{
    if (adjacency == NULL)
        return;
    free(adjacency->offsets);
    free(adjacency->edge_ordinals);
    free(adjacency->edge_keep);
    *adjacency = (TypeResolutionAdjacency){0};
}

static bool
type_resolution_edge_reasons_equal(const char *lhs, const char *rhs)
{
    if (lhs == rhs)
        return true;
    if (lhs == NULL || rhs == NULL)
        return false;
    return strcmp(lhs, rhs) == 0;
}

/* Projects insertion-ordered edge rows into per-source ranges and keeps the
 * first occurrence of each exact edge.  Collection may report the same fact
 * more than once; uniqueness is sealed here without rescanning the complete
 * graph for every insertion. */
static bool
type_resolution_adjacency_build(const TypeResolutionGraph *graph,
                                TypeResolutionAdjacency *adjacency)
{
    size_t *next = NULL;
    if (graph == NULL || adjacency == NULL || graph->node_count == SIZE_MAX ||
        !type_resolution_checked_array_count(graph->node_count + 1,
            sizeof(*adjacency->offsets)) ||
        !type_resolution_checked_array_count(graph->edge_count,
            sizeof(*adjacency->edge_ordinals)))
        return false;

    adjacency->offsets = calloc(graph->node_count + 1,
        sizeof(*adjacency->offsets));
    adjacency->edge_ordinals = graph->edge_count == 0 ? NULL :
        malloc(graph->edge_count * sizeof(*adjacency->edge_ordinals));
    adjacency->edge_keep = graph->edge_count == 0 ? NULL :
        calloc(graph->edge_count, sizeof(*adjacency->edge_keep));
    next = graph->node_count == 0 ? NULL :
        malloc(graph->node_count * sizeof(*next));
    if (adjacency->offsets == NULL ||
        (graph->edge_count != 0 && adjacency->edge_ordinals == NULL) ||
        (graph->edge_count != 0 && adjacency->edge_keep == NULL) ||
        (graph->node_count != 0 && next == NULL)) {
        free(next);
        type_resolution_adjacency_destroy(adjacency);
        return false;
    }

    for (size_t i = 0; i < graph->edge_count; i++) {
        const TypeResolutionEdge *edge = &graph->edges[i];
        if (edge->from >= graph->node_count || edge->to >= graph->node_count ||
            adjacency->offsets[edge->from + 1] == SIZE_MAX) {
            free(next);
            type_resolution_adjacency_destroy(adjacency);
            return false;
        }
        adjacency->offsets[edge->from + 1]++;
    }
    for (size_t i = 1; i <= graph->node_count; i++) {
        if (adjacency->offsets[i] > SIZE_MAX - adjacency->offsets[i - 1]) {
            free(next);
            type_resolution_adjacency_destroy(adjacency);
            return false;
        }
        adjacency->offsets[i] += adjacency->offsets[i - 1];
    }
    if (graph->node_count != 0)
        memcpy(next, adjacency->offsets,
            graph->node_count * sizeof(*next));
    for (size_t i = 0; i < graph->edge_count; i++)
        adjacency->edge_ordinals[next[graph->edges[i].from]++] = i;

    {
        size_t raw_begin = 0;
        size_t write = 0;
        for (size_t source = 0; source < graph->node_count; source++) {
            size_t raw_end = next[source];
            size_t unique_begin = write;
            adjacency->offsets[source] = write;
            for (size_t i = raw_begin; i < raw_end; i++) {
                size_t ordinal = adjacency->edge_ordinals[i];
                const TypeResolutionEdge *edge = &graph->edges[ordinal];
                bool duplicate = false;
                for (size_t j = unique_begin; j < write; j++) {
                    const TypeResolutionEdge *prior =
                        &graph->edges[adjacency->edge_ordinals[j]];
                    if (prior->to == edge->to &&
                        type_resolution_edge_reasons_equal(
                            prior->reason, edge->reason)) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    adjacency->edge_ordinals[write++] = ordinal;
                    adjacency->edge_keep[ordinal] = 1;
                }
            }
            raw_begin = raw_end;
        }
        adjacency->offsets[graph->node_count] = write;
        adjacency->unique_edge_count = write;
    }
    free(next);
    return true;
}

static void
type_resolution_graph_compact_edges(
    TypeResolutionGraph *graph,
    const TypeResolutionAdjacency *adjacency)
{
    size_t write = 0;

    if (graph == NULL || adjacency == NULL ||
        adjacency->unique_edge_count == graph->edge_count)
        return;

    for (size_t i = 0; i < graph->edge_count; i++) {
        if (adjacency->edge_keep[i] == 0) {
            free(graph->edges[i].reason);
            continue;
        }
        if (write != i)
            graph->edges[write] = graph->edges[i];
        write++;
    }
    graph->edge_count = write;
}

static bool
type_resolution_find_cycle_visit(TypeResolutionGraph *graph,
                                 const TypeResolutionAdjacency *adjacency,
                                 size_t current,
                                 unsigned char *color,
                                 size_t *stack,
                                 size_t *stack_len,
                                 size_t *cycle_path,
                                 size_t *cycle_len,
                                 size_t cycle_cap,
                                 size_t *closing_node)
{
    if (graph == NULL || adjacency == NULL || color == NULL || stack == NULL || stack_len == NULL
        || cycle_path == NULL || cycle_len == NULL || closing_node == NULL) {
        return false;
    }

    color[current] = 1;
    stack[(*stack_len)++] = current;

    for (size_t i = adjacency->offsets[current];
         i < adjacency->offsets[current + 1]; i++) {
        TypeResolutionEdge *edge =
            &graph->edges[adjacency->edge_ordinals[i]];

        if (color[edge->to] == 0) {
            if (type_resolution_find_cycle_visit(graph,
                                                 adjacency,
                                                 edge->to,
                                                 color,
                                                 stack,
                                                 stack_len,
                                                 cycle_path,
                                                 cycle_len,
                                                 cycle_cap,
                                                 closing_node)) {
                return true;
            }
        } else if (color[edge->to] == 1) {
            size_t start = 0;
            while (start < *stack_len && stack[start] != edge->to)
                start++;
            *cycle_len = 0;
            for (size_t j = start; j < *stack_len && *cycle_len < cycle_cap; j++)
                cycle_path[(*cycle_len)++] = stack[j];
            *closing_node = edge->to;
            return true;
        }
    }

    if (*stack_len > 0)
        (*stack_len)--;
    color[current] = 2;
    return false;
}

bool
type_resolution_validate_graph(SemanticContext *ctx)
{
    TypeResolutionGraph *graph;
    unsigned char *color = NULL;
    size_t *stack = NULL;
    size_t *cycle_path = NULL;
    size_t stack_len = 0;
    size_t cycle_len = 0;
    size_t closing_node = (size_t)-1;
    TypeResolutionAdjacency adjacency = {0};
    bool ok = true;

    if (ctx == NULL)
        return false;

    graph = &ctx->type_resolution_graph;
    if (graph->node_count == 0)
        return true;
    if (!type_resolution_checked_array_count(graph->node_count,
            sizeof(unsigned char))
        || !type_resolution_checked_array_count(graph->node_count,
            sizeof(size_t))) {
        return false;
    }
    if (!type_resolution_adjacency_build(graph, &adjacency))
        return false;

    color = calloc(graph->node_count, sizeof(unsigned char));
    stack = calloc(graph->node_count, sizeof(size_t));
    cycle_path = calloc(graph->node_count, sizeof(size_t));
    if (color == NULL || stack == NULL || cycle_path == NULL) {
        free(color);
        free(stack);
        free(cycle_path);
        type_resolution_adjacency_destroy(&adjacency);
        return false;
    }

    for (size_t i = 0; i < graph->node_count; i++) {
        if (color[i] != 0)
            continue;
        stack_len = 0;
        cycle_len = 0;
        closing_node = (size_t)-1;
        if (type_resolution_find_cycle_visit(graph,
                                             &adjacency,
                                             i,
                                             color,
                                             stack,
                                             &stack_len,
                                             cycle_path,
                                             &cycle_len,
                                             graph->node_count,
                                             &closing_node)) {
            ASTNode *site = (ASTNode *)graph->nodes[closing_node].site;
            char *cycle_text = type_resolution_format_cycle(
                graph, cycle_path, cycle_len, closing_node);
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_TYPE_DEPENDENCY_CYCLE,
                PGY_CAUSE_TYPE_RESOLUTION_CYCLE,
                PGY_FIX_BREAK_CYCLE_VIA_INDIRECTION,
                site,
                "Type resolution dependency cycle detected in the semantic graph around '%s'.\n"
                "Contract source:\n"
                "- graph edge provenance: %s\n"
                "Reason:\n"
                "- provider/consumer resolution formed a closed dependency loop\n"
                "- cycle path: %s\n"
                "Fix:\n"
                "- break the generic/alias/ability dependency loop so one edge resolves first\n"
                "- or split the contract into acyclic declarations",
                graph->nodes[closing_node].label != NULL
                    ? graph->nodes[closing_node].label : "<type-ref>",
                cycle_text != NULL ? cycle_text : "<cycle>",
                cycle_text != NULL ? cycle_text : "<cycle>");
            free(cycle_text);
            ok = false;
            break;
        }
    }

    free(color);
    free(stack);
    free(cycle_path);
    if (ok)
        type_resolution_graph_compact_edges(graph, &adjacency);
    type_resolution_adjacency_destroy(&adjacency);
    return ok;
}

bool
type_resolution_build_topo_order(TypeResolutionGraph *graph,
                                 size_t **out_order,
                                 size_t *out_count)
{
    size_t *indegree = NULL;
    size_t *queue = NULL;
    size_t *order = NULL;
    size_t head = 0;
    size_t tail = 0;
    size_t produced = 0;
    TypeResolutionAdjacency adjacency = {0};

    if (out_order != NULL)
        *out_order = NULL;
    if (out_count != NULL)
        *out_count = 0;
    if (graph == NULL)
        return false;
    if (graph->node_count == 0)
        return true;
    if (!type_resolution_checked_array_count(graph->node_count,
            sizeof(size_t))) {
        return false;
    }
    if (!type_resolution_adjacency_build(graph, &adjacency))
        return false;

    indegree = calloc(graph->node_count, sizeof(size_t));
    queue = calloc(graph->node_count, sizeof(size_t));
    order = calloc(graph->node_count, sizeof(size_t));
    if (indegree == NULL || queue == NULL || order == NULL) {
        free(indegree);
        free(queue);
        free(order);
        type_resolution_adjacency_destroy(&adjacency);
        return false;
    }

    for (size_t i = 0; i < adjacency.unique_edge_count; i++) {
        const TypeResolutionEdge *edge =
            &graph->edges[adjacency.edge_ordinals[i]];
        indegree[edge->to]++;
    }

    for (size_t i = 0; i < graph->node_count; i++) {
        if (indegree[i] == 0)
            queue[tail++] = i;
    }

    while (head < tail) {
        size_t node = queue[head++];
        order[produced++] = node;
        for (size_t i = adjacency.offsets[node];
             i < adjacency.offsets[node + 1]; i++) {
            TypeResolutionEdge *edge =
                &graph->edges[adjacency.edge_ordinals[i]];
            if (indegree[edge->to] > 0)
                indegree[edge->to]--;
            if (indegree[edge->to] == 0)
                queue[tail++] = edge->to;
        }
    }

    free(indegree);
    free(queue);
    type_resolution_adjacency_destroy(&adjacency);

    if (produced != graph->node_count) {
        free(order);
        return false;
    }

    if (out_order != NULL)
        *out_order = order;
    else
        free(order);
    if (out_count != NULL)
        *out_count = produced;
    return true;
}
