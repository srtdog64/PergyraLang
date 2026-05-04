#ifndef PERGYRA_MIR_SSA_RENAME_H
#define PERGYRA_MIR_SSA_RENAME_H

static bool
mir_collect_ssa_names(const MIRRoutine *routine, const char ***names_out, size_t *count_out)
{
    const char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (names_out == NULL || count_out == NULL)
        return false;
    *names_out = NULL;
    *count_out = 0;
    if (routine == NULL)
        return true;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->source_local_def_count; j++) {
            if (!append_name_unique(&names, &count, &capacity, block->source_local_defs[j])) {
                free((void *)names);
                return false;
            }
        }
        for (size_t j = 0; j < block->source_phi_node_count; j++) {
            if (!append_name_unique(&names, &count, &capacity, block->source_phi_nodes[j].name)) {
                free((void *)names);
                return false;
            }
        }
    }

    *names_out = names;
    *count_out = count;
    return true;
}

static int
mir_find_ssa_name_index(const char **names, size_t count, const char *name)
{
    if (names == NULL || name == NULL)
        return -1;
    for (size_t i = 0; i < count; i++) {
        if (names[i] != NULL && strcmp(names[i], name) == 0)
            return (int)i;
    }
    return -1;
}

static bool
mir_collect_expr_identifier_uses(ASTNode *node,
                                 const char ***uses,
                                 size_t *use_count,
                                 size_t *use_capacity)
{
    if (node == NULL)
        return true;
    switch (node->type) {
        case AST_IDENTIFIER:
            return append_name_unique(uses, use_count, use_capacity, node->data.identifier.name);
        case AST_BINARY:
            return mir_collect_expr_identifier_uses(node->data.binary.left,
                                                    uses,
                                                    use_count,
                                                    use_capacity)
                   && mir_collect_expr_identifier_uses(node->data.binary.right,
                                                       uses,
                                                       use_count,
                                                       use_capacity);
        case AST_UNARY:
            return mir_collect_expr_identifier_uses(node->data.unary.operand,
                                                    uses,
                                                    use_count,
                                                    use_capacity);
        case AST_CALL:
            if (!mir_collect_expr_identifier_uses(node->data.call.callee,
                                                  uses,
                                                  use_count,
                                                  use_capacity))
                return false;
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (!mir_collect_expr_identifier_uses(node->data.call.arguments[i],
                                                      uses,
                                                      use_count,
                                                      use_capacity))
                    return false;
            }
            return true;
        case AST_MEMBER_ACCESS:
            return mir_collect_expr_identifier_uses(node->data.member.object,
                                                    uses,
                                                    use_count,
                                                    use_capacity);
        case AST_ARRAY_ACCESS:
            return mir_collect_expr_identifier_uses(node->data.array_access.array,
                                                    uses,
                                                    use_count,
                                                    use_capacity)
                   && mir_collect_expr_identifier_uses(node->data.array_access.index,
                                                       uses,
                                                       use_count,
                                                       use_capacity);
        case AST_ASSIGNMENT:
            return mir_collect_expr_identifier_uses(node->data.assignment.target,
                                                    uses,
                                                    use_count,
                                                    use_capacity)
                   && mir_collect_expr_identifier_uses(node->data.assignment.value,
                                                       uses,
                                                       use_count,
                                                       use_capacity);
        default:
            return true;
    }
}

static bool
mir_assign_ssa_recursive(MIRRoutine *routine,
                         size_t block_id,
                         const char **ssa_names,
                         size_t ssa_name_count,
                         size_t *next_versions,
                         const size_t *incoming_versions,
                         size_t **out_versions)
{
    MIRBasicBlock *mir_block;
    size_t *current_versions = NULL;

    if (routine == NULL)
        return false;
    if (block_id >= routine->block_count)
        return false;

    mir_block = &routine->blocks[block_id];

    if (!copy_versions(&current_versions, incoming_versions, ssa_name_count))
        return false;
    if (!mir_store_block_versions(mir_block, true, current_versions, ssa_name_count)) {
        free(current_versions);
        return false;
    }

    for (size_t i = 0; i < mir_block->instruction_count; i++) {
        MIRInstruction *inst = &mir_block->instructions[i];
        int name_index;
        char *versioned;
        if (inst->kind != MIR_INST_PHI || inst->name == NULL)
            continue;
        name_index = mir_find_ssa_name_index(ssa_names, ssa_name_count, inst->name);
        if (name_index < 0)
            continue;
        next_versions[name_index]++;
        current_versions[name_index] = next_versions[name_index];
        versioned = mir_make_versioned_name(inst->name, current_versions[name_index]);
        if (versioned == NULL) {
            free(current_versions);
            return false;
        }
        inst->result_name = versioned;
        routine->phi_inserted_count++;
    }

    for (size_t i = 0; i < mir_block->source_local_def_count; i++) {
        int name_index;
        char *versioned;
        const char *name = mir_block->source_local_defs[i];
        name_index = mir_find_ssa_name_index(ssa_names, ssa_name_count, name);
        if (name_index < 0)
            continue;
        next_versions[name_index]++;
        current_versions[name_index] = next_versions[name_index];
        versioned = mir_make_versioned_name(name, current_versions[name_index]);
        if (versioned == NULL) {
            free(current_versions);
            return false;
        }
        if (!append_name(&mir_block->renamed_locals,
                         &mir_block->renamed_local_count,
                         &mir_block->renamed_local_capacity,
                         versioned)) {
            free(versioned);
            free(current_versions);
            return false;
        }
        if (!mir_add_def_instruction(routine,
                                     mir_block,
                                     mir_block->source_phi_node_count + i,
                                     name,
                                     versioned)) {
            free(current_versions);
            return false;
        }
        routine->renamed_value_count++;
    }

    if (!mir_store_block_versions(mir_block, false, current_versions, ssa_name_count)) {
        free(current_versions);
        return false;
    }
    out_versions[block_id] = current_versions;
    for (size_t i = 0; i < mir_block->source_dom_tree_child_count; i++) {
        size_t child = mir_block->source_dom_tree_children[i];
        if (!mir_assign_ssa_recursive(routine,
                                      child,
                                      ssa_names,
                                      ssa_name_count,
                                      next_versions,
                                      current_versions,
                                      out_versions)) {
            return false;
        }
    }
    return true;
}

static bool
mir_materialize_phi_inputs(MIRRoutine *routine,
                           const char **ssa_names,
                           size_t ssa_name_count)
{
    if (routine == NULL || routine->hir_routine == NULL)
        return false;
    if (!routine->hir_routine->has_cfg)
        return true;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *mir_block = &routine->blocks[block_id];
        for (size_t i = 0; i < mir_block->source_phi_node_count && i < mir_block->instruction_count; i++) {
            MIRInstruction *inst = &mir_block->instructions[i];
            const MIRSourcePhiNode *phi = &mir_block->source_phi_nodes[i];
            int name_index;
            if (inst->kind != MIR_INST_PHI)
                continue;
            name_index = mir_find_ssa_name_index(ssa_names, ssa_name_count, phi->name);
            if (name_index < 0 || phi->incoming_predecessor_count == 0)
                continue;
            inst->phi_incomings = calloc(phi->incoming_predecessor_count, sizeof(MIRPhiIncoming));
            if (inst->phi_incomings == NULL)
                return false;
            inst->phi_incoming_count = phi->incoming_predecessor_count;
            for (size_t j = 0; j < phi->incoming_predecessor_count; j++) {
                size_t pred = phi->incoming_predecessors[j];
                size_t version = 0;
                if (pred < routine->block_count
                    && routine->blocks[pred].ssa_exit_versions != NULL
                    && name_index < (int)routine->blocks[pred].ssa_version_count) {
                    version = routine->blocks[pred].ssa_exit_versions[name_index];
                }
                inst->phi_incomings[j].predecessor_block = pred;
                inst->phi_incomings[j].value_name = mir_make_versioned_name(phi->name, version);
                if (inst->phi_incomings[j].value_name == NULL)
                    return false;
            }
        }
    }

    return true;
}

static bool
mir_apply_ssa_rename(MIRRoutine *routine)
{
    const char **ssa_names = NULL;
    size_t ssa_name_count = 0;
    size_t *next_versions = NULL;
    size_t *root_versions = NULL;
    size_t **out_versions = NULL;
    bool ok = false;

    if (routine == NULL || routine->hir_routine == NULL || !routine->hir_routine->has_cfg)
        return true;

    /* Outer SSA-rename tables are pass-local scratch and live in this
     * MIRRoutine's own scratch arena.  Per-block inner out_versions[i]
     * arrays are still heap-owned by mir_assign_ssa_recursive and freed
     * below.  ssa_names is owned by mir_collect_ssa_names and is likewise
     * freed at the end.  NOTE: we deliberately do NOT allocate into
     * routine->hir_routine->scratch — HIR is frozen by the time MIR runs. */
    if (!mir_collect_ssa_names(routine, &ssa_names, &ssa_name_count))
        goto cleanup;
    if (ssa_name_count == 0) {
        ok = true;
        goto cleanup;
    }
    next_versions = pgy_arena_calloc(&routine->scratch, ssa_name_count * sizeof(size_t));
    root_versions = pgy_arena_calloc(&routine->scratch, ssa_name_count * sizeof(size_t));
    out_versions  = pgy_arena_calloc(&routine->scratch, routine->block_count * sizeof(size_t *));
    if (next_versions == NULL || root_versions == NULL || out_versions == NULL)
        goto cleanup;

    if (!mir_assign_ssa_recursive(routine,
                                  routine->entry_block,
                                  ssa_names,
                                  ssa_name_count,
                                  next_versions,
                                  root_versions,
                                  out_versions)) {
        goto cleanup;
    }
    if (!mir_materialize_phi_inputs(routine, ssa_names, ssa_name_count))
        goto cleanup;
    ok = true;

cleanup:
    if (out_versions != NULL) {
        for (size_t i = 0; i < routine->block_count; i++)
            free(out_versions[i]);
    }
    /* next_versions / root_versions / out_versions outer array are
     * routine->scratch-owned; destroyed in mir_destroy(). */
    free((void *)ssa_names);
    return ok;
}

#include "mir_ssa_use_edges.h"

#endif
