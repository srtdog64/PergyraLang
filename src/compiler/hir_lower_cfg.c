#include "hir_lower_cfg.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static bool
cfg_append_stmt(ASTNode ***items, size_t *count, ASTNode *node)
{
    ASTNode **grown = realloc(*items, (*count + 1) * sizeof(ASTNode *));
    if (grown == NULL)
        return false;
    grown[*count] = node;
    *items = grown;
    (*count)++;
    return true;
}

static ssize_t
cfg_new_block(HIRBasicBlock **blocks, size_t *count)
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

static bool
cfg_set_goto(HIRBasicBlock *block, size_t succ)
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

static bool
cfg_set_branch(HIRBasicBlock *block, ASTNode *condition, size_t succ_true, size_t succ_false)
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

static bool
cfg_set_return(HIRBasicBlock *block, ASTNode *value)
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

static bool
cfg_set_unreachable(HIRBasicBlock *block)
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

static ssize_t
hir_lower_stmt_node_to_cfg(ASTNode *node,
                           HIRBasicBlock **blocks,
                           size_t *block_count,
                           ssize_t current_block);

static ssize_t
hir_lower_stmt_list_to_cfg(ASTNode **statements,
                           size_t statement_count,
                           HIRBasicBlock **blocks,
                           size_t *block_count,
                           ssize_t current_block)
{
    ssize_t open_block = current_block;
    for (size_t i = 0; i < statement_count && open_block >= 0; i++) {
        open_block = hir_lower_stmt_node_to_cfg(statements[i],
                                                blocks,
                                                block_count,
                                                open_block);
    }
    return open_block;
}

static ssize_t
hir_lower_block_body_to_cfg(ASTNode *body,
                            HIRBasicBlock **blocks,
                            size_t *block_count,
                            ssize_t current_block)
{
    if (body == NULL)
        return current_block;
    if (body->type == AST_BLOCK) {
        return hir_lower_stmt_list_to_cfg(body->data.block.statements,
                                          body->data.block.count,
                                          blocks,
                                          block_count,
                                          current_block);
    }
    return hir_lower_stmt_node_to_cfg(body, blocks, block_count, current_block);
}

static ssize_t
hir_lower_stmt_node_to_cfg(ASTNode *node,
                           HIRBasicBlock **blocks,
                           size_t *block_count,
                           ssize_t current_block)
{
    if (node == NULL || current_block < 0)
        return current_block;

    switch (node->type) {
        case AST_BLOCK:
            return hir_lower_stmt_list_to_cfg(node->data.block.statements,
                                              node->data.block.count,
                                              blocks,
                                              block_count,
                                              current_block);

        case AST_RETURN:
            if (!cfg_append_stmt(&(*blocks)[(size_t)current_block].statements,
                                 &(*blocks)[(size_t)current_block].statement_count,
                                 node))
                return -1;
            cfg_set_return(&(*blocks)[(size_t)current_block], node->data.return_stmt.value);
            return -1;

        case AST_IF_STMT: {
            HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
            if (!cfg_append_stmt(&block->statements, &block->statement_count, node))
                return -1;
            ssize_t then_block = cfg_new_block(blocks, block_count);
            ssize_t join_block = cfg_new_block(blocks, block_count);
            if (then_block < 0 || join_block < 0)
                return -1;

            if (node->data.if_stmt.else_branch != NULL) {
                ssize_t else_block = cfg_new_block(blocks, block_count);
                if (else_block < 0)
                    return -1;
                block = &(*blocks)[(size_t)current_block];
                cfg_set_branch(block,
                               node->data.if_stmt.condition,
                               (size_t)then_block,
                               (size_t)else_block);

                ssize_t then_open = hir_lower_block_body_to_cfg(node->data.if_stmt.then_branch,
                                                                blocks,
                                                                block_count,
                                                                then_block);
                if (then_open >= 0)
                    cfg_set_goto(&(*blocks)[(size_t)then_open], (size_t)join_block);

                ssize_t else_open = hir_lower_block_body_to_cfg(node->data.if_stmt.else_branch,
                                                                blocks,
                                                                block_count,
                                                                else_block);
                if (else_open >= 0)
                    cfg_set_goto(&(*blocks)[(size_t)else_open], (size_t)join_block);
            } else {
                block = &(*blocks)[(size_t)current_block];
                cfg_set_branch(block,
                               node->data.if_stmt.condition,
                               (size_t)then_block,
                               (size_t)join_block);

                ssize_t then_open = hir_lower_block_body_to_cfg(node->data.if_stmt.then_branch,
                                                                blocks,
                                                                block_count,
                                                                then_block);
                if (then_open >= 0)
                    cfg_set_goto(&(*blocks)[(size_t)then_open], (size_t)join_block);
            }

            return join_block;
        }

        case AST_WHILE_LOOP: {
            HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
            if (!cfg_append_stmt(&block->statements, &block->statement_count, node))
                return -1;
            ssize_t cond_block = cfg_new_block(blocks, block_count);
            ssize_t body_block = cfg_new_block(blocks, block_count);
            ssize_t exit_block = cfg_new_block(blocks, block_count);
            if (cond_block < 0 || body_block < 0 || exit_block < 0)
                return -1;
            block = &(*blocks)[(size_t)current_block];
            cfg_set_goto(block, (size_t)cond_block);
            (*blocks)[(size_t)cond_block].is_loop_header = true;
            cfg_set_branch(&(*blocks)[(size_t)cond_block],
                           node->data.while_loop.condition,
                           (size_t)body_block,
                           (size_t)exit_block);
            ssize_t body_open = hir_lower_block_body_to_cfg(node->data.while_loop.body,
                                                            blocks,
                                                            block_count,
                                                            body_block);
            if (body_open >= 0)
                cfg_set_goto(&(*blocks)[(size_t)body_open], (size_t)cond_block);
            return exit_block;
        }

        case AST_WITH_STMT:
            if (!cfg_append_stmt(&(*blocks)[(size_t)current_block].statements,
                                 &(*blocks)[(size_t)current_block].statement_count,
                                 node)) {
                return -1;
            }
            return hir_lower_block_body_to_cfg(node->data.with_stmt.body,
                                               blocks,
                                               block_count,
                                               current_block);

        default:
            if (!cfg_append_stmt(&(*blocks)[(size_t)current_block].statements,
                                 &(*blocks)[(size_t)current_block].statement_count,
                                 node))
                return -1;
            return current_block;
    }
}

bool
hir_lower_func_body_cfg(ASTNode *body, HIRRoutine *routine)
{
    if (body == NULL)
        return true;

    HIRBasicBlock *blocks = NULL;
    size_t block_count = 0;
    ssize_t entry = cfg_new_block(&blocks, &block_count);
    if (entry < 0)
        return false;

    ssize_t open_block = hir_lower_block_body_to_cfg(body, &blocks, &block_count, entry);
    if (open_block >= 0)
        cfg_set_unreachable(&blocks[(size_t)open_block]);

    routine->cfg.blocks = blocks;
    routine->cfg.block_count = block_count;
    routine->cfg.entry_block = (size_t)entry;
    routine->has_cfg = true;
    return true;
}
