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
            return ast_member_name(node);
        case AST_TYPE:
            return ast_type_name(node);
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
                                  ast_intent_step_name(step),
                                  check_name,
                                  ast_intent_step_name(step),
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
                                  ast_intent_step_name(step),
                                  eval_name,
                                  ast_intent_step_name(step),
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
    ASTNode **involves_nodes;
    size_t involve_count;

    involves_nodes = ast_intent_decl_involves(intent, &involve_count);
    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *involves = involves_nodes[i];
        ASTNode *subject_type;
        const char *alias = NULL;
        const char *type_name = NULL;

        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        alias = ast_intent_involves_alias(involves);
        subject_type = ast_intent_involves_subject_type(involves);
        if (subject_type != NULL && subject_type->type == AST_TYPE) {
            type_name = ast_type_name(subject_type);
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
    const char *step_name = ast_intent_step_name(step) != NULL
        ? ast_intent_step_name(step)
        : "intent.step";

    if (!mir_append_intent_stmt(routine, block, step_name, NULL, NULL, NULL, step))
        return false;
    if (!mir_append_intent_stmt(routine,
                                block,
                                "IntentStep",
                                ast_intent_step_name(step),
                                step_name,
                                ast_intent_step_name(step),
                                step)) {
        return false;
    }
    if (ast_intent_step_where_type(step) != NULL
        && ast_intent_step_where_type(step)->type == AST_TYPE
        && ast_type_name(ast_intent_step_where_type(step)) != NULL) {
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentZoneWhere",
                                    ast_intent_step_name(step),
                                    ast_type_name(ast_intent_step_where_type(step)),
                                    ast_intent_step_name(step),
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

    if (ast_intent_step_using_expr(step) != NULL
        && ast_intent_step_using_expr(step)->type == AST_IDENTIFIER) {
        effective_zone_alias = ast_intent_step_using_expr(step)->data.identifier.name;
    } else if (ast_intent_step_transfer_to_alias(step) != NULL) {
        effective_zone_alias = ast_intent_step_transfer_to_alias(step);
    }
    if (effective_zone_alias != NULL) {
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentZoneAlias",
                                    ast_intent_step_name(step),
                                    effective_zone_alias,
                                    ast_intent_step_name(step),
                                    step)) {
            return false;
        }
    }

    if (ast_intent_step_using_expr(step) != NULL)
        invalidation_target = mir_intent_node_name(ast_intent_step_using_expr(step));
    else if (ast_intent_step_transfer_to_alias(step) != NULL)
        invalidation_target = ast_intent_step_transfer_to_alias(step);
    else if (ast_intent_step_transfer_from_alias(step) != NULL)
        invalidation_target = ast_intent_step_transfer_from_alias(step);
    if (invalidation_target != NULL) {
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentInvalidationTarget",
                                    ast_intent_step_name(step),
                                    invalidation_target,
                                    ast_intent_step_name(step),
                                    step)) {
            return false;
        }
    }

    if (ast_intent_step_transfer_from_alias(step) != NULL) {
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentZoneFrom",
                                    ast_intent_step_name(step),
                                    ast_intent_step_transfer_from_alias(step),
                                    ast_intent_step_name(step),
                                    step)) {
            return false;
        }
    }
    return true;
}

static bool
mir_append_intent_step_authority(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *step)
{
    for (size_t j = 0; j < ast_intent_step_who_count(step); j++) {
        if (ast_intent_step_who_names(step, NULL)[j] == NULL)
            continue;
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentWho",
                                    ast_intent_step_name(step),
                                    ast_intent_step_who_names(step, NULL)[j],
                                    ast_intent_step_name(step),
                                    step)) {
            return false;
        }
    }
    for (size_t j = 0; j < ast_intent_step_authorized_by_count(step); j++) {
        if (ast_intent_step_authorized_by(step, NULL)[j] == NULL)
            continue;
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentAuthorizedBy",
                                    ast_intent_step_name(step),
                                    ast_intent_step_authorized_by(step, NULL)[j],
                                    ast_intent_step_name(step),
                                    step)) {
            return false;
        }
    }
    if (ast_intent_step_causes_effect(step) != NULL) {
        if (!mir_append_intent_stmt(routine,
                                    block,
                                    "IntentCauses",
                                    ast_intent_step_name(step),
                                    ast_intent_step_causes_effect(step),
                                    ast_intent_step_name(step),
                                    step)) {
            return false;
        }
    }
    return true;
}

static bool
mir_append_intent_step_checks(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *step)
{
    if (!mir_append_intent_check(routine, block, step, "pre", ast_intent_step_pre_expr(step)))
        return false;
    if (!mir_append_intent_check(routine, block, step, "invariant-pre", ast_intent_step_invariant_expr(step)))
        return false;
    if (!mir_append_intent_check(routine, block, step, "invariant-post", ast_intent_step_invariant_expr(step)))
        return false;
    if (!mir_append_intent_check(routine, block, step, "guard", ast_intent_step_guard_expr(step)))
        return false;
    if (!mir_append_intent_check(routine, block, step, "expect", ast_intent_step_expect_expr(step)))
        return false;
    return mir_append_intent_check(routine, block, step, "post", ast_intent_step_post_expr(step));
}

static bool
mir_append_intent_step_eval(MIRRoutine *routine, MIRBasicBlock *block, ASTNode *step)
{
    for (size_t j = 0; j < ast_intent_step_on_expr_count(step); j++) {
        if (!mir_append_intent_eval(routine, block, step, "on", ast_intent_step_on_exprs(step, NULL)[j]))
            return false;
    }
    if (!mir_append_intent_eval(routine, block, step, "intent", ast_intent_step_intent_expr(step)))
        return false;

    if (ast_intent_step_on_expr_count(step) == 0
        && ast_intent_step_intent_expr(step) == NULL) {
        for (size_t j = 0; j < ast_intent_step_who_count(step); j++) {
            if (ast_intent_step_who_names(step, NULL)[j] == NULL)
                continue;
            if (!mir_append_intent_stmt(routine,
                                        block,
                                        "IntentDispatch",
                                        ast_intent_step_name(step),
                                        ast_intent_step_who_names(step, NULL)[j],
                                        ast_intent_step_name(step),
                                        step)) {
                return false;
            }
        }
    }

    for (size_t j = 0; j < ast_intent_step_compensate_expr_count(step); j++) {
        if (!mir_append_intent_eval(routine,
                                    block,
                                    step,
                                    "compensate",
                                    ast_intent_step_compensate_exprs(step, NULL)[j])) {
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

    {
        ASTNode **steps;
        size_t step_count;

        steps = ast_intent_decl_steps(intent, &step_count);
        for (size_t i = 0; i < step_count; i++) {
            ASTNode *step = steps[i];
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
    }
    return true;
}
