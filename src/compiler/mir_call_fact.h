#ifndef PERGYRA_MIR_CALL_FACT_H
#define PERGYRA_MIR_CALL_FACT_H

#include "mir.h"

#include "../parser/ast.h"

void mir_attach_statement_call_fact(MIRInstruction *inst, const ASTNode *stmt);
ASTNode *mir_defer_log_expression_fact(const MIRInstruction *inst);
void mir_attach_def_initializer_call_fact(MIRRoutine *routine,
                                          MIRInstruction *inst,
                                          const ASTNode *stmt);
void mir_mark_select_receive_statement_emit(const MIRBasicBlock *block,
                                            MIRInstruction *inst);

#endif /* PERGYRA_MIR_CALL_FACT_H */
