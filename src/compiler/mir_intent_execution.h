#ifndef PERGYRA_MIR_INTENT_EXECUTION_H
#define PERGYRA_MIR_INTENT_EXECUTION_H

#include "mir.h"

#define PGY_MIR_INTENT_EXECUTION_SCHEMA \
    "pgy.selfhost.mir-intent-execution-plan.v3"

bool mir_materialize_intent_execution_plan(MIRRoutine *routine,
                                            const DIRProgram *dir);
bool mir_validate_intent_execution_plan(const MIRRoutine *routine,
                                        char **error_message);
bool mir_validate_intent_execution_program(const MIRProgram *mir,
                                           char **error_message);
uint32_t mir_intent_execution_routine_digest(const MIRRoutine *routine);
uint32_t mir_intent_execution_program_digest(const MIRProgram *mir);

#endif
