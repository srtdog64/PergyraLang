#ifndef PGY_MIR_SOURCE_LOCAL_EXPR_CALL_FACTS_H
#define PGY_MIR_SOURCE_LOCAL_EXPR_CALL_FACTS_H

#include "mir_source_local_expr_types.h"

const char *mir_source_local_call_expr_type_name(
    const MIRProgram *program,
    const MIRRoutine *routine,
    MIRSourceLocalTypeScratch *scratch,
    ASTNode *expr);

#endif
