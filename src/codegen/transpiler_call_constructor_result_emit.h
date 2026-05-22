#ifndef PGY_TRANSPILER_CALL_CONSTRUCTOR_RESULT_EMIT_H
#define PGY_TRANSPILER_CALL_CONSTRUCTOR_RESULT_EMIT_H

#include "transpiler.h"

char *emit_call_domain_constructor(ASTNode *call,
                                   ASTNode *callee,
                                   TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_CALL_CONSTRUCTOR_RESULT_EMIT_H */
