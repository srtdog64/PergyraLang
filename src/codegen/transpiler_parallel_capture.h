#ifndef PGY_TRANSPILER_PARALLEL_CAPTURE_H
#define PGY_TRANSPILER_PARALLEL_CAPTURE_H

#include <stdbool.h>

#include "../parser/ast.h"
#include "transpiler_context.h"

/* Owner for discovering locals captured by generated C parallel blocks. */

void transpiler_parallel_collect_stmt_captures(ASTNode *node,
                                               TranspilerCtx *ctx,
                                               char slot_names[MAX_SLOT_VARS][64],
                                               int *slot_count,
                                               char typed_names[MAX_SLOT_VARS][64],
                                               ASTNode *typed_type_asts[MAX_SLOT_VARS],
                                               bool typed_is_event_handler[MAX_SLOT_VARS],
                                               int *typed_count);

#endif /* PGY_TRANSPILER_PARALLEL_CAPTURE_H */
