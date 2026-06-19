#include "mir_call_fact.h"

#include "../parser/ast_api.h"

void
mir_attach_statement_call_fact(MIRInstruction *inst, const ASTNode *stmt)
{
    if (inst == NULL || stmt == NULL)
        return;
    if (stmt->type == AST_DEFER_STMT) {
        inst->expr0 = ast_defer_body(stmt);
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
    if (ast_call_callee(stmt) == NULL
        || ast_call_callee(stmt)->type != AST_IDENTIFIER) {
        return;
    }
    inst->arg0 = ast_identifier_name(ast_call_callee(stmt));
}

void
mir_attach_def_initializer_call_fact(MIRInstruction *inst, const ASTNode *stmt)
{
    ASTNode *expr = NULL;

    if (inst == NULL || inst->kind != MIR_INST_DEF || stmt == NULL)
        return;
    if (stmt->type == AST_LET_DECL) {
        expr = ast_let_initializer(stmt);
        inst->expr1 = ast_let_type(stmt);
        inst->requires_source_statement_emit = true;
        inst->requires_source_local_decl_emit = true;
    } else if (stmt->type == AST_ASSIGNMENT) {
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
