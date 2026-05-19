#ifndef PGY_TRANSPILER_EXPR_IO_BUILTIN_H
#define PGY_TRANSPILER_EXPR_IO_BUILTIN_H

#include "transpiler.h"
#include "../semantic/builtin_kind.h"

char *emit_builtin_io(ASTNode *call, BuiltinKind bk, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_EXPR_IO_BUILTIN_H */
