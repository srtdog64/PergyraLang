#ifndef PGY_SRC_CODEGEN_TRANSPILER_EXPR_CALL_USER_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_EXPR_CALL_USER_EMIT_H

#include "transpiler.h"

char *emit_call_user_function(ASTNode *call,
                              ASTNode *callee,
                              TranspilerCtx *ctx);

/* Bind argument pieces [starts[i], ends[i]) of `args` to temporaries in
 * source order when C would otherwise choose their order; returns the
 * temporaries' declarations, "" when no order is needed, NULL on failure. */
char *transpiler_user_call_order_args(ASTNode *call, CodeBuf *args,
                                      const size_t *starts, const size_t *ends,
                                      const bool *inline_piece);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_EXPR_CALL_USER_EMIT_H */
