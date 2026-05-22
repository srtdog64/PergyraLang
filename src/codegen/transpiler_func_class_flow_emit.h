#ifndef PGY_TRANSPILER_FUNC_CLASS_FLOW_EMIT_H
#define PGY_TRANSPILER_FUNC_CLASS_FLOW_EMIT_H

#include "transpiler.h"

void emit_func_decl_named(ASTNode *node, const char *emitted_name,
                          CodeBuf *buf, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_FUNC_CLASS_FLOW_EMIT_H */
