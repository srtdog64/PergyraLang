#ifndef PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H
#define PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H

#include "transpiler.h"

void emit_parallel_block(ASTNode *node, TranspilerCtx *ctx);
void emit_async_block(ASTNode *node, TranspilerCtx *ctx);
/* Join form (docs/181 SS1 rung 0; transpiler_parallel_join_emit.c). */
void emit_parallel_join_block(ASTNode *node, TranspilerCtx *ctx);

/* Wrapper emission state shared with the join-form emitter
 * (transpiler_parallel_join_emit.c, docs/181 SS1). Enter swaps the
 * output buffer to ctx->helpers and installs the capture-name rewrite
 * tables; restore puts everything back. */
typedef struct TranspilerParallelWrapperState {
    CodeBuf *out;
    int indent;
    bool in_parallel_wrapper;
    int slot_count;
    int typed_count;
    char slot_names[MAX_SLOT_VARS][64];
    char typed_names[MAX_SLOT_VARS][64];
    bool typed_snapshot[MAX_SLOT_VARS];
} TranspilerParallelWrapperState;

void transpiler_parallel_wrapper_state_enter(
    TranspilerCtx *ctx,
    TranspilerParallelWrapperState *state,
    char capture_slot_names[MAX_SLOT_VARS][64],
    int capture_slot_count,
    char capture_typed_names[MAX_SLOT_VARS][64],
    int capture_typed_count,
    const bool capture_typed_snapshot[MAX_SLOT_VARS]);

void transpiler_parallel_wrapper_state_restore(
    TranspilerCtx *ctx,
    const TranspilerParallelWrapperState *state);

/* Capture-address emission (channel bindings stay on their raw source
 * name; everything else resolves through the SSA map). */
void transpiler_write_capture_address(TranspilerCtx *ctx, const char *name);

#endif /* PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H */
