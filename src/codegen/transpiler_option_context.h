#ifndef PGY_TRANSPILER_OPTION_CONTEXT_H
#define PGY_TRANSPILER_OPTION_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

const char *transpiler_contextual_option_type_name(TranspilerCtx *ctx);
bool transpiler_contextual_option_inner_type_copy(TranspilerCtx *ctx,
                                                  char *out,
                                                  size_t out_size);
char *transpiler_emit_none_with_context(TranspilerCtx *ctx, ASTNode *site);

#endif /* PGY_TRANSPILER_OPTION_CONTEXT_H */
