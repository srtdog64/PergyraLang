#include "air_internal.h"

static bool
air_hir_routine_matches_boundary(const HIRRoutine *routine,
                                 const AIRIntentNode *intent,
                                 const AIRBoundaryNode *boundary)
{
    if (routine == NULL || boundary == NULL)
        return false;
    if (intent == NULL) {
        return air_name_matches(routine->name, boundary->owner_name)
            || air_name_matches(routine->owner_name, boundary->owner_name);
    }
    return air_name_matches(routine->owner_name, intent->intent_owner)
        || air_name_matches(routine->name, intent->step_name)
        || air_name_matches(routine->name, intent->intent_owner)
        || air_name_matches(routine->owner_name, boundary->source_name)
        || air_name_matches(routine->name, boundary->source_name);
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
    HIRRoutineInventory inventory;
    hir_routine_inventory_from_program(hir, &inventory);

    for (size_t i = 0; i < inventory.count; i++) {
        const HIRRoutine *routine = hir_routine_inventory_get(&inventory, i);
        for (size_t j = 0; j < air_boundary_node_count(air); j++) {
            AIRBoundaryNode *boundary = air_boundary_node_mut_at(air, j);
            const AIRIntentNode *intent = NULL;
            if (boundary == NULL)
                continue;
            if (boundary->intent_index != SIZE_MAX)
                intent = air_intent_node_at(air, boundary->intent_index);
            if (boundary->intent_index != SIZE_MAX && intent == NULL)
                continue;
            if (air_hir_routine_matches_boundary(routine, intent, boundary)) {
                const char *routine_name = routine->name != NULL
                    ? routine->name
                    : routine->owner_name;
                if (!air_boundary_has_evidence_kind_provider(
                        air,
                        j,
                        AIR_EVIDENCE_HIR_ROUTINE,
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
                    air_boundary_mark_summary_flag(
                        boundary,
                        AIR_EVIDENCE_HIR_ROUTINE);
                    if (!air_increment_evidence_summary_count(
                            air,
                            AIR_EVIDENCE_HIR_ROUTINE)) {
                        air_set_error(error_message,
                                      "AIR HIR routine evidence counter overflow");
                        return false;
                    }
                }
                if (!air_boundary_has_summary_flag(
                        boundary,
                        AIR_EVIDENCE_HIR_CFG)
                    && air_hir_cfg_contains_boundary_ast(routine, boundary)) {
                    if (!air_append_evidence_node(air,
                                                  AIR_EVIDENCE_HIR_CFG,
                                                  j,
                                                  routine_name,
                                                  boundary->source_name,
                                                  error_message)) {
                        return false;
                    }
                    air_boundary_mark_summary_flag(boundary,
                                                   AIR_EVIDENCE_HIR_CFG);
                    if (!air_increment_evidence_summary_count(
                            air,
                            AIR_EVIDENCE_HIR_CFG)) {
                        air_set_error(error_message,
                                      "AIR HIR CFG evidence counter overflow");
                        return false;
                    }
                }
            }
        }
    }
    return true;
}
