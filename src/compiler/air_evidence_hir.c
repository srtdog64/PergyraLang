#include "air_internal.h"

static bool
air_hir_routine_matches_boundary(const HIRRoutine *routine,
                                 const AIRIntentNode *intent,
                                 const AIRBoundaryNode *boundary)
{
    if (routine == NULL || intent == NULL || boundary == NULL)
        return false;
    return air_name_matches(routine->owner_name, intent->intent_owner)
        || air_name_matches(routine->name, intent->step_name)
        || air_name_matches(routine->name, intent->intent_owner)
        || air_name_matches(routine->owner_name, boundary->source_name)
        || air_name_matches(routine->name, boundary->source_name);
}

static bool
air_has_boundary_evidence_provider(const AIRProgram *air,
                                   AIREvidenceKind kind,
                                   size_t boundary_index,
                                   const char *provider_name)
{
    if (air == NULL || provider_name == NULL)
        return false;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *node = &air->evidence_nodes[i];
        if (node->kind == kind
            && node->boundary_index == boundary_index
            && air_name_matches(node->provider_name, provider_name)) {
            return true;
        }
    }
    return false;
}

static bool
air_hir_cfg_contains_boundary_ast(const HIRRoutine *routine,
                                  const AIRBoundaryNode *boundary)
{
    if (routine == NULL || boundary == NULL || !routine->has_cfg)
        return false;
    if (boundary->ast == NULL)
        return !air_boundary_requires_hir_evidence(boundary);
    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (block == NULL)
            continue;
        for (size_t j = 0; j < block->statement_count; j++) {
            if (block->statements[j] == boundary->ast
                || air_ast_contains_node(block->statements[j], boundary->ast))
                return true;
        }
        if (block->terminator_condition == boundary->ast
            || air_ast_contains_node(block->terminator_condition, boundary->ast))
            return true;
        if (block->terminator_value == boundary->ast
            || air_ast_contains_node(block->terminator_value, boundary->ast))
            return true;
        if (block->pin_block_ast == boundary->ast)
            return true;
    }
    return false;
}

bool
air_collect_hir_evidence(AIRProgram *air, const HIRProgram *hir,
                         char **error_message)
{
    if (air == NULL || hir == NULL)
        return true;
    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        for (size_t j = 0; j < air->boundary_count; j++) {
            AIRBoundaryNode *boundary = &air->boundaries[j];
            const AIRIntentNode *intent = &air->intents[boundary->intent_index];
            if (air_hir_routine_matches_boundary(routine, intent, boundary)) {
                const char *routine_name = routine->name != NULL
                    ? routine->name
                    : routine->owner_name;
                if (!air_has_boundary_evidence_provider(air,
                                                        AIR_EVIDENCE_HIR_ROUTINE,
                                                        j,
                                                        routine_name)) {
                    if (!air_assign_first_owned_name(
                            air,
                            &boundary->hir_routine_evidence_name,
                            routine_name,
                            error_message,
                            "HIR routine")) {
                        return false;
                    }
                    if (!air_append_evidence_node(air,
                                                  AIR_EVIDENCE_HIR_ROUTINE,
                                                  j,
                                                  routine_name,
                                                  boundary->source_name,
                                                  error_message)) {
                        return false;
                    }
                    boundary->has_hir_routine_evidence = true;
                    air->hir_routine_evidence_count++;
                }
                if (!boundary->has_hir_cfg_evidence
                    && air_hir_cfg_contains_boundary_ast(routine, boundary)) {
                    if (!air_append_evidence_node(air,
                                                  AIR_EVIDENCE_HIR_CFG,
                                                  j,
                                                  routine_name,
                                                  boundary->source_name,
                                                  error_message)) {
                        return false;
                    }
                    boundary->has_hir_cfg_evidence = true;
                    air->hir_cfg_evidence_count++;
                }
            }
        }
    }
    return true;
}
