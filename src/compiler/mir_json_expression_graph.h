#ifndef PERGYRA_COMPILER_MIR_JSON_EXPRESSION_GRAPH_H
#define PERGYRA_COMPILER_MIR_JSON_EXPRESSION_GRAPH_H

#include <stdio.h>

#include "mir_types.h"

void mir_json_emit_instruction_expression_graph(
    FILE *out,
    const MIRRoutine *routine,
    const MIRInstruction *inst,
    int lane);
bool mir_expression_graph_identity(ASTNode *expression,
                                   size_t *root_id_out,
                                   uint32_t *digest_out);

#endif
