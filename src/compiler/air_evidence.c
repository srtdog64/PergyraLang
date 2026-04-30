#include "air_internal.h"

#include "../semantic/semantic.h"

#include <string.h>

static bool
air_boundary_authority_matches(const AIRBoundaryNode *boundary, const char *authority_name)
{
    if (boundary == NULL || authority_name == NULL)
        return false;
    for (size_t i = 0; i < boundary->authority_name_count; i++) {
        if (air_name_matches(boundary->authority_names[i], authority_name))
            return true;
    }
    return false;
}

static bool
air_ast_contains_node(const ASTNode *container, const ASTNode *needle);

static bool
air_hir_routine_matches_boundary(const HIRRoutine *routine,
                                 const AIRIntentNode *intent,
                                 const AIRBoundaryNode *boundary)
{
    if (routine == NULL || intent == NULL || boundary == NULL)
        return false;
    return air_name_matches(routine->owner_name, intent->intent_owner)
        || air_name_matches(routine->name, intent->step_name)
        || air_name_matches(routine->name, intent->intent_owner)
        || air_name_matches(routine->owner_name, boundary->source_name)
        || air_name_matches(routine->name, boundary->source_name);
}

static bool
air_hir_cfg_contains_boundary_ast(const HIRRoutine *routine, const AIRBoundaryNode *boundary)
{
    if (routine == NULL || boundary == NULL || !routine->has_cfg)
        return false;
    if (boundary->ast == NULL)
        return true;
    for (size_t i = 0; i < routine->cfg.block_count; i++) {
        const HIRBasicBlock *block = &routine->cfg.blocks[i];
        if (block == NULL)
            continue;
        for (size_t j = 0; j < block->statement_count; j++) {
            if (block->statements[j] == boundary->ast
                || air_ast_contains_node(block->statements[j], boundary->ast))
                return true;
        }
        if (block->terminator_condition == boundary->ast
            || air_ast_contains_node(block->terminator_condition, boundary->ast))
            return true;
        if (block->terminator_value == boundary->ast
            || air_ast_contains_node(block->terminator_value, boundary->ast))
            return true;
        if (block->pin_block_ast == boundary->ast)
            return true;
    }
    return false;
}

bool
air_collect_hir_evidence(AIRProgram *air, const HIRProgram *hir, char **error_message)
{
    if (air == NULL || hir == NULL)
        return true;
    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        for (size_t j = 0; j < air->boundary_count; j++) {
            AIRBoundaryNode *boundary = &air->boundaries[j];
            const AIRIntentNode *intent = &air->intents[boundary->intent_index];
            if (air_hir_routine_matches_boundary(routine, intent, boundary)) {
                const char *routine_name = routine->name != NULL
                    ? routine->name
                    : routine->owner_name;
                if (!air_assign_first_owned_name(air,
                                                 &boundary->hir_routine_evidence_name,
                                                 routine_name,
                                                 error_message,
                                                 "HIR routine")) {
                    return false;
                }
                boundary->has_hir_routine_evidence = true;
                air->hir_routine_evidence_count++;
                if (!air_append_evidence_node(air,
                                              AIR_EVIDENCE_HIR_ROUTINE,
                                              j,
                                              routine_name,
                                              boundary->source_name,
                                              error_message)) {
                    return false;
                }
                if (air_hir_cfg_contains_boundary_ast(routine, boundary)) {
                    boundary->has_hir_cfg_evidence = true;
                    air->hir_cfg_evidence_count++;
                    if (!air_append_evidence_node(air,
                                                  AIR_EVIDENCE_HIR_CFG,
                                                  j,
                                                  routine_name,
                                                  boundary->source_name,
                                                  error_message)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

static bool
air_rir_scope_matches_boundary(const RIRScope *scope, const AIRBoundaryNode *boundary)
{
    if (scope == NULL || boundary == NULL)
        return false;
    if (!(scope->kind == RIR_SCOPE_INTENT
          || scope->kind == RIR_SCOPE_ZONE
          || scope->kind == RIR_SCOPE_WORLD)) {
        return false;
    }
    if (boundary->kind == AIR_BOUNDARY_PARALLEL
        || boundary->kind == AIR_BOUNDARY_IO
        || boundary->kind == AIR_BOUNDARY_CHANNEL) {
        return air_name_matches(scope->name, boundary->source_name);
    }
    if (boundary->kind == AIR_BOUNDARY_WORLD) {
        return air_name_matches(scope->name, boundary->source_name)
            || air_name_matches(scope->name, boundary->owner_name)
            || air_name_matches(scope->owner_name, boundary->owner_name)
            || air_name_matches(scope->owner_name, boundary->source_name);
    }
    return air_name_matches(scope->name, boundary->source_name)
        || air_name_matches(scope->owner_name, boundary->owner_name)
        || air_name_matches(scope->owner_name, boundary->source_name);
}

static bool
air_ast_contains_node(const ASTNode *container, const ASTNode *needle)
{
    if (container == NULL || needle == NULL)
        return false;
    if (container == needle)
        return true;
    switch (container->type) {
    case AST_INTENT_STEP:
        if (air_ast_contains_node(container->data.intent_step.using_expr, needle)
            || air_ast_contains_node(container->data.intent_step.intent_expr, needle)
            || air_ast_contains_node(container->data.intent_step.pre_expr, needle)
            || air_ast_contains_node(container->data.intent_step.guard_expr, needle)
            || air_ast_contains_node(container->data.intent_step.post_expr, needle)
            || air_ast_contains_node(container->data.intent_step.invariant_expr, needle)
            || air_ast_contains_node(container->data.intent_step.expect_expr, needle)) {
            return true;
        }
        for (size_t i = 0; i < container->data.intent_step.on_expr_count; i++) {
            if (air_ast_contains_node(container->data.intent_step.on_exprs[i], needle))
                return true;
        }
        for (size_t i = 0; i < container->data.intent_step.compensate_expr_count; i++) {
            if (air_ast_contains_node(container->data.intent_step.compensate_exprs[i], needle))
                return true;
        }
        return false;
    case AST_CALL:
        if (air_ast_contains_node(container->data.call.callee, needle))
            return true;
        for (size_t i = 0; i < container->data.call.arg_count; i++) {
            if (air_ast_contains_node(container->data.call.arguments[i], needle))
                return true;
        }
        return false;
    case AST_MEMBER_ACCESS:
        return air_ast_contains_node(container->data.member.object, needle);
    case AST_ARRAY_ACCESS:
        return air_ast_contains_node(container->data.array_access.array, needle)
            || air_ast_contains_node(container->data.array_access.index, needle);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < container->data.array_literal.count; i++) {
            if (air_ast_contains_node(container->data.array_literal.elements[i], needle))
                return true;
        }
        return false;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < container->data.tuple_literal.count; i++) {
            if (air_ast_contains_node(container->data.tuple_literal.elements[i], needle))
                return true;
        }
        return false;
    case AST_ASSIGNMENT:
        return air_ast_contains_node(container->data.assignment.target, needle)
            || air_ast_contains_node(container->data.assignment.value, needle);
    case AST_BINARY:
        return air_ast_contains_node(container->data.binary.left, needle)
            || air_ast_contains_node(container->data.binary.right, needle);
    case AST_UNARY:
        return air_ast_contains_node(container->data.unary.operand, needle);
    case AST_CHANNEL_SEND:
        return air_ast_contains_node(container->data.channel_send.channel, needle)
            || air_ast_contains_node(container->data.channel_send.value, needle);
    case AST_CHANNEL_RECV:
        return air_ast_contains_node(container->data.channel_recv.channel, needle);
    case AST_AWAIT_EXPR:
        return air_ast_contains_node(container->data.await_expr.expression, needle);
    case AST_SPAWN_EXPR:
        if (air_ast_contains_node(container->data.spawn_expr.function, needle))
            return true;
        for (size_t i = 0; i < container->data.spawn_expr.arg_count; i++) {
            if (air_ast_contains_node(container->data.spawn_expr.arguments[i], needle))
                return true;
        }
        return false;
    case AST_BLOCK:
        for (size_t i = 0; i < container->data.block.count; i++) {
            if (air_ast_contains_node(container->data.block.statements[i], needle))
                return true;
        }
        return false;
    case AST_LET_DECL:
        return air_ast_contains_node(container->data.let_decl.initializer, needle);
    case AST_LET_DESTRUCTURE:
        return air_ast_contains_node(container->data.let_destructure.initializer, needle);
    case AST_WITH_STMT:
        return air_ast_contains_node(container->data.with_stmt.body, needle);
    case AST_FOR_LOOP:
        return air_ast_contains_node(container->data.for_loop.range_start, needle)
            || air_ast_contains_node(container->data.for_loop.range_end, needle)
            || air_ast_contains_node(container->data.for_loop.iterable, needle)
            || air_ast_contains_node(container->data.for_loop.body, needle);
    case AST_WHILE_LOOP:
        return air_ast_contains_node(container->data.while_loop.condition, needle)
            || air_ast_contains_node(container->data.while_loop.body, needle);
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < container->data.parallel.task_count; i++) {
            if (air_ast_contains_node(container->data.parallel.tasks[i], needle))
                return true;
        }
        return false;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < container->data.async_block.statement_count; i++) {
            if (air_ast_contains_node(container->data.async_block.statements[i], needle))
                return true;
        }
        return false;
    case AST_TASK_GROUP:
        for (size_t i = 0; i < container->data.task_group.task_count; i++) {
            if (air_ast_contains_node(container->data.task_group.tasks[i], needle))
                return true;
        }
        return false;
    case AST_UNSAFE_BLOCK:
        return air_ast_contains_node(container->data.unsafe_block.body, needle);
    case AST_DEFER_STMT:
        return air_ast_contains_node(container->data.defer_stmt.body, needle);
    case AST_RETURN:
        return air_ast_contains_node(container->data.return_stmt.value, needle);
    case AST_IF_STMT:
        return air_ast_contains_node(container->data.if_stmt.condition, needle)
            || air_ast_contains_node(container->data.if_stmt.then_branch, needle)
            || air_ast_contains_node(container->data.if_stmt.else_branch, needle);
    case AST_SELECT_STMT:
        for (size_t i = 0; i < container->data.select_stmt.case_count; i++) {
            if (air_ast_contains_node(container->data.select_stmt.cases[i], needle))
                return true;
        }
        return air_ast_contains_node(container->data.select_stmt.default_case, needle);
    case AST_MATCH_STMT:
        if (air_ast_contains_node(container->data.match_stmt.subject, needle))
            return true;
        for (size_t i = 0; i < container->data.match_stmt.case_count; i++) {
            if (air_ast_contains_node(container->data.match_stmt.cases[i], needle))
                return true;
        }
        return air_ast_contains_node(container->data.match_stmt.default_body, needle);
    case AST_MATCH_CASE:
        if (air_ast_contains_node(container->data.match_case.pattern, needle)
            || air_ast_contains_node(container->data.match_case.guard, needle)
            || air_ast_contains_node(container->data.match_case.body, needle)) {
            return true;
        }
        for (size_t i = 0; i < container->data.match_case.pattern_count; i++) {
            if (air_ast_contains_node(container->data.match_case.patterns[i], needle))
                return true;
        }
        return false;
    case AST_EVENT_INVOKE:
        if (air_ast_contains_node(container->data.event_invoke.event, needle))
            return true;
        for (size_t i = 0; i < container->data.event_invoke.arg_count; i++) {
            if (air_ast_contains_node(container->data.event_invoke.arguments[i], needle))
                return true;
        }
        return false;
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return air_ast_contains_node(container->data.event_op.event, needle)
            || air_ast_contains_node(container->data.event_op.handler, needle);
    case AST_PARTY_SHARED:
        return air_ast_contains_node(container->data.party_shared.initializer, needle);
    case AST_PARTY_INSTANCE:
        for (size_t i = 0; i < container->data.party_instance.assignment_count; i++) {
            if (air_ast_contains_node(container->data.party_instance.assignments[i].value, needle))
                return true;
        }
        return false;
    case AST_WORLD_SYSTEMIC:
        return air_ast_contains_node(container->data.world_roster.initializer, needle);
    case AST_WORLD_ZONE:
        return air_ast_contains_node(container->data.world_zone.initializer, needle);
    case AST_DOMAIN_SLOT:
        return air_ast_contains_node(container->data.domain_slot.initializer, needle);
    case AST_LAMBDA_EXPR:
        return air_ast_contains_node(container->data.lambda_expr.body, needle);
    default:
        return false;
    }
}

static bool
air_rir_op_matches_boundary_ast(const RIROp *op, const AIRBoundaryNode *boundary)
{
    if (op == NULL || boundary == NULL)
        return false;
    if (boundary->ast == NULL)
        return true;
    return op->ast == boundary->ast
        || air_ast_contains_node(boundary->ast, op->ast);
}

static bool
air_rir_io_op_matches_boundary(const RIROp *op, const AIRBoundaryNode *boundary)
{
    return op != NULL
        && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_IO
        && op->kind == RIR_OP_IO
        && air_name_matches(op->subject, boundary->source_name)
        && air_rir_op_matches_boundary_ast(op, boundary);
}

static bool
air_rir_channel_op_matches_boundary(const RIROp *op, const AIRBoundaryNode *boundary)
{
    if (op == NULL || boundary == NULL || boundary->kind != AIR_BOUNDARY_CHANNEL)
        return false;
    if (air_name_matches(boundary->source_name, "channel-send"))
        return op->kind == RIR_OP_CHANNEL_SEND
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "channel-recv"))
        return op->kind == RIR_OP_CHANNEL_RECV
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "select"))
        return op->kind == RIR_OP_CHANNEL_SELECT
            && air_rir_op_matches_boundary_ast(op, boundary);
    return false;
}

static bool
air_rir_parallel_op_matches_boundary(const RIROp *op, const AIRBoundaryNode *boundary)
{
    if (op == NULL || boundary == NULL || boundary->kind != AIR_BOUNDARY_PARALLEL)
        return false;
    if (air_name_matches(boundary->source_name, "await"))
        return op->kind == RIR_OP_AWAIT_REMOTE
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "spawn"))
        return op->kind == RIR_OP_SPAWN
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "async"))
        return op->kind == RIR_OP_ASYNC
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "task-group"))
        return op->kind == RIR_OP_TASK_GROUP
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "parallel"))
        return op->kind == RIR_OP_PARALLEL
            && air_rir_op_matches_boundary_ast(op, boundary);
    return false;
}

static bool
air_rir_scope_provides_boundary_evidence(const RIRScope *scope,
                                         const AIRBoundaryNode *boundary)
{
    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_PARALLEL
        && boundary->ast != NULL) {
        for (size_t i = 0; i < scope->op_count; i++) {
            if (air_rir_parallel_op_matches_boundary(&scope->ops[i], boundary))
                return true;
        }
        return false;
    }

    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_IO
        && boundary->ast != NULL) {
        for (size_t i = 0; i < scope->op_count; i++) {
            if (air_rir_io_op_matches_boundary(&scope->ops[i], boundary))
                return true;
        }
        return false;
    }

    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_CHANNEL
        && boundary->ast != NULL) {
        for (size_t i = 0; i < scope->op_count; i++) {
            if (air_rir_channel_op_matches_boundary(&scope->ops[i], boundary))
                return true;
        }
        return false;
    }

    if (!air_rir_scope_matches_boundary(scope, boundary))
        return false;

    if (boundary->kind != AIR_BOUNDARY_WORLD)
        return true;

    for (size_t i = 0; i < scope->op_count; i++) {
        const RIROp *op = &scope->ops[i];
        if (op->kind == RIR_OP_CLAIM
            && air_name_matches(op->subject, boundary->source_name)
            && air_rir_op_matches_boundary_ast(op, boundary)) {
            return true;
        }
        if (op->kind == RIR_OP_MOVE
            && air_name_matches(op->arg0, boundary->source_name)
            && air_rir_op_matches_boundary_ast(op, boundary)) {
            return true;
        }
    }
    return false;
}

static bool
air_rir_name_or_anchor_matches(const char *name, const char *slot_anchor, const char *needle)
{
    return air_name_matches(name, needle) || air_name_matches(slot_anchor, needle);
}

static bool
air_rir_scope_has_propagation_state(const RIRScope *scope,
                                    const RIROp *op,
                                    RIRResourceKind resource_kind)
{
    if (scope == NULL || op == NULL)
        return false;
    for (size_t i = 0; i < scope->state_summary_count; i++) {
        const RIRStateSummary *summary = &scope->state_summaries[i];
        if (summary->resource_kind == resource_kind
            && air_rir_name_or_anchor_matches(summary->name, summary->slot_anchor, op->subject)) {
            return true;
        }
    }
    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->resource_kind == resource_kind
            && air_rir_name_or_anchor_matches(fact->name, fact->slot_anchor, op->subject)) {
            return true;
        }
    }
    return false;
}

static bool
air_collect_rir_propagation_evidence(AIRProgram *air,
                                     const RIRScope *scope,
                                     const RIROp *op,
                                     const char *scope_name,
                                     char **error_message)
{
    const bool effect_op = op->kind == RIR_OP_ATTACH_EFFECT
        || op->kind == RIR_OP_DETACH_EFFECT;
    const bool relation_op = op->kind == RIR_OP_LINK_RELATION
        || op->kind == RIR_OP_UNLINK_RELATION;
    RIRResourceKind resource_kind;
    AIREvidenceKind evidence_kind;

    if (!effect_op && !relation_op)
        return true;

    resource_kind = effect_op
        ? RIR_RESOURCE_EFFECT_INSTANCE
        : RIR_RESOURCE_RELATION_INSTANCE;
    evidence_kind = effect_op
        ? AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
        : AIR_EVIDENCE_RIR_RELATION_PROPAGATION;

    if (effect_op)
        air->rir_effect_propagation_required_count++;
    else
        air->rir_relation_propagation_required_count++;

    if (!air_rir_scope_has_propagation_state(scope, op, resource_kind))
        return true;

    if (!air_append_evidence_node(air,
                                  evidence_kind,
                                  SIZE_MAX,
                                  scope_name,
                                  op->subject,
                                  error_message)) {
        return false;
    }
    if (effect_op)
        air->rir_effect_propagation_evidence_count++;
    else
        air->rir_relation_propagation_evidence_count++;
    return true;
}

bool
air_collect_rir_evidence(AIRProgram *air, const RIRProgram *rir, char **error_message)
{
    if (air == NULL || rir == NULL)
        return true;
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        const char *scope_name = scope->name != NULL ? scope->name : scope->owner_name;
        for (size_t j = 0; j < scope->fact_count; j++) {
            if (scope->facts[j].kind == RIR_FACT_AUTHORITY)
                air->rir_authority_evidence_count++;
        }
        for (size_t j = 0; j < scope->op_count; j++) {
            if (scope->ops[j].kind == RIR_OP_AUTHORIZE)
                air->rir_authority_evidence_count++;
            if (!air_collect_rir_propagation_evidence(air,
                                                      scope,
                                                      &scope->ops[j],
                                                      scope_name,
                                                      error_message)) {
                return false;
            }
        }
        for (size_t j = 0; j < air->boundary_count; j++) {
            AIRBoundaryNode *boundary = &air->boundaries[j];
            if (!air_rir_scope_provides_boundary_evidence(scope, boundary))
                continue;
            if (!air_assign_first_owned_name(air,
                                             &boundary->rir_boundary_evidence_scope,
                                             scope_name,
                                             error_message,
                                             "RIR boundary")) {
                return false;
            }
            boundary->has_rir_boundary_evidence = true;
            air->rir_boundary_evidence_count++;
            if (!air_append_evidence_node(air,
                                          AIR_EVIDENCE_RIR_BOUNDARY,
                                          j,
                                          scope_name,
                                          boundary->source_name,
                                          error_message)) {
                return false;
            }
            for (size_t k = 0; k < scope->fact_count; k++) {
                if (scope->facts[k].kind == RIR_FACT_AUTHORITY
                    && air_boundary_authority_matches(boundary, scope->facts[k].name)) {
                    if (!air_assign_first_owned_name(air,
                                                     &boundary->rir_authority_evidence_name,
                                                     scope->facts[k].name,
                                                     error_message,
                                                     "RIR authority")) {
                        return false;
                    }
                    boundary->has_rir_authority_evidence = true;
                    if (!air_append_evidence_node(air,
                                                  AIR_EVIDENCE_RIR_AUTHORITY,
                                                  j,
                                                  scope_name,
                                                  scope->facts[k].name,
                                                  error_message)) {
                        return false;
                    }
                    break;
                }
            }
            for (size_t k = 0; !boundary->has_rir_authority_evidence && k < scope->op_count; k++) {
                if (scope->ops[k].kind == RIR_OP_AUTHORIZE
                    && air_boundary_authority_matches(boundary, scope->ops[k].subject)) {
                    if (!air_assign_first_owned_name(air,
                                                     &boundary->rir_authority_evidence_name,
                                                     scope->ops[k].subject,
                                                     error_message,
                                                     "RIR authority")) {
                        return false;
                    }
                    boundary->has_rir_authority_evidence = true;
                    if (!air_append_evidence_node(air,
                                                  AIR_EVIDENCE_RIR_AUTHORITY,
                                                  j,
                                                  scope_name,
                                                  scope->ops[k].subject,
                                                  error_message)) {
                        return false;
                    }
                    break;
                }
            }
        }
    }
    return true;
}

static bool
air_boundary_is_pin_boundary(const AIRBoundaryNode *boundary)
{
    return boundary != NULL
        && boundary->kind == AIR_BOUNDARY_EXECUTION
        && air_name_matches(boundary->source_name, "pin");
}

static bool
air_mir_pin_block_matches_boundary(const MIRBasicBlock *block,
                                   const AIRBoundaryNode *boundary)
{
    if (block == NULL || boundary == NULL || !block->is_pin_region)
        return false;
    if (!air_boundary_is_pin_boundary(boundary))
        return false;
    if (boundary->ast == NULL)
        return true;
    return block->pin_block_ast == boundary->ast;
}

static const MIRInstruction *
air_mir_find_pin_cleanup_instruction(const MIRBasicBlock *block)
{
    if (block == NULL)
        return NULL;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_CLEANUP_EDGE
            && air_name_matches(inst->name, "pin-unpin-cleanup-edge")) {
            return inst;
        }
    }
    return NULL;
}

static bool
air_mir_pin_block_has_cleanup_successor(const MIRRoutine *routine,
                                        const MIRBasicBlock *block)
{
    if (routine == NULL || block == NULL)
        return false;
    if (!routine->has_cleanup_block || routine->cleanup_block >= routine->block_count)
        return false;
    if (!block->has_cleanup_succ || block->cleanup_succ != routine->cleanup_block)
        return false;
    return routine->blocks[routine->cleanup_block].is_cleanup;
}

static size_t
air_mir_routine_cleanup_fact_count(const MIRRoutine *routine)
{
    size_t count = 0;

    if (routine == NULL || !routine->has_cleanup_block)
        return 0;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->instruction_count; j++) {
            if (block->instructions[j].kind == MIR_INST_CLEANUP_EDGE)
                count++;
        }
    }
    return count;
}

bool
air_collect_mir_evidence(AIRProgram *air, const MIRProgram *mir, char **error_message)
{
    if (air == NULL || mir == NULL)
        return true;

    air->has_mir_input = true;

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        const char *routine_name = routine->name != NULL
            ? routine->name
            : routine->owner_name;
        size_t cleanup_fact_count = air_mir_routine_cleanup_fact_count(routine);
        if (cleanup_fact_count == 0)
            continue;
        if (!air_append_evidence_node_ex(air,
                                         AIR_EVIDENCE_MIR_CLEANUP,
                                         SIZE_MAX,
                                         routine_name,
                                         "cleanup-block",
                                         cleanup_fact_count,
                                         0,
                                         error_message)) {
            return false;
        }
        air->mir_cleanup_evidence_count++;
    }

    for (size_t i = 0; i < air->boundary_count; i++) {
        AIRBoundaryNode *boundary = &air->boundaries[i];
        if (!air_boundary_is_pin_boundary(boundary))
            continue;

        for (size_t j = 0; j < mir->routine_count; j++) {
            const MIRRoutine *routine = &mir->routines[j];
            const char *routine_name = routine->name != NULL
                ? routine->name
                : routine->owner_name;
            for (size_t k = 0; k < routine->block_count; k++) {
                const MIRBasicBlock *block = &routine->blocks[k];
                const MIRInstruction *inst;
                if (!air_mir_pin_block_matches_boundary(block, boundary))
                    continue;
                if (!air_mir_pin_block_has_cleanup_successor(routine, block))
                    continue;
                inst = air_mir_find_pin_cleanup_instruction(block);
                if (inst == NULL)
                    continue;
                if (!air_append_evidence_node(air,
                                              AIR_EVIDENCE_MIR_PIN_CLEANUP,
                                              i,
                                              routine_name,
                                              inst->slot_anchor != NULL
                                                  ? inst->slot_anchor
                                                  : boundary->source_name,
                                              error_message)) {
                    return false;
                }
                air->mir_pin_cleanup_evidence_count++;
                break;
            }
        }
    }
    return true;
}

bool
air_collect_dag_evidence(AIRProgram *air, const SemanticResult *sem, char **error_message)
{
    if (air == NULL || sem == NULL)
        return true;

    if (!air_append_evidence_node_ex(air,
                                     AIR_EVIDENCE_DAG_GENERIC,
                                     SIZE_MAX,
                                     "type-resolution-dag",
                                     "generic-contracts",
                                     sem->type_resolution_stage_compat_generic_contract_count,
                                     sem->type_resolution_metadata_materializer_fallbacks,
                                     error_message)) {
        return false;
    }
    air->dag_generic_evidence_count++;

    if (!air_append_evidence_node_ex(air,
                                     AIR_EVIDENCE_DAG_ABILITY,
                                     SIZE_MAX,
                                     "type-resolution-dag",
                                     "ability-consumers",
                                     sem->type_resolution_stage_compat_ability_consumer_count,
                                     sem->type_resolution_metadata_materializer_fallbacks,
                                     error_message)) {
        return false;
    }
    air->dag_ability_evidence_count++;
    return true;
}
