#ifndef PGY_TRANSPILER_LET_BOX_EMIT_H
#define PGY_TRANSPILER_LET_BOX_EMIT_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_try_emit_box_family_let(TranspilerCtx *ctx,
                                        const char *name,
                                        ASTNode *init,
                                        ASTNode *ann,
                                        char **ann_type_name_ptr);

#endif /* PGY_TRANSPILER_LET_BOX_EMIT_H */
