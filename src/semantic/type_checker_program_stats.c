#include "type_checker_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
semantic_count_type_resolution_kinds(TypeResolutionGraph *graph,
                                     size_t kind_counts[7])
{
    if (graph == NULL || kind_counts == NULL)
        return;
    for (size_t i = 0; i < graph->node_count; i++) {
        int kind = (int)graph->nodes[i].kind;
        if (kind >= 0 && kind < 7)
            kind_counts[kind]++;
    }
}

static size_t
semantic_count_duplicate_type_resolution_labels(TypeResolutionGraph *graph)
{
    size_t duplicates = 0;

    if (graph == NULL)
        return 0;
    for (size_t i = 0; i < graph->node_count; i++) {
        const char *left = graph->nodes[i].label;
        if (left == NULL)
            continue;
        for (size_t j = i + 1; j < graph->node_count; j++) {
            const char *right = graph->nodes[j].label;
            if (right != NULL && strcmp(left, right) == 0) {
                duplicates++;
                break;
            }
        }
    }
    return duplicates;
}

static size_t *
semantic_compute_type_resolution_indeg(TypeResolutionGraph *graph)
{
    size_t *indeg;

    if (graph == NULL)
        return NULL;
    indeg = calloc(graph->node_count > 0 ? graph->node_count : 1,
                   sizeof(size_t));
    if (indeg == NULL)
        return NULL;
    for (size_t i = 0; i < graph->edge_count; i++) {
        if (graph->edges[i].to < graph->node_count)
            indeg[graph->edges[i].to]++;
    }
    return indeg;
}

static size_t
semantic_count_owned_type_resolution_metadata(SemanticContext *ctx)
{
    size_t count = 0;

    if (ctx == NULL)
        return 0;
    for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
        if (ctx->type_resolution_metadata.owned[i])
            count++;
    }
    return count;
}

static void
semantic_print_top_indeg(TypeResolutionGraph *graph, size_t *indeg)
{
    if (graph == NULL || indeg == NULL || graph->node_count == 0)
        return;
    for (int rank = 0; rank < 5; rank++) {
        size_t best = 0;
        size_t best_val = 0;
        bool found = false;
        for (size_t i = 0; i < graph->node_count; i++) {
            if (indeg[i] > best_val) {
                best = i;
                best_val = indeg[i];
                found = true;
            }
        }
        if (!found || best_val == 0)
            break;
        fprintf(stderr, "[type-res-stats] top-indeg[%d] %s (in=%llu)\n",
                rank,
                graph->nodes[best].label != NULL
                    ? graph->nodes[best].label
                    : "<?>",
                (unsigned long long)best_val);
        indeg[best] = 0;
    }
}

void
semantic_maybe_print_type_resolution_stats(SemanticContext *ctx)
{
    const char *stats_env = getenv("PGY_TYPE_RES_STATS");
    TypeResolutionGraph *graph;
    size_t kind_counts[7] = {0};
    size_t *indeg = NULL;
    size_t *topo = NULL;
    size_t topo_count = 0;
    bool topo_ok;
    size_t metadata_owned_count;
    size_t cache_total;

    if (ctx == NULL || stats_env == NULL || stats_env[0] == '\0'
        || stats_env[0] == '0') {
        return;
    }

    graph = &ctx->type_resolution_graph;
    semantic_count_type_resolution_kinds(graph, kind_counts);
    indeg = semantic_compute_type_resolution_indeg(graph);
    topo_ok = type_resolution_build_topo_order(graph, &topo, &topo_count);
    metadata_owned_count = semantic_count_owned_type_resolution_metadata(ctx);

    fprintf(stderr,
            "[type-res-stats] nodes=%llu edges=%llu duplicate_labels=%llu topo_ok=%d topo_produced=%llu/%llu\n",
            (unsigned long long)graph->node_count,
            (unsigned long long)graph->edge_count,
            (unsigned long long)semantic_count_duplicate_type_resolution_labels(graph),
            topo_ok ? 1 : 0,
            (unsigned long long)topo_count,
            (unsigned long long)graph->node_count);
    fprintf(stderr,
            "[type-res-stats] retired-compatibility-resolver: calls=%llu unique_nodes=%llu revisit_rate=%.1f%%\n",
            (unsigned long long)g_type_resolution_compat_calls,
            (unsigned long long)g_type_resolution_compat_unique_nodes,
            g_type_resolution_compat_calls > 0
                ? 100.0 * (double)(g_type_resolution_compat_calls
                                    - g_type_resolution_compat_unique_nodes)
                      / (double)g_type_resolution_compat_calls
                : 0.0);
    fprintf(stderr,
            "[type-res-stats] retired-compatibility-resolver-kind: ast_type=%llu channel=%llu future=%llu event_handler=%llu other=%llu\n",
            (unsigned long long)g_type_resolution_compat_ast_type_calls,
            (unsigned long long)g_type_resolution_compat_channel_type_calls,
            (unsigned long long)g_type_resolution_compat_future_type_calls,
            (unsigned long long)g_type_resolution_compat_event_handler_type_calls,
            (unsigned long long)g_type_resolution_compat_other_ast_calls);
    fprintf(stderr,
            "[type-res-stats] stage-metadata-materialize: calls=%llu failed=%llu suppressed_diagnostics=%llu\n",
            (unsigned long long)ctx->type_resolution_stage_compat_resolve_count,
            (unsigned long long)ctx->type_resolution_stage_compat_resolve_failed_count,
            (unsigned long long)ctx->type_resolution_stage_compat_resolve_suppressed_diag_count);
    fprintf(stderr, "[type-res-stats] stage-graph-backed: skips=%llu\n",
            (unsigned long long)ctx->type_resolution_stage_graph_backed_skip_count);
    fprintf(stderr,
            "[type-res-stats] metadata: entries=%llu owned=%llu hits=%llu misses=%llu dead_ends=%llu\n",
            (unsigned long long)ctx->type_resolution_metadata.count,
            (unsigned long long)metadata_owned_count,
            (unsigned long long)ctx->type_resolution_metadata_hits,
            (unsigned long long)ctx->type_resolution_metadata_misses,
            (unsigned long long)ctx->type_resolution_metadata_dead_ends);
    fprintf(stderr,
            "[type-res-stats] metadata-unresolved-audit: named=%llu generic_named=%llu compound=%llu other=%llu\n",
            (unsigned long long)ctx->type_resolution_metadata_unresolved_named,
            (unsigned long long)ctx->type_resolution_metadata_unresolved_generic_named,
            (unsigned long long)ctx->type_resolution_metadata_unresolved_compound,
            (unsigned long long)ctx->type_resolution_metadata_unresolved_other);
    fprintf(stderr,
            "[type-res-stats] metadata-unresolved-audit-named: builtin_shell=%llu generic_class=%llu alias=%llu non_class_symbol=%llu missing_symbol=%llu\n",
            (unsigned long long)ctx->type_resolution_metadata_unresolved_named_builtin_shell,
            (unsigned long long)ctx->type_resolution_metadata_unresolved_named_generic_class,
            (unsigned long long)ctx->type_resolution_metadata_unresolved_named_alias,
            (unsigned long long)ctx->type_resolution_metadata_unresolved_named_non_class_symbol,
            (unsigned long long)ctx->type_resolution_metadata_unresolved_named_missing_symbol);
    fprintf(stderr,
            "[type-res-stats] stage-materialize-family: generic_contract=%llu signature=%llu ability_consumer=%llu domain_contract=%llu alias=%llu other=%llu\n",
            (unsigned long long)ctx->type_resolution_stage_compat_generic_contract_count,
            (unsigned long long)ctx->type_resolution_stage_compat_signature_count,
            (unsigned long long)ctx->type_resolution_stage_compat_ability_consumer_count,
            (unsigned long long)ctx->type_resolution_stage_compat_domain_contract_count,
            (unsigned long long)ctx->type_resolution_stage_compat_alias_count,
            (unsigned long long)ctx->type_resolution_stage_compat_other_count);
    fprintf(stderr,
            "[type-res-stats] dag-evidence: generic_contract=%llu ability_consumer=%llu\n",
            (unsigned long long)ctx->type_resolution_dag_generic_contract_evidence_count,
            (unsigned long long)ctx->type_resolution_dag_ability_evidence_count);
    fprintf(stderr,
            "[type-res-stats] stage-alias: materialized=%llu diagnostic_unresolved=%llu\n",
            (unsigned long long)ctx->type_resolution_stage_alias_materialized_count,
            (unsigned long long)ctx->type_resolution_stage_alias_diagnostic_unresolved_count);
    fprintf(stderr,
            "[type-res-stats] stage-alias-diagnostic: resolver_calls=%llu resolved=%llu cycle_unresolved=%llu\n",
            (unsigned long long)ctx->type_resolution_stage_alias_diagnostic_resolver_call_count,
            (unsigned long long)ctx->type_resolution_stage_alias_diagnostic_resolved_count,
            (unsigned long long)ctx->type_resolution_stage_alias_diagnostic_cycle_count);

    cache_total = g_type_resolution_compat_cache_hits
                + g_type_resolution_compat_cache_misses;
    fprintf(stderr,
            "[type-res-stats] retired-compatibility-cache: hits=%llu misses=%llu hit_rate=%.1f%%\n",
            (unsigned long long)g_type_resolution_compat_cache_hits,
            (unsigned long long)g_type_resolution_compat_cache_misses,
            cache_total > 0
                ? 100.0 * (double)g_type_resolution_compat_cache_hits
                      / (double)cache_total
                : 0.0);
    fprintf(stderr,
            "[type-res-stats] kind: TYPE_REF=%llu BUILTIN=%llu DECL=%llu ALIAS=%llu GENERIC_PARAM=%llu LOCAL_CONTRACT=%llu PROJECTION_PATH=%llu\n",
            (unsigned long long)kind_counts[0],
            (unsigned long long)kind_counts[1],
            (unsigned long long)kind_counts[2],
            (unsigned long long)kind_counts[3],
            (unsigned long long)kind_counts[4],
            (unsigned long long)kind_counts[5],
            (unsigned long long)kind_counts[6]);
    semantic_print_top_indeg(graph, indeg);

    free(indeg);
    free(topo);
}
