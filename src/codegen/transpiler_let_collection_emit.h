#ifndef PGY_TRANSPILER_LET_COLLECTION_EMIT_H
#define PGY_TRANSPILER_LET_COLLECTION_EMIT_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_try_emit_option_let(TranspilerCtx *ctx,
                                    const char *name,
                                    ASTNode *init,
                                    char **ann_type_name_io);

bool transpiler_try_emit_collection_ctor_let(TranspilerCtx *ctx,
                                             const char *name,
                                             ASTNode *init,
                                             ASTNode *resolved_ann,
                                             const char *resolved_ann_type_name,
                                             char **ann_type_name_io);

#endif /* PGY_TRANSPILER_LET_COLLECTION_EMIT_H */
