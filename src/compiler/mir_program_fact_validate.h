#ifndef PERGYRA_COMPILER_MIR_PROGRAM_FACT_VALIDATE_H
#define PERGYRA_COMPILER_MIR_PROGRAM_FACT_VALIDATE_H

#include "mir.h"

bool mir_validate_non_cfg_fallback_state(
    const MIRRoutine *routine,
    char **error_message);
bool mir_validate_function_param_flow_summaries(
    const MIRRoutine *routine,
    char **error_message);
bool mir_validate_resource_flow_symbols(
    const MIRRoutine *routine,
    char **error_message);
bool mir_validate_loop_flow_facts(
    const MIRRoutine *routine,
    char **error_message);
bool mir_validate_program_inventory_shape(
    const MIRProgram *mir,
    char **error_message);
bool mir_validate_non_cfg_fallback_inventory(
    const MIRProgram *mir,
    char **error_message);
bool mir_validate_receiver_carriage_facts(
    const MIRProgram *mir,
    char **error_message);

#endif
