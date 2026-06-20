#ifndef PGY_TRANSPILER_EXPR_CALL_TYPE_INFER_H
#define PGY_TRANSPILER_EXPR_CALL_TYPE_INFER_H

#include "transpiler.h"

const char *transpiler_expr_infer_call_type_name(TranspilerCtx *ctx,
                                                 ASTNode *expr);

#endif /* PGY_TRANSPILER_EXPR_CALL_TYPE_INFER_H */
