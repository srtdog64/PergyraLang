#include "air_internal.h"
#include "mir_cfg_contract_cleanup_fact.h"
#include "mir_cfg_contract_pin.h"

#include "../semantic/semantic.h"

#include <string.h>

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
air_hir_cfg_contains_boundary_ast(const HIRRoutine *routine, const AIRBoundaryNode *boundary)
{
    if (routine == NULL || boundary == NULL || !routine->has_cfg)
        return false;
    if (boundary->ast == NULL)
        return true;
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
air_collect_hir_evidence(AIRProgram *air, const HIRProgram *hir, char **error_message)
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
                if (!air_assign_first_owned_name(air,
                                                 &boundary->hir_routine_evidence_name,
                                                 routine_name,
                                                 error_message,
                                                 "HIR routine")) {
                    return false;
                }
                boundary->has_hir_routine_evidence = true;
                air->hir_routine_evidence_count++;
                if (!air_append_evidence_node(air,
                                              AIR_EVIDENCE_HIR_ROUTINE,
                                              j,
                                              routine_name,
                                              boundary->source_name,
                                              error_message)) {
                    return false;
                }
                if (air_hir_cfg_contains_boundary_ast(routine, boundary)) {
                    boundary->has_hir_cfg_evidence = true;
                    air->hir_cfg_evidence_count++;
                    if (!air_append_evidence_node(air,
                                                  AIR_EVIDENCE_HIR_CFG,
                                                  j,
                                                  routine_name,
                                                  boundary->source_name,
                                                  error_message)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static bool
air_boundary_is_pin_boundary(const AIRBoundaryNode *boundary)
{
    return boundary != NULL
        && boundary->kind == AIR_BOUNDARY_EXECUTION
        && air_name_matches(boundary->source_name, "pin");
}

static bool
air_mir_pin_block_matches_boundary(const MIRBasicBlock *block,
                                   const AIRBoundaryNode *boundary)
{
    if (block == NULL || boundary == NULL || !block->is_pin_region)
        return false;
    if (!air_boundary_is_pin_boundary(boundary))
        return false;
    if (boundary->ast == NULL)
        return true;
    return block->pin_block_ast == boundary->ast;
}

static bool
air_mir_pin_cleanup_instruction_has_anchor(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->slot_anchor != NULL
        && inst->slot_anchor[0] != '\0';
}

static bool
air_mir_pin_block_has_cleanup_successor(const MIRRoutine *routine,
                                        const MIRBasicBlock *block)
{
    if (routine == NULL || block == NULL)
        return false;
    if (!routine->has_cleanup_block || routine->cleanup_block >= routine->block_count)
        return false;
    if (!block->has_cleanup_succ || block->cleanup_succ != routine->cleanup_block)
        return false;
    return routine->blocks[routine->cleanup_block].is_cleanup;
}

static size_t
air_mir_routine_cleanup_fact_count(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL || !routine->has_cleanup_block)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        if (block->has_cleanup_succ
            && block->cleanup_succ == routine->cleanup_block
            && routine->cleanup_block < routine->block_count
            && routine->blocks[routine->cleanup_block].is_cleanup) {
            count++;
            continue;
        }
        if (mir_block_has_cleanup_edge_fact(block, "cleanup-edge"))
            count++;
        if (mir_block_has_cleanup_edge_fact(block, "cleanup-edge-from-rollback"))
            count++;
        if (mir_block_has_cleanup_edge_fact(block, "cleanup-edge-from-invalidation"))
            count++;
    }
    return count;
}

static bool
air_has_mir_pin_cleanup_evidence(const AIRProgram *air,
                                 size_t boundary_index,
                                 const char *routine_name);

static bool
air_collect_mir_pin_block_evidence(AIRProgram *air,
                                   const MIRRoutine *routine,
                                   const MIRBasicBlock *block,
                                   const char *routine_name,
                                   char **error_message)
{
    const MIRInstruction *inst;

    if (air == NULL || routine == NULL || block == NULL)
        return true;
    if (!block->is_pin_region)
        return true;
    if (!air_mir_pin_block_has_cleanup_successor(routine, block))
        return true;

    inst = mir_block_find_pin_cleanup_edge_fact(block);
    if (!air_mir_pin_cleanup_instruction_has_anchor(inst))
        return true;

    for (size_t i = 0; i < air->boundary_count; i++) {
        AIRBoundaryNode *boundary = &air->boundaries[i];

        if (!air_mir_pin_block_matches_boundary(block, boundary))
            continue;
        if (air_has_mir_pin_cleanup_evidence(air, i, routine_name))
            continue;
        if (!air_append_evidence_node(air,
                                      AIR_EVIDENCE_MIR_PIN_CLEANUP,
                                      i,
                                      routine_name,
                                      inst->slot_anchor,
                                      error_message)) {
            return false;
        }
        air->mir_pin_cleanup_evidence_count++;
    }
    return true;
}

static bool
air_has_mir_pin_cleanup_evidence(const AIRProgram *air,
                                 size_t boundary_index,
                                 const char *routine_name)
{
    if (air == NULL)
        return false;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *node = &air->evidence_nodes[i];
        if (node->kind == AIR_EVIDENCE_MIR_PIN_CLEANUP
            && node->boundary_index == boundary_index
            && air_name_matches(node->provider_name, routine_name)) {
            return true;
        }
    }
    return false;
}

bool
air_collect_mir_evidence(AIRProgram *air, const MIRProgram *mir, char **error_message)
{
    if (air == NULL || mir == NULL)
        return true;

    air->has_mir_input = true;

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        const char *routine_name = routine->name != NULL
            ? routine->name
            : routine->owner_name;
        size_t cleanup_fact_count = air_mir_routine_cleanup_fact_count(routine);
        if (cleanup_fact_count == 0)
            continue;
        if (!air_append_evidence_node_ex(air,
                                         AIR_EVIDENCE_MIR_CLEANUP,
                                         SIZE_MAX,
                                         routine_name,
                                         "cleanup-block",
                                         cleanup_fact_count,
                                         0,
                                         error_message)) {
            return false;
        }
        air->mir_cleanup_evidence_count++;
    }

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        const char *routine_name = routine->name != NULL
            ? routine->name
            : routine->owner_name;
        for (size_t j = 0; j < routine->block_count; j++) {
            if (!air_collect_mir_pin_block_evidence(air, routine,
                                                    &routine->blocks[j],
                                                    routine_name,
                                                    error_message)) {
                return false;
            }
        }
    }
    return true;
}

bool
air_collect_dag_evidence(AIRProgram *air, const SemanticResult *sem, char **error_message)
{
    const size_t fallback_count = sem != NULL
        ? sem->type_resolution_metadata_materializer_fallbacks
        : 0;
    const size_t metadata_fact_count = sem != NULL
        ? sem->type_resolution_metadata_entries
        : 0;
    const size_t generic_fact_count = sem != NULL
        ? sem->type_resolution_stage_compat_generic_contract_count
        : 0;
    const size_t ability_fact_count = sem != NULL
        ? sem->type_resolution_stage_compat_ability_consumer_count
        : 0;

    if (air == NULL || sem == NULL)
        return true;

    if (metadata_fact_count > 0 || fallback_count > 0) {
        if (!air_append_evidence_node_ex(air,
                                         AIR_EVIDENCE_DAG_METADATA,
                                         SIZE_MAX,
                                         "type-resolution-dag",
                                         "metadata-inventory",
                                         metadata_fact_count,
                                         fallback_count,
                                         error_message)) {
            return false;
        }
        air->dag_metadata_evidence_count++;
    }

    if (generic_fact_count > 0 || fallback_count > 0) {
        if (!air_append_evidence_node_ex(air,
                                         AIR_EVIDENCE_DAG_GENERIC,
                                         SIZE_MAX,
                                         "type-resolution-dag",
                                         "generic-contracts",
                                         generic_fact_count,
                                         fallback_count,
                                         error_message)) {
            return false;
        }
        air->dag_generic_evidence_count++;
    }

    if (ability_fact_count > 0 || fallback_count > 0) {
        if (!air_append_evidence_node_ex(air,
                                         AIR_EVIDENCE_DAG_ABILITY,
                                         SIZE_MAX,
                                         "type-resolution-dag",
                                         "ability-consumers",
                                         ability_fact_count,
                                         fallback_count,
                                         error_message)) {
            return false;
        }
        air->dag_ability_evidence_count++;
    }
    return true;
}
