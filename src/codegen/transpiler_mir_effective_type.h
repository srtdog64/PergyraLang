#ifndef PGY_TRANSPILER_MIR_EFFECTIVE_TYPE_H
#define PGY_TRANSPILER_MIR_EFFECTIVE_TYPE_H

#include "../parser/ast.h"
#include "transpiler.h"

char *transpiler_render_effective_local_type_name(TranspilerCtx *ctx,
                                                  ASTNode *type_node);

#endif /* PGY_TRANSPILER_MIR_EFFECTIVE_TYPE_H */
