#ifndef PGY_MIR_SOURCE_LOCAL_EXPR_TYPES_H
#define PGY_MIR_SOURCE_LOCAL_EXPR_TYPES_H

#include "mir_source_local_type_shape.h"

const char *mir_source_local_expr_type_name(const MIRProgram *program,
                                            const MIRRoutine *routine,
                                            MIRSourceLocalTypeScratch *scratch,
                                            ASTNode *expr);
const char *mir_source_local_for_loop_variable_type_name(
    const MIRProgram *program,
    const MIRRoutine *routine,
    MIRSourceLocalTypeScratch *scratch,
    ASTNode *node);

#endif
