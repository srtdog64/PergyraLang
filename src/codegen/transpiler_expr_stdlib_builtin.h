#ifndef PGY_TRANSPILER_EXPR_STDLIB_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_BUILTIN_H

#include "transpiler.h"

char *emit_call_stdlib_builtin(ASTNode *call, ASTNode *callee,
                               TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_EXPR_STDLIB_BUILTIN_H */
