#include "mir_call_fact.h"
#include "mir_resource_runtime_population.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/lifecycle_state.h"
#include "mir_type_helpers.h"
#include "mir_abi_layout.h"

static void
mir_attach_text_builder_runtime_row(MIRInstruction *inst,
                                    const ASTNode *call)
{
    ASTNode *callee;
    const char *source_name;

    if (inst == NULL || call == NULL || call->type != AST_CALL)
        return;
    callee = ast_call_callee(call);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return;
    source_name = ast_identifier_name(callee);
    inst->text_builder_runtime_row =
        mir_text_builder_runtime_row_by_source_name(source_name);
}

static void
mir_attach_lifecycle_guard_fact(MIRInstruction *inst, const ASTNode *stmt)
{
    ASTNode *callee;
    ASTNode *receiver = NULL;
    const LcGuardSite *guard;

    if (inst == NULL || stmt == NULL)
        return;
    guard = lc_guard_find(stmt);
    if (guard == NULL)
        return;
    inst->has_lifecycle_guard_fact = true;
    inst->lifecycle_guard_kind =
        guard->kind == LC_GUARD_CHECK
            ? MIR_LIFECYCLE_GUARD_CHECK
            : MIR_LIFECYCLE_GUARD_SET;
    inst->lifecycle_valid_mask = guard->valid_mask;
    inst->lifecycle_to_state = guard->to_state;
    if (stmt->type == AST_CALL) {
        callee = ast_call_callee(stmt);
        receiver = callee != NULL && callee->type == AST_MEMBER_ACCESS
            ? ast_member_object(callee)
            : NULL;
    }
    free(inst->lifecycle_receiver_name);
    free(inst->lifecycle_op);
    free(inst->lifecycle_subject);
    inst->lifecycle_receiver_name =
        receiver != NULL && receiver->type == AST_IDENTIFIER
            && ast_identifier_name(receiver) != NULL
                ? pergyra_strdup(ast_identifier_name(receiver))
                : NULL;
    inst->lifecycle_op = guard->op[0] != '\0'
        ? pergyra_strdup(guard->op)
        : NULL;
    inst->lifecycle_subject = guard->subject[0] != '\0'
        ? pergyra_strdup(guard->subject)
        : NULL;
}

static ASTNode *
mir_defer_single_log_expression(ASTNode *body)
{
    ASTNode *stmt;
    ASTNode *callee;

    if (body == NULL || ast_block_statement_count(body) != 1)
        return NULL;
    stmt = ast_block_statement(body, 0);
    if (stmt == NULL || stmt->type != AST_CALL
        || ast_call_arg_count(stmt) != 1)
        return NULL;
    callee = ast_call_callee(stmt);
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || ast_identifier_name(callee) == NULL
        || strcmp(ast_identifier_name(callee), "Log") != 0)
        return NULL;
    return ast_call_argument(stmt, 0);
}

static ASTNode *
mir_defer_single_call_statement(ASTNode *body)
{
    ASTNode *stmt;

    if (body == NULL || ast_block_statement_count(body) != 1)
        return NULL;
    stmt = ast_block_statement(body, 0);
    if (stmt == NULL || stmt->type != AST_CALL)
        return NULL;
    return stmt;
}

ASTNode *
mir_defer_log_expression_fact(const MIRInstruction *inst)
{
    if (inst == NULL || inst->arg0 == NULL
        || strcmp(inst->arg0, "Log") != 0)
        return NULL;
    return mir_defer_single_log_expression(inst->expr0);
}

ASTNode *
mir_defer_call_expression_fact(const MIRInstruction *inst)
{
    if (inst == NULL || inst->arg0 == NULL
        || strcmp(inst->arg0, "Call") != 0)
        return NULL;
    return mir_defer_single_call_statement(inst->expr0);
}

void
mir_attach_statement_call_fact(MIRInstruction *inst, const ASTNode *stmt)
{
    if (inst == NULL || stmt == NULL)
        return;
    if (stmt->type == AST_DEFER_STMT) {
        inst->expr0 = ast_defer_body(stmt);
        if (mir_defer_single_log_expression(inst->expr0) != NULL)
            inst->arg0 = "Log";
        else if (mir_defer_single_call_statement(inst->expr0) != NULL)
            inst->arg0 = "Call";
        return;
    }
    if (stmt->type == AST_LET_DECL) {
        inst->arg0 = ast_let_name(stmt);
        inst->expr0 = ast_let_initializer(stmt);
        inst->expr1 = ast_let_type(stmt);
        return;
    }
    if (stmt->type == AST_ASSIGNMENT) {
        ASTNode *target = ast_assignment_target(stmt);
        if (target != NULL
            && target->type == AST_IDENTIFIER)
            inst->arg0 = ast_identifier_name(target);
        if (inst->kind != MIR_INST_ASSIGN)
            inst->expr0 = ast_assignment_value(stmt);
        return;
    }
    if (stmt->type != AST_CALL)
    {
        switch (stmt->type) {
        case AST_SPAWN_EXPR:
        case AST_AWAIT_EXPR:
        case AST_CHANNEL_SEND:
        case AST_CHANNEL_RECV:
        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
        case AST_EVENT_INVOKE:
        case AST_PARALLEL_BLOCK:
        case AST_ASYNC_BLOCK:
        case AST_UNSAFE_BLOCK:
        case AST_TRANSACTION_BLOCK:
            inst->expr0 = (ASTNode *)stmt;
            break;
        default:
            break;
        }
        return;
    }
    inst->expr0 = (ASTNode *)stmt;
    mir_attach_lifecycle_guard_fact(inst, stmt);
    if (ast_call_callee(stmt) == NULL
        || ast_call_callee(stmt)->type != AST_IDENTIFIER) {
        return;
    }
    inst->arg0 = ast_identifier_name(ast_call_callee(stmt));
    mir_attach_text_builder_runtime_row(inst, stmt);
}

static void
mir_attach_def_type_name_fact(MIRRoutine *routine,
                              MIRInstruction *inst,
                              ASTNode *type_node)
{
    const char *type_name;
    char *rendered = NULL;

    if (routine == NULL || inst == NULL)
        return;

    if (type_node != NULL) {
        if (type_node->type != AST_TYPE)
            return;
        rendered = mir_capture_type_name(type_node, NULL);
        type_name = rendered;
    } else {
        const char *local_name = inst->arg0 != NULL
            ? inst->arg0
            : inst->slot_anchor;

        /* An inferred let has no annotation node.  Carry the already-owned
         * routine local-type fact onto its DEF here so MIR consumers never
         * need a source-local compatibility read. */
        type_name = mir_routine_source_local_type_name(routine, local_name);
    }

    if (type_name == NULL)
        return;
    inst->abi_type_name = pgy_arena_strdup(&routine->scratch, type_name);
    if (inst->abi_type_name != NULL) {
        inst->type_layout = mir_abi_lookup(inst->abi_type_name);
        inst->abi_layout_id = mir_abi_layout_id(inst->type_layout);
    }
    free(rendered);
}

static void
mir_attach_def_resource_runtime_owner(MIRRoutine *routine,
                                      MIRInstruction *inst)
{
    if (routine == NULL || inst == NULL || inst->abi_type_name == NULL
        || (strncmp(inst->abi_type_name, "Slot<", 5) != 0
            && strncmp(inst->abi_type_name, "SecureSlot<", 11) != 0
            && strncmp(inst->abi_type_name, "DeviceSlot<", 11) != 0))
        return;
    (void)mir_materialize_resource_runtime_fact(routine, inst);
}

void
mir_attach_def_initializer_call_fact(MIRRoutine *routine,
                                     MIRInstruction *inst,
                                     const ASTNode *stmt)
{
    ASTNode *expr = NULL;

    if (inst == NULL || inst->kind != MIR_INST_DEF || stmt == NULL)
        return;
    if (stmt->type == AST_LET_DECL) {
        ASTNode *type_node = ast_let_type(stmt);
        expr = ast_let_initializer(stmt);
        inst->expr1 = type_node;
        mir_attach_def_type_name_fact(routine, inst, type_node);
        mir_attach_def_resource_runtime_owner(routine, inst);
        inst->requires_source_statement_emit = true;
        inst->requires_source_local_decl_emit = true;
    } else if (stmt->type == AST_ASSIGNMENT) {
        inst->expr1 = ast_assignment_target(stmt);
        expr = ast_assignment_value(stmt);
        inst->requires_source_statement_emit = true;
    }
    if (expr != NULL && expr->type == AST_CHANNEL_RECV)
        inst->requires_channel_receive_statement_emit = true;
    inst->expr0 = expr;
    if (expr == NULL || expr->type != AST_CALL)
        return;
    if (ast_call_callee(expr) == NULL
        || ast_call_callee(expr)->type != AST_IDENTIFIER) {
        return;
    }
    inst->arg1 = ast_identifier_name(ast_call_callee(expr));
    mir_attach_text_builder_runtime_row(inst, expr);
}

void
mir_mark_select_receive_statement_emit(const MIRBasicBlock *block,
                                       MIRInstruction *inst)
{
    if (block == NULL
        || !block->is_select_case_body
        || inst == NULL
        || inst->kind != MIR_INST_DEF
        || !inst->requires_channel_receive_statement_emit
        || !mir_instruction_is_first_source_statement(inst)) {
        return;
    }
    inst->requires_select_receive_statement_emit = true;
}
