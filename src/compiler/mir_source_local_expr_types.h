#ifndef PGY_MIR_SOURCE_LOCAL_EXPR_TYPES_H
#define PGY_MIR_SOURCE_LOCAL_EXPR_TYPES_H

#include "mir.h"

#define MIR_SOURCE_LOCAL_TYPE_SCRATCH_COUNT 8
#define MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE 128

typedef struct MIRSourceLocalTypeScratch
{
    char buffers[MIR_SOURCE_LOCAL_TYPE_SCRATCH_COUNT]
                [MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
    size_t next;
} MIRSourceLocalTypeScratch;

char *mir_source_local_type_scratch_next(MIRSourceLocalTypeScratch *scratch);

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
