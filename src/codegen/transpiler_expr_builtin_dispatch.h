#ifndef PGY_TRANSPILER_EXPR_BUILTIN_DISPATCH_H
#define PGY_TRANSPILER_EXPR_BUILTIN_DISPATCH_H

#include <stdbool.h>

#include "../semantic/builtin_kind.h"
#include "transpiler.h"

char *emit_call_builtin_dispatch(ASTNode *call,
                                 BuiltinKind bk,
                                 TranspilerCtx *ctx,
                                 bool *handled);

#endif /* PGY_TRANSPILER_EXPR_BUILTIN_DISPATCH_H */
