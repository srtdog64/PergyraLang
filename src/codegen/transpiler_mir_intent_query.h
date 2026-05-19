#ifndef PGY_TRANSPILER_MIR_INTENT_QUERY_H
#define PGY_TRANSPILER_MIR_INTENT_QUERY_H

#include "transpiler.h"

bool transpiler_mir_intent_has_stmt(const MIRRoutine *routine,
                                    const char *step_name,
                                    const char *inst_name,
                                    const char *arg0);

#endif /* PGY_TRANSPILER_MIR_INTENT_QUERY_H */
