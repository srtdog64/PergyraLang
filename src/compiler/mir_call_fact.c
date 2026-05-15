#include "mir_call_fact.h"

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
        inst->arg0 = stmt->data.let_decl.name;
        inst->expr0 = stmt->data.let_decl.initializer;
        inst->expr1 = stmt->data.let_decl.type;
        return;
    }
    if (stmt->type == AST_ASSIGNMENT) {
        ASTNode *target = ast_assignment_target(stmt);
        if (target != NULL
            && target->type == AST_IDENTIFIER)
            inst->arg0 = target->data.identifier.name;
        inst->expr0 = ast_assignment_value(stmt);
        return;
    }
    if (stmt->type != AST_CALL)
        return;
    if (ast_call_callee(stmt) == NULL
        || ast_call_callee(stmt)->type != AST_IDENTIFIER) {
        return;
    }
    inst->arg0 = ast_call_callee(stmt)->data.identifier.name;
}

void
mir_attach_def_initializer_call_fact(MIRInstruction *inst, const ASTNode *stmt)
{
    ASTNode *expr = NULL;

    if (inst == NULL || inst->kind != MIR_INST_DEF || stmt == NULL)
        return;
    if (stmt->type == AST_LET_DECL) {
        expr = stmt->data.let_decl.initializer;
        inst->expr1 = stmt->data.let_decl.type;
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
    inst->arg1 = ast_call_callee(expr)->data.identifier.name;
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
