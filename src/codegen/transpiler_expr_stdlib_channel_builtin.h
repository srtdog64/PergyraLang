#ifndef PGY_TRANSPILER_EXPR_STDLIB_CHANNEL_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_CHANNEL_BUILTIN_H

#include "transpiler.h"

char *emit_call_stdlib_channel_builtin(const char *fn,
                                       ASTNode *call,
                                       TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_EXPR_STDLIB_CHANNEL_BUILTIN_H */
