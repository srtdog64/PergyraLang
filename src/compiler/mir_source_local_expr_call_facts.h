#ifndef PGY_MIR_SOURCE_LOCAL_EXPR_CALL_FACTS_H
#define PGY_MIR_SOURCE_LOCAL_EXPR_CALL_FACTS_H

#include "mir_source_local_expr_types.h"

const char *mir_source_local_call_return_type_name(
    const MIRProgram *program,
    const MIRRoutine *caller_routine,
    MIRSourceLocalTypeScratch *scratch,
    ASTNode *call,
    const char *name);

const char *mir_source_local_extern_return_type_name(
    const MIRProgram *program,
    MIRSourceLocalTypeScratch *scratch,
    const char *name);

#endif
