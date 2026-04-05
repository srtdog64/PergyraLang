#include "mir.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

static bool
append_instruction(MIRBasicBlock *block, MIRInstruction inst)
{
    MIRInstruction *grown;
    if (block == NULL)
        return false;
    grown = realloc(block->instructions, (block->instruction_count + 1) * sizeof(MIRInstruction));
    if (grown == NULL)
        return false;
    grown[block->instruction_count] = inst;
    block->instructions = grown;
    block->instruction_count++;
    return true;
}

static char *
mir_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    return result;
}

static bool
insert_instruction(MIRBasicBlock *block, size_t index, MIRInstruction inst)
{
    MIRInstruction *grown;
    if (block == NULL)
        return false;
    if (index > block->instruction_count)
        index = block->instruction_count;
    grown = realloc(block->instructions, (block->instruction_count + 1) * sizeof(MIRInstruction));
    if (grown == NULL)
        return false;
    memmove(&grown[index + 1],
            &grown[index],
            (block->instruction_count - index) * sizeof(MIRInstruction));
    grown[index] = inst;
    block->instructions = grown;
    block->instruction_count++;
    return true;
}

static bool
append_name(const char ***names, size_t *count, const char *name)
{
    const char **grown;
    if (names == NULL || count == NULL || name == NULL)
        return false;
    grown = realloc((void *)*names, (*count + 1) * sizeof(const char *));
    if (grown == NULL)
        return false;
    grown[*count] = name;
    *names = grown;
    (*count)++;
    return true;
}

static bool
append_owned_name(const char ***names, size_t *count, char *name)
{
    const char **grown;
    if (names == NULL || count == NULL || name == NULL)
        return false;
    grown = realloc((void *)*names, (*count + 1) * sizeof(const char *));
    if (grown == NULL) {
        free(name);
        return false;
    }
    grown[*count] = name;
    *names = grown;
    (*count)++;
    return true;
}

static bool
append_name_unique(const char ***names, size_t *count, const char *name)
{
    if (names == NULL || count == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < *count; i++) {
        if ((*names)[i] != NULL && strcmp((*names)[i], name) == 0)
            return true;
    }
    return append_name(names, count, name);
}

static bool
append_index_unique(size_t **items, size_t *count, size_t value)
{
    size_t *grown;
    if (items == NULL || count == NULL)
        return false;
    for (size_t i = 0; i < *count; i++) {
        if ((*items)[i] == value)
            return true;
    }
    grown = realloc(*items, (*count + 1) * sizeof(size_t));
    if (grown == NULL)
        return false;
    grown[*count] = value;
    *items = grown;
    (*count)++;
    return true;
}

static bool
append_block(MIRRoutine *routine, MIRBasicBlock block)
{
    MIRBasicBlock *grown;
    if (routine == NULL)
        return false;
    grown = realloc(routine->blocks, (routine->block_count + 1) * sizeof(MIRBasicBlock));
    if (grown == NULL)
        return false;
    grown[routine->block_count] = block;
    routine->blocks = grown;
    routine->block_count++;
    return true;
}

static bool
append_routine(MIRProgram *mir, MIRRoutine routine)
{
    MIRRoutine *grown;
    if (mir == NULL)
        return false;
    grown = realloc(mir->routines, (mir->routine_count + 1) * sizeof(MIRRoutine));
    if (grown == NULL)
        return false;
    grown[mir->routine_count] = routine;
    mir->routines = grown;
    mir->routine_count++;
    return true;
}

static char *
mir_make_versioned_name(const char *base, size_t version)
{
    char buffer[128];
    size_t length;
    char *result;
    if (base == NULL)
        base = "tmp";
    snprintf(buffer, sizeof(buffer), "%s.%zu", base, version);
    length = strlen(buffer);
    result = malloc(length + 1);
    if (result == NULL)
        return NULL;
    memcpy(result, buffer, length + 1);
    return result;
}

static bool
copy_indices(size_t **dst, size_t *dst_count, const size_t *src, size_t src_count)
{
    if (src_count == 0) {
        *dst = NULL;
        *dst_count = 0;
        return true;
    }
    *dst = malloc(src_count * sizeof(size_t));
    if (*dst == NULL)
        return false;
    memcpy(*dst, src, src_count * sizeof(size_t));
    *dst_count = src_count;
    return true;
}

static bool
copy_versions(size_t **dst, const size_t *src, size_t count)
{
    if (count == 0) {
        *dst = NULL;
        return true;
    }
    *dst = malloc(count * sizeof(size_t));
    if (*dst == NULL)
        return false;
    memcpy(*dst, src, count * sizeof(size_t));
    return true;
}

static bool
mir_store_block_versions(MIRBasicBlock *block, bool is_entry, const size_t *versions, size_t count)
{
    size_t **target;
    if (block == NULL)
        return false;
    target = is_entry ? &block->ssa_entry_versions : &block->ssa_exit_versions;
    free(*target);
    *target = NULL;
    if (!copy_versions(target, versions, count))
        return false;
    block->ssa_version_count = count;
    return true;
}

static MIRScopeKind
mir_scope_kind_from_hir(const HIRRoutine *routine)
{
    if (routine == NULL)
        return MIR_SCOPE_FUNCTION;
    if (routine->kind == HIR_TOPLEVEL_INTENT)
        return MIR_SCOPE_INTENT;
    if (routine->is_hosted || routine->is_action_like)
        return MIR_SCOPE_METHOD;
    return MIR_SCOPE_FUNCTION;
}

static const RIRScope *
mir_find_matching_rir_scope(const RIRProgram *rir, const HIRRoutine *routine)
{
    RIRScopeKind wanted_kind;
    if (rir == NULL || routine == NULL || routine->name == NULL)
        return NULL;

    if (routine->kind == HIR_TOPLEVEL_INTENT) {
        wanted_kind = RIR_SCOPE_INTENT;
    } else if (routine->is_hosted || routine->is_action_like) {
        wanted_kind = RIR_SCOPE_METHOD;
    } else {
        wanted_kind = RIR_SCOPE_FUNCTION;
    }

    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        if (scope->kind == wanted_kind
            && scope->name != NULL
            && strcmp(scope->name, routine->name) == 0) {
            return scope;
        }
    }
    return NULL;
}

static const char *
mir_node_name(ASTNode *node)
{
    if (node == NULL)
        return NULL;
    switch (node->type) {
        case AST_IDENTIFIER:
            return node->data.identifier.name;
        case AST_MEMBER_ACCESS:
            return node->data.member.name;
        case AST_TYPE:
            return node->data.type.name;
        default:
            return NULL;
    }
}

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

MIRProgram *
mir_lower(const HIRProgram *hir, const RIRProgram *rir, char **error_message)
{
    MIRProgram *mir;
    if (error_message != NULL)
        *error_message = NULL;
    if (hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR lowering requires HIR");
        return NULL;
    }

    mir = calloc(1, sizeof(MIRProgram));
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return NULL;
    }

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *hir_routine = &hir->routines[i];
        MIRRoutine routine;
        memset(&routine, 0, sizeof(routine));
        routine.id = mir->routine_count;
        routine.kind = mir_scope_kind_from_hir(hir_routine);
        routine.owner_name = NULL;
        routine.name = hir_routine->name;
        routine.hir_routine = hir_routine;
        routine.rir_scope = mir_find_matching_rir_scope(rir, hir_routine);

        if (!mir_build_blocks_from_hir(&routine, hir_routine)
            || !mir_append_cleanup_block(&routine, routine.rir_scope)
            || !mir_populate_instructions(&routine)
            || !mir_apply_ssa_rename(&routine)
            || !mir_populate_use_edges(&routine)
            || !mir_materialize_cleanup_edges(&routine)
            || !append_routine(mir, routine)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }

    return mir;
}

void
mir_destroy(MIRProgram *mir)
{
    if (mir == NULL)
        return;
    for (size_t i = 0; i < mir->routine_count; i++) {
        MIRRoutine *routine = &mir->routines[i];
        for (size_t j = 0; j < routine->block_count; j++) {
            free(routine->blocks[j].predecessors);
            for (size_t k = 0; k < routine->blocks[j].instruction_count; k++) {
                free((void *)routine->blocks[j].instructions[k].result_name);
                for (size_t m = 0; m < routine->blocks[j].instructions[k].use_count; m++)
                    free((void *)routine->blocks[j].instructions[k].uses[m]);
                free((void *)routine->blocks[j].instructions[k].uses);
                if (routine->blocks[j].instructions[k].phi_incomings != NULL) {
                    for (size_t m = 0; m < routine->blocks[j].instructions[k].phi_incoming_count; m++)
                        free((void *)routine->blocks[j].instructions[k].phi_incomings[m].value_name);
                }
                free(routine->blocks[j].instructions[k].phi_incomings);
            }
            for (size_t k = 0; k < routine->blocks[j].renamed_local_count; k++)
                free((void *)routine->blocks[j].renamed_locals[k]);
            free((void *)routine->blocks[j].renamed_locals);
            for (size_t k = 0; k < routine->blocks[j].ssa_entry_value_count; k++)
                free((void *)routine->blocks[j].ssa_entry_values[k]);
            free((void *)routine->blocks[j].ssa_entry_values);
            for (size_t k = 0; k < routine->blocks[j].ssa_exit_value_count; k++)
                free((void *)routine->blocks[j].ssa_exit_values[k]);
            free((void *)routine->blocks[j].ssa_exit_values);
            free(routine->blocks[j].ssa_entry_versions);
            free(routine->blocks[j].ssa_exit_versions);
            free(routine->blocks[j].instructions);
        }
        free(routine->blocks);
    }
    free(mir->routines);
    free(mir);
}

const char *
mir_scope_kind_name(MIRScopeKind kind)
{
    switch (kind) {
        case MIR_SCOPE_FUNCTION: return "function";
        case MIR_SCOPE_METHOD: return "method";
        case MIR_SCOPE_INTENT: return "intent";
        default: return "unknown";
    }
}

const char *
mir_inst_kind_name(MIRInstKind kind)
{
    switch (kind) {
        case MIR_INST_DEF: return "def";
        case MIR_INST_RESOURCE_OP: return "resource-op";
        case MIR_INST_PHI: return "phi";
        case MIR_INST_BRANCH: return "branch";
        case MIR_INST_RETURN: return "return";
        case MIR_INST_CLEANUP_EDGE: return "cleanup";
        default: return "unknown";
    }
}

bool
mir_validate(const MIRProgram *mir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is null");
        return false;
    }

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        if (routine->block_count == 0 || routine->entry_block >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has invalid entry block",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->has_cleanup_block && routine->cleanup_block >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has invalid cleanup block",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];
            for (size_t k = 0; k < block->instruction_count; k++) {
                const MIRInstruction *inst = &block->instructions[k];
                if (inst->kind == MIR_INST_PHI
                    && inst->phi_incoming_count != block->predecessor_count) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi has %zu incoming edges but %zu predecessors",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            inst->phi_incoming_count,
                            block->predecessor_count);
                    }
                    return false;
                }
            }
        }
    }

    return true;
}

void
mir_dump(const MIRProgram *mir, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (mir == NULL) {
        fprintf(out, "MIR: (null)\n");
        return;
    }

    fprintf(out, "MIR Program\n  routines: %zu\n", mir->routine_count);
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        fprintf(out,
                "  routine[%02zu] %-8s %s blocks=%zu instructions=%zu cleanup-block=%s rollback-block=%s invalidation-block=%s phi=%zu renamed=%zu cleanup-edges=%zu uses=%zu\n",
                i,
                mir_scope_kind_name(routine->kind),
                routine->name != NULL ? routine->name : "(anonymous)",
                routine->block_count,
                routine->instruction_count,
                routine->has_cleanup_block ? "yes" : "no",
                routine->has_rollback_block ? "yes" : "no",
                routine->has_invalidation_block ? "yes" : "no",
                routine->phi_inserted_count,
                routine->renamed_value_count,
                routine->cleanup_edge_count,
                routine->use_edge_count);
        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];
            fprintf(out,
                    "    block[%02zu] reachable=%s cleanup=%s preds=%zu succT=%s succF=%s cleanupSucc=%s instructions=%zu\n",
                    j,
                    block->is_reachable ? "yes" : "no",
                    block->is_cleanup ? "yes" : "no",
                    block->predecessor_count,
                    block->has_succ_true ? "yes" : "no",
                    block->has_succ_false ? "yes" : "no",
                    block->has_cleanup_succ ? "yes" : "no",
                    block->instruction_count);
            if (block->has_rollback_succ || block->has_invalidation_succ) {
                fprintf(out,
                        "      exceptional rollback=%s invalidation=%s\n",
                        block->has_rollback_succ ? "yes" : "no",
                        block->has_invalidation_succ ? "yes" : "no");
            }
            if (block->ssa_entry_value_count > 0) {
                fprintf(out, "      entry:");
                for (size_t k = 0; k < block->ssa_entry_value_count; k++)
                    fprintf(out, " %s", block->ssa_entry_values[k]);
                fprintf(out, "\n");
            }
            if (block->renamed_local_count > 0) {
                fprintf(out, "      renamed:");
                for (size_t k = 0; k < block->renamed_local_count; k++)
                    fprintf(out, " %s", block->renamed_locals[k]);
                fprintf(out, "\n");
            }
            if (block->ssa_exit_value_count > 0) {
                fprintf(out, "      exit:");
                for (size_t k = 0; k < block->ssa_exit_value_count; k++)
                    fprintf(out, " %s", block->ssa_exit_values[k]);
                fprintf(out, "\n");
            }
            for (size_t k = 0; k < block->instruction_count; k++) {
                const MIRInstruction *inst = &block->instructions[k];
                fprintf(out,
                        "      inst[%02zu] %-12s name=%s result=%s arg0=%s arg1=%s",
                        k,
                        mir_inst_kind_name(inst->kind),
                        inst->name != NULL ? inst->name : "-",
                        inst->result_name != NULL ? inst->result_name : "-",
                        inst->arg0 != NULL ? inst->arg0 : "-",
                        inst->arg1 != NULL ? inst->arg1 : "-");
                if (inst->phi_incoming_count > 0) {
                    fprintf(out, " incoming=");
                    for (size_t m = 0; m < inst->phi_incoming_count; m++) {
                        fprintf(out,
                                "%s%zu:%s",
                                m == 0 ? "" : ",",
                                inst->phi_incomings[m].predecessor_block,
                                inst->phi_incomings[m].value_name != NULL
                                    ? inst->phi_incomings[m].value_name
                                    : "-");
                    }
                }
                if (inst->use_count > 0) {
                    fprintf(out, " uses=");
                    for (size_t m = 0; m < inst->use_count; m++)
                        fprintf(out, "%s%s", m == 0 ? "" : ",", inst->uses[m]);
                }
                fprintf(out, "\n");
            }
        }
    }
}
