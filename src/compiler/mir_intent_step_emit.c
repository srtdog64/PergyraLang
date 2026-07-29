#include "mir_intent_step_emit.h"
#include "mir_intent.h"

#include "mir_abi_layout.h"
#include "mir_base_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/arena.h"
#include "../common/string_compat.h"
#include "parser/ast_api.h"

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
            return ast_identifier_name(node);
        case AST_MEMBER_ACCESS:
            return ast_member_name(node);
        case AST_TYPE:
            return ast_type_name(node);
        default:
            return NULL;
    }
}

static const DIRIntentStep *
mir_intent_dir_step(const DIRIntentInfo *info,
                    const ASTNode *step,
                    size_t step_index)
{
    const DIRIntentStep *dir_step;

    if (info == NULL || step == NULL || step->type != AST_INTENT_STEP
        || step_index >= info->step_count) {
        return NULL;
    }
    dir_step = &info->steps[step_index];
    if (dir_step->index != step_index || dir_step->ast == NULL
        || ast_node_stable_id(dir_step->ast) != ast_node_stable_id(step)
        || dir_step->on_expr_count != ast_intent_step_on_expr_count(step)
        || (dir_step->outcome_binding_name == NULL)
            != (ast_intent_step_outcome_binding_name(step) == NULL)
        || (dir_step->outcome_binding_type_name == NULL)
            != (ast_intent_step_outcome_binding_type_name(step) == NULL)
        || (dir_step->outcome_binding_name != NULL
            && strcmp(dir_step->outcome_binding_name,
                ast_intent_step_outcome_binding_name(step)) != 0)
        || (dir_step->outcome_binding_type_name != NULL
            && strcmp(dir_step->outcome_binding_type_name,
                ast_intent_step_outcome_binding_type_name(step)) != 0)
        || dir_step->outcome_action_decl_syntax_id
            != ast_intent_step_outcome_action_decl_syntax_id(step)) {
        return NULL;
    }
    return dir_step;
}

static bool
mir_intent_bind_outcome_result(MIRRoutine *routine,
                               MIRInstruction *inst,
                               const DIRIntentStep *step)
{
    if (routine == NULL || inst == NULL || step == NULL
        || step->outcome_binding_name == NULL
        || step->outcome_binding_name[0] == '\0'
        || step->outcome_binding_type_name == NULL
        || step->outcome_binding_type_name[0] == '\0'
        || strcmp(step->outcome_binding_type_name, "Void") == 0
        || step->outcome_action_decl_syntax_id == 0
        || step->on_expr_count != 1) {
        return false;
    }
    inst->result_name = pergyra_strdup(step->outcome_binding_name);
    inst->abi_type_name = pgy_arena_strdup(
        &routine->scratch, step->outcome_binding_type_name);
    if (inst->result_name == NULL || inst->abi_type_name == NULL) {
        free((void *)inst->result_name);
        inst->result_name = NULL;
        return false;
    }
    inst->type_layout = mir_abi_lookup(inst->abi_type_name);
    return true;
}

static bool
mir_intent_bind_outcome_fields(MIRRoutine *routine,
                               MIRInstruction *inst,
                               const DIRIntentStep *step)
{
    char action_id[16];
    int written;

    if (!mir_intent_bind_outcome_result(routine, inst, step))
        return false;
    written = snprintf(action_id, sizeof(action_id), "%u",
        (unsigned)step->outcome_action_decl_syntax_id);
    if (written <= 0 || (size_t)written >= sizeof(action_id)) {
        free((void *)inst->result_name);
        inst->result_name = NULL;
        return false;
    }
    inst->slot_anchor = pgy_arena_strdup(
        &routine->scratch, step->outcome_binding_name);
    inst->arg0 = pgy_arena_strdup(&routine->scratch, action_id);
    inst->arg1 = pgy_arena_strdup(&routine->scratch, step->name);
    if (inst->slot_anchor == NULL || inst->arg0 == NULL
        || inst->arg1 == NULL) {
        free((void *)inst->result_name);
        inst->result_name = NULL;
        return false;
    }
    return true;
}

static bool
mir_append_intent_outcome_binding(MIRRoutine *routine,
                                  MIRBasicBlock *block,
                                  ASTNode *step,
                                  const DIRIntentStep *dir_step)
{
    MIRInstruction inst;

    if (dir_step == NULL || dir_step->outcome_binding_name == NULL)
        return true;
    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_STMT;
    inst.name = "IntentOutcomeBinding";
    inst.ast = step;
    mir_instruction_capture_source_provenance(&inst, step);
    if (!mir_intent_bind_outcome_fields(routine, &inst, dir_step))
        return false;
    if (!mir_intent_commit_instruction(routine, block, &inst)) {
        free((void *)inst.result_name);
        return false;
    }
    return true;
}

bool
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
    mir_instruction_capture_source_provenance(&inst, ast);
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
                       ASTNode *expr,
                       const DIRIntentStep *dir_step)
{
    MIRInstruction inst;

    if (expr == NULL)
        return true;
    memset(&inst, 0, sizeof(inst));
    inst.kind = MIR_INST_STMT;
    inst.name = "IntentEval";
    inst.slot_anchor = ast_intent_step_name(step);
    inst.arg0 = eval_name;
    inst.arg1 = ast_intent_step_name(step);
    inst.ast = expr;
    inst.expr0 = expr;
    inst.has_source_statement_stable_id = true;
    inst.source_statement_stable_id = ast_node_stable_id(step);
    mir_instruction_capture_source_provenance(&inst, expr);
    if (eval_name != NULL && strcmp(eval_name, "on") == 0
        && dir_step != NULL && dir_step->outcome_binding_name != NULL) {
        if (!mir_intent_bind_outcome_result(routine, &inst, dir_step))
            return false;
    }
    if (!mir_intent_commit_instruction(routine, block, &inst)) {
        free((void *)inst.result_name);
        return false;
    }
    return true;
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
            mir_instruction_capture_source_provenance(&inst, src->ast);
            if (!mir_intent_commit_instruction(routine, block, &inst))
                return false;
            routine->cleanup_instruction_count++;
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
        effective_zone_alias = ast_identifier_name(ast_intent_step_using_expr(step));
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
mir_append_intent_step_eval(MIRRoutine *routine,
                            MIRBasicBlock *block,
                            ASTNode *step,
                            const DIRIntentStep *dir_step)
{
    const char *ast_binding_name =
        ast_intent_step_outcome_binding_name(step);

    if ((ast_binding_name == NULL)
            != (dir_step == NULL
                || dir_step->outcome_binding_name == NULL)) {
        return false;
    }
    if (ast_binding_name != NULL
        && !mir_append_intent_outcome_binding(
            routine, block, step, dir_step)) {
        return false;
    }
    for (size_t j = 0; j < ast_intent_step_on_expr_count(step); j++) {
        if (!mir_append_intent_eval(routine, block, step, "on",
                ast_intent_step_on_exprs(step, NULL)[j], dir_step))
            return false;
    }
    if (!mir_append_intent_eval(routine, block, step, "intent",
            ast_intent_step_intent_expr(step), NULL))
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
                                    ast_intent_step_compensate_exprs(step, NULL)[j],
                                    NULL)) {
            return false;
        }
    }
    return true;
}

bool
mir_append_intent_step_facts(MIRRoutine *routine,
                             MIRBasicBlock *block,
                             const DIRIntentInfo *dir_intent,
                             ASTNode *step,
                             size_t step_index)
{
    const DIRIntentStep *dir_step =
        mir_intent_dir_step(dir_intent, step, step_index);

    if (step == NULL || step->type != AST_INTENT_STEP)
        return true;
    if (dir_intent != NULL && dir_step == NULL)
        return false;
    if (ast_intent_step_outcome_binding_name(step) != NULL
        && dir_step == NULL) {
        return false;
    }
    if (!mir_append_intent_step_header(routine, block, step))
        return false;
    if (!mir_append_intent_step_zone(routine, block, step))
        return false;
    if (!mir_append_intent_step_authority(routine, block, step))
        return false;
    if (!mir_append_intent_step_checks(routine, block, step))
        return false;
    return mir_append_intent_step_eval(routine, block, step, dir_step);
}
