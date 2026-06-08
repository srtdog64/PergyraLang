#include "hir_cfg_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
hir_stmt_collect_local_defs(ASTNode *node,
                            const char ***names,
                            size_t *count,
                            size_t *capacity)
{
    if (node == NULL)
        return true;

    switch (node->type) {
        case AST_LET_DECL:
            return hir_cfg_append_name_unique(names, count, capacity, ast_let_name(node));

        case AST_LET_DESTRUCTURE:
            for (size_t i = 0; i < ast_let_destructure_name_count(node); i++) {
                if (!hir_cfg_append_name_unique(names,
                                                count,
                                                capacity,
                                                ast_let_destructure_name(node, i)))
                    return false;
            }
            return true;

        case AST_ASSIGNMENT:
            if (ast_assignment_target(node) != NULL
                && ast_assignment_target(node)->type == AST_IDENTIFIER) {
                return hir_cfg_append_name_unique(names,
                                                  count,
                                                  capacity,
                                                  ast_identifier_name(ast_assignment_target(node)));
            }
            return true;

        case AST_WITH_STMT:
            if (ast_with_alias(node) != NULL
                && !hir_cfg_append_name_unique(names,
                                               count,
                                               capacity,
                                               ast_with_alias(node))) {
                return false;
            }
            if (ast_with_body(node) != NULL) {
                ASTNode *body = ast_with_body(node);
                if (body->type == AST_BLOCK) {
                    size_t stmt_count = 0;
                    ASTNode **stmts = ast_block_statements(body, &stmt_count);
                    for (size_t i = 0; i < stmt_count; i++) {
                        if (!hir_stmt_collect_local_defs(stmts[i], names, count, capacity))
                            return false;
                    }
                } else {
                    if (!hir_stmt_collect_local_defs(body, names, count, capacity))
                        return false;
                }
            }
            return true;

        case AST_UNSAFE_BLOCK:
            if (ast_unsafe_block_body(node) != NULL) {
                ASTNode *body = ast_unsafe_block_body(node);
                if (body->type == AST_BLOCK) {
                    size_t stmt_count = 0;
                    ASTNode **stmts = ast_block_statements(body, &stmt_count);
                    for (size_t i = 0; i < stmt_count; i++) {
                        if (!hir_stmt_collect_local_defs(stmts[i], names, count, capacity))
                            return false;
                    }
                } else {
                    if (!hir_stmt_collect_local_defs(body, names, count, capacity))
                        return false;
                }
            }
            return true;

        default:
            return true;
    }
}

static bool
hir_stmt_collect_phi_seed_names(ASTNode *node,
                                const char ***names,
                                size_t *count,
                                size_t *capacity)
{
    if (node == NULL)
        return true;

    switch (node->type) {
        case AST_ASSIGNMENT:
            if (ast_assignment_target(node) != NULL
                && ast_assignment_target(node)->type == AST_IDENTIFIER) {
                return hir_cfg_append_name_unique(names,
                                                  count,
                                                  capacity,
                                                  ast_identifier_name(ast_assignment_target(node)));
            }
            return true;

        case AST_WITH_STMT:
            if (ast_with_body(node) != NULL) {
                ASTNode *body = ast_with_body(node);
                if (body->type == AST_BLOCK) {
                    size_t stmt_count = 0;
                    ASTNode **stmts = ast_block_statements(body, &stmt_count);
                    for (size_t i = 0; i < stmt_count; i++) {
                        if (!hir_stmt_collect_phi_seed_names(stmts[i], names, count, capacity))
                            return false;
                    }
                } else {
                    if (!hir_stmt_collect_phi_seed_names(body, names, count, capacity))
                        return false;
                }
            }
            return true;

        case AST_UNSAFE_BLOCK:
            if (ast_unsafe_block_body(node) != NULL) {
                ASTNode *body = ast_unsafe_block_body(node);
                if (body->type == AST_BLOCK) {
                    size_t stmt_count = 0;
                    ASTNode **stmts = ast_block_statements(body, &stmt_count);
                    for (size_t i = 0; i < stmt_count; i++) {
                        if (!hir_stmt_collect_phi_seed_names(stmts[i], names, count, capacity))
                            return false;
                    }
                } else {
                    if (!hir_stmt_collect_phi_seed_names(body, names, count, capacity))
                        return false;
                }
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
        block->local_def_capacity = 0;

        for (size_t j = 0; j < block->statement_count; j++) {
            if (!hir_stmt_collect_local_defs(block->statements[j],
                                             &block->local_defs,
                                             &block->local_def_count,
                                             &block->local_def_capacity)) {
                return false;
            }
        }
    }

    return true;
}

static bool
hir_routine_collect_ssa_names(const HIRRoutine *routine,
                              const char ***names,
                              size_t *count,
                              size_t *capacity)
{
    if (routine == NULL || !routine->has_cfg || routine->cfg.blocks == NULL)
        return true;

    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (!block->is_reachable)
            continue;
        for (size_t j = 0; j < block->statement_count; j++) {
            if (!hir_stmt_collect_phi_seed_names(block->statements[j],
                                                 names,
                                                 count,
                                                 capacity))
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
        block->phi_candidate_capacity = 0;
    }

    const char **names = NULL;
    size_t name_count = 0;
    size_t name_capacity = 0;
    if (!hir_routine_collect_ssa_names(routine, &names, &name_count, &name_capacity))
        return false;
    if (name_count == 0) {
        free((void *)names);
        return true;
    }

    if (routine->cfg.block_count > SIZE_MAX / sizeof(bool)
        || routine->cfg.block_count > SIZE_MAX / sizeof(size_t)) {
        free((void *)names);
        return false;
    }
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
                                                &routine->cfg.blocks[frontier_block].phi_candidate_capacity,
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
            if (routine->phi_candidate_count
                    > SIZE_MAX - routine->cfg.blocks[i].phi_candidate_count) {
                free((void *)names);
                free(has_phi);
                free(in_work);
                free(work);
                return false;
            }
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

        if (block->phi_candidate_count > SIZE_MAX / sizeof(HIRPhiNode))
            return false;
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
