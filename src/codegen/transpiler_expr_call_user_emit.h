#ifndef PGY_SRC_CODEGEN_TRANSPILER_EXPR_CALL_USER_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_EXPR_CALL_USER_EMIT_H

#include "transpiler.h"

char *emit_call_user_function(ASTNode *call,
                              ASTNode *callee,
                              TranspilerCtx *ctx);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_EXPR_CALL_USER_EMIT_H */
