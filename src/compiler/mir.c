#include "mir.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

#include "mir_base.inc"

static void mir_clear_block_name_set(const char ***names, size_t *count);
static int mir_find_value_summary(const MIRRoutine *routine, const char *name);
static bool mir_compute_liveness(MIRRoutine *routine);

static bool
mir_add_phi_placeholders(MIRRoutine *routine, MIRBasicBlock *block, const HIRBasicBlock *hir_block)
{
    if (routine == NULL || block == NULL || hir_block == NULL)
        return false;

    for (size_t i = 0; i < hir_block->phi_node_count; i++) {
        MIRInstruction inst;
        memset(&inst, 0, sizeof(inst));
        inst.id = routine->instruction_count++;
        inst.kind = MIR_INST_PHI;
        inst.name = hir_block->phi_nodes[i].name;
        inst.slot_anchor = hir_block->phi_nodes[i].name;
        inst.arg0 = "phi";
        if (!append_instruction(block, inst))
            return false;
    }
    return true;
}

static bool
mir_add_def_instruction(MIRRoutine *routine,
                        MIRBasicBlock *block,
                        size_t insert_index,
                        const char *base_name,
                        const char *result_name)
{
    MIRInstruction inst;
    if (routine == NULL || block == NULL || result_name == NULL)
        return false;
    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_DEF;
    inst.name = "ssa-def";
    inst.slot_anchor = base_name;
    inst.arg0 = base_name;
    inst.result_name = pergyra_strdup(result_name);
    if (inst.result_name == NULL)
        return false;
    return insert_instruction(block, insert_index, inst);
}

static bool
mir_add_terminator_instruction(MIRRoutine *routine, MIRBasicBlock *block, const HIRBasicBlock *hir_block)
{
    MIRInstruction inst;
    if (routine == NULL || block == NULL || hir_block == NULL)
        return false;
    if (hir_block->terminator_kind != HIR_BLOCK_BRANCH
        && hir_block->terminator_kind != HIR_BLOCK_RETURN)
        return true;
    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = (hir_block->terminator_kind == HIR_BLOCK_BRANCH)
                    ? MIR_INST_BRANCH
                    : MIR_INST_RETURN;
    inst.name = (hir_block->terminator_kind == HIR_BLOCK_BRANCH) ? "branch" : "return";
    inst.ast = (hir_block->terminator_kind == HIR_BLOCK_BRANCH)
                   ? hir_block->terminator_condition
                   : hir_block->terminator_value;
    return append_instruction(block, inst);
}

static bool
mir_add_cleanup_instruction(MIRRoutine *routine, MIRBasicBlock *block, const RIROp *op)
{
    MIRInstruction inst;
    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = rir_op_kind_name(op->kind);
    inst.slot_anchor = op->slot_anchor;
    inst.arg0 = op->subject;
    inst.arg1 = op->arg0;
    inst.rir_op = op;
    inst.ast = op->ast;
    routine->cleanup_instruction_count++;
    return append_instruction(block, inst);
}

static bool
mir_add_rollback_invalidation(MIRRoutine *routine, MIRBasicBlock *cleanup, const RIRScope *rir_scope)
{
    if (routine == NULL || cleanup == NULL || rir_scope == NULL)
        return false;
    for (size_t i = 0; i < rir_scope->fact_count; i++) {
        const RIRFact *fact = &rir_scope->facts[i];
        MIRInstruction inst;
        if (fact->kind != RIR_FACT_INTENT_POLICY || fact->name == NULL || strcmp(fact->name, "rollback") != 0)
            continue;
        memset(&inst, 0, sizeof(inst));
        inst.id = routine->instruction_count++;
        inst.kind = MIR_INST_CLEANUP_EDGE;
        inst.name = "RollbackPolicy";
        inst.slot_anchor = fact->slot_anchor;
        inst.arg0 = fact->arg0;
        if (!append_instruction(cleanup, inst))
            return false;
        routine->cleanup_instruction_count++;
    }
    return true;
}

static bool
mir_add_resource_instruction(MIRRoutine *routine, MIRBasicBlock *block, const RIROp *op)
{
    MIRInstruction inst;
    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_RESOURCE_OP;
    inst.name = rir_op_kind_name(op->kind);
    inst.slot_anchor = op->slot_anchor;
    inst.arg0 = op->subject;
    inst.arg1 = op->arg0;
    inst.rir_op = op;
    inst.ast = op->ast;
    return append_instruction(block, inst);
}

static bool
mir_intent_ast_needs_invalidation(const HIRRoutine *hir_routine)
{
    ASTNode *intent;
    if (hir_routine == NULL || hir_routine->ast == NULL || hir_routine->ast->type != AST_INTENT_DECL)
        return false;
    intent = hir_routine->ast;
    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (step->data.intent_step.using_expr != NULL
            || step->data.intent_step.transfer_from_alias != NULL
            || step->data.intent_step.transfer_to_alias != NULL) {
            return true;
        }
    }
    return false;
}

static bool
mir_append_intent_invalidation_markers(MIRRoutine *routine, MIRBasicBlock *block)
{
    ASTNode *intent;
    if (routine == NULL || block == NULL || routine->hir_routine == NULL)
        return false;
    if (routine->hir_routine->ast == NULL || routine->hir_routine->ast->type != AST_INTENT_DECL)
        return true;
    intent = routine->hir_routine->ast;
    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        MIRInstruction inst;
        const char *target = NULL;
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (step->data.intent_step.using_expr != NULL)
            target = mir_node_name(step->data.intent_step.using_expr);
        else if (step->data.intent_step.transfer_to_alias != NULL)
            target = step->data.intent_step.transfer_to_alias;
        else if (step->data.intent_step.transfer_from_alias != NULL)
            target = step->data.intent_step.transfer_from_alias;
        if (target == NULL)
            continue;
        memset(&inst, 0, sizeof(inst));
        inst.id = routine->instruction_count++;
        inst.kind = MIR_INST_CLEANUP_EDGE;
        inst.name = "DetachInvalidation";
        inst.slot_anchor = target;
        inst.arg0 = target;
        inst.arg1 = step->data.intent_step.name;
        inst.ast = step;
        if (!append_instruction(block, inst))
            return false;
        routine->cleanup_instruction_count++;
    }
    return true;
}

static bool
mir_collect_ssa_names(const HIRRoutine *hir_routine, const char ***names_out, size_t *count_out)
{
    const char **names = NULL;
    size_t count = 0;

    if (names_out == NULL || count_out == NULL)
        return false;
    *names_out = NULL;
    *count_out = 0;
    if (hir_routine == NULL || !hir_routine->has_cfg)
        return true;

    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &hir_routine->cfg.blocks[i];
        for (size_t j = 0; j < block->local_def_count; j++) {
            if (!append_name_unique(&names, &count, block->local_defs[j])) {
                free((void *)names);
                return false;
            }
        }
        for (size_t j = 0; j < block->phi_node_count; j++) {
            if (!append_name_unique(&names, &count, block->phi_nodes[j].name)) {
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
mir_collect_expr_identifier_uses(ASTNode *node, const char ***uses, size_t *use_count)
{
    if (node == NULL)
        return true;
    switch (node->type) {
        case AST_IDENTIFIER:
            return append_name_unique(uses, use_count, node->data.identifier.name);
        case AST_BINARY:
            return mir_collect_expr_identifier_uses(node->data.binary.left, uses, use_count)
                   && mir_collect_expr_identifier_uses(node->data.binary.right, uses, use_count);
        case AST_UNARY:
            return mir_collect_expr_identifier_uses(node->data.unary.operand, uses, use_count);
        case AST_CALL:
            if (!mir_collect_expr_identifier_uses(node->data.call.callee, uses, use_count))
                return false;
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                if (!mir_collect_expr_identifier_uses(node->data.call.arguments[i], uses, use_count))
                    return false;
            }
            return true;
        case AST_MEMBER_ACCESS:
            return mir_collect_expr_identifier_uses(node->data.member.object, uses, use_count);
        case AST_ARRAY_ACCESS:
            return mir_collect_expr_identifier_uses(node->data.array_access.array, uses, use_count)
                   && mir_collect_expr_identifier_uses(node->data.array_access.index, uses, use_count);
        case AST_ASSIGNMENT:
            return mir_collect_expr_identifier_uses(node->data.assignment.target, uses, use_count)
                   && mir_collect_expr_identifier_uses(node->data.assignment.value, uses, use_count);
        default:
            return true;
    }
}

static const char *
mir_instruction_slot_anchor(const MIRInstruction *inst)
{
    if (inst == NULL)
        return NULL;
    if (inst->slot_anchor != NULL)
        return inst->slot_anchor;
    if ((inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI) && inst->name != NULL)
        return inst->name;
    return inst->arg0;
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
    const HIRRoutine *hir_routine;
    const HIRBasicBlock *hir_block;
    MIRBasicBlock *mir_block;
    size_t *current_versions = NULL;

    if (routine == NULL || routine->hir_routine == NULL)
        return false;
    if (block_id >= routine->block_count)
        return false;

    hir_routine = routine->hir_routine;
    hir_block = &hir_routine->cfg.blocks[block_id];
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

    for (size_t i = 0; i < hir_block->local_def_count; i++) {
        int name_index;
        char *versioned;
        const char *name = hir_block->local_defs[i];
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
        if (!append_name(&mir_block->renamed_locals, &mir_block->renamed_local_count, versioned)) {
            free(versioned);
            free(current_versions);
            return false;
        }
        if (!mir_add_def_instruction(routine,
                                     mir_block,
                                     hir_block->phi_node_count + i,
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
    for (size_t i = 0; i < hir_block->dom_tree_child_count; i++) {
        size_t child = hir_block->dom_tree_children[i];
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
    const HIRRoutine *hir_routine;
    if (routine == NULL || routine->hir_routine == NULL)
        return false;
    hir_routine = routine->hir_routine;
    if (!hir_routine->has_cfg)
        return true;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const HIRBasicBlock *hir_block = &hir_routine->cfg.blocks[block_id];
        MIRBasicBlock *mir_block = &routine->blocks[block_id];
        for (size_t i = 0; i < hir_block->phi_node_count && i < mir_block->instruction_count; i++) {
            MIRInstruction *inst = &mir_block->instructions[i];
            const HIRPhiNode *phi = &hir_block->phi_nodes[i];
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

    if (!mir_collect_ssa_names(routine->hir_routine, &ssa_names, &ssa_name_count))
        goto cleanup;
    if (ssa_name_count == 0) {
        ok = true;
        goto cleanup;
    }
    next_versions = calloc(ssa_name_count, sizeof(size_t));
    root_versions = calloc(ssa_name_count, sizeof(size_t));
    out_versions = calloc(routine->block_count, sizeof(size_t *));
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
    free(out_versions);
    free(root_versions);
    free(next_versions);
    free((void *)ssa_names);
    return ok;
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
    return append_owned_name(&inst->uses, &inst->use_count, versioned);
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
    if (block == NULL || base == NULL)
        return true;
    versioned = mir_make_versioned_name(base, version);
    if (versioned == NULL)
        return false;
    names = is_entry ? &block->ssa_entry_values : &block->ssa_exit_values;
    count = is_entry ? &block->ssa_entry_value_count : &block->ssa_exit_value_count;
    return append_owned_name(names, count, versioned);
}

static bool
mir_parse_versioned_name(const char *versioned, char *base, size_t base_size, size_t *version_out)
{
    const char *dot;
    size_t len;
    if (versioned == NULL || base == NULL || base_size == 0 || version_out == NULL)
        return false;
    dot = strrchr(versioned, '.');
    if (dot == NULL)
        return false;
    len = (size_t)(dot - versioned);
    if (len + 1 > base_size)
        return false;
    memcpy(base, versioned, len);
    base[len] = '\0';
    *version_out = (size_t)strtoull(dot + 1, NULL, 10);
    return true;
}

static bool
mir_populate_use_edges(MIRRoutine *routine)
{
    const HIRRoutine *hir_routine;
    const char **ssa_names = NULL;
    size_t ssa_name_count = 0;

    if (routine == NULL || routine->hir_routine == NULL)
        return false;
    hir_routine = routine->hir_routine;
    if (!hir_routine->has_cfg)
        return true;
    if (!mir_collect_ssa_names(hir_routine, &ssa_names, &ssa_name_count))
        return false;
    if (ssa_name_count == 0) {
        free((void *)ssa_names);
        return true;
    }

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const HIRBasicBlock *hir_block = &hir_routine->cfg.blocks[block_id];
        MIRBasicBlock *block = &routine->blocks[block_id];
        size_t *current_versions;
        size_t stmt_index = 0;
        if (block->ssa_entry_versions == NULL || block->ssa_version_count != ssa_name_count)
            continue;
        for (size_t n = 0; n < ssa_name_count; n++) {
            if (block->ssa_entry_versions[n] == 0)
                continue;
            if (!mir_append_block_versioned_name(block, true, ssa_names[n], block->ssa_entry_versions[n])) {
                free((void *)ssa_names);
                return false;
            }
        }
        {
            current_versions = calloc(ssa_name_count, sizeof(size_t));
            if (current_versions == NULL) {
                free((void *)ssa_names);
                return false;
            }
            memcpy(current_versions,
                   block->ssa_entry_versions,
                   ssa_name_count * sizeof(size_t));
        for (size_t i = 0; i < block->instruction_count; i++) {
            MIRInstruction *inst = &block->instructions[i];
            if (inst->kind == MIR_INST_PHI) {
                for (size_t j = 0; j < inst->phi_incoming_count; j++) {
                    if (!append_owned_name(&inst->uses,
                                           &inst->use_count,
                                           pergyra_strdup(inst->phi_incomings[j].value_name)))
                        return false;
                    routine->use_edge_count++;
                }
                if (inst->result_name != NULL) {
                    char base[128];
                    size_t version = 0;
                    int idx;
                    if (mir_parse_versioned_name(inst->result_name, base, sizeof(base), &version)) {
                        idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, base);
                        if (idx >= 0)
                            current_versions[idx] = version;
                    }
                }
                continue;
            }
            if (inst->kind == MIR_INST_DEF) {
                while (hir_block != NULL && stmt_index < hir_block->statement_count) {
                    ASTNode *stmt = hir_block->statements[stmt_index];
                    if (stmt != NULL
                        && (stmt->type == AST_LET_DECL
                            || (stmt->type == AST_ASSIGNMENT
                                && stmt->data.assignment.target != NULL
                                && stmt->data.assignment.target->type == AST_IDENTIFIER))) {
                        break;
                    }
                    stmt_index++;
                }
                if (hir_block != NULL && stmt_index < hir_block->statement_count) {
                    ASTNode *stmt = hir_block->statements[stmt_index];
                    ASTNode *expr = NULL;
                    const char **raw_uses = NULL;
                    size_t raw_use_count = 0;
                    if (stmt != NULL && stmt->type == AST_LET_DECL)
                        expr = stmt->data.let_decl.initializer;
                    else if (stmt != NULL && stmt->type == AST_ASSIGNMENT)
                        expr = stmt->data.assignment.value;
                    if (expr != NULL
                        && !mir_collect_expr_identifier_uses(expr, &raw_uses, &raw_use_count)) {
                        free((void *)raw_uses);
                        free(current_versions);
                        free((void *)ssa_names);
                        return false;
                    }
                    for (size_t j = 0; j < raw_use_count; j++) {
                        int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, raw_uses[j]);
                        if (idx >= 0) {
                            if (!mir_append_versioned_use(inst, raw_uses[j], current_versions[idx])) {
                                free((void *)raw_uses);
                                free(current_versions);
                                free((void *)ssa_names);
                                return false;
                            }
                            routine->use_edge_count++;
                        }
                    }
                    free((void *)raw_uses);
                    stmt_index++;
                }
                if (inst->result_name != NULL) {
                    char base[128];
                    size_t version = 0;
                    int idx;
                    if (mir_parse_versioned_name(inst->result_name, base, sizeof(base), &version)) {
                        idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, base);
                        if (idx >= 0)
                            current_versions[idx] = version;
                    }
                }
                continue;
            }
            if (inst->kind == MIR_INST_BRANCH || inst->kind == MIR_INST_RETURN) {
                const char **raw_uses = NULL;
                size_t raw_use_count = 0;
                ASTNode *expr = (inst->kind == MIR_INST_BRANCH)
                                    ? hir_block->terminator_condition
                                    : hir_block->terminator_value;
                if (!mir_collect_expr_identifier_uses(expr, &raw_uses, &raw_use_count)) {
                    free((void *)raw_uses);
                    return false;
                }
                for (size_t j = 0; j < raw_use_count; j++) {
                    int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, raw_uses[j]);
                    if (idx >= 0) {
                        if (!mir_append_versioned_use(inst, raw_uses[j], current_versions[idx]))
                            return false;
                        routine->use_edge_count++;
                    }
                }
                free((void *)raw_uses);
                continue;
            }
            if (inst->kind == MIR_INST_RESOURCE_OP || inst->kind == MIR_INST_CLEANUP_EDGE) {
                const char **raw_uses = NULL;
                size_t raw_use_count = 0;
                if (inst->ast != NULL && !mir_collect_expr_identifier_uses(inst->ast, &raw_uses, &raw_use_count)) {
                    free((void *)raw_uses);
                    return false;
                }
                if (raw_use_count == 0) {
                    const char *candidates[2] = {inst->arg0, inst->arg1};
                    for (size_t j = 0; j < 2; j++) {
                        int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, candidates[j]);
                        if (idx >= 0) {
                            if (!mir_append_versioned_use(inst, candidates[j], current_versions[idx]))
                                return false;
                            routine->use_edge_count++;
                        }
                    }
                } else {
                    for (size_t j = 0; j < raw_use_count; j++) {
                        int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, raw_uses[j]);
                        if (idx >= 0) {
                            if (!mir_append_versioned_use(inst, raw_uses[j], current_versions[idx]))
                                return false;
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
                if (!mir_append_block_versioned_name(block, false, ssa_names[n], current_versions[n])) {
                    free(current_versions);
                    free((void *)ssa_names);
                    return false;
                }
            }
            free(current_versions);
        }
    }
    free((void *)ssa_names);
    return true;
}

static bool
mir_append_block_set(const char ***names, size_t *count, const char *name)
{
    if (name == NULL)
        return true;
    return append_name_unique(names, count, name);
}

static bool
mir_collect_block_defs_uses(MIRRoutine *routine)
{
    if (routine == NULL)
        return false;

    for (size_t i = 0; i < routine->block_count; i++) {
        MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->instruction_count; j++) {
            MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_PHI) {
                if (inst->result_name != NULL) {
                    if (!mir_append_block_set(&block->def_names, &block->def_name_count, inst->result_name))
                        return false;
                }
                continue;
            }
            for (size_t k = 0; k < inst->use_count; k++) {
                const char *use = inst->uses[k];
                if (!mir_name_set_contains(block->def_names, block->def_name_count, use)) {
                    if (!mir_append_block_set(&block->use_names, &block->use_name_count, use))
                        return false;
                }
            }
            if (inst->result_name != NULL) {
                if (!mir_append_block_set(&block->def_names, &block->def_name_count, inst->result_name))
                    return false;
            }
        }
    }

    return true;
}

static bool
mir_collect_successor_live_in(const MIRRoutine *routine,
                              size_t predecessor_block,
                              const MIRBasicBlock *block,
                              const char ***names,
                              size_t *count)
{
    size_t succs[5];
    size_t succ_count = 0;

    if (routine == NULL || block == NULL || names == NULL || count == NULL)
        return false;

    if (block->has_succ_true)
        succs[succ_count++] = block->succ_true;
    if (block->has_succ_false)
        succs[succ_count++] = block->succ_false;
    if (block->has_cleanup_succ)
        succs[succ_count++] = block->cleanup_succ;
    if (block->has_rollback_succ)
        succs[succ_count++] = block->rollback_succ;
    if (block->has_invalidation_succ)
        succs[succ_count++] = block->invalidation_succ;

    for (size_t i = 0; i < succ_count; i++) {
        size_t succ = succs[i];
        if (succ >= routine->block_count)
            continue;
        for (size_t j = 0; j < routine->blocks[succ].live_in_name_count; j++) {
            if (!mir_append_block_set(names, count, routine->blocks[succ].live_in_names[j]))
                return false;
        }
        for (size_t j = 0; j < routine->blocks[succ].instruction_count; j++) {
            const MIRInstruction *inst = &routine->blocks[succ].instructions[j];
            if (inst->kind != MIR_INST_PHI)
                continue;
            for (size_t k = 0; k < inst->phi_incoming_count; k++) {
                if (inst->phi_incomings[k].predecessor_block == predecessor_block) {
                    if (!mir_append_block_set(names, count, inst->phi_incomings[k].value_name))
                        return false;
                }
            }
        }
    }

    return true;
}

static void
mir_clear_block_name_set(const char ***names, size_t *count)
{
    free((void *)*names);
    *names = NULL;
    *count = 0;
}

static int
mir_find_value_summary(const MIRRoutine *routine, const char *name)
{
    if (routine == NULL || name == NULL)
        return -1;
    for (size_t i = 0; i < routine->value_summary_count; i++) {
        if (routine->value_summaries[i].name != NULL
            && strcmp(routine->value_summaries[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static bool
mir_append_value_summary(MIRRoutine *routine,
                         const char *name,
                         size_t def_block,
                         size_t def_inst)
{
    MIRValueSummary *grown;
    MIRValueSummary summary;

    if (routine == NULL || name == NULL)
        return false;
    if (mir_find_value_summary(routine, name) >= 0)
        return true;

    memset(&summary, 0, sizeof(summary));
    summary.name = pergyra_strdup(name);
    if (summary.name == NULL)
        return false;
    summary.slot_anchor = NULL;
    summary.def_block = def_block;
    summary.def_inst = def_inst;
    summary.first_use_block = SIZE_MAX;
    summary.last_use_block = SIZE_MAX;

    grown = realloc(routine->value_summaries,
                    sizeof(MIRValueSummary) * (routine->value_summary_count + 1));
    if (grown == NULL) {
        free((void *)summary.name);
        return false;
    }
    routine->value_summaries = grown;
    routine->value_summaries[routine->value_summary_count++] = summary;
    return true;
}

static bool
mir_build_value_summaries(MIRRoutine *routine)
{
    if (routine == NULL)
        return false;

    for (size_t i = 0; i < routine->value_summary_count; i++)
        free((void *)routine->value_summaries[i].name);
    free(routine->value_summaries);
    routine->value_summaries = NULL;
    routine->value_summary_count = 0;
    routine->has_use_def_summary = false;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
            const MIRInstruction *inst = &block->instructions[inst_id];
            if (inst->result_name != NULL) {
                if (!mir_append_value_summary(routine, inst->result_name, block_id, inst_id))
                    return false;
                {
                    int idx = mir_find_value_summary(routine, inst->result_name);
                    if (idx >= 0 && routine->value_summaries[idx].slot_anchor == NULL)
                        routine->value_summaries[idx].slot_anchor = mir_instruction_slot_anchor(inst);
                }
            }
        }
    }

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];

        for (size_t i = 0; i < block->live_in_name_count; i++) {
            int idx = mir_find_value_summary(routine, block->live_in_names[i]);
            if (idx >= 0)
                routine->value_summaries[idx].live_in_block_count++;
        }
        for (size_t i = 0; i < block->live_out_name_count; i++) {
            int idx = mir_find_value_summary(routine, block->live_out_names[i]);
            if (idx >= 0)
                routine->value_summaries[idx].live_out_block_count++;
        }
        for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
            const MIRInstruction *inst = &block->instructions[inst_id];
            for (size_t use_i = 0; use_i < inst->use_count; use_i++) {
                int idx = mir_find_value_summary(routine, inst->uses[use_i]);
                MIRValueSummary *summary;
                if (idx < 0)
                    continue;
                summary = &routine->value_summaries[idx];
                summary->use_count++;
                if (summary->first_use_block == SIZE_MAX)
                    summary->first_use_block = block_id;
                summary->last_use_block = block_id;
                if (block->is_cleanup)
                    summary->reaches_cleanup = true;
            }
        }
    }

    routine->has_use_def_summary = true;
    return true;
}

static bool
mir_free_instruction_payload(MIRInstruction *inst)
{
    if (inst == NULL)
        return true;
    free((void *)inst->result_name);
    inst->result_name = NULL;
    for (size_t i = 0; i < inst->use_count; i++)
        free((void *)inst->uses[i]);
    free((void *)inst->uses);
    inst->uses = NULL;
    inst->use_count = 0;
    if (inst->phi_incomings != NULL) {
        for (size_t i = 0; i < inst->phi_incoming_count; i++)
            free((void *)inst->phi_incomings[i].value_name);
    }
    free(inst->phi_incomings);
    inst->phi_incomings = NULL;
    inst->phi_incoming_count = 0;
    return true;
}

static void
mir_reset_routine_analysis(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    routine->live_value_count = 0;
    routine->has_liveness = false;
    routine->has_use_def_summary = false;
    for (size_t i = 0; i < routine->value_summary_count; i++)
        free((void *)routine->value_summaries[i].name);
    free(routine->value_summaries);
    routine->value_summaries = NULL;
    routine->value_summary_count = 0;

    for (size_t i = 0; i < routine->block_count; i++) {
        MIRBasicBlock *block = &routine->blocks[i];
        mir_clear_block_name_set(&block->def_names, &block->def_name_count);
        mir_clear_block_name_set(&block->use_names, &block->use_name_count);
        mir_clear_block_name_set(&block->live_in_names, &block->live_in_name_count);
        mir_clear_block_name_set(&block->live_out_names, &block->live_out_name_count);
    }
}

static bool
mir_recompute_analysis(MIRRoutine *routine)
{
    mir_reset_routine_analysis(routine);
    return mir_compute_liveness(routine);
}

static bool
mir_remove_instruction(MIRBasicBlock *block, size_t index)
{
    if (block == NULL || index >= block->instruction_count)
        return false;
    mir_free_instruction_payload(&block->instructions[index]);
    if (index + 1 < block->instruction_count) {
        memmove(&block->instructions[index],
                &block->instructions[index + 1],
                (block->instruction_count - index - 1) * sizeof(MIRInstruction));
    }
    block->instruction_count--;
    if (block->instruction_count == 0) {
        free(block->instructions);
        block->instructions = NULL;
    } else {
        MIRInstruction *shrunk = realloc(block->instructions,
                                         block->instruction_count * sizeof(MIRInstruction));
        if (shrunk != NULL)
            block->instructions = shrunk;
    }
    return true;
}

static bool
mir_instruction_is_dead_value(const MIRRoutine *routine, const MIRInstruction *inst)
{
    int idx;
    const MIRValueSummary *summary;

    if (routine == NULL || inst == NULL || inst->result_name == NULL)
        return false;
    if (inst->kind != MIR_INST_DEF && inst->kind != MIR_INST_PHI)
        return false;

    idx = mir_find_value_summary(routine, inst->result_name);
    if (idx < 0)
        return false;
    summary = &routine->value_summaries[idx];
    return summary->use_count == 0
           && summary->live_in_block_count == 0
           && summary->live_out_block_count == 0
           && !summary->reaches_cleanup;
}

static bool
mir_run_dce_on_routine(MIRRoutine *routine, bool *changed_out)
{
    bool changed = false;

    if (changed_out != NULL)
        *changed_out = false;
    if (routine == NULL)
        return false;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_id = block->instruction_count; inst_id-- > 0;) {
            if (!mir_instruction_is_dead_value(routine, &block->instructions[inst_id]))
                continue;
            if (!mir_remove_instruction(block, inst_id))
                return false;
            routine->dce_removed_count++;
            changed = true;
        }
    }

    if (changed_out != NULL)
        *changed_out = changed;
    return true;
}

static bool
mir_compute_liveness(MIRRoutine *routine)
{
    bool changed;

    if (routine == NULL)
        return false;
    if (!mir_collect_block_defs_uses(routine))
        return false;

    do {
        changed = false;
        for (size_t idx = routine->block_count; idx-- > 0;) {
            MIRBasicBlock *block = &routine->blocks[idx];
            const char **new_live_out = NULL;
            size_t new_live_out_count = 0;
            const char **new_live_in = NULL;
            size_t new_live_in_count = 0;
            bool same_live_out = true;
            bool same_live_in = true;

            if (!mir_collect_successor_live_in(routine, idx, block, &new_live_out, &new_live_out_count)) {
                free((void *)new_live_out);
                free((void *)new_live_in);
                return false;
            }

            for (size_t i = 0; i < block->use_name_count; i++) {
                if (!mir_append_block_set(&new_live_in, &new_live_in_count, block->use_names[i])) {
                    free((void *)new_live_out);
                    free((void *)new_live_in);
                    return false;
                }
            }
            for (size_t i = 0; i < new_live_out_count; i++) {
                const char *name = new_live_out[i];
                if (!mir_name_set_contains(block->def_names, block->def_name_count, name)) {
                    if (!mir_append_block_set(&new_live_in, &new_live_in_count, name)) {
                        free((void *)new_live_out);
                        free((void *)new_live_in);
                        return false;
                    }
                }
            }

            if (new_live_out_count != block->live_out_name_count)
                same_live_out = false;
            else {
                for (size_t i = 0; i < new_live_out_count; i++) {
                    if (!mir_name_set_contains(block->live_out_names, block->live_out_name_count, new_live_out[i])) {
                        same_live_out = false;
                        break;
                    }
                }
            }

            if (new_live_in_count != block->live_in_name_count)
                same_live_in = false;
            else {
                for (size_t i = 0; i < new_live_in_count; i++) {
                    if (!mir_name_set_contains(block->live_in_names, block->live_in_name_count, new_live_in[i])) {
                        same_live_in = false;
                        break;
                    }
                }
            }

            if (!same_live_out) {
                mir_clear_block_name_set(&block->live_out_names, &block->live_out_name_count);
                block->live_out_names = new_live_out;
                block->live_out_name_count = new_live_out_count;
                new_live_out = NULL;
                changed = true;
            }
            if (!same_live_in) {
                mir_clear_block_name_set(&block->live_in_names, &block->live_in_name_count);
                block->live_in_names = new_live_in;
                block->live_in_name_count = new_live_in_count;
                new_live_in = NULL;
                changed = true;
            }

            free((void *)new_live_out);
            free((void *)new_live_in);
        }
    } while (changed);

    routine->live_value_count = 0;
    routine->has_liveness = true;
    for (size_t i = 0; i < routine->block_count; i++)
        routine->live_value_count += routine->blocks[i].live_in_name_count + routine->blocks[i].live_out_name_count;
    return mir_build_value_summaries(routine);
}

static bool
mir_block_has_value(const MIRBasicBlock *block, const char *name)
{
    if (block == NULL || name == NULL)
        return false;
    return mir_name_set_contains(block->ssa_entry_values, block->ssa_entry_value_count, name)
           || mir_name_set_contains(block->def_names, block->def_name_count, name)
           || mir_name_set_contains(block->ssa_exit_values, block->ssa_exit_value_count, name);
}

static bool
mir_block_has_predecessor(const MIRBasicBlock *block, size_t predecessor)
{
    if (block == NULL)
        return false;
    for (size_t i = 0; i < block->predecessor_count; i++) {
        if (block->predecessors[i] == predecessor)
            return true;
    }
    return false;
}

static bool
mir_block_can_use_value_before_inst(const MIRBasicBlock *block, const char *name, size_t inst_index)
{
    if (block == NULL || name == NULL)
        return false;
    if (mir_name_set_contains(block->live_in_names, block->live_in_name_count, name)
        || mir_name_set_contains(block->ssa_entry_values, block->ssa_entry_value_count, name)) {
        return true;
    }

    for (size_t i = 0; i < inst_index && i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->result_name != NULL && strcmp(inst->result_name, name) == 0)
            return true;
    }
    return false;
}

static bool
mir_validate_block_liveness_sets(const MIRRoutine *routine,
                                 const MIRBasicBlock *block,
                                 size_t block_index,
                                 char **error_message)
{
    if (block == NULL)
        return false;

    for (size_t i = 0; i < block->live_out_name_count; i++) {
        const char *name = block->live_out_names[i];
        if (name == NULL)
            continue;
        if (!mir_block_has_value(block, name) && !mir_name_set_contains(block->live_in_names, block->live_in_name_count, name)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] live-out '%s' is not produced by block state",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    name);
            }
            return false;
        }
    }

    return true;
}

static bool
mir_validate_instruction_uses(const MIRRoutine *routine,
                              const MIRBasicBlock *block,
                              size_t block_index,
                              char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_PHI) {
            for (size_t j = 0; j < inst->phi_incoming_count; j++) {
                size_t pred = inst->phi_incomings[j].predecessor_block;
                const char *value = inst->phi_incomings[j].value_name;
                if (pred >= routine->block_count) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi references invalid predecessor %zu",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            block_index,
                            pred);
                    }
                    return false;
                }
                if (!mir_block_has_predecessor(block, pred)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi predecessor %zu is not in predecessor list",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            block_index,
                            pred);
                    }
                    return false;
                }
                if (!mir_name_set_contains(routine->blocks[pred].ssa_exit_values,
                                           routine->blocks[pred].ssa_exit_value_count,
                                           value)
                    && !mir_name_set_contains(routine->blocks[pred].live_out_names,
                                              routine->blocks[pred].live_out_name_count,
                                              value)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi incoming '%s' is not available from predecessor %zu",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            block_index,
                            value != NULL ? value : "(null)",
                            pred);
                    }
                    return false;
                }
            }
            continue;
        }

        for (size_t j = 0; j < inst->use_count; j++) {
            const char *use = inst->uses[j];
            if (!mir_block_can_use_value_before_inst(block, use, i)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] instruction[%zu] uses '%s' before definition",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        block_index,
                        i,
                        use != NULL ? use : "(null)");
                }
                return false;
            }
        }
    }

    return true;
}

static bool
mir_materialize_cleanup_edges(MIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cleanup_block)
        return true;
    for (size_t i = 0; i < routine->block_count; i++) {
        MIRBasicBlock *block = &routine->blocks[i];
        if (i == routine->cleanup_block
            || (routine->has_rollback_block && i == routine->rollback_block)
            || (routine->has_invalidation_block && i == routine->invalidation_block)
            || !block->is_reachable)
            continue;
        block->cleanup_succ = routine->cleanup_block;
        block->has_cleanup_succ = true;
        routine->cleanup_edge_count++;
        if (!append_instruction(block,
                                (MIRInstruction){
                                    .id = routine->instruction_count++,
                                    .kind = MIR_INST_CLEANUP_EDGE,
                                    .name = "cleanup-edge",
                                    .slot_anchor = "cleanup",
                                    .arg0 = "cleanup",
                                    .arg1 = NULL,
                                    .ast = NULL,
                                })) {
            return false;
        }
        if (!append_index_unique(&routine->blocks[routine->cleanup_block].predecessors,
                                 &routine->blocks[routine->cleanup_block].predecessor_count,
                                 i)) {
            return false;
        }
    }
    if (routine->has_rollback_block) {
        MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];
        cleanup->rollback_succ = routine->rollback_block;
        cleanup->has_rollback_succ = true;
        if (!append_index_unique(&routine->blocks[routine->rollback_block].predecessors,
                                 &routine->blocks[routine->rollback_block].predecessor_count,
                                 cleanup->id)) {
            return false;
        }
    }
    if (routine->has_invalidation_block) {
        MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];
        cleanup->invalidation_succ = routine->invalidation_block;
        cleanup->has_invalidation_succ = true;
        if (!append_index_unique(&routine->blocks[routine->invalidation_block].predecessors,
                                 &routine->blocks[routine->invalidation_block].predecessor_count,
                                 cleanup->id)) {
            return false;
        }
    }
    if (routine->has_rollback_block && routine->has_invalidation_block) {
        MIRBasicBlock *rollback = &routine->blocks[routine->rollback_block];
        MIRBasicBlock *invalidation = &routine->blocks[routine->invalidation_block];
        rollback->cleanup_succ = invalidation->id;
        rollback->has_cleanup_succ = true;
        rollback->invalidation_succ = invalidation->id;
        rollback->has_invalidation_succ = true;
        if (!append_index_unique(&invalidation->predecessors,
                                 &invalidation->predecessor_count,
                                 rollback->id)) {
            return false;
        }
    }
    return true;
}

static bool
mir_populate_instructions(MIRRoutine *routine)
{
    const RIRScope *rir_scope;
    MIRBasicBlock *entry;
    MIRBasicBlock *rollback;
    MIRBasicBlock *invalidation;

    if (routine == NULL || routine->block_count == 0)
        return true;

    rir_scope = routine->rir_scope;
    entry = &routine->blocks[routine->entry_block];
    rollback = routine->has_rollback_block ? &routine->blocks[routine->rollback_block] : NULL;
    invalidation = routine->has_invalidation_block ? &routine->blocks[routine->invalidation_block] : NULL;

    if (rir_scope == NULL)
        return true;

    for (size_t i = 0; i < rir_scope->op_count; i++) {
        const RIROp *op = &rir_scope->ops[i];
        switch (op->kind) {
            case RIR_OP_ABORT_INTENT:
            case RIR_OP_COMPENSATE_INTENT_STEP:
                if (rollback != NULL) {
                    if (!mir_add_cleanup_instruction(routine, rollback, op))
                        return false;
                    break;
                }
                /* fallthrough */
            default:
                if (!mir_add_resource_instruction(routine, entry, op))
                    return false;
                break;
        }
    }

    if (invalidation != NULL) {
        for (size_t i = 0; i < rir_scope->fact_count; i++) {
            const RIRFact *fact = &rir_scope->facts[i];
            MIRInstruction inst;
            if (fact->kind != RIR_FACT_PROJECTION
                && fact->resource_kind != RIR_RESOURCE_EFFECT_INSTANCE
                && fact->resource_kind != RIR_RESOURCE_RELATION_INSTANCE
                && fact->resource_kind != RIR_RESOURCE_ZONE_HANDLE) {
                continue;
            }
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_CLEANUP_EDGE;
            inst.name = "DetachInvalidation";
            inst.slot_anchor = fact->slot_anchor != NULL ? fact->slot_anchor : fact->name;
            inst.arg0 = fact->name;
            inst.arg1 = rir_resource_kind_name(fact->resource_kind);
            inst.ast = fact->ast;
            if (!append_instruction(invalidation, inst))
                return false;
            routine->cleanup_instruction_count++;
        }
        if (!mir_append_intent_invalidation_markers(routine, invalidation))
            return false;
    }

    return true;
}

static bool
mir_build_blocks_from_hir(MIRRoutine *routine, const HIRRoutine *hir_routine)
{
    if (routine == NULL)
        return false;

    if (hir_routine == NULL || !hir_routine->has_cfg || hir_routine->cfg.block_count == 0) {
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = 0;
        block.is_entry = true;
        block.is_reachable = true;
        block.source_hir_block_id = SIZE_MAX;
        routine->entry_block = 0;
        return append_block(routine, block);
    }

    routine->entry_block = hir_routine->cfg.entry_block;
    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        const HIRBasicBlock *src = &hir_routine->cfg.blocks[i];
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = i;
        block.is_entry = (i == hir_routine->cfg.entry_block);
        block.is_reachable = src->is_reachable;
        block.source_hir_block_id = src->id;
        block.succ_true = src->succ_true;
        block.succ_false = src->succ_false;
        block.has_succ_true = src->has_succ_true;
        block.has_succ_false = src->has_succ_false;
        if (!copy_indices(&block.predecessors,
                          &block.predecessor_count,
                          src->predecessors,
                          src->predecessor_count)) {
            free(block.predecessors);
            return false;
        }
        if (!append_block(routine, block))
            return false;
    }

    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        if (!mir_add_phi_placeholders(routine, &routine->blocks[i], &hir_routine->cfg.blocks[i]))
            return false;
        if (!mir_add_terminator_instruction(routine, &routine->blocks[i], &hir_routine->cfg.blocks[i]))
            return false;
    }

    return true;
}

static bool
mir_append_cleanup_block(MIRRoutine *routine, const RIRScope *rir_scope)
{
    bool needs_cleanup = false;
    bool needs_rollback = false;
    bool needs_invalidation = false;
    MIRBasicBlock block;

    if (routine == NULL || rir_scope == NULL)
        return true;

    fprintf(stdout, "[MIR CLEANUP] Checking scope for routine '%s': ops=%zu, facts=%zu\n",
        routine->name ? routine->name : "(null)", rir_scope->op_count, rir_scope->fact_count);

    for (size_t i = 0; i < rir_scope->op_count; i++) {
        if (rir_scope->ops[i].kind == RIR_OP_ABORT_INTENT
            || rir_scope->ops[i].kind == RIR_OP_COMPENSATE_INTENT_STEP) {
            needs_cleanup = true;
            needs_rollback = true;
            break;
        }
    }

    for (size_t i = 0; i < rir_scope->fact_count; i++) {
        const RIRFact *fact = &rir_scope->facts[i];
        if (fact->kind == RIR_FACT_INTENT_POLICY
            && fact->name != NULL
            && strcmp(fact->name, "rollback") == 0) {
            needs_cleanup = true;
            needs_rollback = true;
        }
        if (fact->kind == RIR_FACT_PROJECTION
            || fact->resource_kind == RIR_RESOURCE_EFFECT_INSTANCE
            || fact->resource_kind == RIR_RESOURCE_RELATION_INSTANCE
            || fact->resource_kind == RIR_RESOURCE_ZONE_HANDLE) {
            needs_cleanup = true;
            needs_invalidation = true;
        }
    }
    if (mir_intent_ast_needs_invalidation(routine->hir_routine)) {
        needs_cleanup = true;
        needs_invalidation = true;
    }

    if (!needs_cleanup)
        return true;

    memset(&block, 0, sizeof(block));
    block.id = routine->block_count;
    block.is_cleanup = true;
    block.is_reachable = true;
    block.source_hir_block_id = SIZE_MAX;
    routine->cleanup_block = block.id;
    routine->has_cleanup_block = true;
    if (!append_block(routine, block))
        return false;

    if (needs_rollback) {
        memset(&block, 0, sizeof(block));
        block.id = routine->block_count;
        block.is_cleanup = true;
        block.is_reachable = true;
        block.source_hir_block_id = SIZE_MAX;
        routine->rollback_block = block.id;
        routine->has_rollback_block = true;
        if (!append_block(routine, block))
            return false;
        if (!mir_add_rollback_invalidation(routine, &routine->blocks[routine->rollback_block], rir_scope))
            return false;
    }

    if (needs_invalidation) {
        memset(&block, 0, sizeof(block));
        block.id = routine->block_count;
        block.is_cleanup = true;
        block.is_reachable = true;
        block.source_hir_block_id = SIZE_MAX;
        routine->invalidation_block = block.id;
        routine->has_invalidation_block = true;
        if (!append_block(routine, block))
            return false;
    }

    return true;
}

#include "mir_public.inc"
