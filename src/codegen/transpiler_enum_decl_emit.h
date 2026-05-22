#ifndef PGY_TRANSPILER_ENUM_DECL_EMIT_H
#define PGY_TRANSPILER_ENUM_DECL_EMIT_H

#include "../parser/ast.h"
#include "transpiler.h"

void emit_enum_decl_stmt(ASTNode *node, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_ENUM_DECL_EMIT_H */
