#include "hir_cfg.h"

#include <stdlib.h>
#include <string.h>

static bool
hir_cfg_append_index_unique(size_t **items, size_t *count, size_t value)
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
hir_cfg_append_name_unique(const char ***names, size_t *count, const char *name)
{
    if (names == NULL || count == NULL || name == NULL || *name == '\0')
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

bool
hir_finalize_cfg(HIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (block->has_succ_true
            && !hir_cfg_append_index_unique(&routine->cfg.blocks[block->succ_true].predecessors,
                                            &routine->cfg.blocks[block->succ_true].predecessor_count,
                                            i)) {
            return false;
        }
        if (block->has_succ_false
            && !hir_cfg_append_index_unique(&routine->cfg.blocks[block->succ_false].predecessors,
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

bool
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

    bool *visited = pgy_arena_calloc(&routine->scratch,
        routine->cfg.block_count * sizeof(bool));
    size_t *postorder = pgy_arena_calloc(&routine->scratch,
        routine->cfg.block_count * sizeof(size_t));
    size_t *idoms = pgy_arena_alloc(&routine->scratch,
        routine->cfg.block_count * sizeof(size_t));
    if (visited == NULL || postorder == NULL || idoms == NULL)
        return false;

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

    return true;
}

bool
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
                if (!hir_cfg_append_index_unique(&routine->cfg.blocks[runner].dominance_frontier,
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

bool
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
        if (!hir_cfg_append_index_unique(&routine->cfg.blocks[block->immediate_dominator].dom_tree_children,
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

    bool *in_loop = pgy_arena_calloc(&routine->scratch,
        routine->cfg.block_count * sizeof(bool));
    size_t *stack = pgy_arena_alloc(&routine->scratch,
        routine->cfg.block_count * sizeof(size_t));
    if (in_loop == NULL || stack == NULL)
        return false;

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

    return true;
}

bool
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
hir_stmt_collect_local_defs(ASTNode *node, const char ***names, size_t *count)
{
    if (node == NULL)
        return true;

    switch (node->type) {
        case AST_LET_DECL:
            return hir_cfg_append_name_unique(names, count, node->data.let_decl.name);

        case AST_LET_DESTRUCTURE:
            for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
                if (!hir_cfg_append_name_unique(names, count, node->data.let_destructure.names[i]))
                    return false;
            }
            return true;

        case AST_ASSIGNMENT:
            if (node->data.assignment.target != NULL
                && node->data.assignment.target->type == AST_IDENTIFIER) {
                return hir_cfg_append_name_unique(names,
                                                  count,
                                                  node->data.assignment.target->data.identifier.name);
            }
            return true;

        default:
            return true;
    }
}

bool
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
            if (!hir_cfg_append_name_unique(names, count, block->local_defs[j]))
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

bool
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

                if (!hir_cfg_append_name_unique(&routine->cfg.blocks[frontier_block].phi_candidates,
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

bool
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

void
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
