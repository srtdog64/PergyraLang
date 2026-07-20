#ifndef PERGYRA_MIR_MACHINE_LAYER_H
#define PERGYRA_MIR_MACHINE_LAYER_H

#include "mir.h"

bool mir_attach_machine_layer_fact(MIRInstruction *inst, const RIROp *op);
bool mir_attach_machine_layer_fact_for_ast(MIRRoutine *routine,
                                           MIRInstruction *inst,
                                           const ASTNode *ast);
bool mir_enrich_machine_layer_facts(MIRRoutine *routine);
bool mir_machine_layer_fact_is_valid(const MIRInstruction *inst);
const char *mir_machine_layer_runtime_operation(
    const MIRInstruction *inst);
bool mir_machine_layer_fact_matches_runtime_operation(
    const MIRInstruction *inst,
    const char *runtime_operation);

#endif
