#ifndef PGY_SRC_CODEGEN_TRANSPILER_EXPR_CORE_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_EXPR_CORE_EMIT_H

#include "transpiler.h"
#include "transpiler_expr_core_builtins_emit.h"

char *emit_binary(ASTNode *expr, TranspilerCtx *ctx);
char *transpiler_emit_scalar_operation(PgyTokenType op_type, const char *left_type,
    const char *right_type, const char *left, const char *right,
    const ASTNode *right_expr);
char *transpiler_sequence_scalar_operation(TranspilerCtx *ctx, PgyTokenType op,
    const char *type, const char *left, const char *right, const ASTNode *right_expr);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_EXPR_CORE_EMIT_H */
