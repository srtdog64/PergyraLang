#include "hir_lower_cfg.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static bool
intent_cfg_append_stmt(ASTNode ***items, size_t *count, size_t *capacity, ASTNode *node)
{
    ASTNode **grown;

    if (node == NULL)
        return true;
    if (*count == *capacity) {
        size_t next_capacity = *capacity == 0 ? 8 : *capacity * 2;
        grown = realloc(*items, next_capacity * sizeof(ASTNode *));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
    }
    (*items)[*count] = node;
    (*count)++;
    return true;
}

static ssize_t
intent_cfg_new_block(HIRBasicBlock **blocks, size_t *count, size_t *capacity)
{
    if (*count == *capacity) {
        size_t next_capacity = *capacity == 0 ? 8 : *capacity * 2;
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

static void
intent_cfg_set_goto(HIRBasicBlock *block, size_t succ)
{
    if (block == NULL)
        return;
    block->terminator_kind = HIR_BLOCK_GOTO;
    block->succ_true = succ;
    block->has_succ_true = true;
    block->has_succ_false = false;
    block->terminator_condition = NULL;
    block->terminator_value = NULL;
}

static void
intent_cfg_set_unreachable(HIRBasicBlock *block)
{
    if (block == NULL)
        return;
    block->terminator_kind = HIR_BLOCK_UNREACHABLE;
    block->has_succ_true = false;
    block->has_succ_false = false;
    block->terminator_condition = NULL;
    block->terminator_value = NULL;
}

static bool
intent_cfg_append_step_statements(HIRBasicBlock *block, ASTNode *step)
{
    if (block == NULL || step == NULL || step->type != AST_INTENT_STEP)
        return true;

    if (!intent_cfg_append_stmt(&block->statements, &block->statement_count, &block->statement_capacity, step)
        || !intent_cfg_append_stmt(&block->statements,
                                   &block->statement_count,
                                   &block->statement_capacity,
                                   step->data.intent_step.where_type)
        || !intent_cfg_append_stmt(&block->statements,
                                   &block->statement_count,
                                   &block->statement_capacity,
                                   step->data.intent_step.using_expr)
        || !intent_cfg_append_stmt(&block->statements,
                                   &block->statement_count,
                                   &block->statement_capacity,
                                   step->data.intent_step.intent_expr)
        || !intent_cfg_append_stmt(&block->statements,
                                   &block->statement_count,
                                   &block->statement_capacity,
                                   step->data.intent_step.pre_expr)
        || !intent_cfg_append_stmt(&block->statements,
                                   &block->statement_count,
                                   &block->statement_capacity,
                                   step->data.intent_step.guard_expr)
        || !intent_cfg_append_stmt(&block->statements,
                                   &block->statement_count,
                                   &block->statement_capacity,
                                   step->data.intent_step.post_expr)
        || !intent_cfg_append_stmt(&block->statements,
                                   &block->statement_count,
                                   &block->statement_capacity,
                                   step->data.intent_step.invariant_expr)
        || !intent_cfg_append_stmt(&block->statements,
                                   &block->statement_count,
                                   &block->statement_capacity,
                                   step->data.intent_step.expect_expr)) {
        return false;
    }

    for (size_t i = 0; i < step->data.intent_step.required_ability_count; i++) {
        if (!intent_cfg_append_stmt(&block->statements,
                                    &block->statement_count,
                                    &block->statement_capacity,
                                    step->data.intent_step.required_abilities[i])) {
            return false;
        }
    }
    for (size_t i = 0; i < step->data.intent_step.on_expr_count; i++) {
        if (!intent_cfg_append_stmt(&block->statements,
                                    &block->statement_count,
                                    &block->statement_capacity,
                                    step->data.intent_step.on_exprs[i])) {
            return false;
        }
    }
    for (size_t i = 0; i < step->data.intent_step.compensate_expr_count; i++) {
        if (!intent_cfg_append_stmt(&block->statements,
                                    &block->statement_count,
                                    &block->statement_capacity,
                                    step->data.intent_step.compensate_exprs[i])) {
            return false;
        }
    }
    return true;
}

static void
intent_cfg_free_blocks(HIRBasicBlock *blocks, size_t block_count)
{
    if (blocks == NULL)
        return;
    for (size_t i = 0; i < block_count; i++)
        free(blocks[i].statements);
    free(blocks);
}

bool
hir_lower_intent_cfg(ASTNode *intent, HIRRoutine *routine)
{
    HIRBasicBlock *blocks = NULL;
    size_t block_count = 0;
    size_t block_capacity = 0;
    ssize_t entry;
    ssize_t current;

    if (intent == NULL || routine == NULL || intent->type != AST_INTENT_DECL)
        return true;

    entry = intent_cfg_new_block(&blocks, &block_count, &block_capacity);
    if (entry < 0)
        return false;
    current = entry;

    if (!intent_cfg_append_stmt(&blocks[(size_t)current].statements,
                                &blocks[(size_t)current].statement_count,
                                &blocks[(size_t)current].statement_capacity,
                                intent->data.intent_decl.priority_expr)
        || !intent_cfg_append_stmt(&blocks[(size_t)current].statements,
                                   &blocks[(size_t)current].statement_count,
                                   &blocks[(size_t)current].statement_capacity,
                                   intent->data.intent_decl.success_expr)
        || !intent_cfg_append_stmt(&blocks[(size_t)current].statements,
                                   &blocks[(size_t)current].statement_count,
                                   &blocks[(size_t)current].statement_capacity,
                                   intent->data.intent_decl.failure_expr)) {
        intent_cfg_free_blocks(blocks, block_count);
        return false;
    }

    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (blocks[(size_t)current].statement_count > 0) {
            ssize_t next = intent_cfg_new_block(&blocks, &block_count, &block_capacity);
            if (next < 0) {
                intent_cfg_free_blocks(blocks, block_count);
                return false;
            }
            intent_cfg_set_goto(&blocks[(size_t)current], (size_t)next);
            current = next;
        }
        if (!intent_cfg_append_step_statements(&blocks[(size_t)current], step)) {
            intent_cfg_free_blocks(blocks, block_count);
            return false;
        }
    }

    intent_cfg_set_unreachable(&blocks[(size_t)current]);
    routine->cfg.blocks = blocks;
    routine->cfg.block_count = block_count;
    routine->cfg.block_capacity = block_capacity;
    routine->cfg.entry_block = (size_t)entry;
    routine->has_cfg = true;
    return true;
}
