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

void
semantic_stage_record_legacy_family(SemanticContext *ctx,
                                    const char *reason)
{
    if (ctx == NULL)
        return;

    if (reason == NULL) {
        ctx->type_resolution_stage_legacy_other_count++;
    } else if (strstr(reason, "default-type") != NULL
               || strstr(reason, "generic constraint") != NULL
               || strstr(reason, "where-bound") != NULL) {
        ctx->type_resolution_stage_legacy_generic_contract_count++;
    } else if (strstr(reason, "ability consumer") != NULL
               || strstr(reason, "impl ability") != NULL) {
        ctx->type_resolution_stage_legacy_ability_consumer_count++;
    } else if (strstr(reason, "parameter type") != NULL
               || strstr(reason, "return type") != NULL
               || strstr(reason, "payload type") != NULL) {
        ctx->type_resolution_stage_legacy_signature_count++;
    } else if (strstr(reason, "type-alias") != NULL) {
        ctx->type_resolution_stage_legacy_alias_count++;
    } else if (strstr(reason, "slot type") != NULL
               || strstr(reason, "shared field") != NULL
               || strstr(reason, "host-type") != NULL
               || strstr(reason, "extends") != NULL
               || strstr(reason, "involves type") != NULL
               || strstr(reason, "value type") != NULL
               || strstr(reason, "where-type") != NULL
               || strstr(reason, "between-") != NULL
               || strstr(reason, "field type") != NULL) {
        ctx->type_resolution_stage_legacy_domain_contract_count++;
    } else {
        ctx->type_resolution_stage_legacy_other_count++;
    }
}
