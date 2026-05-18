#ifndef PGY_TRANSPILER_CHANNEL_TYPE_QUERY_H
#define PGY_TRANSPILER_CHANNEL_TYPE_QUERY_H

#include <stddef.h>

#include "transpiler.h"

bool channel_inner_type_name_copy(TranspilerCtx *ctx, ASTNode *expr,
                                  char *out, size_t out_size);
const char *transpiler_require_channel_inner_type(TranspilerCtx *ctx,
                                                  ASTNode *expr,
                                                  const char *operation,
                                                  char *inner_buf,
                                                  size_t inner_buf_size);

#endif /* PGY_TRANSPILER_CHANNEL_TYPE_QUERY_H */
