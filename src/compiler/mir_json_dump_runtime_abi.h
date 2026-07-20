#ifndef PERGYRA_COMPILER_MIR_JSON_DUMP_RUNTIME_ABI_H
#define PERGYRA_COMPILER_MIR_JSON_DUMP_RUNTIME_ABI_H

#include "mir.h"

#include <stdbool.h>
#include <stdio.h>

bool mir_json_emit_instruction_runtime_abi(FILE *out,
                                           const MIRInstruction *inst);

#endif
