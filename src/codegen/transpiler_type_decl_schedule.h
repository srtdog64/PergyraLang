#ifndef PGY_TRANSPILER_TYPE_DECL_SCHEDULE_H
#define PGY_TRANSPILER_TYPE_DECL_SCHEDULE_H

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"
#include "transpiler.h"

bool transpiler_emit_mir_type_declarations(TranspilerCtx *ctx,
                                           ASTNode **types,
                                           size_t type_count);

#endif /* PGY_TRANSPILER_TYPE_DECL_SCHEDULE_H */
