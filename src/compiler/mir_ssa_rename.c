#include "mir_ssa_rename.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/arena.h"
#include "../parser/ast_api.h"
#include "mir_base_helpers.h"
#include "mir_ssa_rename_internal.h"

static bool
mir_checked_array_size(size_t count, size_t elem_size, size_t *bytes_out)
{
    if (bytes_out == NULL || elem_size == 0)
        return false;
    if (count > SIZE_MAX / elem_size)
        return false;
    *bytes_out = count * elem_size;
    return true;
}

bool
mir_append_ssa_binding(MIRLocalBinding **rows, size_t *count,
                       size_t *capacity, MIRLocalBinding binding)
{
    if (binding.name == NULL)
        return true;
    for (size_t i = 0; i < *count; i++) {
        if ((*rows)[i].binding_syntax_id == binding.binding_syntax_id
            && (binding.binding_syntax_id != 0
                || strcmp((*rows)[i].name, binding.name) == 0))
            return strcmp((*rows)[i].name, binding.name) == 0;
    }
    if (*count == *capacity) {
        size_t cap = *capacity == 0 ? 8 : *capacity * 2;
        if (cap < *capacity || cap > SIZE_MAX / sizeof(MIRLocalBinding))
            return false;
        MIRLocalBinding *next = realloc(*rows, cap * sizeof(*next));
        if (next == NULL)
            return false;
        *rows = next;
        *capacity = cap;
    }
    (*rows)[(*count)++] = binding;
    return true;
}

bool
mir_collect_ssa_names(const MIRRoutine *routine,
                      MIRLocalBinding **names_out,
                      size_t *count_out)
{
    MIRLocalBinding *names = NULL;
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
            if (block->source_local_defs[j].binding_syntax_id == 0
                || !mir_append_ssa_binding(&names, &count, &capacity,
                    block->source_local_defs[j])) {
                free((void *)names);
                return false;
            }
        }
        for (size_t j = 0; j < block->source_phi_node_count; j++) {
            if (block->source_phi_nodes[j].binding_syntax_id == 0
                || !mir_append_ssa_binding(&names, &count, &capacity,
                    (MIRLocalBinding){block->source_phi_nodes[j].name,
                        block->source_phi_nodes[j].binding_syntax_id})) {
                free((void *)names);
                return false;
            }
        }
    }

    /* A shadowed formal still owns version zero. Keep its declaration in the
     * identity index even if only the inner local has a DEF in this routine. */
    for (size_t i = 0; i < mir_routine_param_count(routine); i++) {
        FuncParam *param = mir_routine_param(routine, i);
        if (param == NULL || param->name == NULL)
            continue;
        bool reached = mir_routine_param_carriage(routine, i)
            == MIR_PARAM_CARRIAGE_VALUE_RESULT;
        for (size_t j = 0; j < count; j++)
            reached |= strcmp(names[j].name, param->name) == 0;
        if (reached && !mir_append_ssa_binding(&names, &count, &capacity,
                (MIRLocalBinding){param->name, ast_func_param_stable_id(param)})) {
            free(names);
            return false;
        }
    }
    *names_out = names;
    *count_out = count;
    return true;
}

int
mir_find_ssa_binding_index(const MIRLocalBinding *names, size_t count,
                           uint32_t binding_syntax_id)
{
    if (names == NULL || binding_syntax_id == 0)
        return -1;
    for (size_t i = 0; i < count; i++) {
        if (names[i].binding_syntax_id == binding_syntax_id)
            return (int)i;
    }
    return -1;
}

bool
mir_block_binding_exit_ssa_name(const MIRRoutine *routine,
                                const MIRBasicBlock *block,
                                uint32_t binding_syntax_id,
                                char *out, size_t out_size)
{
    if (routine == NULL || block == NULL || out == NULL || out_size == 0
        || routine->ssa_bindings == NULL || block->ssa_exit_versions == NULL
        || block->ssa_version_count != routine->ssa_binding_count)
        return false;
    int index = mir_find_ssa_binding_index(routine->ssa_bindings,
        routine->ssa_binding_count, binding_syntax_id);
    if (index < 0)
        return false;
    int written = snprintf(out, out_size, "%s.%zu",
        routine->ssa_bindings[index].name, block->ssa_exit_versions[index]);
    return written > 0 && (size_t)written < out_size;
}

bool
mir_collect_expr_identifier_uses(ASTNode *node,
                                 MIRLocalBinding **uses,
                                 size_t *use_count,
                                 size_t *use_capacity)
{
    if (node == NULL)
        return true;
    switch (node->type) {
    case AST_IDENTIFIER:
        return mir_append_ssa_binding(uses, use_count, use_capacity,
            (MIRLocalBinding){ast_identifier_name(node),
                             ast_identifier_binding_syntax_id(node)});
    case AST_BINARY:
        return mir_collect_expr_identifier_uses(ast_binary_left(node),
                                                uses,
                                                use_count,
                                                use_capacity)
            && mir_collect_expr_identifier_uses(ast_binary_right(node),
                                                uses,
                                                use_count,
                                                use_capacity);
    case AST_UNARY:
        return mir_collect_expr_identifier_uses(ast_unary_operand(node),
                                                uses,
                                                use_count,
                                                use_capacity);
    case AST_CALL:
        if (!mir_collect_expr_identifier_uses(ast_call_callee(node),
                                              uses,
                                              use_count,
                                              use_capacity))
            return false;
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            if (!mir_collect_expr_identifier_uses(ast_call_argument(node, i),
                                                  uses,
                                                  use_count,
                                                  use_capacity))
                return false;
        }
        return true;
    case AST_MEMBER_ACCESS:
        return mir_collect_expr_identifier_uses(ast_member_object(node),
                                                uses,
                                                use_count,
                                                use_capacity);
    case AST_ARRAY_ACCESS:
        return mir_collect_expr_identifier_uses(ast_array_access_array(node),
                                                uses,
                                                use_count,
                                                use_capacity)
            && mir_collect_expr_identifier_uses(ast_array_access_index(node),
                                                uses,
                                                use_count,
                                                use_capacity);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(node); i++) {
            if (!mir_collect_expr_identifier_uses(
                    ast_array_literal_element(node, i),
                    uses,
                    use_count,
                    use_capacity))
                return false;
        }
        return true;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
            if (!mir_collect_expr_identifier_uses(
                    ast_tuple_literal_element(node, i),
                    uses,
                    use_count,
                    use_capacity))
                return false;
        }
        return true;
    case AST_MAP_LITERAL:
        for (size_t i = 0; i < ast_map_literal_count(node); i++) {
            if (!mir_collect_expr_identifier_uses(
                    ast_map_literal_key(node, i),
                    uses,
                    use_count,
                    use_capacity)
                || !mir_collect_expr_identifier_uses(
                    ast_map_literal_value(node, i),
                    uses,
                    use_count,
                    use_capacity)) {
                return false;
            }
        }
        return true;
    case AST_CAST:
        return mir_collect_expr_identifier_uses(ast_cast_operand(node),
                                                uses,
                                                use_count,
                                                use_capacity);
    case AST_TYPE_TEST:
        return mir_collect_expr_identifier_uses(ast_type_test_operand(node),
                                                uses,
                                                use_count,
                                                use_capacity);
    case AST_ASSIGNMENT:
        return mir_collect_expr_identifier_uses(ast_assignment_target(node),
                                                uses,
                                                use_count,
                                                use_capacity)
            && mir_collect_expr_identifier_uses(ast_assignment_value(node),
                                                uses,
                                                use_count,
                                                use_capacity);
    default:
        return true;
    }
}

/* The printed SSA ordinal is unique per spelling. Current values, unlike this
 * output numbering, are indexed exclusively by semantic declaration identity. */
static size_t
mir_next_ssa_version(const MIRLocalBinding *names, size_t count,
                     size_t *versions, size_t index)
{
    size_t version = versions[index] + 1;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(names[i].name, names[index].name) == 0)
            versions[i] = version;
    }
    return version;
}

static bool
mir_assign_ssa_recursive(MIRRoutine *routine,
                         size_t block_id,
                         const MIRLocalBinding *ssa_names,
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
        name_index = mir_find_ssa_binding_index(ssa_names, ssa_name_count,
                                                inst->binding_syntax_id);
        if (name_index < 0)
            continue;
        current_versions[name_index] = mir_next_ssa_version(
            ssa_names, ssa_name_count, next_versions, (size_t)name_index);
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
        MIRLocalBinding binding = mir_block->source_local_defs[i];
        const char *name = binding.name;
        name_index = mir_find_ssa_binding_index(ssa_names, ssa_name_count,
                                                binding.binding_syntax_id);
        if (name_index < 0)
            continue;
        current_versions[name_index] = mir_next_ssa_version(
            ssa_names, ssa_name_count, next_versions, (size_t)name_index);
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
                                     versioned,
                                     binding.binding_syntax_id)) {
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
                           const MIRLocalBinding *ssa_names,
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
            name_index = mir_find_ssa_binding_index(ssa_names, ssa_name_count,
                                                    phi->binding_syntax_id);
            if (name_index < 0 || phi->incoming_predecessor_count == 0)
                continue;
            if (phi->incoming_predecessor_count
                    > SIZE_MAX / sizeof(MIRPhiIncoming))
                return false;
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

bool
mir_apply_ssa_rename(MIRRoutine *routine)
{
    MIRLocalBinding *ssa_names = NULL;
    size_t ssa_name_count = 0;
    size_t *next_versions = NULL;
    size_t *root_versions = NULL;
    size_t **out_versions = NULL;
    size_t ssa_version_bytes = 0;
    size_t out_version_bytes = 0;
    bool ok = false;

    if (routine == NULL || routine->hir_routine == NULL || !routine->hir_routine->has_cfg)
        return true;

    if (!mir_collect_ssa_names(routine, &ssa_names, &ssa_name_count))
        goto cleanup;
    if (ssa_name_count > 0) {
        if (ssa_name_count > SIZE_MAX / sizeof(MIRLocalBinding))
            goto cleanup;
        routine->ssa_bindings = pgy_arena_calloc(&routine->scratch,
            ssa_name_count * sizeof(MIRLocalBinding));
        if (routine->ssa_bindings == NULL)
            goto cleanup;
        memcpy(routine->ssa_bindings, ssa_names,
            ssa_name_count * sizeof(MIRLocalBinding));
    }
    routine->ssa_binding_count = ssa_name_count;
    if (ssa_name_count == 0) {
        ok = true;
        goto cleanup;
    }
    if (!mir_checked_array_size(ssa_name_count, sizeof(size_t),
            &ssa_version_bytes)
        || !mir_checked_array_size(routine->block_count, sizeof(size_t *),
            &out_version_bytes)) {
        goto cleanup;
    }
    next_versions = pgy_arena_calloc(&routine->scratch, ssa_version_bytes);
    root_versions = pgy_arena_calloc(&routine->scratch, ssa_version_bytes);
    out_versions = pgy_arena_calloc(&routine->scratch, out_version_bytes);
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
