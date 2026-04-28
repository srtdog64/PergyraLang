#include "hir_lower_cfg.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct
{
    bool     active;
    bool     view_is_write;
    const char *source_name;
    const char *view_name;
    ASTNode *block_ast;
} HIRPinRegionContext;

typedef struct HIRLoopContext
{
    bool                    active;
    const char             *label;
    size_t                  break_target;
    size_t                  continue_target;
    const struct HIRLoopContext *parent;
} HIRLoopContext;

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

static void
cfg_apply_pin_region(HIRBasicBlock *block, const HIRPinRegionContext *pin)
{
    if (block == NULL || pin == NULL || !pin->active)
        return;
    block->is_pin_region = true;
    block->pin_view_is_write = pin->view_is_write;
    block->pin_source_name = pin->source_name;
    block->pin_view_name = pin->view_name;
    block->pin_block_ast = pin->block_ast;
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

static ssize_t
cfg_new_region_block(HIRBasicBlock **blocks,
                     size_t *count,
                     const HIRPinRegionContext *pin)
{
    ssize_t id = cfg_new_block(blocks, count);
    if (id >= 0)
        cfg_apply_pin_region(&(*blocks)[(size_t)id], pin);
    return id;
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

static bool
cfg_resolve_loop_target(const HIRLoopContext *loop,
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

static ssize_t
hir_lower_stmt_node_to_cfg(ASTNode *node,
                           HIRBasicBlock **blocks,
                           size_t *block_count,
                           ssize_t current_block,
                           const HIRPinRegionContext *pin,
                           const HIRLoopContext *loop);

static ssize_t
hir_lower_stmt_list_to_cfg(ASTNode **statements,
                           size_t statement_count,
                           HIRBasicBlock **blocks,
                           size_t *block_count,
                           ssize_t current_block,
                           const HIRPinRegionContext *pin,
                           const HIRLoopContext *loop)
{
    ssize_t open_block = current_block;
    for (size_t i = 0; i < statement_count && open_block >= 0; i++) {
        open_block = hir_lower_stmt_node_to_cfg(statements[i],
                                                blocks,
                                                block_count,
                                                open_block,
                                                pin,
                                                loop);
    }
    return open_block;
}

static ssize_t
hir_lower_block_body_to_cfg(ASTNode *body,
                            HIRBasicBlock **blocks,
                            size_t *block_count,
                            ssize_t current_block,
                            const HIRPinRegionContext *pin,
                            const HIRLoopContext *loop)
{
    if (body == NULL)
        return current_block;
    if (body->type == AST_BLOCK) {
        return hir_lower_stmt_list_to_cfg(body->data.block.statements,
                                          body->data.block.count,
                                          blocks,
                                          block_count,
                                          current_block,
                                          pin,
                                          loop);
    }
    return hir_lower_stmt_node_to_cfg(body, blocks, block_count, current_block, pin, loop);
}

static ASTNode *cfg_choice_body(ASTNode *choice, bool choice_is_match_case)
{
    if (choice == NULL)
        return NULL;
    return choice_is_match_case ? choice->data.match_case.body : choice;
}

static ssize_t
hir_lower_choice_to_cfg(ASTNode *node, ASTNode **choices,
                        size_t choice_count,
                        ASTNode *default_body,
                        bool choice_is_match_case,
                        HIRBasicBlock **blocks,
                        size_t *block_count,
                        ssize_t current_block,
                        const HIRPinRegionContext *pin,
                        const HIRLoopContext *loop)
{
    HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
    if (!cfg_append_stmt(&block->statements, &block->statement_count, node))
        return -1;

    ssize_t join_block = cfg_new_region_block(blocks, block_count, pin);
    if (join_block < 0)
        return -1;

    if (choice_count == 0) {
        if (default_body == NULL) {
            cfg_set_goto(&(*blocks)[(size_t)current_block], (size_t)join_block);
            return join_block;
        }
        ssize_t default_block = cfg_new_region_block(blocks, block_count, pin);
        if (default_block < 0)
            return -1;
        cfg_set_goto(&(*blocks)[(size_t)current_block], (size_t)default_block);
        ssize_t default_open = hir_lower_block_body_to_cfg(default_body,
                                                           blocks,
                                                           block_count,
                                                           default_block,
                                                           pin,
                                                           loop);
        if (default_open >= 0)
            cfg_set_goto(&(*blocks)[(size_t)default_open], (size_t)join_block);
        return join_block;
    }

    ssize_t dispatch_block = current_block;
    for (size_t i = 0; i < choice_count; i++) {
        ASTNode *choice = choices[i];
        ssize_t body_block = cfg_new_region_block(blocks, block_count, pin);
        ssize_t next_dispatch = -1;
        ssize_t false_target = join_block;
        if (body_block < 0)
            return -1;

        if (i + 1 < choice_count) {
            next_dispatch = cfg_new_region_block(blocks, block_count, pin);
            if (next_dispatch < 0)
                return -1;
            false_target = next_dispatch;
        } else if (default_body != NULL) {
            false_target = cfg_new_region_block(blocks, block_count, pin);
            if (false_target < 0)
                return -1;
        }

        cfg_set_branch(&(*blocks)[(size_t)dispatch_block],
                       choice,
                       (size_t)body_block,
                       (size_t)false_target);

        ssize_t body_open = hir_lower_block_body_to_cfg(cfg_choice_body(choice, choice_is_match_case),
                                                        blocks,
                                                        block_count,
                                                        body_block,
                                                        pin,
                                                        loop);
        if (body_open >= 0)
            cfg_set_goto(&(*blocks)[(size_t)body_open], (size_t)join_block);

        if (i + 1 < choice_count) {
            dispatch_block = next_dispatch;
        } else if (default_body != NULL) {
            ssize_t default_open = hir_lower_block_body_to_cfg(default_body, blocks,
                                                               block_count, false_target,
                                                               pin, loop);
            if (default_open >= 0)
                cfg_set_goto(&(*blocks)[(size_t)default_open], (size_t)join_block);
        }
    }
    return join_block;
}

static ssize_t
hir_lower_stmt_node_to_cfg(ASTNode *node,
                           HIRBasicBlock **blocks,
                           size_t *block_count,
                           ssize_t current_block,
                           const HIRPinRegionContext *pin,
                           const HIRLoopContext *loop)
{
    if (node == NULL || current_block < 0)
        return current_block;

    switch (node->type) {
        case AST_BLOCK:
            if (node->data.block.is_pin_block) {
                HIRPinRegionContext nested_pin = {
                    .active = true,
                    .view_is_write = node->data.block.pin_view_is_write,
                    .source_name = node->data.block.pin_source_name,
                    .view_name = node->data.block.pin_view_name,
                    .block_ast = node
                };
                HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
                ssize_t pin_entry = current_block;
                if (block->statement_count > 0
                    || block->terminator_kind != HIR_BLOCK_FALLTHROUGH) {
                    pin_entry = cfg_new_region_block(blocks, block_count, &nested_pin);
                    if (pin_entry < 0)
                        return -1;
                    cfg_set_goto(&(*blocks)[(size_t)current_block], (size_t)pin_entry);
                } else {
                    cfg_apply_pin_region(block, &nested_pin);
                }

                ssize_t pin_open = hir_lower_stmt_list_to_cfg(node->data.block.statements,
                                                              node->data.block.count,
                                                              blocks,
                                                              block_count,
                                                              pin_entry,
                                                              &nested_pin,
                                                              loop);
                if (pin_open < 0)
                    return -1;

                ssize_t after_pin = cfg_new_region_block(blocks, block_count, pin);
                if (after_pin < 0)
                    return -1;
                cfg_set_goto(&(*blocks)[(size_t)pin_open], (size_t)after_pin);
                return after_pin;
            }
            return hir_lower_stmt_list_to_cfg(node->data.block.statements,
                                              node->data.block.count,
                                              blocks,
                                              block_count,
                                              current_block,
                                              pin,
                                              loop);

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
            ssize_t then_block = cfg_new_region_block(blocks, block_count, pin);
            ssize_t join_block = cfg_new_region_block(blocks, block_count, pin);
            if (then_block < 0 || join_block < 0)
                return -1;

            if (node->data.if_stmt.else_branch != NULL) {
                ssize_t else_block = cfg_new_region_block(blocks, block_count, pin);
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
                                                                then_block,
                                                                pin,
                                                                loop);
                if (then_open >= 0)
                    cfg_set_goto(&(*blocks)[(size_t)then_open], (size_t)join_block);

                ssize_t else_open = hir_lower_block_body_to_cfg(node->data.if_stmt.else_branch,
                                                                blocks,
                                                                block_count,
                                                                else_block,
                                                                pin,
                                                                loop);
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
                                                                then_block,
                                                                pin,
                                                                loop);
                if (then_open >= 0)
                    cfg_set_goto(&(*blocks)[(size_t)then_open], (size_t)join_block);
            }

            return join_block;
        }

        case AST_WHILE_LOOP: {
            HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
            if (!cfg_append_stmt(&block->statements, &block->statement_count, node))
                return -1;
            ssize_t cond_block = cfg_new_region_block(blocks, block_count, pin);
            ssize_t body_block = cfg_new_region_block(blocks, block_count, pin);
            ssize_t exit_block = cfg_new_region_block(blocks, block_count, pin);
            if (cond_block < 0 || body_block < 0 || exit_block < 0)
                return -1;
            block = &(*blocks)[(size_t)current_block];
            cfg_set_goto(block, (size_t)cond_block);
            (*blocks)[(size_t)cond_block].is_loop_header = true;
            cfg_set_branch(&(*blocks)[(size_t)cond_block],
                           node->data.while_loop.condition,
                           (size_t)body_block,
                           (size_t)exit_block);
            HIRLoopContext nested_loop = {
                .active = true,
                .label = node->data.while_loop.label,
                .break_target = (size_t)exit_block,
                .continue_target = (size_t)cond_block,
                .parent = loop
            };
            ssize_t body_open = hir_lower_block_body_to_cfg(node->data.while_loop.body,
                                                            blocks,
                                                            block_count,
                                                            body_block,
                                                            pin,
                                                            &nested_loop);
            if (body_open >= 0)
                cfg_set_goto(&(*blocks)[(size_t)body_open], (size_t)cond_block);
            return exit_block;
        }

        case AST_FOR_LOOP: {
            HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
            if (!cfg_append_stmt(&block->statements, &block->statement_count, node))
                return -1;
            ssize_t header_block = cfg_new_region_block(blocks, block_count, pin);
            ssize_t body_block = cfg_new_region_block(blocks, block_count, pin);
            ssize_t exit_block = cfg_new_region_block(blocks, block_count, pin);
            if (header_block < 0 || body_block < 0 || exit_block < 0)
                return -1;

            block = &(*blocks)[(size_t)current_block];
            cfg_set_goto(block, (size_t)header_block);
            (*blocks)[(size_t)header_block].is_loop_header = true;
            cfg_set_branch(&(*blocks)[(size_t)header_block],
                           node,
                           (size_t)body_block,
                           (size_t)exit_block);

            HIRLoopContext nested_loop = {
                .active = true,
                .label = node->data.for_loop.label,
                .break_target = (size_t)exit_block,
                .continue_target = (size_t)header_block,
                .parent = loop
            };
            ssize_t body_open = hir_lower_block_body_to_cfg(node->data.for_loop.body,
                                                            blocks,
                                                            block_count,
                                                            body_block,
                                                            pin,
                                                            &nested_loop);
            if (body_open >= 0)
                cfg_set_goto(&(*blocks)[(size_t)body_open], (size_t)header_block);
            return exit_block;
        }

        case AST_SELECT_STMT:
            return hir_lower_choice_to_cfg(node,
                                           node->data.select_stmt.cases,
                                           node->data.select_stmt.case_count,
                                           node->data.select_stmt.default_case,
                                           false,
                                           blocks,
                                           block_count,
                                           current_block,
                                           pin,
                                           loop);

        case AST_MATCH_STMT:
            return hir_lower_choice_to_cfg(node,
                                           node->data.match_stmt.cases,
                                           node->data.match_stmt.case_count,
                                           node->data.match_stmt.default_body,
                                           true,
                                           blocks,
                                           block_count,
                                           current_block,
                                           pin,
                                           loop);

        case AST_BREAK:
            {
                size_t target = 0;
                HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
                if (cfg_resolve_loop_target(loop, node->data.break_stmt.label, false, &target)) {
                    if (!cfg_append_stmt(&block->statements, &block->statement_count, node))
                        return -1;
                    cfg_set_goto(block, target);
                    return -1;
                }
            }
            if (!cfg_append_stmt(&(*blocks)[(size_t)current_block].statements,
                                 &(*blocks)[(size_t)current_block].statement_count,
                                 node))
                return -1;
            return current_block;

        case AST_CONTINUE:
            {
                size_t target = 0;
                HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
                if (cfg_resolve_loop_target(loop, node->data.continue_stmt.label, true, &target)) {
                    if (!cfg_append_stmt(&block->statements, &block->statement_count, node))
                        return -1;
                    cfg_set_goto(block, target);
                    return -1;
                }
            }
            if (!cfg_append_stmt(&(*blocks)[(size_t)current_block].statements,
                                 &(*blocks)[(size_t)current_block].statement_count,
                                 node))
                return -1;
            return current_block;

        case AST_WITH_STMT:
            if (!cfg_append_stmt(&(*blocks)[(size_t)current_block].statements,
                                 &(*blocks)[(size_t)current_block].statement_count,
                                 node)) {
                return -1;
            }
            return hir_lower_block_body_to_cfg(node->data.with_stmt.body,
                                               blocks,
                                               block_count,
                                               current_block,
                                               pin,
                                               loop);

        case AST_UNSAFE_BLOCK:
            if (!cfg_append_stmt(&(*blocks)[(size_t)current_block].statements,
                                 &(*blocks)[(size_t)current_block].statement_count,
                                 node)) {
                return -1;
            }
            return hir_lower_block_body_to_cfg(node->data.unsafe_block.body,
                                               blocks,
                                               block_count,
                                               current_block,
                                               pin,
                                               loop);

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

    ssize_t open_block = hir_lower_block_body_to_cfg(body, &blocks, &block_count, entry, NULL, NULL);
    if (open_block >= 0)
        cfg_set_unreachable(&blocks[(size_t)open_block]);

    routine->cfg.blocks = blocks;
    routine->cfg.block_count = block_count;
    routine->cfg.entry_block = (size_t)entry;
    routine->has_cfg = true;
    return true;
}
