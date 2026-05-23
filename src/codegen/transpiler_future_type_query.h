#ifndef PGY_TRANSPILER_FUTURE_TYPE_QUERY_H
#define PGY_TRANSPILER_FUTURE_TYPE_QUERY_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

const char *infer_spawn_return_type_name_scratch(TranspilerCtx *ctx,
                                                 ASTNode *spawn_expr);
bool infer_spawn_return_type_name_copy(TranspilerCtx *ctx,
                                       ASTNode *spawn_expr,
                                       char *out,
                                       size_t out_size);
bool is_remote_future_expr(TranspilerCtx *ctx, ASTNode *expr);
bool lookup_future_inner_type_copy(TranspilerCtx *ctx,
                                   ASTNode *expr,
                                   char *out,
                                   size_t out_size);

#endif /* PGY_TRANSPILER_FUTURE_TYPE_QUERY_H */
