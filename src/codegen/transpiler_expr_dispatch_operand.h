#ifndef PERGYRA_TRANSPILER_EXPR_DISPATCH_OPERAND_H
#define PERGYRA_TRANSPILER_EXPR_DISPATCH_OPERAND_H

#include "transpiler_context.h"
#include "../parser/ast.h"

char *transpiler_dispatch_emit_part(TranspilerCtx *ctx,
                                    ASTNode *expr,
                                    const char *owner,
                                    const char *role);

#endif /* PERGYRA_TRANSPILER_EXPR_DISPATCH_OPERAND_H */
