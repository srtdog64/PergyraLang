#ifndef PGY_TRANSPILER_FUNC_FORWARD_HELPERS_H
#define PGY_TRANSPILER_FUNC_FORWARD_HELPERS_H

#include "transpiler_future_type_query.h"

void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx);

void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx);
#endif /* PGY_TRANSPILER_FUNC_FORWARD_HELPERS_H */
