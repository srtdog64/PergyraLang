#ifndef PGY_SRC_CODEGEN_TRANSPILER_EXPR_STDLIB_MISC_BUILTIN_H
#define PGY_SRC_CODEGEN_TRANSPILER_EXPR_STDLIB_MISC_BUILTIN_H

#include "transpiler.h"

char *emit_call_stdlib_misc_builtin(const char *fn,
                                    ASTNode *call,
                                    TranspilerCtx *ctx);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_EXPR_STDLIB_MISC_BUILTIN_H */
