#include <stdbool.h>
#include <string.h>

#include "type_checker_internal.h"

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
stage_has_graph_dependency(SemanticContext *ctx,
                           const ASTNode *consumer_site,
                           const char *consumer_name,
                           const char *reason)
{
    TypeResolutionGraph *graph;

    if (ctx == NULL || consumer_site == NULL || consumer_name == NULL)
        return false;

    graph = &ctx->type_resolution_graph;
    for (size_t i = 0; i < graph->node_count; i++) {
        TypeResolutionNode *node = &graph->nodes[i];
        if (node->kind != TYPE_RES_NODE_TYPE_REF
            || node->site != consumer_site
            || !stage_string_eq(node->label, consumer_name)) {
            continue;
        }
        for (size_t e = 0; e < graph->edge_count; e++) {
            TypeResolutionEdge *edge = &graph->edges[e];
            if (edge->from == i && stage_string_eq(edge->reason, reason))
                return true;
        }
    }

    return false;
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
    if (type_node->data.type.name == NULL)
        return false;
    return stage_has_graph_dependency(ctx, consumer_site, consumer_name, reason);
}
