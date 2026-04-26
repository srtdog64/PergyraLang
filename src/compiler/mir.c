#include "mir.h"

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../common/arena.h"
#include "../runtime/pgy_abi_spec.h"

#include "mir_base_helpers.h"

static void mir_clear_block_name_set(const char ***names, size_t *count);
static int mir_find_value_summary(const MIRRoutine *routine, const char *name);
static bool mir_compute_liveness(MIRRoutine *routine);
static const char *mir_stmt_def_name(const ASTNode *stmt);
static bool mir_let_decl_requires_stmt_preservation(const ASTNode *stmt);
static bool mir_assignment_requires_stmt_preservation(const ASTNode *func_decl,
                                                      ASTNode **statements,
                                                      size_t statement_count,
                                                      size_t stmt_index,
                                                      const ASTNode *stmt);

static bool
mir_type_node_is_slot_like(const ASTNode *type_node)
{
    const char *name;

    if (type_node == NULL || type_node->type != AST_TYPE)
        return false;
    name = type_node->data.type.name;
    if (name == NULL)
        return false;
    return strcmp(name, "Slot") == 0
        || strcmp(name, "SecureSlot") == 0
        || strcmp(name, "DeviceSlot") == 0
        || strncmp(name, "Slot<", 5) == 0
        || strncmp(name, "SecureSlot<", 11) == 0
        || strncmp(name, "DeviceSlot<", 11) == 0;
}

static bool
mir_expr_is_claim_like(const ASTNode *expr)
{
    const char *callee;

    if (expr == NULL || expr->type != AST_CALL
        || expr->data.call.callee == NULL
        || expr->data.call.callee->type != AST_IDENTIFIER
        || expr->data.call.callee->data.identifier.name == NULL)
        return false;

    callee = expr->data.call.callee->data.identifier.name;
    return strcmp(callee, "ClaimSlot") == 0
        || strcmp(callee, "ClaimSecureSlot") == 0
        || strcmp(callee, "ClaimDeviceSlot") == 0;
}

static bool
mir_binding_name_is_slot_like(const ASTNode *func_decl,
                              ASTNode **statements,
                              size_t statement_count,
                              size_t stmt_index,
                              const char *binding_name)
{
    if (binding_name == NULL || binding_name[0] == '\0')
        return false;

    if (func_decl != NULL && func_decl->type == AST_FUNC_DECL) {
        for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
            FuncParam *param = func_decl->data.func_decl.params[i];
            if (param == NULL || param->name == NULL)
                continue;
            if (strcmp(param->name, binding_name) != 0)
                continue;
            return mir_type_node_is_slot_like(param->type);
        }
    }

    if (statements == NULL || statement_count == 0)
        return false;
    if (stmt_index > statement_count)
        stmt_index = statement_count;

    for (size_t i = 0; i < stmt_index; i++) {
        ASTNode *prior = statements[i];
        if (prior == NULL || prior->type != AST_LET_DECL
            || prior->data.let_decl.name == NULL)
            continue;
        if (strcmp(prior->data.let_decl.name, binding_name) != 0)
            continue;
        if (mir_type_node_is_slot_like(prior->data.let_decl.type))
            return true;
        if (mir_expr_is_claim_like(prior->data.let_decl.initializer))
            return true;
    }

    return false;
}

static bool
mir_assignment_requires_stmt_preservation(const ASTNode *func_decl,
                                          ASTNode **statements,
                                          size_t statement_count,
                                          size_t stmt_index,
                                          const ASTNode *stmt)
{
    const char *target_name;

    if (stmt == NULL || stmt->type != AST_ASSIGNMENT
        || stmt->data.assignment.target == NULL
        || stmt->data.assignment.target->type != AST_IDENTIFIER
        || stmt->data.assignment.target->data.identifier.name == NULL)
        return false;

    target_name = stmt->data.assignment.target->data.identifier.name;
    return mir_binding_name_is_slot_like(func_decl,
                                         statements,
                                         statement_count,
                                         stmt_index,
                                         target_name);
}

static char *
mir_render_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return pergyra_strdup("Int");
    if (type_node->type == AST_TYPE) {
        size_t cap = 256;
        char *buf = calloc(cap, 1);
        if (buf == NULL)
            return NULL;
        snprintf(buf, cap, "%s", type_node->data.type.name != NULL
            ? type_node->data.type.name : "Int");
        if (type_node->data.type.generic_args != NULL
            && type_node->data.type.generic_args->count > 0) {
            strncat(buf, "<", cap - strlen(buf) - 1);
            for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
                GenericParam *param = type_node->data.type.generic_args->params[i];
                char *inner = mir_render_type_name(
                    param != NULL ? param->constraint : NULL);
                if (i > 0)
                    strncat(buf, ",", cap - strlen(buf) - 1);
                strncat(buf, inner != NULL ? inner : "Int", cap - strlen(buf) - 1);
                free(inner);
            }
            strncat(buf, ">", cap - strlen(buf) - 1);
        }
        return buf;
    }
    if (type_node->type == AST_CHANNEL_TYPE) {
        char *inner = mir_render_type_name(type_node->data.channel_type.element_type);
        char *result = NULL;
        if (inner != NULL)
            result = mir_strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result != NULL ? result : pergyra_strdup("Channel<Int>");
    }
    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = mir_render_type_name(type_node->data.future_type.value_type);
        char *result = NULL;
        if (inner != NULL)
            result = mir_strdup_fmt("Future<%s>", inner);
        free(inner);
        return result != NULL ? result : pergyra_strdup("Future<Int>");
    }
    return pergyra_strdup("Int");
}

static char *
mir_claim_abi_type_name_from_ast(const ASTNode *ast)
{
    if (ast == NULL)
        return NULL;
    if (ast->type == AST_WITH_STMT) {
        char *inner = mir_render_type_name(ast->data.with_stmt.slot_type);
        char *result = mir_strdup_fmt("%s<%s>",
                                  ast->data.with_stmt.is_secure ? "SecureSlot" : "Slot",
                                  inner != NULL ? inner : "Int");
        free(inner);
        return result;
    }
    if (ast->type == AST_CALL
        && ast->data.call.callee != NULL
        && ast->data.call.callee->type == AST_IDENTIFIER
        && ast->data.call.callee->data.identifier.name != NULL) {
        const char *callee = ast->data.call.callee->data.identifier.name;
        if (ast->data.call.arg_count >= 1 && ast->data.call.arguments[0] != NULL) {
            char *inner = mir_render_type_name(ast->data.call.arguments[0]);
            char *result = NULL;
            if (strcmp(callee, "ClaimSlot") == 0)
                result = mir_strdup_fmt("Slot<%s>", inner != NULL ? inner : "Int");
            else if (strcmp(callee, "ClaimSecureSlot") == 0)
                result = mir_strdup_fmt("SecureSlot<%s>", inner != NULL ? inner : "Int");
            else if (strcmp(callee, "ClaimDeviceSlot") == 0)
                result = mir_strdup_fmt("DeviceSlot<%s>", inner != NULL ? inner : "Int");
            free(inner);
            return result;
        }
    }
    return NULL;
}

static bool
mir_add_phi_placeholders(MIRRoutine *routine, MIRBasicBlock *block)
{
    if (routine == NULL || block == NULL)
        return false;

    for (size_t i = 0; i < block->source_phi_node_count; i++) {
        MIRInstruction inst;
        memset(&inst, 0, sizeof(inst));
        inst.id = routine->instruction_count++;
        inst.kind = MIR_INST_PHI;
        inst.name = block->source_phi_nodes[i].name;
        inst.slot_anchor = block->source_phi_nodes[i].name;
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
mir_add_terminator_instruction(MIRRoutine *routine, MIRBasicBlock *block)
{
    MIRInstruction inst;
    if (routine == NULL || block == NULL)
        return false;
    if (block->source_terminator_kind != HIR_BLOCK_BRANCH
        && block->source_terminator_kind != HIR_BLOCK_RETURN)
        return true;
    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = (block->source_terminator_kind == HIR_BLOCK_BRANCH)
                    ? MIR_INST_BRANCH
                    : MIR_INST_RETURN;
    inst.name = (block->source_terminator_kind == HIR_BLOCK_BRANCH) ? "branch" : "return";
    inst.ast = (block->source_terminator_kind == HIR_BLOCK_BRANCH)
                   ? block->source_terminator_condition
                   : block->source_terminator_value;
    return append_instruction(block, inst);
}

static bool
mir_copy_ast_nodes(ASTNode ***dst, size_t *dst_count, ASTNode **src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(ASTNode *));
    if (*dst == NULL)
        return false;
    memcpy(*dst, src, src_count * sizeof(ASTNode *));
    *dst_count = src_count;
    return true;
}

static bool
mir_copy_names(const char ***dst, size_t *dst_count, const char **src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(const char *));
    if (*dst == NULL)
        return false;
    memcpy((void *)*dst, src, src_count * sizeof(const char *));
    *dst_count = src_count;
    return true;
}

static bool
mir_copy_phi_nodes(MIRSourcePhiNode **dst, size_t *dst_count,
                   const HIRPhiNode *src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(MIRSourcePhiNode));
    if (*dst == NULL)
        return false;
    *dst_count = src_count;
    for (size_t i = 0; i < src_count; i++) {
        (*dst)[i].name = src[i].name;
        if (!copy_indices(&(*dst)[i].incoming_predecessors,
                          &(*dst)[i].incoming_predecessor_count,
                          src[i].incoming_predecessors,
                          src[i].incoming_predecessor_count)) {
            for (size_t j = 0; j < i; j++)
                free((*dst)[j].incoming_predecessors);
            free(*dst);
            *dst = NULL;
            *dst_count = 0;
            return false;
        }
    }
    return true;
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
    char *claim_type_name = NULL;
    const char *abi_type_name = NULL;
    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_RESOURCE_OP;
    inst.name = rir_op_kind_name(op->kind);
    inst.slot_anchor = op->slot_anchor;
    inst.arg0 = op->subject;
    inst.arg1 = op->arg0;
    inst.rir_op = op;
    inst.ast = op->ast;
    /* ABI type layout — lookup from type table */
    if (op->kind == RIR_OP_CLAIM)
        claim_type_name = mir_claim_abi_type_name_from_ast(op->ast);
    abi_type_name = claim_type_name != NULL
        ? claim_type_name
        : (op->arg0 != NULL ? op->arg0 : op->subject);
    inst.type_layout = mir_abi_lookup(abi_type_name);
    free(claim_type_name);
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
    if (routine == NULL || block == NULL)
        return false;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *src_block = &routine->blocks[bi];
        if (src_block->is_cleanup || !src_block->is_reachable)
            continue;
        for (size_t ii = 0; ii < src_block->instruction_count; ii++) {
            const MIRInstruction *src = &src_block->instructions[ii];
            MIRInstruction inst;

            if (src->kind != MIR_INST_STMT)
                continue;
            if (src->name == NULL || strcmp(src->name, "IntentInvalidationTarget") != 0)
                continue;
            if (src->arg0 == NULL)
                continue;

            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_CLEANUP_EDGE;
            inst.name = "DetachInvalidation";
            inst.slot_anchor = src->arg0;
            inst.arg0 = src->arg0;
            inst.arg1 = src->arg1;
            inst.ast = src->ast;
            if (!append_instruction(block, inst))
                return false;
            routine->cleanup_instruction_count++;
        }
    }

    return true;
}

static bool
mir_append_intent_step_instructions(MIRRoutine *routine, MIRBasicBlock *block)
{
    ASTNode *intent;

    if (routine == NULL || block == NULL || routine->hir_routine == NULL)
        return false;
    if (routine->hir_routine->ast == NULL || routine->hir_routine->ast->type != AST_INTENT_DECL)
        return true;

    intent = routine->hir_routine->ast;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        MIRInstruction inst;
        const char *alias = NULL;
        const char *type_name = NULL;

        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        alias = involves->data.intent_involves.alias;
        if (involves->data.intent_involves.subject_type != NULL
            && involves->data.intent_involves.subject_type->type == AST_TYPE) {
            type_name = involves->data.intent_involves.subject_type->data.type.name;
        }
        if (alias == NULL || type_name == NULL)
            continue;

        memset(&inst, 0, sizeof(inst));
        inst.id = routine->instruction_count++;
        inst.kind = MIR_INST_STMT;
        inst.name = "IntentParticipant";
        inst.slot_anchor = routine->name;
        inst.arg0 = alias;
        inst.arg1 = type_name;
        inst.ast = involves;
        if (!append_instruction(block, inst))
            return false;
    }

    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        MIRInstruction inst;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        memset(&inst, 0, sizeof(inst));
        inst.id = routine->instruction_count++;
        inst.kind = MIR_INST_STMT;
        inst.name = step->data.intent_step.name != NULL
            ? step->data.intent_step.name
            : "intent.step";
        inst.ast = step;
        if (!append_instruction(block, inst))
            return false;

        memset(&inst, 0, sizeof(inst));
        inst.id = routine->instruction_count++;
        inst.kind = MIR_INST_STMT;
        inst.name = "IntentStep";
        inst.slot_anchor = step->data.intent_step.name;
        inst.arg0 = step->data.intent_step.name != NULL
            ? step->data.intent_step.name
            : "intent.step";
        inst.ast = step;
        if (!append_instruction(block, inst))
            return false;

        if (step->data.intent_step.where_type != NULL
            && step->data.intent_step.where_type->type == AST_TYPE
            && step->data.intent_step.where_type->data.type.name != NULL) {
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentZoneWhere";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = step->data.intent_step.where_type->data.type.name;
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step;
            if (!append_instruction(block, inst))
                return false;
        }

        {
            const char *effective_zone_alias = NULL;
            if (step->data.intent_step.using_expr != NULL
                && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
                effective_zone_alias = step->data.intent_step.using_expr->data.identifier.name;
            } else if (step->data.intent_step.transfer_to_alias != NULL) {
                effective_zone_alias = step->data.intent_step.transfer_to_alias;
            }
            if (effective_zone_alias != NULL) {
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_STMT;
                inst.name = "IntentZoneAlias";
                inst.slot_anchor = step->data.intent_step.name;
                inst.arg0 = effective_zone_alias;
                inst.arg1 = step->data.intent_step.name;
                inst.ast = step;
                if (!append_instruction(block, inst))
                    return false;
            }
        }

        {
            const char *invalidation_target = NULL;
            if (step->data.intent_step.using_expr != NULL)
                invalidation_target = mir_node_name(step->data.intent_step.using_expr);
            else if (step->data.intent_step.transfer_to_alias != NULL)
                invalidation_target = step->data.intent_step.transfer_to_alias;
            else if (step->data.intent_step.transfer_from_alias != NULL)
                invalidation_target = step->data.intent_step.transfer_from_alias;
            if (invalidation_target != NULL) {
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_STMT;
                inst.name = "IntentInvalidationTarget";
                inst.slot_anchor = step->data.intent_step.name;
                inst.arg0 = invalidation_target;
                inst.arg1 = step->data.intent_step.name;
                inst.ast = step;
                if (!append_instruction(block, inst))
                    return false;
            }
        }

        if (step->data.intent_step.transfer_from_alias != NULL) {
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentZoneFrom";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = step->data.intent_step.transfer_from_alias;
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step;
            if (!append_instruction(block, inst))
                return false;
        }

        for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
            if (step->data.intent_step.who_names[j] == NULL)
                continue;
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentWho";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = step->data.intent_step.who_names[j];
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step;
            if (!append_instruction(block, inst))
                return false;
        }

        for (size_t j = 0; j < step->data.intent_step.authorized_by_count; j++) {
            if (step->data.intent_step.authorized_by[j] == NULL)
                continue;
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentAuthorizedBy";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = step->data.intent_step.authorized_by[j];
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step;
            if (!append_instruction(block, inst))
                return false;
        }

        if (step->data.intent_step.causes_effect != NULL) {
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentCauses";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = step->data.intent_step.causes_effect;
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step;
            if (!append_instruction(block, inst))
                return false;
        }

        if (step->data.intent_step.pre_expr != NULL) {
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentCheck";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = "pre";
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step->data.intent_step.pre_expr;
            if (!append_instruction(block, inst))
                return false;
        }
        if (step->data.intent_step.invariant_expr != NULL) {
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentCheck";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = "invariant-pre";
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step->data.intent_step.invariant_expr;
            if (!append_instruction(block, inst))
                return false;

            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentCheck";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = "invariant-post";
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step->data.intent_step.invariant_expr;
            if (!append_instruction(block, inst))
                return false;
        }
        if (step->data.intent_step.guard_expr != NULL) {
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentCheck";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = "guard";
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step->data.intent_step.guard_expr;
            if (!append_instruction(block, inst))
                return false;
        }
        if (step->data.intent_step.expect_expr != NULL) {
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentCheck";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = "expect";
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step->data.intent_step.expect_expr;
            if (!append_instruction(block, inst))
                return false;
        }
        if (step->data.intent_step.post_expr != NULL) {
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentCheck";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = "post";
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step->data.intent_step.post_expr;
            if (!append_instruction(block, inst))
                return false;
        }
        for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
            if (step->data.intent_step.on_exprs[j] == NULL)
                continue;
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentEval";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = "on";
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step->data.intent_step.on_exprs[j];
            if (!append_instruction(block, inst))
                return false;
        }
        if (step->data.intent_step.intent_expr != NULL) {
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentEval";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = "intent";
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step->data.intent_step.intent_expr;
            if (!append_instruction(block, inst))
                return false;
        }
        if (step->data.intent_step.on_expr_count == 0
            && step->data.intent_step.intent_expr == NULL) {
            for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
                if (step->data.intent_step.who_names[j] == NULL)
                    continue;
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_STMT;
                inst.name = "IntentDispatch";
                inst.slot_anchor = step->data.intent_step.name;
                inst.arg0 = step->data.intent_step.who_names[j];
                inst.arg1 = step->data.intent_step.name;
                inst.ast = step;
                if (!append_instruction(block, inst))
                    return false;
            }
        }
        for (size_t j = 0; j < step->data.intent_step.compensate_expr_count; j++) {
            if (step->data.intent_step.compensate_exprs[j] == NULL)
                continue;
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_STMT;
            inst.name = "IntentEval";
            inst.slot_anchor = step->data.intent_step.name;
            inst.arg0 = "compensate";
            inst.arg1 = step->data.intent_step.name;
            inst.ast = step->data.intent_step.compensate_exprs[j];
            if (!append_instruction(block, inst))
                return false;
        }
    }

    return true;
}

static bool
mir_collect_ssa_names(const MIRRoutine *routine, const char ***names_out, size_t *count_out)
{
    const char **names = NULL;
    size_t count = 0;

    if (names_out == NULL || count_out == NULL)
        return false;
    *names_out = NULL;
    *count_out = 0;
    if (routine == NULL)
        return true;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->source_local_def_count; j++) {
            if (!append_name_unique(&names, &count, block->source_local_defs[j])) {
                free((void *)names);
                return false;
            }
        }
        for (size_t j = 0; j < block->source_phi_node_count; j++) {
            if (!append_name_unique(&names, &count, block->source_phi_nodes[j].name)) {
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
        if (!append_name(&mir_block->renamed_locals, &mir_block->renamed_local_count, versioned)) {
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
                while (stmt_index < block->source_statement_count) {
                    ASTNode *stmt = block->source_statements[stmt_index];
                    if (stmt != NULL
                        && (stmt->type == AST_LET_DECL
                            || (stmt->type == AST_ASSIGNMENT
                                && stmt->data.assignment.target != NULL
                                && stmt->data.assignment.target->type == AST_IDENTIFIER))) {
                        break;
                    }
                    stmt_index++;
                }
                if (stmt_index < block->source_statement_count) {
                    ASTNode *stmt = block->source_statements[stmt_index];
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
                                    ? block->source_terminator_condition
                                    : block->source_terminator_value;
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

static bool
mir_postorder_visit(const MIRRoutine *routine,
                    size_t block_index,
                    bool *visited,
                    size_t *order,
                    size_t *order_count)
{
    const MIRBasicBlock *block;
    size_t succs[5];
    size_t succ_count = 0;

    if (routine == NULL || visited == NULL || order == NULL || order_count == NULL)
        return false;
    if (block_index >= routine->block_count)
        return true;
    if (visited[block_index])
        return true;

    visited[block_index] = true;
    block = &routine->blocks[block_index];

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
        if (!mir_postorder_visit(routine, succs[i], visited, order, order_count))
            return false;
    }

    order[(*order_count)++] = block_index;
    return true;
}

static bool
mir_build_liveness_postorder(const MIRRoutine *routine, size_t **order_out, size_t *count_out)
{
    bool *visited = NULL;
    size_t *order = NULL;
    size_t order_count = 0;

    if (order_out != NULL)
        *order_out = NULL;
    if (count_out != NULL)
        *count_out = 0;
    if (routine == NULL || order_out == NULL || count_out == NULL)
        return false;
    if (routine->block_count == 0)
        return true;

    visited = calloc(routine->block_count, sizeof(bool));
    order = calloc(routine->block_count, sizeof(size_t));
    if (visited == NULL || order == NULL) {
        free(visited);
        free(order);
        return false;
    }

    if (!mir_postorder_visit(routine, routine->entry_block, visited, order, &order_count)) {
        free(visited);
        free(order);
        return false;
    }

    for (size_t i = 0; i < routine->block_count; i++) {
        if (!visited[i] && !mir_postorder_visit(routine, i, visited, order, &order_count)) {
            free(visited);
            free(order);
            return false;
        }
    }

    free(visited);
    *order_out = order;
    *count_out = order_count;
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
    summary.ast_write_count = 0;
    summary.used_outside_def_block = false;
    summary.used_by_phi = false;
    summary.crosses_block_boundary = false;
    summary.has_ast_reassignment = false;

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
                if (block_id != summary->def_block) {
                    summary->used_outside_def_block = true;
                    summary->crosses_block_boundary = true;
                }
                if (inst->kind == MIR_INST_PHI) {
                    summary->used_by_phi = true;
                    summary->crosses_block_boundary = true;
                }
                if (block->is_cleanup)
                    summary->reaches_cleanup = true;
            }
        }
    }

    for (size_t i = 0; i < routine->value_summary_count; i++) {
        MIRValueSummary *summary = &routine->value_summaries[i];
        if (summary->live_in_block_count > 0
            || summary->live_out_block_count > 0
            || summary->used_outside_def_block
            || summary->used_by_phi) {
            summary->crosses_block_boundary = true;
        }
    }

    if (routine->hir_routine != NULL && routine->hir_routine->has_cfg) {
        for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
            const MIRBasicBlock *block = &routine->blocks[block_id];
            for (size_t stmt_id = 0; stmt_id < block->source_statement_count; stmt_id++) {
                const char *write_name = mir_stmt_def_name(block->source_statements[stmt_id]);
                if (write_name == NULL)
                    continue;
                for (size_t summary_id = 0; summary_id < routine->value_summary_count; summary_id++) {
                    MIRValueSummary *summary = &routine->value_summaries[summary_id];
                    if (summary->slot_anchor == NULL)
                        continue;
                    if (strcmp(summary->slot_anchor, write_name) != 0)
                        continue;
                    summary->ast_write_count++;
                    if (summary->ast_write_count > 1)
                        summary->has_ast_reassignment = true;
                }
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
    /* AST-backed DEFs (let / assignment) remain conservatively preserved.
     * Value-summary provenance is now richer, but loop-carried seed values
     * still are not distinguished well enough to reopen dead local removal
     * without changing runtime behavior. */
    if (inst->ast != NULL)
        return false;
    return summary->use_count == 0
           && summary->live_in_block_count == 0
           && summary->live_out_block_count == 0
           && !summary->reaches_cleanup;
}

static bool
mir_stmt_is_semantic_carrier(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT || inst->name == NULL)
        return false;

    if (strncmp(inst->name, "Intent", 6) == 0)
        return true;

    return false;
}

static bool
mir_call_is_whitelisted_pure_query(const char *callee)
{
    if (callee == NULL)
        return false;
    return strcmp(callee, "HasState") == 0
        || strcmp(callee, "HasLayer") == 0
        || strcmp(callee, "HasProjection") == 0
        || strcmp(callee, "HasZone") == 0
        || strcmp(callee, "HasZoneProjection") == 0
        || strcmp(callee, "HasZoneLayer") == 0
        || strcmp(callee, "HasZoneState") == 0
        || strcmp(callee, "ChannelLength") == 0
        || strcmp(callee, "ChannelCapacity") == 0
        || strcmp(callee, "ChannelSpace") == 0
        || strcmp(callee, "ChannelFull") == 0
        || strcmp(callee, "ChannelClosed") == 0;
}

static bool
mir_stmt_has_side_effect(const ASTNode *stmt)
{
    if (stmt == NULL)
        return false;
    if (stmt->type == AST_IF_STMT
        || stmt->type == AST_FOR_LOOP
        || stmt->type == AST_WHILE_LOOP
        || stmt->type == AST_MATCH_STMT
        || stmt->type == AST_DEFER_STMT
        || stmt->type == AST_ASYNC_BLOCK
        || stmt->type == AST_PARALLEL_BLOCK
        || stmt->type == AST_SELECT_STMT
        || stmt->type == AST_SPAWN_EXPR
        || stmt->type == AST_AWAIT_EXPR
        || stmt->type == AST_CHANNEL_SEND
        || stmt->type == AST_CHANNEL_RECV
        || stmt->type == AST_EVENT_SUBSCRIBE
        || stmt->type == AST_EVENT_UNSUBSCRIBE
        || stmt->type == AST_EVENT_INVOKE)
        return true;
    if (stmt->type == AST_ASSIGNMENT)
        return true;
    if (stmt->type == AST_LET_DECL)
        /* A let that reached MIR as a STMT (rather than being merged into
         * a DEF) still defines a binding whose downstream uses cannot be
         * discovered from MIR_INST_STMT alone. Keep it so the transpiler
         * emits the declaration + initializer side effects. */
        return true;
    if (stmt->type == AST_LET_DESTRUCTURE)
        /* Defines new bindings whose downstream uses DCE cannot see via
         * MIR_INST_STMT alone (the pre-declared SSA locals live in header).
         * Conservatively always keep destructuring statements. */
        return true;
    if (stmt->type == AST_BIND_STMT)
        return true;
    if (stmt->type == AST_UNSAFE_BLOCK)
        return true;
    if (stmt->type == AST_INTENT_STEP)
        return true;
    if (stmt->type == AST_WITH_STMT)
        return true;
    if (stmt->type == AST_CALL
        && stmt->data.call.callee != NULL
        && stmt->data.call.callee->type == AST_IDENTIFIER
        && stmt->data.call.callee->data.identifier.name != NULL) {
        const char *callee = stmt->data.call.callee->data.identifier.name;
        if (mir_call_is_whitelisted_pure_query(callee))
            return false;
        return true;
    }
    if (stmt->type == AST_CALL)
        return true;
    return false;
}

static bool
mir_instruction_is_dead_stmt(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT)
        return false;
    if (mir_stmt_is_semantic_carrier(inst))
        return false;
    if (inst->ast == NULL)
        return true;
    return !mir_stmt_has_side_effect(inst->ast);
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
            MIRInstruction *inst = &block->instructions[inst_id];
            if (mir_instruction_is_dead_value(routine, inst)
                || mir_instruction_is_dead_stmt(inst)) {
                if (!mir_remove_instruction(block, inst_id))
                    return false;
                routine->dce_removed_count++;
                changed = true;
            }
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
    size_t *order = NULL;
    size_t order_count = 0;

    if (routine == NULL)
        return false;
    if (!mir_collect_block_defs_uses(routine))
        return false;
    if (!mir_build_liveness_postorder(routine, &order, &order_count))
        return false;

    do {
        changed = false;
        for (size_t order_index = 0; order_index < order_count; order_index++) {
            size_t idx = order[order_index];
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

    free(order);
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
        /* Skip cleanup/rollback/invalidation blocks themselves and unreachable blocks */
        if (block->is_cleanup
            || !block->is_reachable)
            continue;
        /* All reachable non-cleanup blocks get a cleanup edge to the cleanup block */
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
    /* Rollback block gets cleanup edge pointing to cleanup block */
    if (routine->has_rollback_block) {
        MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];
        MIRBasicBlock *rollback = &routine->blocks[routine->rollback_block];
        size_t rollback_cleanup_target = routine->has_invalidation_block
            ? routine->invalidation_block
            : routine->cleanup_block;
        cleanup->rollback_succ = routine->rollback_block;
        cleanup->has_rollback_succ = true;
        if (!append_index_unique(&rollback->predecessors,
                                 &rollback->predecessor_count,
                                 cleanup->id)) {
            return false;
        }
        /* Rollback block also needs cleanup edge back to cleanup block */
        rollback->cleanup_succ = rollback_cleanup_target;
        rollback->has_cleanup_succ = true;
        routine->cleanup_edge_count++;
        if (!append_instruction(rollback,
                                (MIRInstruction){
                                    .id = routine->instruction_count++,
                                    .kind = MIR_INST_CLEANUP_EDGE,
                                    .name = "cleanup-edge-from-rollback",
                                    .slot_anchor = "cleanup",
                                    .arg0 = "cleanup",
                                    .arg1 = NULL,
                                    .ast = NULL,
                                })) {
            return false;
        }
        if (!append_index_unique(&routine->blocks[rollback_cleanup_target].predecessors,
                                 &routine->blocks[rollback_cleanup_target].predecessor_count,
                                 routine->rollback_block)) {
            return false;
        }
    }
    if (routine->has_invalidation_block) {
        MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];
        MIRBasicBlock *invalidation = &routine->blocks[routine->invalidation_block];
        cleanup->invalidation_succ = routine->invalidation_block;
        cleanup->has_invalidation_succ = true;
        if (!append_index_unique(&invalidation->predecessors,
                                 &invalidation->predecessor_count,
                                 cleanup->id)) {
            return false;
        }
        /* Invalidation block also needs cleanup edge back to cleanup block */
        invalidation->cleanup_succ = routine->cleanup_block;
        invalidation->has_cleanup_succ = true;
        routine->cleanup_edge_count++;
        if (!append_instruction(invalidation,
                                (MIRInstruction){
                                    .id = routine->instruction_count++,
                                    .kind = MIR_INST_CLEANUP_EDGE,
                                    .name = "cleanup-edge-from-invalidation",
                                    .slot_anchor = "cleanup",
                                    .arg0 = "cleanup",
                                    .arg1 = NULL,
                                    .ast = NULL,
                                })) {
            return false;
        }
        if (!append_index_unique(&cleanup->predecessors,
                                 &cleanup->predecessor_count,
                                 routine->invalidation_block)) {
            return false;
        }
    }
    if (routine->has_rollback_block && routine->has_invalidation_block) {
        MIRBasicBlock *rollback = &routine->blocks[routine->rollback_block];
        MIRBasicBlock *invalidation = &routine->blocks[routine->invalidation_block];
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

/* ---------------------------------------------------------------------------
 * mir_populate_stmt_instructions
 *
 * After SSA rename has placed DEF instructions, this pass walks each HIR
 * block's statement list and rebuilds the MIR block instruction array so that
 * general statements (function calls, expression statements, assignments to
 * non-identifier targets, etc.) are represented as MIR_INST_STMT instructions
 * interleaved with the existing DEF/PHI/BRANCH/RETURN instructions in the
 * correct source order.
 * -------------------------------------------------------------------------*/
static bool
mir_stmt_is_def_source(const ASTNode *stmt)
{
    if (stmt == NULL)
        return false;
    if (stmt->type == AST_LET_DECL)
        return true;
    if (stmt->type == AST_ASSIGNMENT
        && stmt->data.assignment.target != NULL
        && stmt->data.assignment.target->type == AST_IDENTIFIER)
        return true;
    return false;
}

static const char *
mir_stmt_def_name(const ASTNode *stmt)
{
    if (stmt == NULL)
        return NULL;
    if (stmt->type == AST_LET_DECL)
        return stmt->data.let_decl.name;
    if (stmt->type == AST_ASSIGNMENT
        && stmt->data.assignment.target != NULL
        && stmt->data.assignment.target->type == AST_IDENTIFIER) {
        return stmt->data.assignment.target->data.identifier.name;
    }
    return NULL;
}

static bool
mir_let_decl_requires_stmt_preservation(const ASTNode *stmt)
{
    ASTNode *init;
    ASTNode *callee;
    const char *name;

    if (stmt == NULL || stmt->type != AST_LET_DECL)
        return false;

    init = stmt->data.let_decl.initializer;
    if (init == NULL || init->type != AST_CALL)
        return false;

    callee = init->data.call.callee;
    if (callee == NULL
        || callee->type != AST_IDENTIFIER
        || callee->data.identifier.name == NULL) {
        return false;
    }

    name = callee->data.identifier.name;
    return strcmp(name, "Read") == 0
        || strcmp(name, "ViewRead") == 0
        || strcmp(name, "ViewWrite") == 0
        || strcmp(name, "Move") == 0;
}

static bool
mir_routine_has_def_for_name(const MIRRoutine *routine, const char *base_name)
{
    if (routine == NULL || base_name == NULL)
        return false;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
            const MIRInstruction *inst = &block->instructions[inst_id];
            const char *def_name = NULL;

            if (inst->kind != MIR_INST_DEF)
                continue;

            def_name = inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor;
            if (def_name != NULL && strcmp(def_name, base_name) == 0)
                return true;
        }
    }

    return false;
}

/* Check if a statement is control flow that the HIR has already lowered into
 * separate CFG blocks (and therefore should NOT be emitted as a STMT
 * instruction, because the CFG blocks handle it).
 *
 * Only skip statements whose control flow was actually expanded by the HIR
 * builder.  The HIR builder expands while loops and if statements into
 * separate CFG blocks, but for loops and some other constructs remain as
 * single statements inside a block.  We detect this by checking whether the
 * MIR block has successor edges — if it does, the HIR already created the
 * CFG, so we skip the original statement; if it doesn't, we keep it.
 *
 * AST_RETURN is always skipped because MIR already has MIR_INST_RETURN. */
static bool
mir_stmt_is_control_flow(const ASTNode *stmt, const MIRBasicBlock *mir_block)
{
    if (stmt == NULL)
        return true;
    if (stmt->type == AST_RETURN)
        return true;
    /* If/while that were CFG-split: the containing MIR block will have
     * successor edges from the HIR terminator.  When the block has
     * successors, skip the original statement node (the CFG handles it). */
    if ((stmt->type == AST_IF_STMT || stmt->type == AST_WHILE_LOOP)
        && (mir_block->has_succ_true || mir_block->has_succ_false))
        return true;
    return false;
}

static bool
mir_append_non_cfg_body_statements(MIRRoutine *routine, MIRBasicBlock *entry);

static bool
mir_populate_stmt_instructions(MIRRoutine *routine)
{
    bool has_stmt_inst = false;
    if (routine == NULL)
        return true;
    if (routine->hir_routine == NULL || !routine->hir_routine->has_cfg)
        return true;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        if (block->is_cleanup)
            continue;
        if (block->source_statement_count == 0)
            continue;

        /* Separate existing instructions into categories */
        MIRInstruction *old_insts = block->instructions;
        size_t old_count = block->instruction_count;
        bool *copied_flags = calloc(old_count, sizeof(bool));
        if (copied_flags == NULL)
            return false;

        /* Count max possible new STMT instructions (worst case: all
         * non-control-flow statements including let/assignment that
         * might not have matching DEFs). */
        size_t stmt_count = 0;
        for (size_t s = 0; s < block->source_statement_count; s++) {
            ASTNode *stmt = block->source_statements[s];
            if (mir_stmt_is_control_flow(stmt, block)) {
                if (block->has_succ_true || block->has_succ_false)
                    break;
                continue;
            }
            stmt_count++;
        }
        if (stmt_count == 0)
            continue;

        /* Allocate new instruction array */
        size_t new_cap = old_count + stmt_count;
        MIRInstruction *new_insts = calloc(new_cap, sizeof(MIRInstruction));
        if (new_insts == NULL)
            return false;
        size_t new_count = 0;

        /* Phase 1: copy PHI instructions */
        size_t old_cursor = 0;
        while (old_cursor < old_count && old_insts[old_cursor].kind == MIR_INST_PHI) {
            new_insts[new_count++] = old_insts[old_cursor];
            copied_flags[old_cursor++] = true;
        }

        /* Phase 2: interleave DEFs and STMTs based on HIR statement order */
        size_t def_cursor = old_cursor;
        /* Find the first DEF in remaining old instructions */
        while (def_cursor < old_count
               && old_insts[def_cursor].kind != MIR_INST_DEF)
            def_cursor++;

        /* Copy any RESOURCE_OP between PHIs and DEFs */
        for (size_t r = old_cursor; r < def_cursor; r++) {
            if (old_insts[r].kind == MIR_INST_RESOURCE_OP
                || old_insts[r].kind == MIR_INST_CLEANUP_EDGE) {
                new_insts[new_count++] = old_insts[r];
                copied_flags[r] = true;
            }
        }

        for (size_t s = 0; s < block->source_statement_count; s++) {
            ASTNode *stmt = block->source_statements[s];
            if (mir_stmt_is_control_flow(stmt, block)) {
                if (block->has_succ_true || block->has_succ_false)
                    break;
                continue;
            }
            if (mir_assignment_requires_stmt_preservation(routine->ast,
                                                          block->source_statements,
                                                          block->source_statement_count,
                                                          s,
                                                          stmt)) {
                MIRInstruction inst;
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_STMT;
                inst.name = "stmt";
                inst.ast = stmt;
                new_insts[new_count++] = inst;
                continue;
            }
            if (stmt != NULL
                && stmt->type == AST_LET_DECL
                && mir_let_decl_requires_stmt_preservation(stmt)) {
                MIRInstruction inst;
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_STMT;
                inst.name = "stmt";
                inst.ast = stmt;
                new_insts[new_count++] = inst;
                continue;
            }
            if (mir_stmt_is_def_source(stmt)) {
                const char *stmt_name = mir_stmt_def_name(stmt);
                /* Find the next DEF from old instructions */
                size_t saved_cursor = def_cursor;
                while (def_cursor < old_count
                       && old_insts[def_cursor].kind != MIR_INST_DEF)
                    def_cursor++;
                if (def_cursor < old_count) {
                    MIRInstruction def_inst = old_insts[def_cursor];
                    const char *def_name = def_inst.arg0 != NULL
                        ? def_inst.arg0
                        : def_inst.slot_anchor;
                    if (stmt_name == NULL || def_name == NULL
                        || strcmp(stmt_name, def_name) != 0) {
                        bool owned_by_later_def =
                            stmt_name != NULL
                            && stmt != NULL
                            && stmt->type == AST_LET_DECL
                            && mir_routine_has_def_for_name(routine, stmt_name);

                        def_cursor = saved_cursor;
                        if (owned_by_later_def) {
                            /* The CFG/SSA path already materializes this
                             * binding in another block. Do not resurrect the
                             * original source let/assignment as a fallback
                             * STMT here, or C/LLVM backends will emit the
                             * declaration twice (plain AST stmt + SSA DEF). */
                            continue;
                        }
                        memset(&def_inst, 0, sizeof(def_inst));
                        def_inst.id = routine->instruction_count++;
                        def_inst.kind = MIR_INST_STMT;
                        def_inst.name = "stmt";
                        def_inst.ast = stmt;
                        new_insts[new_count++] = def_inst;
                        continue;
                    }
                    /* Attach the full statement AST so LLVM emitter can
                     * extract both the type annotation and the initializer. */
                    if (def_inst.ast == NULL)
                        def_inst.ast = stmt;
                    new_insts[new_count++] = def_inst;
                    copied_flags[def_cursor] = true;
                    def_cursor++;
                } else {
                    /* No matching DEF (SSA had no local_defs for this var).
                     * Emit the let/assignment as a regular STMT so it still
                     * generates code. */
                    if (stmt_name != NULL
                        && stmt != NULL
                        && stmt->type == AST_LET_DECL
                        && mir_routine_has_def_for_name(routine, stmt_name)) {
                        def_cursor = saved_cursor;
                        continue;
                    }
                    def_cursor = saved_cursor;
                    MIRInstruction inst;
                    memset(&inst, 0, sizeof(inst));
                    inst.id = routine->instruction_count++;
                    inst.kind = MIR_INST_STMT;
                    inst.name = "stmt";
                    inst.ast = stmt;
                    new_insts[new_count++] = inst;
                }
            } else {
                /* Create new STMT instruction */
                MIRInstruction inst;
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_STMT;
                inst.name = "stmt";
                inst.ast = stmt;
                new_insts[new_count++] = inst;
            }
        }

        /* Copy remaining RESOURCE_OP / CLEANUP_EDGE after DEFs (preserve order) */
        for (size_t r = 0; r < old_count; r++) {
            if (old_insts[r].kind == MIR_INST_RESOURCE_OP
                || old_insts[r].kind == MIR_INST_CLEANUP_EDGE) {
                if (copied_flags[r])
                    continue;
                new_insts[new_count++] = old_insts[r];
                copied_flags[r] = true;
            }
        }

        /* Phase 3: copy terminators */
        for (size_t t = 0; t < old_count; t++) {
            if (old_insts[t].kind == MIR_INST_BRANCH
                || old_insts[t].kind == MIR_INST_RETURN) {
                new_insts[new_count++] = old_insts[t];
            }
        }

        /* Replace block's instruction array */
        free(copied_flags);
        free(old_insts);
        block->instructions = new_insts;
        block->instruction_count = new_count;
    }

    for (size_t block_id = 0; block_id < routine->block_count && !has_stmt_inst; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t i = 0; i < block->instruction_count; i++) {
            if (block->instructions[i].kind == MIR_INST_STMT) {
                has_stmt_inst = true;
                break;
            }
        }
    }

    return true;
}

static bool
mir_append_non_cfg_body_statements(MIRRoutine *routine, MIRBasicBlock *entry)
{
    ASTNode *func_decl;
    ASTNode *body;
    ASTNode **statements = NULL;
    size_t statement_count = 0;

    if (routine == NULL || entry == NULL || routine->ast == NULL)
        return true;

    func_decl = routine->ast;
    if (func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.body == NULL) {
        return true;
    }

    body = func_decl->data.func_decl.body;
    if (body->type != AST_BLOCK)
        return append_instruction(entry, (MIRInstruction){
            .id = routine->instruction_count++,
            .kind = MIR_INST_STMT,
            .name = "stmt",
            .ast = body,
        });

    if (entry->source_statements != NULL && entry->source_statement_count > 0) {
        statements = entry->source_statements;
        statement_count = entry->source_statement_count;
    } else {
        statements = body->data.block.statements;
        statement_count = body->data.block.count;
    }

    for (size_t i = 0; i < statement_count; i++) {
        ASTNode *stmt = statements[i];
        bool matched_def = false;
        if (stmt == NULL)
            continue;
        if (mir_stmt_is_control_flow(stmt, entry))
            continue;
        if (mir_assignment_requires_stmt_preservation(func_decl,
                                                      statements,
                                                      statement_count,
                                                      i,
                                                      stmt)) {
            if (!append_instruction(entry, (MIRInstruction){
                    .id = routine->instruction_count++,
                    .kind = MIR_INST_STMT,
                    .name = "stmt",
                    .ast = stmt,
                })) {
                return false;
            }
            continue;
        }
        if (stmt->type == AST_LET_DECL
            && mir_let_decl_requires_stmt_preservation(stmt)) {
            if (!append_instruction(entry, (MIRInstruction){
                    .id = routine->instruction_count++,
                    .kind = MIR_INST_STMT,
                    .name = "stmt",
                    .ast = stmt,
                })) {
                return false;
            }
            continue;
        }
        if (mir_stmt_is_def_source(stmt)) {
            const char *stmt_name = mir_stmt_def_name(stmt);
            for (size_t j = 0; j < entry->instruction_count; j++) {
                MIRInstruction *inst = &entry->instructions[j];
                const char *def_name;
                if (inst->kind != MIR_INST_DEF)
                    continue;
                def_name = inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor;
                if (stmt_name == NULL || def_name == NULL
                    || strcmp(stmt_name, def_name) != 0) {
                    continue;
                }
                if (inst->ast == NULL)
                    inst->ast = stmt;
                matched_def = true;
                break;
            }
            if (matched_def)
                continue;
        }
        if (!append_instruction(entry, (MIRInstruction){
                .id = routine->instruction_count++,
                .kind = MIR_INST_STMT,
                .name = "stmt",
                .ast = stmt,
            })) {
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
    bool appended_non_cfg_intent_steps = false;

    if (routine == NULL || routine->block_count == 0)
        return true;

    rir_scope = routine->rir_scope;
    entry = &routine->blocks[routine->entry_block];
    rollback = routine->has_rollback_block ? &routine->blocks[routine->rollback_block] : NULL;
    invalidation = routine->has_invalidation_block ? &routine->blocks[routine->invalidation_block] : NULL;

    if (rir_scope == NULL)
        return true;

    if (routine->kind == MIR_SCOPE_INTENT
        && routine->hir_routine != NULL
        && !routine->hir_routine->has_cfg) {
        if (!mir_append_intent_step_instructions(routine, entry))
            return false;
        appended_non_cfg_intent_steps = true;
    }

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

    if (!appended_non_cfg_intent_steps && routine->kind == MIR_SCOPE_INTENT
        && routine->hir_routine != NULL
        && !routine->hir_routine->has_cfg) {
        if (!mir_append_intent_step_instructions(routine, entry))
            return false;
    } else if (routine->hir_routine != NULL
               && !routine->hir_routine->has_cfg) {
        if (!mir_append_non_cfg_body_statements(routine, entry))
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
        block.source_ast = NULL;
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
        block.source_hir_block = src;
        block.source_terminator_condition = src->terminator_condition;
        block.source_terminator_value = src->terminator_value;
        block.source_terminator_kind = src->terminator_kind;
        if (src->statement_count > 0)
            block.source_ast = src->statements[0];
        else if (src->terminator_condition != NULL)
            block.source_ast = src->terminator_condition;
        else if (src->terminator_value != NULL)
            block.source_ast = src->terminator_value;
        else
            block.source_ast = NULL;
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
        if (!mir_copy_ast_nodes(&block.source_statements,
                                &block.source_statement_count,
                                src->statements,
                                src->statement_count)
            || !mir_copy_names(&block.source_local_defs,
                               &block.source_local_def_count,
                               src->local_defs,
                               src->local_def_count)
            || !copy_indices(&block.source_dom_tree_children,
                             &block.source_dom_tree_child_count,
                             src->dom_tree_children,
                             src->dom_tree_child_count)
            || !mir_copy_phi_nodes(&block.source_phi_nodes,
                                   &block.source_phi_node_count,
                                   src->phi_nodes,
                                   src->phi_node_count)) {
            free(block.predecessors);
            free(block.source_statements);
            free((void *)block.source_local_defs);
            free(block.source_dom_tree_children);
            if (block.source_phi_nodes != NULL) {
                for (size_t j = 0; j < block.source_phi_node_count; j++)
                    free(block.source_phi_nodes[j].incoming_predecessors);
            }
            free(block.source_phi_nodes);
            return false;
        }
        if (!append_block(routine, block))
            return false;
    }

    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        if (!mir_add_phi_placeholders(routine, &routine->blocks[i]))
            return false;
        if (!mir_add_terminator_instruction(routine, &routine->blocks[i]))
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

#include "mir_decl_headers.h"
#include "mir_lower_public_api.h"
#include "mir_cfg_contract_validate.h"
#include "mir_public_surface.h"
#include "mir_abi_layout.h"
