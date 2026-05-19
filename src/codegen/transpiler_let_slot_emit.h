#ifndef PGY_TRANSPILER_LET_SLOT_EMIT_H
#define PGY_TRANSPILER_LET_SLOT_EMIT_H

#include <stdbool.h>

#include "transpiler.h"

const char *transpiler_let_slot_inner_from_call_type_arg(TranspilerCtx *ctx,
                                                         ASTNode *call);
bool transpiler_try_emit_let_slot_claim(ASTNode *node,
                                        TranspilerCtx *ctx,
                                        const char *name,
                                        ASTNode *init,
                                        ASTNode *ann,
                                        char **ann_type_name_io);
bool transpiler_try_emit_let_slot_view_or_move(TranspilerCtx *ctx,
                                               const char *name,
                                               ASTNode *init,
                                               ASTNode *ann,
                                               char **ann_type_name_io);
bool transpiler_try_emit_let_slot_sugar(TranspilerCtx *ctx,
                                        const char *name,
                                        ASTNode *init,
                                        ASTNode *ann,
                                        char **ann_type_name_io);

#endif /* PGY_TRANSPILER_LET_SLOT_EMIT_H */
