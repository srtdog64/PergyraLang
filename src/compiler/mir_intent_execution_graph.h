#ifndef PGY_MIR_INTENT_EXECUTION_GRAPH_H
#define PGY_MIR_INTENT_EXECUTION_GRAPH_H

#include "dir.h"
#include "mir.h"

bool intent_execution_set_goto(MIRRoutine *routine,
                               size_t from,
                               size_t to);
bool intent_execution_set_branch(MIRRoutine *routine,
                                 size_t from,
                                 size_t success,
                                 size_t failure);
bool intent_execution_append_block(MIRRoutine *routine,
                                   bool reachable,
                                   size_t *block_id_out);
bool intent_execution_materialize_step(
    MIRRoutine *routine,
    const DIRIntentStep *step,
    MIRIntentStepTransitionFact *row,
    size_t compensation_block_id);

#endif /* PGY_MIR_INTENT_EXECUTION_GRAPH_H */
