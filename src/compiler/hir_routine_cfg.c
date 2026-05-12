#include "hir_internal.h"

#include "hir_analysis.h"
#include "hir_cfg.h"

static bool
hir_cfg_successor_in_range(const HIRRoutine *routine, size_t successor)
{
    return routine != NULL && successor < routine->cfg.block_count;
}

static bool
hir_cfg_block_targets(const HIRBasicBlock *block, size_t target)
{
    if (block == NULL)
        return false;
    return (block->has_succ_true && block->succ_true == target)
        || (block->has_succ_false && block->succ_false == target);
}

static bool
hir_cfg_predecessors_contain(const HIRBasicBlock *block, size_t predecessor)
{
    if (block == NULL)
        return false;
    for (size_t i = 0; i < block->predecessor_count; i++) {
        if (block->predecessors[i] == predecessor)
            return true;
    }
    return false;
}

static bool
hir_validate_cfg_shape(const HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg)
        return true;
    if (routine->cfg.blocks == NULL || routine->cfg.block_count == 0)
        return false;
    if (routine->cfg.entry_block >= routine->cfg.block_count)
        return false;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (block->id != i)
            return false;

        switch (block->terminator_kind) {
        case HIR_BLOCK_FALLTHROUGH:
            return false;
        case HIR_BLOCK_GOTO:
            if (!block->has_succ_true || block->has_succ_false)
                return false;
            if (!hir_cfg_successor_in_range(routine, block->succ_true))
                return false;
            break;
        case HIR_BLOCK_BRANCH:
            if (block->terminator_condition == NULL)
                return false;
            if (!block->has_succ_true || !block->has_succ_false)
                return false;
            if (!hir_cfg_successor_in_range(routine, block->succ_true)
                || !hir_cfg_successor_in_range(routine, block->succ_false))
                return false;
            break;
        case HIR_BLOCK_RETURN:
        case HIR_BLOCK_UNREACHABLE:
            if (block->has_succ_true || block->has_succ_false)
                return false;
            break;
        default:
            return false;
        }
    }

    return true;
}

static bool
hir_validate_cfg_predecessors(const HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg)
        return true;
    if (!hir_validate_cfg_shape(routine))
        return false;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        for (size_t p = 0; p < block->predecessor_count; p++) {
            size_t predecessor = block->predecessors[p];
            if (predecessor >= routine->cfg.block_count)
                return false;
            if (!hir_cfg_block_targets(&routine->cfg.blocks[predecessor], i))
                return false;
        }

        if (block->has_succ_true
            && !hir_cfg_predecessors_contain(&routine->cfg.blocks[block->succ_true], i))
            return false;
        if (block->has_succ_false
            && !hir_cfg_predecessors_contain(&routine->cfg.blocks[block->succ_false], i))
            return false;
    }

    return true;
}

bool
hir_finish_cfg_routine(HIRRoutine *routine)
{
    if (!hir_validate_cfg_shape(routine))
        return false;
    if (!hir_finalize_cfg(routine))
        return false;
    if (!hir_validate_cfg_predecessors(routine))
        return false;
    if (!hir_compute_cfg_dominance(routine))
        return false;
    if (!hir_compute_cfg_dominance_frontier(routine))
        return false;
    if (!hir_compute_cfg_dom_tree(routine))
        return false;
    if (!hir_compute_cfg_loops(routine))
        return false;
    if (!hir_collect_cfg_local_defs(routine))
        return false;
    if (!hir_compute_cfg_phi_candidates(routine))
        return false;
    if (!hir_materialize_phi_nodes(routine))
        return false;
    hir_finalize_cfg_summary(routine);
    return true;
}
