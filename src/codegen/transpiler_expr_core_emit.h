#ifndef PGY_SRC_CODEGEN_TRANSPILER_EXPR_CORE_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_EXPR_CORE_EMIT_H

#include "transpiler.h"
#include "transpiler_expr_core_builtins_emit.h"

char *emit_binary(ASTNode *expr, TranspilerCtx *ctx);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_EXPR_CORE_EMIT_H */
