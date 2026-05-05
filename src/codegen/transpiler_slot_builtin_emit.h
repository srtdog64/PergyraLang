#ifndef PGY_TRANSPILER_SLOT_BUILTIN_EMIT_H
#define PGY_TRANSPILER_SLOT_BUILTIN_EMIT_H

#include "transpiler.h"

char *emit_builtin_claim_slot(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_claim_device_slot(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_write(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_view(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_read(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_release(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_device_write(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_device_read(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_release_device_slot(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_submit_device_read(ASTNode *call, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_SLOT_BUILTIN_EMIT_H */
