#include "hir_lower_cfg_internal.h"

#include <stdlib.h>
#include <string.h>

bool
hir_cfg_append_stmt(ASTNode ***items, size_t *count, ASTNode *node)
{
    ASTNode **grown = realloc(*items, (*count + 1) * sizeof(ASTNode *));
    if (grown == NULL)
        return false;
    grown[*count] = node;
    *items = grown;
    (*count)++;
    return true;
}

void
hir_cfg_apply_pin_region(HIRBasicBlock *block, const HIRPinRegionContext *pin)
{
    if (block == NULL || pin == NULL || !pin->active)
        return;
    block->is_pin_region = true;
    block->pin_view_is_write = pin->view_is_write;
    block->pin_source_name = pin->source_name;
    block->pin_view_name = pin->view_name;
    block->pin_block_ast = pin->block_ast;
}

ssize_t
hir_cfg_new_block(HIRBasicBlock **blocks, size_t *count)
{
    HIRBasicBlock *grown = realloc(*blocks, (*count + 1) * sizeof(HIRBasicBlock));
    if (grown == NULL)
        return -1;
    memset(&grown[*count], 0, sizeof(HIRBasicBlock));
    grown[*count].id = *count;
    grown[*count].terminator_kind = HIR_BLOCK_FALLTHROUGH;
    *blocks = grown;
    (*count)++;
    return (ssize_t)(*count - 1);
}

ssize_t
hir_cfg_new_region_block(HIRBasicBlock **blocks,
                         size_t *count,
                         const HIRPinRegionContext *pin)
{
    ssize_t id = hir_cfg_new_block(blocks, count);
    if (id >= 0)
        hir_cfg_apply_pin_region(&(*blocks)[(size_t)id], pin);
    return id;
}

bool
hir_cfg_set_goto(HIRBasicBlock *block, size_t succ)
{
    if (block == NULL)
        return false;
    block->terminator_kind = HIR_BLOCK_GOTO;
    block->succ_true = succ;
    block->has_succ_true = true;
    block->has_succ_false = false;
    block->terminator_condition = NULL;
    block->terminator_value = NULL;
    return true;
}

bool
hir_cfg_set_branch(HIRBasicBlock *block,
                   ASTNode *condition,
                   size_t succ_true,
                   size_t succ_false)
{
    if (block == NULL)
        return false;
    block->terminator_kind = HIR_BLOCK_BRANCH;
    block->terminator_condition = condition;
    block->succ_true = succ_true;
    block->succ_false = succ_false;
    block->has_succ_true = true;
    block->has_succ_false = true;
    block->terminator_value = NULL;
    return true;
}

bool
hir_cfg_set_return(HIRBasicBlock *block, ASTNode *value)
{
    if (block == NULL)
        return false;
    block->terminator_kind = HIR_BLOCK_RETURN;
    block->terminator_value = value;
    block->has_succ_true = false;
    block->has_succ_false = false;
    block->terminator_condition = NULL;
    return true;
}

bool
hir_cfg_set_unreachable(HIRBasicBlock *block)
{
    if (block == NULL)
        return false;
    block->terminator_kind = HIR_BLOCK_UNREACHABLE;
    block->has_succ_true = false;
    block->has_succ_false = false;
    block->terminator_condition = NULL;
    block->terminator_value = NULL;
    return true;
}

bool
hir_cfg_resolve_loop_target(const HIRLoopContext *loop,
                            const char *label,
                            bool continue_target,
                            size_t *target_out)
{
    for (const HIRLoopContext *it = loop; it != NULL; it = it->parent) {
        if (!it->active)
            continue;
        if (label != NULL) {
            if (it->label == NULL || strcmp(it->label, label) != 0)
                continue;
        }
        if (target_out != NULL)
            *target_out = continue_target ? it->continue_target : it->break_target;
        return true;
    }
    return false;
}
