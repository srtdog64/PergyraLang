#ifndef PGY_MIR_SOURCE_LOCAL_TYPES_H
#define PGY_MIR_SOURCE_LOCAL_TYPES_H

#include "mir.h"

void mir_routine_source_local_type_names_clear(MIRRoutine *routine);
bool mir_routine_source_local_type_names_capture(const MIRProgram *program,
                                                 MIRRoutine *routine);
/* Explicit non-MIR compatibility lookup for legacy callers that do not have
 * a materialized MIRRoutine::source_local_types fact. */
const char *mir_source_local_type_name_in_ast(ASTNode *body,
                                              const char *local_name);

#endif
