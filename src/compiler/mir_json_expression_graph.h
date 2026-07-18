#ifndef PERGYRA_COMPILER_MIR_JSON_EXPRESSION_GRAPH_H
#define PERGYRA_COMPILER_MIR_JSON_EXPRESSION_GRAPH_H

#include <stdio.h>

#include "mir_types.h"

void mir_json_emit_instruction_expression_graph(
    FILE *out,
    const MIRInstruction *inst,
    int lane);

#endif
