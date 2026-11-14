#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_resolution_graph_internal.h"

static bool
stage_string_eq(const char *lhs, const char *rhs)
{
    if (lhs == rhs)
        return true;
    if (lhs == NULL || rhs == NULL)
        return false;
    return strcmp(lhs, rhs) == 0;
}

static bool
stage_has_graph_dependency_replay(const TypeResolutionGraph *graph,
                                  const ASTNode *consumer_site,
                                  const char *consumer_name,
                                  const char *reason)
{
    if (graph == NULL || consumer_site == NULL || consumer_name == NULL)
        return false;
    for (size_t i = 0; i < graph->node_count; i++) {
        const TypeResolutionNode *node = &graph->nodes[i];
        if (node->kind != TYPE_RES_NODE_TYPE_REF
            || node->site != consumer_site
            || !stage_string_eq(node->label, consumer_name)) {
            continue;
        }
        for (size_t e = 0; e < graph->edge_count; e++) {
            const TypeResolutionEdge *edge = &graph->edges[e];
            if (edge->from == i && stage_string_eq(edge->reason, reason))
                return true;
        }
    }

    return false;
}

static bool
stage_has_graph_dependency(SemanticContext *ctx,
                           const ASTNode *consumer_site,
                           const char *consumer_name,
                           const char *reason)
{
    static int verify = -1;
    size_t node_ordinal = (size_t)-1;
    bool projected = false;

    if (ctx == NULL || consumer_site == NULL || consumer_name == NULL)
        return false;
    TypeResolutionGraph *graph = &ctx->type_resolution_graph;
    if (!type_resolution_node_identity_current(graph,
            TYPE_RES_NODE_TYPE_REF, consumer_site, consumer_name,
            &node_ordinal)) {
        semantic_error(ctx, (ASTNode *)consumer_site,
            "Type-resolution graph identity projection is unavailable");
        return true;
    }
    if (node_ordinal != (size_t)-1) {
        if (node_ordinal >= graph->node_count) {
            semantic_error(ctx, (ASTNode *)consumer_site,
                "Type-resolution graph identity projection is stale");
            return true;
        }
        if (!type_resolution_edge_reason_current(
                graph, node_ordinal, reason, &projected)) {
            semantic_error(ctx, (ASTNode *)consumer_site,
                "Type-resolution graph edge projection is unavailable");
            return true;
        }
    }
    if (verify < 0)
        verify = getenv("PGY_VERIFY_TYPE_RESOLUTION_STAGE_INDEX") != NULL;
    if (verify && projected != stage_has_graph_dependency_replay(
            graph, consumer_site, consumer_name, reason)) {
        semantic_error(ctx, (ASTNode *)consumer_site,
            "Type-resolution graph identity projection diverged from graph rows");
        return true;
    }
    return projected;
}

bool
semantic_stage_should_defer_to_graph(ASTNode *type_node,
                                     SemanticContext *ctx,
                                     const ASTNode *consumer_site,
                                     const char *consumer_name,
                                     const char *reason)
{
    if (type_node == NULL || type_node->type != AST_TYPE)
        return false;
    if (ast_type_name(type_node) == NULL)
        return false;
    return stage_has_graph_dependency(ctx, consumer_site, consumer_name, reason);
}
