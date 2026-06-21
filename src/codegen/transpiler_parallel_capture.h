#ifndef PGY_TRANSPILER_PARALLEL_CAPTURE_H
#define PGY_TRANSPILER_PARALLEL_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"
#include "transpiler_context.h"

/* Owner for discovering locals captured by generated C parallel blocks. */

typedef struct TranspilerParallelCallableCapture {
    bool is_callable;
    const char *return_type_name;
    size_t param_count;
    char *const *param_type_names;
} TranspilerParallelCallableCapture;

void transpiler_parallel_collect_stmt_captures(ASTNode *node,
                                               TranspilerCtx *ctx,
                                               char slot_names[MAX_SLOT_VARS][64],
                                               int *slot_count,
                                               char typed_names[MAX_SLOT_VARS][64],
                                               TranspilerParallelCallableCapture typed_callables[MAX_SLOT_VARS],
                                               int *typed_count);

#endif /* PGY_TRANSPILER_PARALLEL_CAPTURE_H */
