#ifndef PGY_TRANSPILER_CONSTRUCTOR_CHANNEL_GUARD_H
#define PGY_TRANSPILER_CONSTRUCTOR_CHANNEL_GUARD_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_constructor_field_is_channel(TranspilerCtx *ctx,
                                             ASTNode *field_type);
const char *transpiler_constructor_find_channel_field(TranspilerCtx *ctx,
                                                      ASTNode *decl);
bool transpiler_constructor_reject_channel_field(TranspilerCtx *ctx,
                                                 const char *field_name);

#endif /* PGY_TRANSPILER_CONSTRUCTOR_CHANNEL_GUARD_H */
