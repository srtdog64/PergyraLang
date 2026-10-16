#ifndef PGY_TRANSPILER_EXPR_CALL_MEMBER_EMIT_H
#define PGY_TRANSPILER_EXPR_CALL_MEMBER_EMIT_H

#include "transpiler.h"

char *emit_call_member_style(ASTNode *call, ASTNode *callee,
                             TranspilerCtx *ctx);

char *transpiler_member_call_emit_part(TranspilerCtx *ctx, ASTNode *expr,
                                      const char *method_name, const char *role);
char *transpiler_emit_member_slot(ASTNode *call, ASTNode *obj,
                                  const char *method, TranspilerCtx *ctx,
                                  bool *handled);

#endif /* PGY_TRANSPILER_EXPR_CALL_MEMBER_EMIT_H */
