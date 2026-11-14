#ifndef PERGYRA_MIR_CALL_FACT_H
#define PERGYRA_MIR_CALL_FACT_H

#include "../mir.h"

#include "../../parser/ast.h"

void mir_attach_statement_call_fact(MIRInstruction *inst, const ASTNode *stmt);
ASTNode *mir_defer_part_statement(const MIRInstruction *inst);
bool mir_instruction_executable_roots(const MIRRoutine *routine,
    const MIRInstruction *inst, ASTNode *roots[2], bool *resource_summary, char **error_message);
/* SSA use graphs also carry loop bounds/storage. Only these roots perform a
 * new source evaluation, rather than reusing an already-evaluated carrier. */
bool mir_instruction_evaluation_roots(const MIRRoutine *routine,
    const MIRInstruction *inst, ASTNode *roots[2]);
const char *mir_defer_statement_routing(const ASTNode *stmt);
ASTNode *mir_defer_assignment_value_fact(const MIRInstruction *inst);
ASTNode *mir_defer_assignment_target_fact(const MIRInstruction *inst);
ASTNode *mir_defer_log_expression_fact(const MIRInstruction *inst);
ASTNode *mir_defer_call_expression_fact(const MIRInstruction *inst);
void mir_attach_def_initializer_call_fact(MIRRoutine *routine,
                                          MIRInstruction *inst,
                                          const ASTNode *stmt);
void mir_mark_select_receive_statement_emit(const MIRBasicBlock *block,
                                            MIRInstruction *inst);

#endif /* PERGYRA_MIR_CALL_FACT_H */
