#ifndef PGY_TRANSPILER_CONTROL_FLOW_EMIT_H
#define PGY_TRANSPILER_CONTROL_FLOW_EMIT_H

#include "transpiler.h"

int transpiler_find_loop_label_depth(const TranspilerCtx *ctx,
                                     const char *label);
void transpiler_write_condition_head(TranspilerCtx *ctx,
                                     const char *keyword,
                                     const char *expr,
                                     const char *suffix);
void emit_if_stmt(ASTNode *node, TranspilerCtx *ctx);
void emit_for_loop(ASTNode *node, TranspilerCtx *ctx);
void emit_while_loop(ASTNode *node, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_CONTROL_FLOW_EMIT_H */
