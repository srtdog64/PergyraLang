#include "hir_lower_cfg.h"
#include "hir_lower_cfg_internal.h"

#include <sys/types.h>

static ssize_t
hir_lower_stmt_node_to_cfg(ASTNode *node,
                           HIRBasicBlock **blocks,
                           size_t *block_count,
                           size_t *block_capacity,
                           ssize_t current_block,
                           const HIRPinRegionContext *pin,
                           const HIRLoopContext *loop);

static ssize_t
hir_lower_stmt_list_to_cfg(ASTNode **statements,
                           size_t statement_count,
                           HIRBasicBlock **blocks,
                           size_t *block_count,
                           size_t *block_capacity,
                           ssize_t current_block,
                           const HIRPinRegionContext *pin,
                           const HIRLoopContext *loop)
{
    ssize_t open_block = current_block;
    for (size_t i = 0; i < statement_count && open_block >= 0; i++) {
        open_block = hir_lower_stmt_node_to_cfg(statements[i],
                                                blocks,
                                                block_count,
                                                block_capacity,
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
                            size_t *block_capacity,
                            ssize_t current_block,
                            const HIRPinRegionContext *pin,
                            const HIRLoopContext *loop)
{
    if (body == NULL)
        return current_block;
    if (body->type == AST_BLOCK) {
        size_t statement_count = 0;
        ASTNode **statements = ast_block_statements(body, &statement_count);
        return hir_lower_stmt_list_to_cfg(statements,
                                          statement_count,
                                          blocks,
                                          block_count,
                                          block_capacity,
                                          current_block,
                                          pin,
                                          loop);
    }
    return hir_lower_stmt_node_to_cfg(body,
                                      blocks,
                                      block_count,
                                      block_capacity,
                                      current_block,
                                      pin,
                                      loop);
}

static ASTNode *cfg_choice_body(ASTNode *choice, bool choice_is_match_case)
{
    if (choice == NULL)
        return NULL;
    return choice_is_match_case ? ast_match_case_body(choice) : choice;
}

static bool
hir_cfg_append_block_stmt(HIRBasicBlock *block, ASTNode *node)
{
    return block != NULL
        && hir_cfg_append_stmt(&block->statements,
                               &block->statement_count,
                               &block->statement_capacity,
                               node);
}

static ssize_t
hir_lower_choice_to_cfg(ASTNode *node, ASTNode **choices,
                        size_t choice_count,
                        ASTNode *default_body,
                        bool choice_is_match_case,
                        HIRBasicBlock **blocks,
                        size_t *block_count,
                        size_t *block_capacity,
                        ssize_t current_block,
                        const HIRPinRegionContext *pin,
                        const HIRLoopContext *loop)
{
    HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
    if (!hir_cfg_append_block_stmt(block, node))
        return -1;

    ssize_t join_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
    if (join_block < 0)
        return -1;

    if (choice_count == 0) {
        if (default_body == NULL) {
            hir_cfg_set_goto(&(*blocks)[(size_t)current_block], (size_t)join_block);
            return join_block;
        }
        ssize_t default_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
        if (default_block < 0)
            return -1;
        hir_cfg_set_goto(&(*blocks)[(size_t)current_block], (size_t)default_block);
        ssize_t default_open = hir_lower_block_body_to_cfg(default_body,
                                                           blocks,
                                                           block_count,
                                                           block_capacity,
                                                           default_block,
                                                           pin,
                                                           loop);
        if (default_open >= 0)
            hir_cfg_set_goto(&(*blocks)[(size_t)default_open], (size_t)join_block);
        return join_block;
    }

    ssize_t dispatch_block = current_block;
    for (size_t i = 0; i < choice_count; i++) {
        ASTNode *choice = choices[i];
        ssize_t body_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
        ssize_t next_dispatch = -1;
        ssize_t false_target = join_block;
        if (body_block < 0)
            return -1;
        if (!choice_is_match_case)
            (*blocks)[(size_t)body_block].is_select_case_body = true;

        if (i + 1 < choice_count) {
            next_dispatch = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
            if (next_dispatch < 0)
                return -1;
            false_target = next_dispatch;
        } else if (default_body != NULL) {
            false_target = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
            if (false_target < 0)
                return -1;
        }

        hir_cfg_set_branch(&(*blocks)[(size_t)dispatch_block],
                       choice,
                       (size_t)body_block,
                       (size_t)false_target);

        ssize_t body_open = hir_lower_block_body_to_cfg(cfg_choice_body(choice, choice_is_match_case),
                                                        blocks,
                                                        block_count,
                                                        block_capacity,
                                                        body_block,
                                                        pin,
                                                        loop);
        if (body_open >= 0)
            hir_cfg_set_goto(&(*blocks)[(size_t)body_open], (size_t)join_block);

        if (i + 1 < choice_count) {
            dispatch_block = next_dispatch;
        } else if (default_body != NULL) {
            ssize_t default_open = hir_lower_block_body_to_cfg(default_body, blocks,
                                                               block_count, block_capacity, false_target,
                                                               pin, loop);
            if (default_open >= 0)
                hir_cfg_set_goto(&(*blocks)[(size_t)default_open], (size_t)join_block);
        }
    }
    return join_block;
}

static ssize_t
hir_lower_stmt_node_to_cfg(ASTNode *node,
                           HIRBasicBlock **blocks,
                           size_t *block_count,
                           size_t *block_capacity,
                           ssize_t current_block,
                           const HIRPinRegionContext *pin,
                           const HIRLoopContext *loop)
{
    if (node == NULL || current_block < 0)
        return current_block;

    switch (node->type) {
        case AST_BLOCK:
            if (ast_block_is_pin_block(node)) {
                HIRPinRegionContext nested_pin = {
                    .active = true,
                    .view_is_write = ast_block_pin_view_is_write(node),
                    .source_name = ast_block_pin_source_name(node),
                    .view_name = ast_block_pin_view_name(node),
                    .block_ast = node
                };
                HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
                ssize_t pin_entry = current_block;
                if (block->statement_count > 0
                    || block->terminator_kind != HIR_BLOCK_FALLTHROUGH) {
                    pin_entry = hir_cfg_new_region_block(blocks, block_count, block_capacity, &nested_pin);
                    if (pin_entry < 0)
                        return -1;
                    hir_cfg_set_goto(&(*blocks)[(size_t)current_block], (size_t)pin_entry);
                } else {
                    hir_cfg_apply_pin_region(block, &nested_pin);
                }

                size_t statement_count = 0;
                ASTNode **statements = ast_block_statements(node, &statement_count);
                ssize_t pin_open = hir_lower_stmt_list_to_cfg(statements,
                                                              statement_count,
                                                              blocks,
                                                              block_count,
                                                              block_capacity,
                                                              pin_entry,
                                                              &nested_pin,
                                                              loop);
                if (pin_open < 0)
                    return -1;

                ssize_t after_pin = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
                if (after_pin < 0)
                    return -1;
                hir_cfg_set_goto(&(*blocks)[(size_t)pin_open], (size_t)after_pin);
                return after_pin;
            }
            {
                size_t statement_count = 0;
                ASTNode **statements = ast_block_statements(node, &statement_count);
                return hir_lower_stmt_list_to_cfg(statements,
                                              statement_count,
                                              blocks,
                                              block_count,
                                              block_capacity,
                                              current_block,
                                              pin,
                                              loop);
            }

        case AST_RETURN:
            if (!hir_cfg_append_block_stmt(&(*blocks)[(size_t)current_block], node))
                return -1;
            hir_cfg_set_return(&(*blocks)[(size_t)current_block], ast_return_value(node));
            return -1;

        case AST_IF_STMT: {
            HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
            if (!hir_cfg_append_block_stmt(block, node))
                return -1;
            ssize_t then_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
            ssize_t join_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
            if (then_block < 0 || join_block < 0)
                return -1;

            if (ast_if_else_branch(node) != NULL) {
                ssize_t else_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
                if (else_block < 0)
                    return -1;
                block = &(*blocks)[(size_t)current_block];
                hir_cfg_set_branch(block,
                               ast_if_condition(node),
                               (size_t)then_block,
                               (size_t)else_block);

                ssize_t then_open = hir_lower_block_body_to_cfg(ast_if_then_branch(node),
                                                                blocks,
                                                                block_count,
                                                                block_capacity,
                                                                then_block,
                                                                pin,
                                                                loop);
                if (then_open >= 0)
                    hir_cfg_set_goto(&(*blocks)[(size_t)then_open], (size_t)join_block);

                ssize_t else_open = hir_lower_block_body_to_cfg(ast_if_else_branch(node),
                                                                blocks,
                                                                block_count,
                                                                block_capacity,
                                                                else_block,
                                                                pin,
                                                                loop);
                if (else_open >= 0)
                    hir_cfg_set_goto(&(*blocks)[(size_t)else_open], (size_t)join_block);
            } else {
                block = &(*blocks)[(size_t)current_block];
                hir_cfg_set_branch(block,
                               ast_if_condition(node),
                               (size_t)then_block,
                               (size_t)join_block);

                ssize_t then_open = hir_lower_block_body_to_cfg(ast_if_then_branch(node),
                                                                blocks,
                                                                block_count,
                                                                block_capacity,
                                                                then_block,
                                                                pin,
                                                                loop);
                if (then_open >= 0)
                    hir_cfg_set_goto(&(*blocks)[(size_t)then_open], (size_t)join_block);
            }

            return join_block;
        }

        case AST_WHILE_LOOP: {
            HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
            if (!hir_cfg_append_block_stmt(block, node))
                return -1;
            ssize_t cond_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
            ssize_t body_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
            ssize_t exit_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
            if (cond_block < 0 || body_block < 0 || exit_block < 0)
                return -1;
            block = &(*blocks)[(size_t)current_block];
            hir_cfg_set_goto(block, (size_t)cond_block);
            (*blocks)[(size_t)cond_block].is_loop_header = true;
            hir_cfg_set_branch(&(*blocks)[(size_t)cond_block],
                           ast_while_condition(node),
                           (size_t)body_block,
                           (size_t)exit_block);
            HIRLoopContext nested_loop = {
                .active = true,
                .label = ast_while_label(node),
                .break_target = (size_t)exit_block,
                .continue_target = (size_t)cond_block,
                .parent = loop
            };
            ssize_t body_open = hir_lower_block_body_to_cfg(ast_while_body(node),
                                                            blocks,
                                                            block_count,
                                                            block_capacity,
                                                            body_block,
                                                            pin,
                                                            &nested_loop);
            if (body_open >= 0)
                hir_cfg_set_goto(&(*blocks)[(size_t)body_open], (size_t)cond_block);
            return exit_block;
        }

        case AST_FOR_LOOP: {
            HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
            if (!hir_cfg_append_block_stmt(block, node))
                return -1;
            ssize_t header_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
            ssize_t body_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
            ssize_t exit_block = hir_cfg_new_region_block(blocks, block_count, block_capacity, pin);
            if (header_block < 0 || body_block < 0 || exit_block < 0)
                return -1;

            block = &(*blocks)[(size_t)current_block];
            hir_cfg_set_goto(block, (size_t)header_block);
            (*blocks)[(size_t)header_block].is_loop_header = true;
            hir_cfg_set_branch(&(*blocks)[(size_t)header_block],
                           node,
                           (size_t)body_block,
                           (size_t)exit_block);

            HIRLoopContext nested_loop = {
                .active = true,
                .label = ast_for_label(node),
                .break_target = (size_t)exit_block,
                .continue_target = (size_t)header_block,
                .parent = loop
            };
            ssize_t body_open = hir_lower_block_body_to_cfg(ast_for_body(node),
                                                            blocks,
                                                            block_count,
                                                            block_capacity,
                                                            body_block,
                                                            pin,
                                                            &nested_loop);
            if (body_open >= 0)
                hir_cfg_set_goto(&(*blocks)[(size_t)body_open], (size_t)header_block);
            return exit_block;
        }

        case AST_SELECT_STMT:
            return hir_lower_choice_to_cfg(node,
                                           ast_select_cases(node, NULL),
                                           ast_select_case_count(node),
                                           ast_select_default_case(node),
                                           false,
                                           blocks,
                                           block_count,
                                           block_capacity,
                                           current_block,
                                           pin,
                                           loop);

        case AST_MATCH_STMT:
            return hir_lower_choice_to_cfg(node,
                                           ast_match_cases(node, NULL),
                                           ast_match_case_count(node),
                                           ast_match_default_body(node),
                                           true,
                                           blocks,
                                           block_count,
                                           block_capacity,
                                           current_block,
                                           pin,
                                           loop);

        case AST_BREAK:
            {
                size_t target = 0;
                HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
                if (hir_cfg_resolve_loop_target(loop, ast_break_label(node), false, &target)) {
                    if (!hir_cfg_append_block_stmt(block, node))
                        return -1;
                    hir_cfg_set_goto(block, target);
                    return -1;
                }
            }
            if (!hir_cfg_append_block_stmt(&(*blocks)[(size_t)current_block], node))
                return -1;
            return current_block;

        case AST_CONTINUE:
            {
                size_t target = 0;
                HIRBasicBlock *block = &(*blocks)[(size_t)current_block];
                if (hir_cfg_resolve_loop_target(loop, ast_continue_label(node), true, &target)) {
                    if (!hir_cfg_append_block_stmt(block, node))
                        return -1;
                    hir_cfg_set_goto(block, target);
                    return -1;
                }
            }
            if (!hir_cfg_append_block_stmt(&(*blocks)[(size_t)current_block], node))
                return -1;
            return current_block;

        case AST_WITH_STMT:
            if (!hir_cfg_append_block_stmt(&(*blocks)[(size_t)current_block], node)) {
                return -1;
            }
            return hir_lower_block_body_to_cfg(ast_with_body(node),
                                               blocks,
                                               block_count,
                                               block_capacity,
                                               current_block,
                                               pin,
                                               loop);

        case AST_UNSAFE_BLOCK:
            if (!hir_cfg_append_block_stmt(&(*blocks)[(size_t)current_block], node)) {
                return -1;
            }
            return hir_lower_block_body_to_cfg(ast_unsafe_block_body(node),
                                               blocks,
                                               block_count,
                                               block_capacity,
                                               current_block,
                                               pin,
                                               loop);

        default:
            if (!hir_cfg_append_block_stmt(&(*blocks)[(size_t)current_block], node))
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
    size_t block_capacity = 0;
    ssize_t entry = hir_cfg_new_block(&blocks, &block_count, &block_capacity);
    if (entry < 0)
        return false;

    ssize_t open_block = hir_lower_block_body_to_cfg(body,
                                                     &blocks,
                                                     &block_count,
                                                     &block_capacity,
                                                     entry,
                                                     NULL,
                                                     NULL);
    if (open_block >= 0)
        hir_cfg_set_unreachable(&blocks[(size_t)open_block]);

    routine->cfg.blocks = blocks;
    routine->cfg.block_count = block_count;
    routine->cfg.block_capacity = block_capacity;
    routine->cfg.entry_block = (size_t)entry;
    routine->has_cfg = true;
    return true;
}
