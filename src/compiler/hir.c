#include "hir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../common/string_compat.h"

static bool
append_ast(ASTNode ***items, size_t *count, ASTNode *node)
{
    ASTNode **grown = realloc(*items, (*count + 1) * sizeof(ASTNode *));
    if (grown == NULL)
        return false;
    grown[*count] = node;
    *items = grown;
    (*count)++;
    return true;
}

static bool
append_item(HIRTopLevelItem **items, size_t *count, HIRTopLevelItem item)
{
    HIRTopLevelItem *grown = realloc(*items, (*count + 1) * sizeof(HIRTopLevelItem));
    if (grown == NULL)
        return false;
    grown[*count] = item;
    *items = grown;
    (*count)++;
    return true;
}

static bool
append_decl(HIRDecl **decls, size_t *count, HIRDecl decl)
{
    HIRDecl *grown = realloc(*decls, (*count + 1) * sizeof(HIRDecl));
    if (grown == NULL)
        return false;
    grown[*count] = decl;
    *decls = grown;
    (*count)++;
    return true;
}

static bool
append_index_unique(size_t **items, size_t *count, size_t value)
{
    for (size_t i = 0; i < *count; i++) {
        if ((*items)[i] == value)
            return true;
    }

    size_t *grown = realloc(*items, (*count + 1) * sizeof(size_t));
    if (grown == NULL)
        return false;
    grown[*count] = value;
    *items = grown;
    (*count)++;
    return true;
}

static bool
cfg_append_stmt(ASTNode ***items, size_t *count, ASTNode *node)
{
    return append_ast(items, count, node);
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

static bool
append_call_name(const char ***names, size_t *count, const char *name)
{
    if (name == NULL || *name == '\0')
        return true;

    for (size_t i = 0; i < *count; i++) {
        if ((*names)[i] != NULL && strcmp((*names)[i], name) == 0)
            return true;
    }

    const char **grown = realloc((void *)*names, (*count + 1) * sizeof(const char *));
    if (grown == NULL)
        return false;
    grown[*count] = name;
    *names = grown;
    (*count)++;
    return true;
}

static bool
append_name_unique(const char ***names, size_t *count, const char *name)
{
    return append_call_name(names, count, name);
}

static bool
hir_collect_type_refs(ASTNode *type_node, const char ***names, size_t *count)
{
    if (type_node == NULL)
        return true;

    switch (type_node->type) {
        case AST_TYPE:
            if (!append_call_name(names, count, type_node->data.type.name))
                return false;
            if (type_node->data.type.generic_args != NULL) {
                for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
                    GenericParam *arg = type_node->data.type.generic_args->params[i];
                    if (arg != NULL
                        && arg->constraint != NULL
                        && !hir_collect_type_refs(arg->constraint, names, count)) {
                        return false;
                    }
                }
            }
            return true;

        case AST_CHANNEL_TYPE:
            return hir_collect_type_refs(type_node->data.channel_type.element_type,
                                         names,
                                         count);

        case AST_FUTURE_TYPE:
            return hir_collect_type_refs(type_node->data.future_type.value_type,
                                         names,
                                         count);

        default:
            return true;
    }
}

static bool
hir_collect_func_signature_refs(ASTNode *node, const char ***names, size_t *count)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return true;

    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *param = node->data.func_decl.params[i];
        if (param != NULL && !hir_collect_type_refs(param->type, names, count))
            return false;
    }

    if (!hir_collect_type_refs(node->data.func_decl.return_type, names, count))
        return false;

    if (node->data.func_decl.within_zone != NULL
        && !append_call_name(names, count, node->data.func_decl.within_zone)) {
        return false;
    }

    if (node->data.func_decl.causes_effect != NULL
        && !append_call_name(names, count, node->data.func_decl.causes_effect)) {
        return false;
    }

    return true;
}

static bool
hir_collect_intent_signature_refs(ASTNode *node, const char ***names, size_t *count)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return true;

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *inv = node->data.intent_decl.involves[i];
        if (inv != NULL
            && inv->type == AST_INTENT_INVOLVES
            && !hir_collect_type_refs(inv->data.intent_involves.subject_type, names, count)) {
            return false;
        }
    }

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (!hir_collect_type_refs(step->data.intent_step.where_type, names, count))
            return false;
        if (step->data.intent_step.causes_effect != NULL
            && !append_call_name(names, count, step->data.intent_step.causes_effect)) {
            return false;
        }
    }

    return true;
}

static bool
ast_contains_control_flow(ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
        case AST_IF_STMT:
        case AST_FOR_LOOP:
        case AST_WHILE_LOOP:
        case AST_SELECT_STMT:
        case AST_MATCH_STMT:
        case AST_PARALLEL_BLOCK:
        case AST_WITH_STMT:
        case AST_AWAIT_EXPR:
        case AST_SPAWN_EXPR:
        case AST_TASK_GROUP:
            return true;
        default:
            break;
    }

    switch (node->type) {
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                if (ast_contains_control_flow(node->data.block.statements[i]))
                    return true;
            }
            return false;

        case AST_RETURN:
            return ast_contains_control_flow(node->data.return_stmt.value);

        case AST_LET_DECL:
            return ast_contains_control_flow(node->data.let_decl.initializer);

        case AST_ASSIGNMENT:
            return ast_contains_control_flow(node->data.assignment.target)
                   || ast_contains_control_flow(node->data.assignment.value);

        case AST_BINARY:
            return ast_contains_control_flow(node->data.binary.left)
                   || ast_contains_control_flow(node->data.binary.right);

        case AST_UNARY:
            return ast_contains_control_flow(node->data.unary.operand);

        case AST_CALL:
            if (ast_contains_control_flow(node->data.call.callee))
                return true;
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (ast_contains_control_flow(node->data.call.arguments[i]))
                    return true;
            }
            return false;

        case AST_MEMBER_ACCESS:
            return ast_contains_control_flow(node->data.member.object);

        case AST_ARRAY_ACCESS:
            return ast_contains_control_flow(node->data.array_access.array)
                   || ast_contains_control_flow(node->data.array_access.index);

        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < node->data.array_literal.count; i++) {
                if (ast_contains_control_flow(node->data.array_literal.elements[i]))
                    return true;
            }
            return false;

        case AST_MATCH_CASE:
            return ast_contains_control_flow(node->data.match_case.pattern)
                   || ast_contains_control_flow(node->data.match_case.guard)
                   || ast_contains_control_flow(node->data.match_case.body);

        case AST_ASYNC_BLOCK:
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                if (ast_contains_control_flow(node->data.async_block.statements[i]))
                    return true;
            }
            return false;

        case AST_TASK_GROUP:
            for (size_t i = 0; i < node->data.task_group.task_count; i++) {
                if (ast_contains_control_flow(node->data.task_group.tasks[i]))
                    return true;
            }
            return false;

        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                if (ast_contains_control_flow(node->data.parallel.tasks[i]))
                    return true;
            }
            return false;

        default:
            return false;
    }
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
                /* if-else: create separate else block */
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
                /* if without else: branch directly to join for false case */
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

static bool
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

static bool
hir_finalize_cfg(HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (block->has_succ_true
            && !append_index_unique(&routine->cfg.blocks[block->succ_true].predecessors,
                                    &routine->cfg.blocks[block->succ_true].predecessor_count,
                                    i)) {
            return false;
        }
        if (block->has_succ_false
            && !append_index_unique(&routine->cfg.blocks[block->succ_false].predecessors,
                                    &routine->cfg.blocks[block->succ_false].predecessor_count,
                                    i)) {
            return false;
        }
    }

    return true;
}

static void
hir_cfg_mark_reachable(HIRRoutine *routine, size_t block_id)
{
    if (routine == NULL || !routine->has_cfg || block_id >= routine->cfg.block_count)
        return;

    HIRBasicBlock *block = &routine->cfg.blocks[block_id];
    if (block->is_reachable)
        return;
    block->is_reachable = true;

    if (block->has_succ_true)
        hir_cfg_mark_reachable(routine, block->succ_true);
    if (block->has_succ_false)
        hir_cfg_mark_reachable(routine, block->succ_false);
}

static void
hir_cfg_collect_rpo_postorder(const HIRRoutine *routine,
                              size_t block_id,
                              bool *visited,
                              size_t *postorder,
                              size_t *post_count)
{
    if (routine == NULL || !routine->has_cfg || block_id >= routine->cfg.block_count || visited[block_id])
        return;

    visited[block_id] = true;
    const HIRBasicBlock *block = &routine->cfg.blocks[block_id];
    if (block->has_succ_true)
        hir_cfg_collect_rpo_postorder(routine, block->succ_true, visited, postorder, post_count);
    if (block->has_succ_false)
        hir_cfg_collect_rpo_postorder(routine, block->succ_false, visited, postorder, post_count);
    postorder[(*post_count)++] = block_id;
}

static size_t
hir_cfg_intersect_idom(const HIRRoutine *routine, size_t *idoms, size_t a, size_t b)
{
    while (a != b) {
        while (routine->cfg.blocks[a].rpo_index > routine->cfg.blocks[b].rpo_index)
            a = idoms[a];
        while (routine->cfg.blocks[b].rpo_index > routine->cfg.blocks[a].rpo_index)
            b = idoms[b];
    }
    return a;
}

static bool
hir_cfg_block_dominates(const HIRRoutine *routine, size_t dom, size_t block_id)
{
    if (routine == NULL || !routine->has_cfg || dom >= routine->cfg.block_count
        || block_id >= routine->cfg.block_count) {
        return false;
    }

    if (!routine->cfg.blocks[dom].is_reachable || !routine->cfg.blocks[block_id].is_reachable)
        return false;

    size_t runner = block_id;
    while (true) {
        if (runner == dom)
            return true;
        if (!routine->cfg.blocks[runner].has_immediate_dominator)
            return false;
        size_t next = routine->cfg.blocks[runner].immediate_dominator;
        if (next == runner)
            return false;
        runner = next;
    }
}

static bool
hir_compute_cfg_dominance(HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    if (routine->cfg.entry_block >= routine->cfg.block_count)
        return false;

    if (routine->cfg.block_count > SIZE_MAX / sizeof(bool))
        return false;
    if (routine->cfg.block_count > SIZE_MAX / sizeof(size_t))
        return false;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        routine->cfg.blocks[i].is_reachable = false;
        routine->cfg.blocks[i].rpo_index = 0;
        routine->cfg.blocks[i].has_immediate_dominator = false;
        routine->cfg.blocks[i].immediate_dominator = 0;
    }

    hir_cfg_mark_reachable(routine, routine->cfg.entry_block);

    bool *visited = calloc(routine->cfg.block_count, sizeof(bool));
    size_t *postorder = calloc(routine->cfg.block_count, sizeof(size_t));
    size_t *idoms = malloc(routine->cfg.block_count * sizeof(size_t));
    if (visited == NULL || postorder == NULL || idoms == NULL) {
        free(visited);
        free(postorder);
        free(idoms);
        return false;
    }

    size_t post_count = 0;
    hir_cfg_collect_rpo_postorder(routine,
                                  routine->cfg.entry_block,
                                  visited,
                                  postorder,
                                  &post_count);

    for (size_t i = 0; i < routine->cfg.block_count; i++)
        idoms[i] = SIZE_MAX;

    for (size_t i = 0; i < post_count; i++) {
        size_t block_id = postorder[post_count - 1 - i];
        routine->cfg.blocks[block_id].rpo_index = i;
    }

    idoms[routine->cfg.entry_block] = routine->cfg.entry_block;
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < post_count; i++) {
            size_t block_id = postorder[post_count - 1 - i];
            if (block_id == routine->cfg.entry_block)
                continue;

            const HIRBasicBlock *block = &routine->cfg.blocks[block_id];
            size_t new_idom = SIZE_MAX;
            for (size_t j = 0; j < block->predecessor_count; j++) {
                size_t pred = block->predecessors[j];
                if (!routine->cfg.blocks[pred].is_reachable || idoms[pred] == SIZE_MAX)
                    continue;
                if (new_idom == SIZE_MAX)
                    new_idom = pred;
                else
                    new_idom = hir_cfg_intersect_idom(routine, idoms, pred, new_idom);
            }

            if (new_idom != SIZE_MAX && idoms[block_id] != new_idom) {
                idoms[block_id] = new_idom;
                changed = true;
            }
        }
    }

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        if (!routine->cfg.blocks[i].is_reachable || idoms[i] == SIZE_MAX)
            continue;
        routine->cfg.blocks[i].immediate_dominator = idoms[i];
        routine->cfg.blocks[i].has_immediate_dominator = true;
    }

    free(visited);
    free(postorder);
    free(idoms);
    return true;
}

static bool
hir_compute_cfg_dominance_frontier(HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        HIRBasicBlock *block = &routine->cfg.blocks[i];
        free(block->dominance_frontier);
        block->dominance_frontier = NULL;
        block->dominance_frontier_count = 0;
    }

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (!block->is_reachable || block->predecessor_count < 2 || !block->has_immediate_dominator)
            continue;

        for (size_t j = 0; j < block->predecessor_count; j++) {
            size_t runner = block->predecessors[j];
            while (runner != block->immediate_dominator) {
                if (!append_index_unique(&routine->cfg.blocks[runner].dominance_frontier,
                                         &routine->cfg.blocks[runner].dominance_frontier_count,
                                         i)) {
                    return false;
                }
                if (!routine->cfg.blocks[runner].has_immediate_dominator)
                    break;
                size_t next = routine->cfg.blocks[runner].immediate_dominator;
                if (next == runner)
                    break;
                runner = next;
            }
        }
    }

    return true;
}

static bool
hir_compute_cfg_dom_tree(HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        HIRBasicBlock *block = &routine->cfg.blocks[i];
        free(block->dom_tree_children);
        block->dom_tree_children = NULL;
        block->dom_tree_child_count = 0;
    }

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (!block->is_reachable || !block->has_immediate_dominator)
            continue;
        if (block->immediate_dominator == i)
            continue;
        if (!append_index_unique(&routine->cfg.blocks[block->immediate_dominator].dom_tree_children,
                                 &routine->cfg.blocks[block->immediate_dominator].dom_tree_child_count,
                                 i)) {
            return false;
        }
    }

    return true;
}

static bool
hir_mark_natural_loop(HIRRoutine *routine, size_t header, size_t latch)
{
    if (routine == NULL || !routine->has_cfg || header >= routine->cfg.block_count
        || latch >= routine->cfg.block_count) {
        return false;
    }

    bool *in_loop = calloc(routine->cfg.block_count, sizeof(bool));
    size_t *stack = malloc(routine->cfg.block_count * sizeof(size_t));
    if (in_loop == NULL || stack == NULL) {
        free(in_loop);
        free(stack);
        return false;
    }

    size_t stack_count = 0;
    in_loop[header] = true;
    routine->cfg.blocks[header].loop_depth++;
    routine->cfg.blocks[header].is_loop_header = true;

    if (!in_loop[latch]) {
        in_loop[latch] = true;
        routine->cfg.blocks[latch].loop_depth++;
        stack[stack_count++] = latch;
    }

    while (stack_count > 0) {
        size_t block_id = stack[--stack_count];
        HIRBasicBlock *block = &routine->cfg.blocks[block_id];
        for (size_t i = 0; i < block->predecessor_count; i++) {
            size_t pred = block->predecessors[i];
            if (pred >= routine->cfg.block_count || !routine->cfg.blocks[pred].is_reachable)
                continue;
            if (!in_loop[pred]) {
                in_loop[pred] = true;
                routine->cfg.blocks[pred].loop_depth++;
                stack[stack_count++] = pred;
            }
        }
    }

    free(in_loop);
    free(stack);
    return true;
}

static bool
hir_compute_cfg_loops(HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        routine->cfg.blocks[i].loop_depth = 0;
        routine->cfg.blocks[i].is_loop_header = false;
    }

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (!block->is_reachable)
            continue;

        if (block->has_succ_true
            && hir_cfg_block_dominates(routine, block->succ_true, i)
            && !hir_mark_natural_loop(routine, block->succ_true, i)) {
            return false;
        }
        if (block->has_succ_false
            && hir_cfg_block_dominates(routine, block->succ_false, i)
            && !hir_mark_natural_loop(routine, block->succ_false, i)) {
            return false;
        }
    }

    return true;
}

static bool
hir_collect_direct_calls(ASTNode *node, const char ***names, size_t *count)
{
    if (node == NULL)
        return true;

    switch (node->type) {
        case AST_CALL:
            if (node->data.call.callee != NULL
                && node->data.call.callee->type == AST_IDENTIFIER) {
                if (!append_call_name(names,
                                      count,
                                      node->data.call.callee->data.identifier.name)) {
                    return false;
                }
            }
            if (!hir_collect_direct_calls(node->data.call.callee, names, count))
                return false;
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (!hir_collect_direct_calls(node->data.call.arguments[i], names, count))
                    return false;
            }
            return true;

        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                if (!hir_collect_direct_calls(node->data.block.statements[i], names, count))
                    return false;
            }
            return true;

        case AST_RETURN:
            return hir_collect_direct_calls(node->data.return_stmt.value, names, count);

        case AST_LET_DECL:
            return hir_collect_direct_calls(node->data.let_decl.initializer, names, count);

        case AST_ASSIGNMENT:
            return hir_collect_direct_calls(node->data.assignment.target, names, count)
                   && hir_collect_direct_calls(node->data.assignment.value, names, count);

        case AST_BINARY:
            return hir_collect_direct_calls(node->data.binary.left, names, count)
                   && hir_collect_direct_calls(node->data.binary.right, names, count);

        case AST_UNARY:
            return hir_collect_direct_calls(node->data.unary.operand, names, count);

        case AST_MEMBER_ACCESS:
            return hir_collect_direct_calls(node->data.member.object, names, count);

        case AST_ARRAY_ACCESS:
            return hir_collect_direct_calls(node->data.array_access.array, names, count)
                   && hir_collect_direct_calls(node->data.array_access.index, names, count);

        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < node->data.array_literal.count; i++) {
                if (!hir_collect_direct_calls(node->data.array_literal.elements[i], names, count))
                    return false;
            }
            return true;

        case AST_IF_STMT:
            return hir_collect_direct_calls(node->data.if_stmt.condition, names, count)
                   && hir_collect_direct_calls(node->data.if_stmt.then_branch, names, count)
                   && hir_collect_direct_calls(node->data.if_stmt.else_branch, names, count);

        case AST_FOR_LOOP:
            return hir_collect_direct_calls(node->data.for_loop.range_start, names, count)
                   && hir_collect_direct_calls(node->data.for_loop.range_end, names, count)
                   && hir_collect_direct_calls(node->data.for_loop.iterable, names, count)
                   && hir_collect_direct_calls(node->data.for_loop.body, names, count);

        case AST_WHILE_LOOP:
            return hir_collect_direct_calls(node->data.while_loop.condition, names, count)
                   && hir_collect_direct_calls(node->data.while_loop.body, names, count);

        case AST_MATCH_STMT:
            if (!hir_collect_direct_calls(node->data.match_stmt.subject, names, count))
                return false;
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
                if (!hir_collect_direct_calls(node->data.match_stmt.cases[i], names, count))
                    return false;
            }
            return hir_collect_direct_calls(node->data.match_stmt.default_body, names, count);

        case AST_MATCH_CASE:
            return hir_collect_direct_calls(node->data.match_case.pattern, names, count)
                   && hir_collect_direct_calls(node->data.match_case.guard, names, count)
                   && hir_collect_direct_calls(node->data.match_case.body, names, count);

        case AST_SELECT_STMT:
            for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
                if (!hir_collect_direct_calls(node->data.select_stmt.cases[i], names, count))
                    return false;
            }
            return hir_collect_direct_calls(node->data.select_stmt.default_case, names, count);

        case AST_ASYNC_BLOCK:
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                if (!hir_collect_direct_calls(node->data.async_block.statements[i], names, count))
                    return false;
            }
            return true;

        case AST_SPAWN_EXPR:
            if (!hir_collect_direct_calls(node->data.spawn_expr.function, names, count))
                return false;
            for (size_t i = 0; i < node->data.spawn_expr.arg_count; i++) {
                if (!hir_collect_direct_calls(node->data.spawn_expr.arguments[i], names, count))
                    return false;
            }
            return true;

        case AST_TASK_GROUP:
            for (size_t i = 0; i < node->data.task_group.task_count; i++) {
                if (!hir_collect_direct_calls(node->data.task_group.tasks[i], names, count))
                    return false;
            }
            return true;

        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                if (!hir_collect_direct_calls(node->data.parallel.tasks[i], names, count))
                    return false;
            }
            return true;

        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
        case AST_EVENT_INVOKE:
        case AST_LAMBDA_EXPR:
        default:
            return true;
    }
}

static bool
hir_stmt_collect_local_defs(ASTNode *node, const char ***names, size_t *count)
{
    if (node == NULL)
        return true;

    switch (node->type) {
        case AST_LET_DECL:
            return append_name_unique(names, count, node->data.let_decl.name);

        case AST_LET_DESTRUCTURE:
            for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
                if (!append_name_unique(names, count, node->data.let_destructure.names[i]))
                    return false;
            }
            return true;

        case AST_ASSIGNMENT:
            if (node->data.assignment.target != NULL
                && node->data.assignment.target->type == AST_IDENTIFIER) {
                return append_name_unique(names,
                                          count,
                                          node->data.assignment.target->data.identifier.name);
            }
            return true;

        default:
            return true;
    }
}

static bool
hir_collect_cfg_local_defs(HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        HIRBasicBlock *block = &routine->cfg.blocks[i];
        free((void *)block->local_defs);
        block->local_defs = NULL;
        block->local_def_count = 0;

        for (size_t j = 0; j < block->statement_count; j++) {
            if (!hir_stmt_collect_local_defs(block->statements[j],
                                             &block->local_defs,
                                             &block->local_def_count)) {
                return false;
            }
        }
    }

    return true;
}

static bool
hir_routine_collect_ssa_names(const HIRRoutine *routine, const char ***names, size_t *count)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (!block->is_reachable)
            continue;
        for (size_t j = 0; j < block->local_def_count; j++) {
            if (!append_name_unique(names, count, block->local_defs[j]))
                return false;
        }
    }

    return true;
}

static bool
hir_block_defines_name(const HIRBasicBlock *block, const char *name)
{
    if (block == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < block->local_def_count; i++) {
        if (block->local_defs[i] != NULL && strcmp(block->local_defs[i], name) == 0)
            return true;
    }
    return false;
}

static bool
hir_compute_cfg_phi_candidates(HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    routine->phi_candidate_count = 0;
    routine->phi_candidate_block_count = 0;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        HIRBasicBlock *block = &routine->cfg.blocks[i];
        free((void *)block->phi_candidates);
        block->phi_candidates = NULL;
        block->phi_candidate_count = 0;
    }

    const char **names = NULL;
    size_t name_count = 0;
    if (!hir_routine_collect_ssa_names(routine, &names, &name_count))
        return false;

    bool *has_phi = calloc(routine->cfg.block_count, sizeof(bool));
    bool *in_work = calloc(routine->cfg.block_count, sizeof(bool));
    size_t *work = malloc(routine->cfg.block_count * sizeof(size_t));
    if ((name_count > 0) && (has_phi == NULL || in_work == NULL || work == NULL)) {
        free((void *)names);
        free(has_phi);
        free(in_work);
        free(work);
        return false;
    }

    for (size_t n = 0; n < name_count; n++) {
        const char *name = names[n];
        memset(has_phi, 0, routine->cfg.block_count * sizeof(bool));
        memset(in_work, 0, routine->cfg.block_count * sizeof(bool));
        size_t work_count = 0;

        for (size_t b = 0; b < routine->cfg.block_count; b++) {
            const HIRBasicBlock *block = &routine->cfg.blocks[b];
            if (block->is_reachable && hir_block_defines_name(block, name)) {
                work[work_count++] = b;
                in_work[b] = true;
            }
        }

        for (size_t wi = 0; wi < work_count; wi++) {
            size_t def_block = work[wi];
            const HIRBasicBlock *block = &routine->cfg.blocks[def_block];
            for (size_t df_i = 0; df_i < block->dominance_frontier_count; df_i++) {
                size_t frontier_block = block->dominance_frontier[df_i];
                if (frontier_block >= routine->cfg.block_count || has_phi[frontier_block])
                    continue;

                if (!append_name_unique(&routine->cfg.blocks[frontier_block].phi_candidates,
                                        &routine->cfg.blocks[frontier_block].phi_candidate_count,
                                        name)) {
                    free((void *)names);
                    free(has_phi);
                    free(in_work);
                    free(work);
                    return false;
                }
                has_phi[frontier_block] = true;

                if (!hir_block_defines_name(&routine->cfg.blocks[frontier_block], name)
                    && !in_work[frontier_block]) {
                    work[work_count++] = frontier_block;
                    in_work[frontier_block] = true;
                }
            }
        }
    }

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        if (routine->cfg.blocks[i].phi_candidate_count > 0) {
            routine->phi_candidate_block_count++;
            routine->phi_candidate_count += routine->cfg.blocks[i].phi_candidate_count;
        }
    }

    free((void *)names);
    free(has_phi);
    free(in_work);
    free(work);
    return true;
}

static bool
hir_materialize_phi_nodes(HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (block->phi_nodes != NULL) {
            for (size_t j = 0; j < block->phi_node_count; j++)
                free(block->phi_nodes[j].incoming_predecessors);
            free(block->phi_nodes);
        }
        block->phi_nodes = NULL;
        block->phi_node_count = 0;

        if (block->phi_candidate_count == 0)
            continue;

        block->phi_nodes = calloc(block->phi_candidate_count, sizeof(HIRPhiNode));
        if (block->phi_nodes == NULL)
            return false;
        block->phi_node_count = block->phi_candidate_count;

        for (size_t j = 0; j < block->phi_candidate_count; j++) {
            HIRPhiNode *phi = &block->phi_nodes[j];
            phi->name = block->phi_candidates[j];
            phi->incoming_predecessor_count = block->predecessor_count;
            if (block->predecessor_count > 0) {
                if (block->predecessor_count > SIZE_MAX / sizeof(size_t))
                    return false;
                phi->incoming_predecessors = malloc(block->predecessor_count * sizeof(size_t));
                if (phi->incoming_predecessors == NULL)
                    return false;
                memcpy(phi->incoming_predecessors,
                       block->predecessors,
                       block->predecessor_count * sizeof(size_t));
            }
        }
    }

    return true;
}

static void
hir_finalize_cfg_summary(HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return;

    routine->reachable_block_count = 0;
    routine->dead_block_count = 0;
    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        if (routine->cfg.blocks[i].is_reachable)
            routine->reachable_block_count++;
        else
            routine->dead_block_count++;
    }
}

static HIRPhase
hir_phase_for_kind(HIRTopLevelKind kind)
{
    switch (kind) {
        case HIR_TOPLEVEL_EXTERN:
            return HIR_PHASE_EXTERN;
        case HIR_TOPLEVEL_TYPE:
            return HIR_PHASE_TYPE;
        case HIR_TOPLEVEL_ABILITY:
        case HIR_TOPLEVEL_ROLE:
            return HIR_PHASE_CAPABILITY;
        case HIR_TOPLEVEL_PARTY:
        case HIR_TOPLEVEL_SYSTEMIC:
        case HIR_TOPLEVEL_WORLD:
        case HIR_TOPLEVEL_RELATION:
        case HIR_TOPLEVEL_EFFECT:
        case HIR_TOPLEVEL_ZONE:
        case HIR_TOPLEVEL_EVENT:
            return HIR_PHASE_DOMAIN;
        case HIR_TOPLEVEL_FUNCTION:
        case HIR_TOPLEVEL_INTENT:
            return HIR_PHASE_ROUTINE;
        case HIR_TOPLEVEL_EXECUTABLE:
            return HIR_PHASE_EXECUTABLE;
        default:
            return HIR_PHASE_EXECUTABLE;
    }
}

static ASTNode *
hir_routine_body(ASTNode *node)
{
    if (node == NULL)
        return NULL;
    if (node->type == AST_FUNC_DECL)
        return node->data.func_decl.body;
    return NULL;
}

static bool
hir_append_hidden_method_routine(HIRProgram *hir,
                                 size_t decl_id,
                                 const char *owner_name,
                                 ASTNodeType owner_ast_type,
                                 ASTNode *method)
{
    HIRRoutine routine;
    HIRRoutine *grown;

    if (hir == NULL || owner_name == NULL || method == NULL || method->type != AST_FUNC_DECL)
        return true;

    memset(&routine, 0, sizeof(routine));
    routine.decl_id = decl_id;
    routine.kind = HIR_TOPLEVEL_FUNCTION;
    routine.name = method->data.func_decl.name;
    routine.owner_name = owner_name;
    routine.owner_ast_type = owner_ast_type;
    routine.ast = method;
    routine.body = hir_routine_body(method);
    routine.is_hosted = true;
    routine.is_action_like = method->data.func_decl.is_action;
    routine.is_exported = method->is_exported;
    routine.has_control_flow = ast_contains_control_flow(routine.body);

    if (!hir_lower_func_body_cfg(method->data.func_decl.body, &routine))
        goto oom_free_calls;
    if (!hir_finalize_cfg(&routine))
        goto oom_free_calls;
    if (!hir_compute_cfg_dominance(&routine))
        goto oom_free_calls;
    if (!hir_compute_cfg_dominance_frontier(&routine))
        goto oom_free_calls;
    if (!hir_compute_cfg_dom_tree(&routine))
        goto oom_free_calls;
    if (!hir_compute_cfg_loops(&routine))
        goto oom_free_calls;
    if (!hir_collect_cfg_local_defs(&routine))
        goto oom_free_calls;
    if (!hir_compute_cfg_phi_candidates(&routine))
        goto oom_free_calls;
    if (!hir_materialize_phi_nodes(&routine))
        goto oom_free_calls;
    hir_finalize_cfg_summary(&routine);
    if (!hir_collect_func_signature_refs(method,
                                         &routine.signature_type_refs,
                                         &routine.signature_type_ref_count)) {
        free((void *)routine.signature_type_refs);
        return false;
    }
    if (!hir_collect_direct_calls(method->data.func_decl.body,
                                  &routine.direct_calls,
                                  &routine.direct_call_count))
        goto oom_free_calls;

    grown = realloc(hir->routines, (hir->routine_count + 1) * sizeof(HIRRoutine));
    if (grown == NULL)
        goto oom_free_calls;
    grown[hir->routine_count] = routine;
    hir->routines = grown;
    hir->routine_count++;
    return true;

oom_free_calls:
    free((void *)routine.signature_type_refs);
    free((void *)routine.direct_calls);
    return false;
}

static bool
hir_decl_method_slice(ASTNode *decl, ASTNode ***methods_out, size_t *method_count_out,
                      const char **owner_name_out)
{
    if (methods_out != NULL)
        *methods_out = NULL;
    if (method_count_out != NULL)
        *method_count_out = 0;
    if (owner_name_out != NULL)
        *owner_name_out = NULL;

    if (decl == NULL)
        return false;

    switch (decl->type) {
    case AST_CLASS_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.class_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.class_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.class_decl.name;
        return true;
    case AST_ENUM_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.enum_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.enum_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.enum_decl.name;
        return true;
    case AST_PARTY_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.party_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.party_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.party_decl.name;
        return true;
    case AST_ROSTER_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.roster_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.roster_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.roster_decl.name;
        return true;
    case AST_WORLD_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.world_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.world_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.world_decl.name;
        return true;
    case AST_RELATION_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.relation_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.relation_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.relation_decl.name;
        return true;
    case AST_EFFECT_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.effect_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.effect_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.effect_decl.name;
        return true;
    case AST_ZONE_DECL:
        if (methods_out != NULL)
            *methods_out = decl->data.zone_decl.methods;
        if (method_count_out != NULL)
            *method_count_out = decl->data.zone_decl.method_count;
        if (owner_name_out != NULL)
            *owner_name_out = decl->data.zone_decl.name;
        return true;
    default:
        return false;
    }
}

static bool
hir_append_role_impl_method_routines(HIRProgram *hir, size_t decl_id, ASTNode *role_decl)
{
    const char *owner_name;

    if (hir == NULL || role_decl == NULL || role_decl->type != AST_ROLE_DECL)
        return true;

    owner_name = role_decl->data.role_decl.name;
    if (owner_name == NULL)
        return true;

    for (size_t i = 0; i < role_decl->data.role_decl.impl_count; i++) {
        ASTNode *impl = role_decl->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
            ASTNode *method = impl->data.impl_ability.methods[j];
            if (!hir_append_hidden_method_routine(hir,
                                                  decl_id,
                                                  owner_name,
                                                  AST_ROLE_DECL,
                                                  method)) {
                return false;
            }
        }
    }

    return true;
}

static bool
hir_append_decl_and_routine(HIRProgram *hir, HIRTopLevelItem item, char **error_message)
{
    HIRDecl decl;
    memset(&decl, 0, sizeof(decl));
    decl.id = hir->decl_count;
    decl.kind = item.kind;
    decl.phase = hir_phase_for_kind(item.kind);
    decl.ast = item.ast;
    decl.name = item.name;
    if (!append_decl(&hir->decls, &hir->decl_count, decl))
        goto oom;

    if (item.kind == HIR_TOPLEVEL_FUNCTION
        || item.kind == HIR_TOPLEVEL_INTENT
        || (item.kind == HIR_TOPLEVEL_EXECUTABLE
            && item.ast != NULL
            && item.ast->type == AST_FUNC_DECL)) {
        HIRRoutine routine;
        memset(&routine, 0, sizeof(routine));
        routine.decl_id = decl.id;
        routine.kind = item.kind;
        routine.name = item.name;
        routine.ast = item.ast;
        routine.body = hir_routine_body(item.ast);
        routine.is_hosted = (item.ast != NULL
                             && item.ast->type == AST_FUNC_DECL
                             && item.ast->data.func_decl.name != NULL
                             && strchr(item.ast->data.func_decl.name, '.') != NULL);
        routine.is_action_like = (item.ast != NULL
                                  && item.ast->type == AST_FUNC_DECL
                                  && item.ast->data.func_decl.is_action);
        routine.is_exported = (item.ast != NULL && item.ast->is_exported);
        routine.has_control_flow = ast_contains_control_flow(routine.body);

        if (item.ast != NULL && item.ast->type == AST_FUNC_DECL) {
            if (!hir_lower_func_body_cfg(item.ast->data.func_decl.body, &routine))
                goto oom_free_calls;
            if (!hir_finalize_cfg(&routine))
                goto oom_free_calls;
            if (!hir_compute_cfg_dominance(&routine))
                goto oom_free_calls;
            if (!hir_compute_cfg_dominance_frontier(&routine))
                goto oom_free_calls;
            if (!hir_compute_cfg_dom_tree(&routine))
                goto oom_free_calls;
            if (!hir_compute_cfg_loops(&routine))
                goto oom_free_calls;
            if (!hir_collect_cfg_local_defs(&routine))
                goto oom_free_calls;
            if (!hir_compute_cfg_phi_candidates(&routine))
                goto oom_free_calls;
            if (!hir_materialize_phi_nodes(&routine))
                goto oom_free_calls;
            hir_finalize_cfg_summary(&routine);
            if (!hir_collect_func_signature_refs(item.ast,
                                                 &routine.signature_type_refs,
                                                 &routine.signature_type_ref_count)) {
                free((void *)routine.signature_type_refs);
                goto oom;
            }
            if (!hir_collect_direct_calls(item.ast->data.func_decl.body,
                                          &routine.direct_calls,
                                          &routine.direct_call_count)) {
                free((void *)routine.signature_type_refs);
                free((void *)routine.direct_calls);
                goto oom;
            }
        } else if (item.ast != NULL && item.ast->type == AST_INTENT_DECL) {
            ASTNode *intent = item.ast;
            if (!hir_collect_intent_signature_refs(intent,
                                                   &routine.signature_type_refs,
                                                   &routine.signature_type_ref_count)) {
                free((void *)routine.signature_type_refs);
                goto oom;
            }
            if (intent->data.intent_decl.priority_expr != NULL
                && !hir_collect_direct_calls(intent->data.intent_decl.priority_expr,
                                             &routine.direct_calls,
                                             &routine.direct_call_count)) {
                free((void *)routine.signature_type_refs);
                free((void *)routine.direct_calls);
                goto oom;
            }
            if (intent->data.intent_decl.success_expr != NULL
                && !hir_collect_direct_calls(intent->data.intent_decl.success_expr,
                                             &routine.direct_calls,
                                             &routine.direct_call_count)) {
                free((void *)routine.signature_type_refs);
                free((void *)routine.direct_calls);
                goto oom;
            }
            if (intent->data.intent_decl.failure_expr != NULL
                && !hir_collect_direct_calls(intent->data.intent_decl.failure_expr,
                                             &routine.direct_calls,
                                             &routine.direct_call_count)) {
                free((void *)routine.signature_type_refs);
                free((void *)routine.direct_calls);
                goto oom;
            }
            for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
                ASTNode *step = intent->data.intent_decl.steps[i];
                if (step == NULL || step->type != AST_INTENT_STEP)
                    continue;
                if (!hir_collect_direct_calls(step->data.intent_step.using_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                if (!hir_collect_direct_calls(step->data.intent_step.pre_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                if (!hir_collect_direct_calls(step->data.intent_step.guard_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                if (!hir_collect_direct_calls(step->data.intent_step.post_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                if (!hir_collect_direct_calls(step->data.intent_step.invariant_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                if (!hir_collect_direct_calls(step->data.intent_step.expect_expr,
                                              &routine.direct_calls,
                                              &routine.direct_call_count))
                    goto oom_free_calls;
                for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
                    if (!hir_collect_direct_calls(step->data.intent_step.on_exprs[j],
                                                  &routine.direct_calls,
                                                  &routine.direct_call_count))
                        goto oom_free_calls;
                }
                for (size_t j = 0; j < step->data.intent_step.compensate_expr_count; j++) {
                    if (!hir_collect_direct_calls(step->data.intent_step.compensate_exprs[j],
                                                  &routine.direct_calls,
                                                  &routine.direct_call_count))
                        goto oom_free_calls;
                }
            }
            routine.has_control_flow = intent->data.intent_decl.step_count > 1
                                       || routine.direct_call_count > 0;
        }

        HIRRoutine *grown = realloc(hir->routines,
                                    (hir->routine_count + 1) * sizeof(HIRRoutine));
        if (grown == NULL) {
oom_free_calls:
            free((void *)routine.signature_type_refs);
            free((void *)routine.direct_calls);
            goto oom;
        }
        grown[hir->routine_count] = routine;
        hir->routines = grown;
        hir->routine_count++;
    }

    if (item.ast != NULL) {
        ASTNode **methods = NULL;
        size_t method_count = 0;
        const char *owner_name = NULL;
        if (hir_decl_method_slice(item.ast, &methods, &method_count, &owner_name)) {
            for (size_t i = 0; i < method_count; i++) {
                if (!hir_append_hidden_method_routine(hir,
                                                      decl.id,
                                                      owner_name,
                                                      item.ast->type,
                                                      methods[i])) {
                    goto oom;
                }
            }
        }
        if (!hir_append_role_impl_method_routines(hir, decl.id, item.ast))
            goto oom;
    }

    return true;

oom:
    if (error_message != NULL)
        *error_message = pergyra_strdup("Out of memory");
    return false;
}

static const char *
hir_node_name(ASTNode *node)
{
    if (node == NULL)
        return "(null)";

    switch (node->type) {
        case AST_FUNC_DECL:
            return node->data.func_decl.name;
        case AST_CLASS_DECL:
            return node->data.class_decl.name;
        case AST_TYPE_ALIAS:
            return node->data.type_alias.name;
        case AST_EXTERN_BLOCK:
            return node->data.extern_block.abi;
        case AST_ABILITY_DECL:
            return node->data.ability_decl.name;
        case AST_ROLE_DECL:
            return node->data.role_decl.name;
        case AST_PARTY_DECL:
            return node->data.party_decl.name;
        case AST_ROSTER_DECL:
            return node->data.roster_decl.name;
        case AST_WORLD_DECL:
            return node->data.world_decl.name;
        case AST_INTENT_DECL:
            return node->data.intent_decl.name;
        case AST_RELATION_DECL:
            return node->data.relation_decl.name;
        case AST_EFFECT_DECL:
            return node->data.effect_decl.name;
        case AST_ZONE_DECL:
            return node->data.zone_decl.name;
        case AST_EVENT_DECL:
            return node->data.event_decl.name;
        case AST_LET_DECL:
            return node->data.let_decl.name;
        default:
            return NULL;
    }
}

static ssize_t
hir_find_routine_index_by_name(const HIRProgram *hir, const char *name)
{
    if (hir == NULL || name == NULL)
        return -1;

    for (size_t i = 0; i < hir->routine_count; i++) {
        if (hir->routines[i].name != NULL && strcmp(hir->routines[i].name, name) == 0)
            return (ssize_t)i;
    }

    return -1;
}

const char *
hir_top_level_kind_name(HIRTopLevelKind kind)
{
    switch (kind) {
        case HIR_TOPLEVEL_EXTERN: return "extern";
        case HIR_TOPLEVEL_TYPE: return "type";
        case HIR_TOPLEVEL_ABILITY: return "ability";
        case HIR_TOPLEVEL_ROLE: return "role";
        case HIR_TOPLEVEL_PARTY: return "party";
        case HIR_TOPLEVEL_SYSTEMIC: return "roster";
        case HIR_TOPLEVEL_WORLD: return "world";
        case HIR_TOPLEVEL_RELATION: return "relation";
        case HIR_TOPLEVEL_EFFECT: return "effect";
        case HIR_TOPLEVEL_ZONE: return "zone";
        case HIR_TOPLEVEL_EVENT: return "event";
        case HIR_TOPLEVEL_INTENT: return "intent";
        case HIR_TOPLEVEL_FUNCTION: return "function";
        case HIR_TOPLEVEL_EXECUTABLE: return "executable";
        default: return "unknown";
    }
}

const char *
hir_phase_name(HIRPhase phase)
{
    switch (phase) {
        case HIR_PHASE_EXTERN: return "extern";
        case HIR_PHASE_TYPE: return "type";
        case HIR_PHASE_CAPABILITY: return "capability";
        case HIR_PHASE_DOMAIN: return "domain";
        case HIR_PHASE_ROUTINE: return "routine";
        case HIR_PHASE_EXECUTABLE: return "executable";
        default: return "unknown";
    }
}

static bool
hir_classify_top_level(HIRProgram *hir, ASTNode *node, char **error_message)
{
    HIRTopLevelItem item;
    memset(&item, 0, sizeof(item));
    item.ast = node;
    item.name = hir_node_name(node);

    switch (node->type) {
        case AST_EXTERN_BLOCK:
            item.kind = HIR_TOPLEVEL_EXTERN;
            if (!append_ast(&hir->externs, &hir->extern_count, node))
                goto oom;
            break;
        case AST_CLASS_DECL:
        case AST_TYPE_ALIAS:
        case AST_ENUM_DECL:
            item.kind = HIR_TOPLEVEL_TYPE;
            if (!append_ast(&hir->types, &hir->type_count, node))
                goto oom;
            break;
        case AST_ABILITY_DECL:
            item.kind = HIR_TOPLEVEL_ABILITY;
            if (!append_ast(&hir->abilities, &hir->ability_count, node))
                goto oom;
            break;
        case AST_ROLE_DECL:
            item.kind = HIR_TOPLEVEL_ROLE;
            if (!append_ast(&hir->roles, &hir->role_count, node))
                goto oom;
            break;
        case AST_PARTY_DECL:
            item.kind = HIR_TOPLEVEL_PARTY;
            if (!append_ast(&hir->parties, &hir->party_count, node))
                goto oom;
            break;
        case AST_ROSTER_DECL:
            item.kind = HIR_TOPLEVEL_SYSTEMIC;
            if (!append_ast(&hir->rosters, &hir->roster_count, node))
                goto oom;
            break;
        case AST_WORLD_DECL:
            item.kind = HIR_TOPLEVEL_WORLD;
            if (!append_ast(&hir->worlds, &hir->world_count, node))
                goto oom;
            break;
        case AST_INTENT_DECL:
            item.kind = HIR_TOPLEVEL_INTENT;
            if (!append_ast(&hir->intents, &hir->intent_count, node))
                goto oom;
            break;
        case AST_RELATION_DECL:
            item.kind = HIR_TOPLEVEL_RELATION;
            if (!append_ast(&hir->relations, &hir->relation_count, node))
                goto oom;
            break;
        case AST_EFFECT_DECL:
            item.kind = HIR_TOPLEVEL_EFFECT;
            if (!append_ast(&hir->effects, &hir->effect_count, node))
                goto oom;
            break;
        case AST_ZONE_DECL:
            item.kind = HIR_TOPLEVEL_ZONE;
            if (!append_ast(&hir->zones, &hir->zone_count, node))
                goto oom;
            break;
        case AST_EVENT_DECL:
            item.kind = HIR_TOPLEVEL_EVENT;
            if (!append_ast(&hir->events, &hir->event_count, node))
                goto oom;
            break;
        case AST_FUNC_DECL:
            item.kind = HIR_TOPLEVEL_FUNCTION;
            if (!append_ast(&hir->functions, &hir->function_count, node))
                goto oom;
            if (node->data.func_decl.name != NULL
                && strcmp(node->data.func_decl.name, "Main") == 0) {
                hir->has_main_function = true;
            }
            break;

        case AST_LET_DECL:
        case AST_WITH_STMT:
        case AST_PARALLEL_BLOCK:
        case AST_FOR_LOOP:
        case AST_WHILE_LOOP:
        case AST_IF_STMT:
        case AST_RETURN:
        case AST_BREAK:
        case AST_CONTINUE:
        case AST_SELECT_STMT:
        case AST_MATCH_STMT:
        case AST_BINARY:
        case AST_UNARY:
        case AST_CALL:
        case AST_MEMBER_ACCESS:
        case AST_ARRAY_ACCESS:
        case AST_ASSIGNMENT:
        case AST_AWAIT_EXPR:
        case AST_CHANNEL_SEND:
        case AST_CHANNEL_RECV:
        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:
        case AST_IDENTIFIER:
        case AST_ASYNC_BLOCK:
        case AST_SPAWN_EXPR:
        case AST_TASK_GROUP:
        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
        case AST_EVENT_INVOKE:
        case AST_LAMBDA_EXPR:
        case AST_BLOCK:
            item.kind = HIR_TOPLEVEL_EXECUTABLE;
            if (!append_ast(&hir->executables, &hir->executable_count, node))
                goto oom;
            break;

        case AST_IMPORT_DECL:
        case AST_USE_DECL:
            /* Already resolved by driver — skip */
            break;
        case AST_UNSAFE_BLOCK:
        case AST_DEFER_STMT:
        case AST_BIND_STMT:
            item.kind = HIR_TOPLEVEL_EXECUTABLE;
            if (!append_ast(&hir->executables, &hir->executable_count, node))
                goto oom;
            break;

        default:
            if (error_message != NULL) {
                char message[128];
                snprintf(message, sizeof(message),
                         "Unsupported top-level AST node for HIR lowering: %d",
                         (int)node->type);
                *error_message = pergyra_strdup(message);
            }
            return false;
    }

    if (!append_item(&hir->items, &hir->item_count, item))
        goto oom;
    if (!hir_append_decl_and_routine(hir, item, error_message))
        goto oom;

    return true;

oom:
    if (error_message != NULL)
        *error_message = pergyra_strdup("Out of memory");
    return false;
}

static bool
hir_append_synthetic_executable_routine(HIRProgram *hir, char **error_message)
{
    ASTNode *func;
    ASTNode *body;
    HIRTopLevelItem item;

    if (hir == NULL || hir->executable_count == 0 || hir->synthetic_executable_func != NULL)
        return true;

    func = ast_create_function("__pgy_top_level_exec");
    body = ast_create_block();
    if (func == NULL || body == NULL) {
        ast_destroy(func);
        ast_destroy(body);
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return false;
    }

    for (size_t i = 0; i < hir->executable_count; i++)
        ast_add_statement(body, hir->executables[i]);
    func->data.func_decl.body = body;
    hir->synthetic_executable_func = func;

    memset(&item, 0, sizeof(item));
    item.kind = HIR_TOPLEVEL_EXECUTABLE;
    item.ast = func;
    item.name = func->data.func_decl.name;

    if (!append_item(&hir->items, &hir->item_count, item)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return false;
    }
    if (!hir_append_decl_and_routine(hir, item, error_message))
        return false;

    return true;
}

static void
hir_destroy_synthetic_executable_func(ASTNode *func)
{
    ASTNode *body;

    if (func == NULL || func->type != AST_FUNC_DECL)
        return;

    body = func->data.func_decl.body;
    free(func->data.func_decl.name);
    if (body != NULL && body->type == AST_BLOCK) {
        free(body->data.block.statements);
        free(body);
    }
    free(func);
}

HIRProgram *
hir_lower(ASTNode *annotated_ast, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (annotated_ast == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Cannot lower null AST");
        return NULL;
    }
    if (annotated_ast->type != AST_PROGRAM) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("HIR lowering requires AST_PROGRAM root");
        return NULL;
    }

    HIRProgram *hir = calloc(1, sizeof(HIRProgram));
    if (hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return NULL;
    }

    for (size_t i = 0; i < annotated_ast->data.program.count; i++) {
        if (!hir_classify_top_level(hir,
                                    annotated_ast->data.program.statements[i],
                                    error_message)) {
            hir_destroy(hir);
            return NULL;
        }
    }

    if (!hir_append_synthetic_executable_routine(hir, error_message)) {
        hir_destroy(hir);
        return NULL;
    }

    for (size_t i = 0; i < hir->routine_count; i++) {
        HIRRoutine *routine = &hir->routines[i];
        for (size_t j = 0; j < routine->direct_call_count; j++) {
            ssize_t callee = hir_find_routine_index_by_name(hir, routine->direct_calls[j]);
            if (callee >= 0
                && !append_index_unique(&routine->callee_routine_ids,
                                        &routine->callee_routine_count,
                                        (size_t)callee)) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup("Out of memory");
                hir_destroy(hir);
                return NULL;
            }
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < hir->routine_count; i++) {
            HIRRoutine *routine = &hir->routines[i];
            bool is_root = (routine->kind == HIR_TOPLEVEL_INTENT)
                           || (routine->kind == HIR_TOPLEVEL_EXECUTABLE)
                           || routine->is_exported
                           || (routine->name != NULL
                               && strcmp(routine->name, "Main") == 0);
            if (!routine->is_entry_reachable && is_root) {
                routine->is_entry_reachable = true;
                changed = true;
            }
            if (!routine->is_entry_reachable)
                continue;
            for (size_t j = 0; j < routine->callee_routine_count; j++) {
                size_t callee = routine->callee_routine_ids[j];
                if (callee < hir->routine_count && !hir->routines[callee].is_entry_reachable) {
                    hir->routines[callee].is_entry_reachable = true;
                    changed = true;
                }
            }
        }
    }

    return hir;
}

void
hir_destroy(HIRProgram *hir)
{
    if (hir == NULL)
        return;

    free(hir->items);
    free(hir->decls);
    if (hir->routines != NULL) {
        for (size_t i = 0; i < hir->routine_count; i++) {
            if (hir->routines[i].cfg.blocks != NULL) {
                for (size_t j = 0; j < hir->routines[i].cfg.block_count; j++) {
                    free(hir->routines[i].cfg.blocks[j].statements);
                    free(hir->routines[i].cfg.blocks[j].predecessors);
                    free(hir->routines[i].cfg.blocks[j].dom_tree_children);
                    free((void *)hir->routines[i].cfg.blocks[j].local_defs);
                    free(hir->routines[i].cfg.blocks[j].dominance_frontier);
                    free((void *)hir->routines[i].cfg.blocks[j].phi_candidates);
                    if (hir->routines[i].cfg.blocks[j].phi_nodes != NULL) {
                        for (size_t k = 0; k < hir->routines[i].cfg.blocks[j].phi_node_count; k++)
                            free(hir->routines[i].cfg.blocks[j].phi_nodes[k].incoming_predecessors);
                    }
                    free(hir->routines[i].cfg.blocks[j].phi_nodes);
                }
            }
            free(hir->routines[i].cfg.blocks);
            free((void *)hir->routines[i].signature_type_refs);
            free((void *)hir->routines[i].direct_calls);
            free(hir->routines[i].callee_routine_ids);
        }
    }
    free(hir->routines);
    free(hir->externs);
    free(hir->types);
    free(hir->abilities);
    free(hir->roles);
    free(hir->parties);
    free(hir->rosters);
    free(hir->worlds);
    free(hir->relations);
    free(hir->effects);
    free(hir->zones);
    free(hir->subjects);
    free(hir->events);
    free(hir->intents);
    free(hir->functions);
    free(hir->executables);
    hir_destroy_synthetic_executable_func(hir->synthetic_executable_func);
    free(hir);
}

void
hir_dump(const HIRProgram *hir, FILE *out)
{
    if (out == NULL)
        out = stdout;

    if (hir == NULL) {
        fprintf(out, "HIR: (null)\n");
        return;
    }

    fprintf(out,
            "HIR Program\n"
            "  items: %zu\n"
            "  decls: %zu\n"
            "  routines: %zu\n"
            "  externs: %zu\n"
            "  types: %zu\n"
            "  abilities: %zu\n"
            "  roles: %zu\n"
            "  parties: %zu\n"
            "  rosters: %zu\n"
            "  worlds: %zu\n"
            "  subjects: %zu\n"
            "  events: %zu\n"
            "  functions: %zu\n"
            "  executables: %zu\n"
            "  has_main: %s\n",
            hir->item_count,
            hir->decl_count,
            hir->routine_count,
            hir->extern_count,
            hir->type_count,
            hir->ability_count,
            hir->role_count,
            hir->party_count,
            hir->roster_count,
            hir->world_count,
            hir->subject_count,
            hir->event_count,
            hir->function_count,
            hir->executable_count,
            hir->has_main_function ? "true" : "false");

    for (size_t i = 0; i < hir->item_count; i++) {
        const HIRTopLevelItem *item = &hir->items[i];
        fprintf(out, "  [%02zu] %-10s", i, hir_top_level_kind_name(item->kind));
        if (item->name != NULL)
            fprintf(out, " %s", item->name);
        fprintf(out, "\n");
    }

    if (hir->routine_count > 0) {
        fprintf(out, "  routines:\n");
        for (size_t i = 0; i < hir->routine_count; i++) {
            const HIRRoutine *routine = &hir->routines[i];
            fprintf(out,
                    "    [%02zu] %-8s %-18s phase=%s calls=%zu callees=%zu hosted=%s action=%s exported=%s reachable=%s cf=%s\n",
                    i,
                    hir_top_level_kind_name(routine->kind),
                    routine->name != NULL ? routine->name : "(anonymous)",
                    hir_phase_name(HIR_PHASE_ROUTINE),
                    routine->direct_call_count,
                    routine->callee_routine_count,
                    routine->is_hosted ? "true" : "false",
                    routine->is_action_like ? "true" : "false",
                    routine->is_exported ? "true" : "false",
                    routine->is_entry_reachable ? "true" : "false",
                    routine->has_control_flow ? "true" : "false");
            if (routine->signature_type_ref_count > 0) {
                fprintf(out, "         types=");
                for (size_t j = 0; j < routine->signature_type_ref_count; j++) {
                    if (j > 0)
                        fprintf(out, ",");
                    fprintf(out, "%s", routine->signature_type_refs[j]);
                }
                fprintf(out, "\n");
            }
            if (routine->has_cfg) {
                fprintf(out,
                        "         cfg=blocks:%zu entry:%zu\n",
                        routine->cfg.block_count,
                        routine->cfg.entry_block);
                for (size_t j = 0; j < routine->cfg.block_count; j++) {
                    const HIRBasicBlock *block = &routine->cfg.blocks[j];
                    fprintf(out,
                            "           block[%02zu] preds=%zu df=%zu succ=%s%s%s loop=%s depth=%zu reach=%s rpo=%zu idom=%s%zu stmts=%zu\n",
                            j,
                            block->predecessor_count,
                            block->dominance_frontier_count,
                            block->has_succ_true ? "T" : "",
                            block->has_succ_false ? "F" : "",
                            (!block->has_succ_true && !block->has_succ_false) ? "-" : "",
                            block->is_loop_header ? "true" : "false",
                            block->loop_depth,
                            block->is_reachable ? "true" : "false",
                            block->rpo_index,
                            block->has_immediate_dominator ? "" : "-",
                            block->has_immediate_dominator ? block->immediate_dominator : 0,
                            block->statement_count);
                }
            }
        }
    }
}

void
hir_dump_mode(const HIRProgram *hir, FILE *out, HIRDumpMode mode)
{
    if (out == NULL)
        out = stdout;
    if (hir == NULL) {
        fprintf(out, "HIR: (null)\n");
        return;
    }

    if (mode == HIR_DUMP_SUMMARY) {
        hir_dump(hir, out);
        return;
    }

    fprintf(out,
            "HIR %s view\n"
            "  decls: %zu\n"
            "  routines: %zu\n",
            mode == HIR_DUMP_CFG ? "cfg"
            : mode == HIR_DUMP_DOM ? "dom"
            : "ssa",
            hir->decl_count,
            hir->routine_count);

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        fprintf(out,
                "  [%02zu] %s %s reachable=%s calls=%zu blocks=%zu live=%zu dead=%zu phi=%zu blocks-with-phi=%zu\n",
                i,
                hir_top_level_kind_name(routine->kind),
                routine->name != NULL ? routine->name : "(anonymous)",
                routine->is_entry_reachable ? "true" : "false",
                routine->direct_call_count,
                routine->has_cfg ? routine->cfg.block_count : 0,
                routine->reachable_block_count,
                routine->dead_block_count,
                routine->phi_candidate_count,
                routine->phi_candidate_block_count);

        if (!routine->has_cfg)
            continue;

        for (size_t j = 0; j < routine->cfg.block_count; j++) {
            const HIRBasicBlock *block = &routine->cfg.blocks[j];
            fprintf(out,
                    "    block[%02zu] reach=%s preds=%zu succ=%s%s%s",
                    j,
                    block->is_reachable ? "true" : "false",
                    block->predecessor_count,
                    block->has_succ_true ? "T" : "",
                    block->has_succ_false ? "F" : "",
                    (!block->has_succ_true && !block->has_succ_false) ? "-" : "");
            if (mode == HIR_DUMP_DOM || mode == HIR_DUMP_SSA) {
                fprintf(out,
                        " rpo=%zu idom=%s%zu df=%zu loop=%s depth=%zu",
                        block->rpo_index,
                        block->has_immediate_dominator ? "" : "-",
                        block->has_immediate_dominator ? block->immediate_dominator : 0,
                        block->dominance_frontier_count,
                        block->is_loop_header ? "true" : "false",
                        block->loop_depth);
            }
            if (mode == HIR_DUMP_SSA) {
                fprintf(out,
                        " defs=%zu phi=%zu dom-children=%zu",
                        block->local_def_count,
                        block->phi_node_count,
                        block->dom_tree_child_count);
            }
            fprintf(out, "\n");

            if (mode == HIR_DUMP_SSA) {
                if (block->local_def_count > 0) {
                    fprintf(out, "      defs=");
                    for (size_t k = 0; k < block->local_def_count; k++) {
                        if (k > 0)
                            fprintf(out, ",");
                        fprintf(out, "%s", block->local_defs[k]);
                    }
                    fprintf(out, "\n");
                }
                if (block->phi_node_count > 0) {
                    fprintf(out, "      phi =");
                    for (size_t k = 0; k < block->phi_node_count; k++) {
                        if (k > 0)
                            fprintf(out, ",");
                        fprintf(out, "%s", block->phi_nodes[k].name);
                    }
                    fprintf(out, "\n");
                }
            }
        }
    }
}

const HIRDecl *
hir_find_decl(const HIRProgram *hir, const char *name, HIRTopLevelKind kind)
{
    if (hir == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < hir->decl_count; i++) {
        const HIRDecl *decl = &hir->decls[i];
        if (decl->kind == kind && decl->name != NULL && strcmp(decl->name, name) == 0)
            return decl;
    }
    return NULL;
}

const HIRRoutine *
hir_find_routine(const HIRProgram *hir, const char *name, HIRTopLevelKind kind)
{
    if (hir == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        if (routine->kind == kind
            && routine->name != NULL
            && strcmp(routine->name, name) == 0) {
            return routine;
        }
    }
    return NULL;
}

bool
hir_run_routine_pass(HIRProgram *hir, HIRRoutinePass *pass, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (hir == NULL || pass == NULL || pass->run == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("HIR routine pass requires program and callback");
        return false;
    }

    pass->routines_visited = 0;
    pass->routines_matched = 0;

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        bool kind_ok = ((routine->kind == HIR_TOPLEVEL_FUNCTION && pass->filter.include_functions)
                        || (routine->kind == HIR_TOPLEVEL_INTENT && pass->filter.include_intents));
        if (!kind_ok)
            continue;

        pass->routines_visited++;

        if (pass->filter.require_control_flow && !routine->has_control_flow)
            continue;
        if (pass->filter.require_action_like && !routine->is_action_like)
            continue;
        if (pass->filter.require_cfg && !routine->has_cfg)
            continue;
        if (pass->filter.require_entry_reachable && !routine->is_entry_reachable)
            continue;

        pass->routines_matched++;
        if (!pass->run(hir, routine, pass->userdata, error_message))
            return false;
    }

    return true;
}

bool
hir_run_block_pass(HIRProgram *hir, HIRBlockPass *pass, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (hir == NULL || pass == NULL || pass->run == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("HIR block pass requires program and callback");
        return false;
    }

    pass->routines_visited = 0;
    pass->blocks_visited = 0;
    pass->blocks_matched = 0;

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        bool kind_ok = ((routine->kind == HIR_TOPLEVEL_FUNCTION && pass->filter.include_functions)
                        || (routine->kind == HIR_TOPLEVEL_INTENT && pass->filter.include_intents));
        if (!kind_ok)
            continue;

        pass->routines_visited++;

        if (pass->filter.require_cfg && !routine->has_cfg)
            continue;
        if (pass->filter.require_entry_reachable && !routine->is_entry_reachable)
            continue;
        if (!routine->has_cfg)
            continue;

        for (size_t j = 0; j < routine->cfg.block_count; j++) {
            const HIRBasicBlock *block = &routine->cfg.blocks[j];
            pass->blocks_visited++;
            if (block->is_reachable && !pass->filter.include_reachable_blocks)
                continue;
            if (!block->is_reachable && !pass->filter.include_dead_blocks)
                continue;
            pass->blocks_matched++;
            if (!pass->run(hir, routine, block, pass->userdata, error_message))
                return false;
        }
    }

    return true;
}
