#ifndef PGY_TRANSPILER_EXPR_TYPE_INFER_H
#define PGY_TRANSPILER_EXPR_TYPE_INFER_H

#include "transpiler.h"

const char *transpiler_expr_infer_type_name(TranspilerCtx *ctx,
                                            ASTNode *expr);

/*
 * Compatibility name for the remaining implementation-header consumers.
 * New linked owners should use transpiler_expr_infer_type_name directly.
 */
#define infer_expression_type_name transpiler_expr_infer_type_name

#endif /* PGY_TRANSPILER_EXPR_TYPE_INFER_H */
