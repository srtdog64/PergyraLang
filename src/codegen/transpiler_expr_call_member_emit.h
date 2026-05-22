#ifndef PGY_TRANSPILER_EXPR_CALL_MEMBER_EMIT_H
#define PGY_TRANSPILER_EXPR_CALL_MEMBER_EMIT_H

#include "transpiler.h"

char *emit_call_member_style(ASTNode *call, ASTNode *callee,
                             TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_EXPR_CALL_MEMBER_EMIT_H */
