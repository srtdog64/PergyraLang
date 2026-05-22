#ifndef PGY_TRANSPILER_MIR_FUNC_SSA_LOCALS_EMIT_H
#define PGY_TRANSPILER_MIR_FUNC_SSA_LOCALS_EMIT_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_emit_mir_func_ssa_local_decls(TranspilerCtx *ctx,
                                              ASTNode *node,
                                              const MIRRoutine *mir_routine,
                                              const char *name);

#endif /* PGY_TRANSPILER_MIR_FUNC_SSA_LOCALS_EMIT_H */
