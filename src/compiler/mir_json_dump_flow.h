#ifndef PERGYRA_COMPILER_MIR_JSON_DUMP_FLOW_H
#define PERGYRA_COMPILER_MIR_JSON_DUMP_FLOW_H

#include "mir.h"

#include <stdio.h>

void mir_json_emit_resource_flow_symbols(FILE *out,
                                         const MIRRoutine *routine);
void mir_json_emit_loop_flow_facts(FILE *out, const MIRRoutine *routine);
void mir_json_emit_iteration_type_facts(FILE *out,
                                        const MIRRoutine *routine);

#endif
