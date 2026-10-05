#ifndef PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H
#define PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H

#include "transpiler.h"

void emit_parallel_block(ASTNode *node, TranspilerCtx *ctx);
void emit_async_block(ASTNode *node, TranspilerCtx *ctx);
/* Join form (docs/181 SS1 rungs 0-2; transpiler_parallel_join_emit.c). */
void emit_parallel_join_block(ASTNode *node, TranspilerCtx *ctx);
/* Expression form (docs/181 R2): the whole fan-out as a GNU statement
 * expression yielding a PgyArray_<R> value (give results, index order). */
char *transpiler_emit_parallel_join_expr_parts(ASTNode *node,
                                               TranspilerCtx *ctx);

/* Wrapper emission state shared with the join-form emitter
 * (transpiler_parallel_join_emit.c, docs/181 SS1). Enter points the
 * output at ctx->wrappers, where the caller has opened the wrapper
 * function, and installs the capture-name rewrite tables; restore puts
 * everything back.
 *
 * While the body is emitted, ctx->wrappers is a pending buffer: a file-scope
 * definition that the body emits (a nested spawn wrapper, an inner parallel
 * wrapper) lands there instead of inside the open function. After writing
 * the function's closing brace the caller flushes the pending buffer. */
typedef struct TranspilerParallelWrapperState {
    CodeBuf *out;
    CodeBuf *wrappers;
    CodeBuf *pending;
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

/* Append the file-scope definitions the body emitted, after the wrapper
 * function the caller has just closed. */
void transpiler_parallel_wrapper_state_flush(
    TranspilerCtx *ctx,
    TranspilerParallelWrapperState *state);

/* Capture-address emission (channel bindings stay on their raw source
 * name; everything else resolves through the SSA map). */
void transpiler_write_capture_address(TranspilerCtx *ctx, const char *name);

#endif /* PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H */
