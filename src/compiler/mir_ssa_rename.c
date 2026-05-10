#include "mir_ssa_rename.h"

#include <stdlib.h>
#include <string.h>

#include "../common/arena.h"
#include "../common/string_compat.h"
#include "mir_base_helpers.h"

static bool
mir_collect_ssa_names(const MIRRoutine *routine,
                      const char ***names_out,
                      size_t *count_out)
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
            if (!append_name_unique(&names, &count, &capacity,
                    block->source_local_defs[j])) {
                free((void *)names);
                return false;
            }
        }
        for (size_t j = 0; j < block->source_phi_node_count; j++) {
            if (!append_name_unique(&names, &count, &capacity,
                    block->source_phi_nodes[j].name)) {
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
        return append_name_unique(uses, use_count, use_capacity,
                                  node->data.identifier.name);
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
        for (size_t i = 0;
             i < mir_block->source_phi_node_count && i < mir_block->instruction_count;
             i++) {
            MIRInstruction *inst = &mir_block->instructions[i];
            const MIRSourcePhiNode *phi = &mir_block->source_phi_nodes[i];
            int name_index;
            if (inst->kind != MIR_INST_PHI)
                continue;
            name_index = mir_find_ssa_name_index(ssa_names, ssa_name_count, phi->name);
            if (name_index < 0 || phi->incoming_predecessor_count == 0)
                continue;
            inst->phi_incomings = calloc(phi->incoming_predecessor_count,
                                         sizeof(MIRPhiIncoming));
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
                inst->phi_incomings[j].value_name =
                    mir_make_versioned_name(phi->name, version);
                if (inst->phi_incomings[j].value_name == NULL)
                    return false;
            }
        }
    }

    return true;
}

static bool
mir_append_versioned_use(MIRInstruction *inst, const char *base, size_t version)
{
    char *versioned;
    if (inst == NULL || base == NULL)
        return true;
    versioned = mir_make_versioned_name(base, version);
    if (versioned == NULL)
        return false;
    return append_owned_name(&inst->uses, &inst->use_count, &inst->use_capacity,
                             versioned);
}

static bool
mir_append_block_versioned_name(MIRBasicBlock *block,
                                bool is_entry,
                                const char *base,
                                size_t version)
{
    char *versioned;
    const char ***names;
    size_t *count;
    size_t *capacity;
    if (block == NULL || base == NULL)
        return true;
    versioned = mir_make_versioned_name(base, version);
    if (versioned == NULL)
        return false;
    names = is_entry ? &block->ssa_entry_values : &block->ssa_exit_values;
    count = is_entry ? &block->ssa_entry_value_count : &block->ssa_exit_value_count;
    capacity = is_entry ? &block->ssa_entry_value_capacity : &block->ssa_exit_value_capacity;
    return append_owned_name(names, count, capacity, versioned);
}

static char *
mir_parse_versioned_name_owned(const char *versioned, size_t *version_out)
{
    const char *dot;
    size_t len;

    if (versioned == NULL || version_out == NULL)
        return NULL;
    dot = strrchr(versioned, '.');
    if (dot == NULL)
        return NULL;
    len = (size_t)(dot - versioned);
    *version_out = (size_t)strtoull(dot + 1, NULL, 10);
    return pergyra_strndup(versioned, len);
}

static ASTNode *
mir_def_instruction_source_expr(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_DEF)
        return NULL;
    return inst->expr0;
}

bool
mir_populate_use_edges(MIRRoutine *routine)
{
    const char **ssa_names = NULL;
    size_t ssa_name_count = 0;

    if (routine == NULL || routine->hir_routine == NULL)
        return false;
    if (!routine->hir_routine->has_cfg)
        return true;
    if (!mir_collect_ssa_names(routine, &ssa_names, &ssa_name_count))
        return false;
    if (ssa_name_count == 0) {
        free((void *)ssa_names);
        return true;
    }

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        size_t *current_versions;
        if (block->ssa_entry_versions == NULL
            || block->ssa_version_count != ssa_name_count)
            continue;
        for (size_t n = 0; n < ssa_name_count; n++) {
            if (block->ssa_entry_versions[n] == 0)
                continue;
            if (!mir_append_block_versioned_name(block, true, ssa_names[n],
                    block->ssa_entry_versions[n])) {
                free((void *)ssa_names);
                return false;
            }
        }
        current_versions = calloc(ssa_name_count, sizeof(size_t));
        if (current_versions == NULL) {
            free((void *)ssa_names);
            return false;
        }
        memcpy(current_versions, block->ssa_entry_versions,
               ssa_name_count * sizeof(size_t));

        for (size_t i = 0; i < block->instruction_count; i++) {
            MIRInstruction *inst = &block->instructions[i];
            if (inst->kind == MIR_INST_PHI) {
                for (size_t j = 0; j < inst->phi_incoming_count; j++) {
                    if (!append_owned_name(&inst->uses,
                                           &inst->use_count,
                                           &inst->use_capacity,
                                           pergyra_strdup(inst->phi_incomings[j].value_name))) {
                        free(current_versions);
                        free((void *)ssa_names);
                        return false;
                    }
                    routine->use_edge_count++;
                }
                if (inst->result_name != NULL) {
                    size_t version = 0;
                    int idx;
                    char *base = mir_parse_versioned_name_owned(inst->result_name,
                                                                &version);
                    if (base != NULL) {
                        idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, base);
                        if (idx >= 0)
                            current_versions[idx] = version;
                        free(base);
                    }
                }
                continue;
            }
            if (inst->kind == MIR_INST_DEF) {
                ASTNode *expr = mir_def_instruction_source_expr(inst);
                if (expr != NULL) {
                    const char **raw_uses = NULL;
                    size_t raw_use_count = 0;
                    size_t raw_use_capacity = 0;
                    if (!mir_collect_expr_identifier_uses(expr, &raw_uses,
                            &raw_use_count, &raw_use_capacity)) {
                        free((void *)raw_uses);
                        free(current_versions);
                        free((void *)ssa_names);
                        return false;
                    }
                    for (size_t j = 0; j < raw_use_count; j++) {
                        int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count,
                                                          raw_uses[j]);
                        if (idx >= 0) {
                            if (!mir_append_versioned_use(inst, raw_uses[j],
                                    current_versions[idx])) {
                                free((void *)raw_uses);
                                free(current_versions);
                                free((void *)ssa_names);
                                return false;
                            }
                            routine->use_edge_count++;
                        }
                    }
                    free((void *)raw_uses);
                }
                if (inst->result_name != NULL) {
                    size_t version = 0;
                    int idx;
                    char *base = mir_parse_versioned_name_owned(inst->result_name,
                                                                &version);
                    if (base != NULL) {
                        idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, base);
                        if (idx >= 0)
                            current_versions[idx] = version;
                        free(base);
                    }
                }
                continue;
            }
            if (inst->kind == MIR_INST_BRANCH || inst->kind == MIR_INST_RETURN
                || inst->kind == MIR_INST_STMT
                || inst->kind == MIR_INST_RESOURCE_OP
                || inst->kind == MIR_INST_CLEANUP_EDGE) {
                const char **raw_uses = NULL;
                size_t raw_use_count = 0;
                size_t raw_use_capacity = 0;
                ASTNode *expr = inst->expr0 != NULL ? inst->expr0 : inst->expr1;
                if (expr == NULL)
                    expr = inst->ast;
                if (expr != NULL
                    && !mir_collect_expr_identifier_uses(expr, &raw_uses,
                        &raw_use_count, &raw_use_capacity)) {
                    free((void *)raw_uses);
                    free(current_versions);
                    free((void *)ssa_names);
                    return false;
                }
                if (raw_use_count == 0
                    && (inst->kind == MIR_INST_RESOURCE_OP
                        || inst->kind == MIR_INST_CLEANUP_EDGE)) {
                    const char *candidates[2] = {inst->arg0, inst->arg1};
                    for (size_t j = 0; j < 2; j++) {
                        int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count,
                                                          candidates[j]);
                        if (idx >= 0) {
                            if (!mir_append_versioned_use(inst, candidates[j],
                                    current_versions[idx])) {
                                free((void *)raw_uses);
                                free(current_versions);
                                free((void *)ssa_names);
                                return false;
                            }
                            routine->use_edge_count++;
                        }
                    }
                } else {
                    for (size_t j = 0; j < raw_use_count; j++) {
                        int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count,
                                                          raw_uses[j]);
                        if (idx >= 0) {
                            if (!mir_append_versioned_use(inst, raw_uses[j],
                                    current_versions[idx])) {
                                free((void *)raw_uses);
                                free(current_versions);
                                free((void *)ssa_names);
                                return false;
                            }
                            routine->use_edge_count++;
                        }
                    }
                }
                free((void *)raw_uses);
            }
        }

        for (size_t n = 0; n < ssa_name_count; n++) {
            if (current_versions[n] == 0)
                continue;
            if (!mir_append_block_versioned_name(block, false, ssa_names[n],
                    current_versions[n])) {
                free(current_versions);
                free((void *)ssa_names);
                return false;
            }
        }
        free(current_versions);
    }
    free((void *)ssa_names);
    return true;
}

bool
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

    if (!mir_collect_ssa_names(routine, &ssa_names, &ssa_name_count))
        goto cleanup;
    if (ssa_name_count == 0) {
        ok = true;
        goto cleanup;
    }
    next_versions = pgy_arena_calloc(&routine->scratch,
                                     ssa_name_count * sizeof(size_t));
    root_versions = pgy_arena_calloc(&routine->scratch,
                                     ssa_name_count * sizeof(size_t));
    out_versions = pgy_arena_calloc(&routine->scratch,
                                    routine->block_count * sizeof(size_t *));
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
    free((void *)ssa_names);
    return ok;
}
