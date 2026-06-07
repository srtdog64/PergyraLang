#include "hir_lower_cfg_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
hir_lower_cfg_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    if (capacity == NULL || initial == 0 || elem_size == 0)
        return false;

    size_t current = *capacity;
    size_t next_capacity = initial;
    if (current != 0) {
        if (current > SIZE_MAX / 2)
            return false;
        next_capacity = current * 2;
    }
    if (next_capacity > SIZE_MAX / elem_size)
        return false;

    *capacity = next_capacity;
    return true;
}

bool
hir_cfg_append_stmt(ASTNode ***items, size_t *count, size_t *capacity, ASTNode *node)
{
    if (items == NULL || count == NULL || capacity == NULL)
        return false;
    if (node == NULL)
        return true;

    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!hir_lower_cfg_next_capacity(&next_capacity, 8, sizeof(ASTNode *)))
            return false;
        ASTNode **grown = realloc(*items, next_capacity * sizeof(ASTNode *));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
    }
    (*items)[*count] = node;
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
hir_cfg_new_block(HIRBasicBlock **blocks, size_t *count, size_t *capacity)
{
    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!hir_lower_cfg_next_capacity(&next_capacity, 8, sizeof(HIRBasicBlock)))
            return -1;
        HIRBasicBlock *grown = realloc(*blocks, next_capacity * sizeof(HIRBasicBlock));
        if (grown == NULL)
            return -1;
        *blocks = grown;
        *capacity = next_capacity;
    }
    memset(&(*blocks)[*count], 0, sizeof(HIRBasicBlock));
    (*blocks)[*count].id = *count;
    (*blocks)[*count].terminator_kind = HIR_BLOCK_FALLTHROUGH;
    (*count)++;
    return (ssize_t)(*count - 1);
}

ssize_t
hir_cfg_new_region_block(HIRBasicBlock **blocks,
                         size_t *count,
                         size_t *capacity,
                         const HIRPinRegionContext *pin)
{
    ssize_t id = hir_cfg_new_block(blocks, count, capacity);
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
    return hir_cfg_set_branch_with_value(block, condition, NULL,
                                         succ_true, succ_false);
}

bool
hir_cfg_set_branch_with_value(HIRBasicBlock *block,
                              ASTNode *condition,
                              ASTNode *value,
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
    block->terminator_value = value;
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
