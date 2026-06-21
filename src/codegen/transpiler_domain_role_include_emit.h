#ifndef PGY_TRANSPILER_DOMAIN_ROLE_INCLUDE_EMIT_H
#define PGY_TRANSPILER_DOMAIN_ROLE_INCLUDE_EMIT_H

#include "../parser/ast.h"
#include "transpiler_context.h"

void emit_included_role_impls(ASTNode *role, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_DOMAIN_ROLE_INCLUDE_EMIT_H */
