#ifndef PGY_SRC_CODEGEN_TRANSPILER_EVENT_BUILTIN_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_EVENT_BUILTIN_EMIT_H

#include "transpiler.h"

char *emit_call_event_builtin(ASTNode *call, ASTNode *callee,
                              TranspilerCtx *ctx);
#endif /* PGY_SRC_CODEGEN_TRANSPILER_EVENT_BUILTIN_EMIT_H */
