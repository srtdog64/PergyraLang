#ifndef PGY_TRANSPILER_LET_CHANNEL_EMIT_H
#define PGY_TRANSPILER_LET_CHANNEL_EMIT_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_try_emit_channel_let(TranspilerCtx *ctx,
                                     const char *name,
                                     ASTNode *init,
                                     char **ann_type_name_io);

#endif /* PGY_TRANSPILER_LET_CHANNEL_EMIT_H */
