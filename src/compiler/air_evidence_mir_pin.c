#include "air_internal.h"
#include "mir_cfg_contract_pin.h"

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
    if (boundary->ast == NULL || block->pin_block_ast == NULL)
        return false;
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
    if (!block->is_reachable || block->is_cleanup)
        return false;
    if (!air_mir_cleanup_root_is_valid(routine))
        return false;
    if (!block->has_cleanup_succ || block->cleanup_succ != routine->cleanup_block)
        return false;
    return true;
}

bool
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

    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        AIRBoundaryNode *boundary = air_boundary_node_mut_at(air, i);
        if (boundary == NULL)
            continue;

        if (!air_mir_pin_block_matches_boundary(block, boundary))
            continue;
        if (air_boundary_has_evidence_kind_provider(
                air,
                i,
                AIR_EVIDENCE_MIR_PIN_CLEANUP,
                routine_name)) {
            continue;
        }
        if (!air_append_evidence_node(air,
                                      AIR_EVIDENCE_MIR_PIN_CLEANUP,
                                      i,
                                      routine_name,
                                      inst->slot_anchor,
                                      error_message)) {
            return false;
        }
        if (!air_increment_evidence_summary_count(
                air,
                AIR_EVIDENCE_MIR_PIN_CLEANUP)) {
            air_set_error(error_message,
                          "AIR MIR pin cleanup evidence counter overflow");
            return false;
        }
    }
    return true;
}
