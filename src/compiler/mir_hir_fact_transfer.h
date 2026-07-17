#ifndef PERGYRA_COMPILER_MIR_HIR_FACT_TRANSFER_H
#define PERGYRA_COMPILER_MIR_HIR_FACT_TRANSFER_H

#include "mir.h"

bool mir_copy_function_param_flow_summaries(MIRRoutine *routine,
                                            const HIRRoutine *hir_routine,
                                            char **error_message);
bool mir_copy_resource_flow_symbols(MIRRoutine *routine,
                                    const HIRRoutine *hir_routine,
                                    char **error_message);
void mir_free_resource_flow_symbols(MIRRoutine *routine);
bool mir_copy_loop_flow_facts(MIRRoutine *routine,
                              const HIRRoutine *hir_routine,
                              char **error_message);
bool mir_copy_iteration_type_facts(MIRRoutine *routine,
                                   const HIRRoutine *hir_routine,
                                   char **error_message);
void mir_free_iteration_type_facts(MIRRoutine *routine);

#endif
