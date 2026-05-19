#ifndef PGY_TRANSPILER_INTENT_OBSERVABILITY_BUILTIN_EMIT_H
#define PGY_TRANSPILER_INTENT_OBSERVABILITY_BUILTIN_EMIT_H

#include "../semantic/builtin_kind.h"
#include "transpiler.h"

char *emit_builtin_intent_observability(ASTNode *call,
                                        BuiltinKind bk,
                                        TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_INTENT_OBSERVABILITY_BUILTIN_EMIT_H */
