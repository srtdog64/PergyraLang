#ifndef PERGYRA_MIR_JSON_DUMP_INTENT_EXECUTION_H
#define PERGYRA_MIR_JSON_DUMP_INTENT_EXECUTION_H

#include <stdio.h>

#include "mir_program.h"

void mir_json_emit_intent_execution(FILE *out, const MIRProgram *mir);

#endif
