/*
 * Whole-loop transfer summaries.
 *
 * One summary is keyed by loop syntax identity plus its exact stable-indexed
 * entry state.  A hit applies the already proven exit transfer, including
 * break/continue joins and the effect delta, without re-entering the body.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker_flow_effects.h"
#include "type_checker_flow_loop_summary.h"

typedef struct
{
    const ASTNode *node;
    ResourceConsumeSnapshot entry;
    ResourceConsumeSnapshot exit;
    uint32_t effect_base;
    uint32_t effect_delta;
    FlowFlags flags;
} LoopFlowSummary;

typedef struct
{
    const ASTNode *node;
    const char *kind;
    size_t body_checks;
    size_t summary_hits;
    size_t summaries_recorded;
} LoopFlowTrace;

struct LoopFlowSummaryStore
{
    LoopFlowSummary *summaries;
    size_t summary_count;
    size_t summary_capacity;
    LoopFlowTrace *traces;
    size_t trace_count;
    size_t trace_capacity;
    bool trace_enabled;
    bool body_trace_enabled;
};

static void
loop_flow_summary_store_destroy(LoopFlowSummaryStore *store)
{
    if (store == NULL)
        return;
    for (size_t i = 0; i < store->summary_count; i++) {
        destroy_resource_snapshot(&store->summaries[i].entry);
        destroy_resource_snapshot(&store->summaries[i].exit);
    }
    free(store->summaries);
    free(store->traces);
    free(store);
}

void
loop_flow_summary_begin_function(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;
    loop_flow_summary_store_destroy(ctx->loop_flow_summaries);
    ctx->loop_flow_summaries = calloc(1, sizeof(LoopFlowSummaryStore));
    if (ctx->loop_flow_summaries != NULL)
        ctx->loop_flow_summaries->trace_enabled =
            getenv("PGY_DEBUG_LOOP_FLOW") != NULL;
    if (ctx->loop_flow_summaries != NULL)
        ctx->loop_flow_summaries->body_trace_enabled =
            getenv("PGY_DEBUG_LOOP_FLOW_BODY") != NULL;
}

static LoopFlowTrace *
loop_flow_trace(SemanticContext *ctx, const ASTNode *node, const char *kind)
{
    LoopFlowSummaryStore *store =
        ctx != NULL ? ctx->loop_flow_summaries : NULL;
    if (store == NULL)
        return NULL;
    for (size_t i = 0; i < store->trace_count; i++) {
        if (store->traces[i].node == node)
            return &store->traces[i];
    }
    if (store->trace_count == store->trace_capacity) {
        size_t next = store->trace_capacity == 0 ? 8 : store->trace_capacity * 2;
        LoopFlowTrace *grown;
        if (next <= store->trace_capacity
            || next > SIZE_MAX / sizeof(LoopFlowTrace))
            return NULL;
        grown = realloc(store->traces, next * sizeof(LoopFlowTrace));
        if (grown == NULL)
            return NULL;
        store->traces = grown;
        store->trace_capacity = next;
    }
    LoopFlowTrace *trace = &store->traces[store->trace_count++];
    memset(trace, 0, sizeof(*trace));
    trace->node = node;
    trace->kind = kind;
    return trace;
}

void
loop_flow_summary_note_body_check(SemanticContext *ctx,
                                  const ASTNode *node,
                                  const char *kind)
{
    LoopFlowTrace *trace = loop_flow_trace(ctx, node, kind);
    if (trace != NULL) {
        trace->body_checks++;
        LoopFlowSummaryStore *store = ctx->loop_flow_summaries;
        size_t count = trace->body_checks;
        if (store->body_trace_enabled
            && (count <= 4 || (count & (count - 1)) == 0)) {
            fprintf(stderr,
                    "pgy: loop-flow-body kind=%s syntax=%u file=%s "
                    "line=%u count=%zu\n",
                    kind != NULL ? kind : "loop",
                    ast_node_stable_id(node),
                    node != NULL && node->origin_path != NULL
                        ? node->origin_path : "<unknown>",
                    node != NULL ? node->line : 0,
                    count);
            fflush(stderr);
        }
    }
}

bool
loop_flow_summary_try_apply(SemanticContext *ctx,
                            const ASTNode *node,
                            const ResourceConsumeSnapshot *entry,
                            uint32_t effect_base,
                            FlowFlags *flags_out)
{
    LoopFlowSummaryStore *store =
        ctx != NULL ? ctx->loop_flow_summaries : NULL;
    if (store == NULL || entry == NULL || !entry->valid)
        return false;
    for (size_t i = 0; i < store->summary_count; i++) {
        LoopFlowSummary *summary = &store->summaries[i];
        if (summary->node != node
            || type_effect_mask_compare(summary->effect_base,
                                        effect_base) != 0
            || !resource_snapshots_equal(&summary->entry, entry))
            continue;
        restore_resource_states_for_context(&summary->exit, ctx);
        ctx->current_function_effects =
            type_effect_mask_join(effect_base, summary->effect_delta);
        if (flags_out != NULL)
            *flags_out = summary->flags;
        LoopFlowTrace *trace = loop_flow_trace(ctx, node, NULL);
        if (trace != NULL)
            trace->summary_hits++;
        return true;
    }
    return false;
}

void
loop_flow_summary_record(SemanticContext *ctx,
                         const ASTNode *node,
                         const ResourceConsumeSnapshot *entry,
                         const ResourceConsumeSnapshot *exit,
                         uint32_t effect_base,
                         uint32_t effect_delta,
                         FlowFlags flags)
{
    LoopFlowSummaryStore *store =
        ctx != NULL ? ctx->loop_flow_summaries : NULL;
    LoopFlowSummary *summary;
    if (store == NULL || entry == NULL || exit == NULL
        || !entry->valid || !exit->valid)
        return;
    for (size_t i = 0; i < store->summary_count; i++) {
        if (store->summaries[i].node == node
            && type_effect_mask_compare(store->summaries[i].effect_base,
                                        effect_base) == 0
            && resource_snapshots_equal(&store->summaries[i].entry, entry))
            return;
    }
    if (store->summary_count == store->summary_capacity) {
        size_t next = store->summary_capacity == 0
            ? 8 : store->summary_capacity * 2;
        LoopFlowSummary *grown;
        if (next <= store->summary_capacity
            || next > SIZE_MAX / sizeof(LoopFlowSummary))
            return;
        grown = realloc(store->summaries, next * sizeof(LoopFlowSummary));
        if (grown == NULL)
            return;
        store->summaries = grown;
        store->summary_capacity = next;
    }
    summary = &store->summaries[store->summary_count];
    memset(summary, 0, sizeof(*summary));
    summary->entry = copy_resource_snapshot(entry);
    summary->exit = copy_resource_snapshot(exit);
    if (!summary->entry.valid || !summary->exit.valid) {
        destroy_resource_snapshot(&summary->entry);
        destroy_resource_snapshot(&summary->exit);
        return;
    }
    summary->node = node;
    summary->effect_base = effect_base;
    summary->effect_delta = effect_delta;
    summary->flags = flags;
    store->summary_count++;
    LoopFlowTrace *trace = loop_flow_trace(ctx, node, NULL);
    if (trace != NULL)
        trace->summaries_recorded++;
}

void
loop_flow_summary_end_function(SemanticContext *ctx)
{
    LoopFlowSummaryStore *store =
        ctx != NULL ? ctx->loop_flow_summaries : NULL;
    if (store == NULL)
        return;
    if (store->trace_enabled) {
        for (size_t i = 0; i < store->trace_count; i++) {
            const LoopFlowTrace *trace = &store->traces[i];
            fprintf(stderr,
                    "pgy: loop-flow kind=%s syntax=%u line=%u "
                    "body_reentry_count=%zu summary_hit_count=%zu "
                    "summary_record_count=%zu\n",
                    trace->kind != NULL ? trace->kind : "loop",
                    ast_node_stable_id(trace->node),
                    trace->node != NULL ? trace->node->line : 0,
                    trace->body_checks,
                    trace->summary_hits,
                    trace->summaries_recorded);
        }
    }
    loop_flow_summary_store_destroy(store);
    ctx->loop_flow_summaries = NULL;
}
