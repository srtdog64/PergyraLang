#ifndef PGY_TRANSPILER_EXPR_ASSIGNMENT_EMIT_H
#define PGY_TRANSPILER_EXPR_ASSIGNMENT_EMIT_H

#include "transpiler.h"

char *transpiler_emit_assignment_expression_parts(TranspilerCtx *ctx,
                                                  ASTNode *target_node,
                                                  ASTNode *value_node);

#endif /* PGY_TRANSPILER_EXPR_ASSIGNMENT_EMIT_H */
