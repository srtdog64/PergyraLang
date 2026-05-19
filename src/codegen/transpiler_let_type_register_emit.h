#ifndef PGY_TRANSPILER_LET_TYPE_REGISTER_EMIT_H
#define PGY_TRANSPILER_LET_TYPE_REGISTER_EMIT_H

#include "transpiler.h"

void transpiler_register_let_type_after_emit(TranspilerCtx *ctx,
                                             const char *name,
                                             ASTNode *init,
                                             char *ann_type_name);

#endif /* PGY_TRANSPILER_LET_TYPE_REGISTER_EMIT_H */
