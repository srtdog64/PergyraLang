#include "mir_surface_usage.h"

#include <stddef.h>

#include "../parser/ast_analysis.h"

static void
mir_accumulate_ast_surface_usage(MIRSurfaceUsageSummary *summary,
                                 const ASTNode *node)
{
    if (summary == NULL || node == NULL)
        return;

    if (!summary->uses_thread_pool)
        summary->uses_thread_pool = ast_uses_thread_pool_surface(node);
    if (!summary->uses_intent_observability) {
        summary->uses_intent_observability =
            ast_uses_intent_observability_surface(node);
    }
}

static void
mir_accumulate_ast_array_surface_usage(MIRSurfaceUsageSummary *summary,
                                       ASTNode *const *nodes,
                                       size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (summary != NULL && summary->uses_thread_pool
            && summary->uses_intent_observability) {
            return;
        }
        mir_accumulate_ast_surface_usage(summary, nodes[i]);
    }
}

MIRSurfaceUsageSummary
mir_inventory_surface_usage_summary(const MIRProgram *mir)
{
    MIRSurfaceUsageSummary summary = {0};

    if (mir == NULL)
        return summary;

    mir_accumulate_ast_array_surface_usage(
        &summary, mir->types, mir->type_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->abilities, mir->ability_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->roles, mir->role_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->parties, mir->party_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->rosters, mir->roster_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->worlds, mir->world_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->relations, mir->relation_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->effects, mir->effect_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->zones, mir->zone_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->events, mir->event_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->intents, mir->intent_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->functions, mir->function_count);
    mir_accumulate_ast_array_surface_usage(
        &summary, mir->externs, mir->extern_count);
    return summary;
}

bool
mir_inventory_uses_thread_pool_surface(const MIRProgram *mir)
{
    return mir_inventory_surface_usage_summary(mir).uses_thread_pool;
}

bool
mir_inventory_uses_intent_observability_surface(const MIRProgram *mir)
{
    return mir_inventory_surface_usage_summary(mir).uses_intent_observability;
}

void
mir_program_record_inventory_surface_usage(MIRProgram *mir)
{
    MIRSurfaceUsageSummary summary;

    if (mir == NULL)
        return;

    summary = mir_inventory_surface_usage_summary(mir);
    mir->has_inventory_surface_usage_facts = true;
    mir->inventory_uses_thread_pool_surface = summary.uses_thread_pool;
    mir->inventory_uses_intent_observability_surface =
        summary.uses_intent_observability;
}
