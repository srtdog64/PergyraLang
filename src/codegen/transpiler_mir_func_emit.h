#ifndef PGY_TRANSPILER_MIR_FUNC_EMIT_H
#define PGY_TRANSPILER_MIR_FUNC_EMIT_H

#include "../compiler/mir.h"
#include "../parser/ast.h"
#include "transpiler.h"

void emit_func_decl_from_mir_named(ASTNode *node,
                                   const MIRRoutine *mir_routine,
                                   const char *emitted_name,
                                   CodeBuf *buf,
                                   TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_MIR_FUNC_EMIT_H */
