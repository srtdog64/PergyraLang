#ifndef PGY_TRANSPILER_EXPR_STDLIB_SCALAR_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_SCALAR_BUILTIN_H

#include "transpiler.h"

char *emit_call_stdlib_scalar_builtin(const char *fn,
                                      ASTNode *call,
                                      TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_EXPR_STDLIB_SCALAR_BUILTIN_H */
