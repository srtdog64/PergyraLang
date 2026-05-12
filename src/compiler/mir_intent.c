#include "mir_intent.h"

#include "mir_base_helpers.h"

#include <stdlib.h>
#include <string.h>

static bool
mir_intent_append_instruction(MIRBasicBlock *block, MIRInstruction inst)
{
    return append_instruction(block, inst);
}

static bool
mir_intent_commit_instruction(MIRRoutine *routine, MIRBasicBlock *block, MIRInstruction *inst)
{
    if (routine == NULL || block == NULL || inst == NULL)
        return false;
    inst->id = routine->instruction_count;
    if (!mir_intent_append_instruction(block, *inst))
        return false;
    routine->instruction_count++;
    return true;
}

static const char *
mir_intent_node_name(ASTNode *node)
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
mir_append_intent_stmt(MIRRoutine *routine,
                       MIRBasicBlock *block,
                       const char *name,
                       const char *slot_anchor,
                       const char *arg0,
                       const char *arg1,
                       ASTNode *ast)
{
    MIRInstruction inst;

    if (routine == NULL || block == NULL)
        return false;

    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_STMT;
    inst.name = name;
    inst.slot_anchor = slot_anchor;
    inst.arg0 = arg0;
    inst.arg1 = arg1;
    inst.ast = ast;
    if (name != NULL
        && (strcmp(name, "IntentCheck") == 0
            || strcmp(name, "IntentEval") == 0)) {
        inst.expr0 = ast;
    }
    return mir_intent_commit_instruction(routine, block, &inst);
}

static bool
mir_append_intent_check(MIRRoutine *routine,
                        MIRBasicBlock *block,
                        ASTNode *step,
                        const char *check_name,
                        ASTNode *expr)
{
    if (expr == NULL)
        return true;
    return mir_append_intent_stmt(routine,
                                  block,
                                  "IntentCheck",
                                  step->data.intent_step.name,
                                  check_name,
                                  step->data.intent_step.name,
                                  expr);
}

static bool
mir_append_intent_eval(MIRRoutine *routine,
                       MIRBasicBlock *block,
                       ASTNode *step,
                       const char *eval_name,
                       ASTNode *expr)
{
    if (expr == NULL)
        return true;
    return mir_append_intent_stmt(routine,
                                  block,
                                  "IntentEval",
                                  step->data.intent_step.name,
                                  eval_name,
                                  step->data.intent_step.name,
                                  expr);
}

bool
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
            inst.kind = MIR_INST_CLEANUP_EDGE;
            inst.name = "DetachInvalidation";
            inst.slot_anchor = src->arg0;
            inst.arg0 = src->arg0;
            inst.arg1 = src->arg1;
            inst.ast = src->ast;
            if (!mir_intent_commit_instruction(routine, block, &inst))
                return false;
            routine->cleanup_instruction_count++;
        }
    }

    return true;
}

static bool
mir_append_intent_participants(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *intent)
{
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
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
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentParticipant",
                                    routine->name,
                                    alias,
                                    type_name,
                                    involves)) {
            return false;
        }
    }
    return true;
}

static bool
mir_append_intent_step_header(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *step)
{
    const char *step_name = step->data.intent_step.name != NULL
        ? step->data.intent_step.name
        : "intent.step";

    if (!mir_append_intent_stmt(routine, block, step_name, NULL, NULL, NULL, step))
        return false;
    if (!mir_append_intent_stmt(routine,
                                block,
                                "IntentStep",
                                step->data.intent_step.name,
                                step_name,
                                step->data.intent_step.name,
                                step)) {
        return false;
    }
    if (step->data.intent_step.where_type != NULL
        && step->data.intent_step.where_type->type == AST_TYPE
        && step->data.intent_step.where_type->data.type.name != NULL) {
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentZoneWhere",
                                    step->data.intent_step.name,
                                    step->data.intent_step.where_type->data.type.name,
                                    step->data.intent_step.name,
                                    step)) {
            return false;
        }
    }
    return true;
}

static bool
mir_append_intent_step_zone(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *step)
{
    const char *effective_zone_alias = NULL;
    const char *invalidation_target = NULL;

    if (step->data.intent_step.using_expr != NULL
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
        effective_zone_alias = step->data.intent_step.using_expr->data.identifier.name;
    } else if (step->data.intent_step.transfer_to_alias != NULL) {
        effective_zone_alias = step->data.intent_step.transfer_to_alias;
    }
    if (effective_zone_alias != NULL) {
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentZoneAlias",
                                    step->data.intent_step.name,
                                    effective_zone_alias,
                                    step->data.intent_step.name,
                                    step)) {
            return false;
        }
    }

    if (step->data.intent_step.using_expr != NULL)
        invalidation_target = mir_intent_node_name(step->data.intent_step.using_expr);
    else if (step->data.intent_step.transfer_to_alias != NULL)
        invalidation_target = step->data.intent_step.transfer_to_alias;
    else if (step->data.intent_step.transfer_from_alias != NULL)
        invalidation_target = step->data.intent_step.transfer_from_alias;
    if (invalidation_target != NULL) {
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentInvalidationTarget",
                                    step->data.intent_step.name,
                                    invalidation_target,
                                    step->data.intent_step.name,
                                    step)) {
            return false;
        }
    }

    if (step->data.intent_step.transfer_from_alias != NULL) {
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentZoneFrom",
                                    step->data.intent_step.name,
                                    step->data.intent_step.transfer_from_alias,
                                    step->data.intent_step.name,
                                    step)) {
            return false;
        }
    }
    return true;
}

static bool
mir_append_intent_step_authority(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *step)
{
    for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
        if (step->data.intent_step.who_names[j] == NULL)
            continue;
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentWho",
                                    step->data.intent_step.name,
                                    step->data.intent_step.who_names[j],
                                    step->data.intent_step.name,
                                    step)) {
            return false;
        }
    }
    for (size_t j = 0; j < step->data.intent_step.authorized_by_count; j++) {
        if (step->data.intent_step.authorized_by[j] == NULL)
            continue;
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentAuthorizedBy",
                                    step->data.intent_step.name,
                                    step->data.intent_step.authorized_by[j],
                                    step->data.intent_step.name,
                                    step)) {
            return false;
        }
    }
    if (step->data.intent_step.causes_effect != NULL) {
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentCauses",
                                    step->data.intent_step.name,
                                    step->data.intent_step.causes_effect,
                                    step->data.intent_step.name,
                                    step)) {
            return false;
        }
    }
    return true;
}

static bool
mir_append_intent_step_checks(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *step)
{
    if (!mir_append_intent_check(routine, block, step, "pre", step->data.intent_step.pre_expr))
        return false;
    if (!mir_append_intent_check(routine, block, step, "invariant-pre", step->data.intent_step.invariant_expr))
        return false;
    if (!mir_append_intent_check(routine, block, step, "invariant-post", step->data.intent_step.invariant_expr))
        return false;
    if (!mir_append_intent_check(routine, block, step, "guard", step->data.intent_step.guard_expr))
        return false;
    if (!mir_append_intent_check(routine, block, step, "expect", step->data.intent_step.expect_expr))
        return false;
    return mir_append_intent_check(routine, block, step, "post", step->data.intent_step.post_expr);
}

static bool
mir_append_intent_step_eval(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *step)
{
    for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
        if (!mir_append_intent_eval(routine, block, step, "on", step->data.intent_step.on_exprs[j]))
            return false;
    }
    if (!mir_append_intent_eval(routine, block, step, "intent", step->data.intent_step.intent_expr))
        return false;

    if (step->data.intent_step.on_expr_count == 0
        && step->data.intent_step.intent_expr == NULL) {
        for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
            if (step->data.intent_step.who_names[j] == NULL)
                continue;
            if (!mir_append_intent_stmt(routine,
                                        block,
                                        "IntentDispatch",
                                        step->data.intent_step.name,
                                        step->data.intent_step.who_names[j],
                                        step->data.intent_step.name,
                                        step)) {
                return false;
            }
        }
    }

    for (size_t j = 0; j < step->data.intent_step.compensate_expr_count; j++) {
        if (!mir_append_intent_eval(routine,
                                    block,
                                    step,
                                    "compensate",
                                    step->data.intent_step.compensate_exprs[j])) {
            return false;
        }
    }
    return true;
}

bool
mir_append_intent_step_instructions(MIRRoutine *routine, MIRBasicBlock *block)
{
    ASTNode *intent;

    if (routine == NULL || block == NULL || routine->hir_routine == NULL)
        return false;
    if (routine->hir_routine->ast == NULL || routine->hir_routine->ast->type != AST_INTENT_DECL)
        return true;

    intent = routine->hir_routine->ast;
    if (!mir_append_intent_participants(routine, block, intent))
        return false;

    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (!mir_append_intent_step_header(routine, block, step))
            return false;
        if (!mir_append_intent_step_zone(routine, block, step))
            return false;
        if (!mir_append_intent_step_authority(routine, block, step))
            return false;
        if (!mir_append_intent_step_checks(routine, block, step))
            return false;
        if (!mir_append_intent_step_eval(routine, block, step))
            return false;
    }
    return true;
}
