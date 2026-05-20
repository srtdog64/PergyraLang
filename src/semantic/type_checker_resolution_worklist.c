/*
 * Copyright (c) 2026 Pergyra Language Project
 * Type-resolution DAG worklist execution owner.
 */

#include "type_checker_resolution_graph_internal.h"
#include "type_checker_internal.h"

void
semantic_run_type_resolution_worklist(ASTNode *program,
                                      SemanticContext *ctx,
                                      size_t *topo_order,
                                      size_t topo_count)
{
    TypeResolutionGraph *graph;

    if (program == NULL || ctx == NULL || topo_order == NULL)
        return;

    graph = &ctx->type_resolution_graph;
    for (size_t i = topo_count; i > 0; i--) {
        size_t node_index = topo_order[i - 1];
        TypeResolutionNode *node;
        ASTNode *decl;
        ASTNode *host_decl;

        if (node_index >= graph->node_count)
            continue;
        node = &graph->nodes[node_index];
        if (node->kind == TYPE_RES_NODE_DECL
            || node->kind == TYPE_RES_NODE_ALIAS) {
            decl = semantic_find_top_level_decl_by_label(program,
                                                         node->label,
                                                         node->kind);
            if (decl == NULL)
                continue;

            semantic_stage_top_level_decl(decl, ctx);
            continue;
        }

        if (node->kind == TYPE_RES_NODE_LOCAL_CONTRACT
            || node->kind == TYPE_RES_NODE_PROJECTION_PATH) {
            host_decl = semantic_find_graph_host_decl(program, node->label);
            if (host_decl == NULL)
                continue;
            if (host_decl->type == AST_WORLD_DECL)
                semantic_stage_world_local_contract_from_label(host_decl,
                                                               node->label,
                                                               ctx);
            else if (host_decl->type == AST_ZONE_DECL)
                semantic_stage_zone_local_contract_from_label(host_decl,
                                                              node->label,
                                                              ctx);
        }
    }
}
