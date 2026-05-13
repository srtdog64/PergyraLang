#include "hir_lower_cfg_internal.h"

#include <stdlib.h>

static bool
intent_cfg_append_step_statements(HIRBasicBlock *block, ASTNode *step)
{
    if (block == NULL || step == NULL || step->type != AST_INTENT_STEP)
        return true;

    if (!hir_cfg_append_stmt(&block->statements, &block->statement_count, &block->statement_capacity, step)
        || !hir_cfg_append_stmt(&block->statements,
                                &block->statement_count,
                                &block->statement_capacity,
                                ast_intent_step_where_type(step))
        || !hir_cfg_append_stmt(&block->statements,
                                &block->statement_count,
                                &block->statement_capacity,
                                ast_intent_step_using_expr(step))
        || !hir_cfg_append_stmt(&block->statements,
                                &block->statement_count,
                                &block->statement_capacity,
                                ast_intent_step_intent_expr(step))
        || !hir_cfg_append_stmt(&block->statements,
                                &block->statement_count,
                                &block->statement_capacity,
                                ast_intent_step_pre_expr(step))
        || !hir_cfg_append_stmt(&block->statements,
                                &block->statement_count,
                                &block->statement_capacity,
                                ast_intent_step_guard_expr(step))
        || !hir_cfg_append_stmt(&block->statements,
                                &block->statement_count,
                                &block->statement_capacity,
                                ast_intent_step_post_expr(step))
        || !hir_cfg_append_stmt(&block->statements,
                                &block->statement_count,
                                &block->statement_capacity,
                                ast_intent_step_invariant_expr(step))
        || !hir_cfg_append_stmt(&block->statements,
                                &block->statement_count,
                                &block->statement_capacity,
                                ast_intent_step_expect_expr(step))) {
        return false;
    }

    for (size_t i = 0; i < ast_intent_step_required_ability_count(step); i++) {
        if (!hir_cfg_append_stmt(&block->statements,
                                 &block->statement_count,
                                 &block->statement_capacity,
                                 ast_intent_step_required_abilities(step, NULL)[i])) {
            return false;
        }
    }
    for (size_t i = 0; i < ast_intent_step_on_expr_count(step); i++) {
        if (!hir_cfg_append_stmt(&block->statements,
                                 &block->statement_count,
                                 &block->statement_capacity,
                                 ast_intent_step_on_exprs(step, NULL)[i])) {
            return false;
        }
    }
    for (size_t i = 0; i < ast_intent_step_compensate_expr_count(step); i++) {
        if (!hir_cfg_append_stmt(&block->statements,
                                 &block->statement_count,
                                 &block->statement_capacity,
                                 ast_intent_step_compensate_exprs(step, NULL)[i])) {
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

    entry = hir_cfg_new_block(&blocks, &block_count, &block_capacity);
    if (entry < 0)
        return false;
    current = entry;

    if (!hir_cfg_append_stmt(&blocks[(size_t)current].statements,
                             &blocks[(size_t)current].statement_count,
                             &blocks[(size_t)current].statement_capacity,
                             intent->data.intent_decl.priority_expr)
        || !hir_cfg_append_stmt(&blocks[(size_t)current].statements,
                                &blocks[(size_t)current].statement_count,
                                &blocks[(size_t)current].statement_capacity,
                                intent->data.intent_decl.success_expr)
        || !hir_cfg_append_stmt(&blocks[(size_t)current].statements,
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
            ssize_t next = hir_cfg_new_block(&blocks, &block_count, &block_capacity);
            if (next < 0) {
                intent_cfg_free_blocks(blocks, block_count);
                return false;
            }
            hir_cfg_set_goto(&blocks[(size_t)current], (size_t)next);
            current = next;
        }
        if (!intent_cfg_append_step_statements(&blocks[(size_t)current], step)) {
            intent_cfg_free_blocks(blocks, block_count);
            return false;
        }
    }

    hir_cfg_set_unreachable(&blocks[(size_t)current]);
    routine->cfg.blocks = blocks;
    routine->cfg.block_count = block_count;
    routine->cfg.block_capacity = block_capacity;
    routine->cfg.entry_block = (size_t)entry;
    routine->has_cfg = true;
    return true;
}
