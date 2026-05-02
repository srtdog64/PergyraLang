#ifndef PERGYRA_MIR_CALL_FACT_H
#define PERGYRA_MIR_CALL_FACT_H

static void
mir_attach_statement_call_fact(MIRInstruction *inst, const ASTNode *stmt)
{
    if (inst == NULL || stmt == NULL || stmt->type != AST_CALL)
        return;
    if (stmt->data.call.callee == NULL
        || stmt->data.call.callee->type != AST_IDENTIFIER) {
        return;
    }
    inst->arg0 = stmt->data.call.callee->data.identifier.name;
}

static void
mir_attach_def_initializer_call_fact(MIRInstruction *inst, const ASTNode *stmt)
{
    const ASTNode *expr = NULL;

    if (inst == NULL || inst->kind != MIR_INST_DEF || stmt == NULL)
        return;
    if (stmt->type == AST_LET_DECL)
        expr = stmt->data.let_decl.initializer;
    else if (stmt->type == AST_ASSIGNMENT)
        expr = stmt->data.assignment.value;
    if (expr == NULL || expr->type != AST_CALL)
        return;
    if (expr->data.call.callee == NULL
        || expr->data.call.callee->type != AST_IDENTIFIER) {
        return;
    }
    inst->arg1 = expr->data.call.callee->data.identifier.name;
}

#endif /* PERGYRA_MIR_CALL_FACT_H */
