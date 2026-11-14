#ifndef PGY_MIR_JSON_LOCAL_REF_H
#define PGY_MIR_JSON_LOCAL_REF_H

#include <stdbool.h>
#include <stdio.h>
#include "mir_types.h"

bool mir_json_routine_local_refs_required(const MIRRoutine *routine);
void mir_json_emit_instruction_local_ref(FILE *out, const MIRRoutine *routine,
                                        const MIRInstruction *inst);

#endif
