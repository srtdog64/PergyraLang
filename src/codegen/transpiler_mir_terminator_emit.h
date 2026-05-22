#ifndef PGY_TRANSPILER_MIR_TERMINATOR_EMIT_H
#define PGY_TRANSPILER_MIR_TERMINATOR_EMIT_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler_mir_expr_ssa.h"

/* C backend MIR terminator emission owner. */

bool transpiler_emit_mir_explicit_terminator(
    ASTNode *node,
    const MIRRoutine *mir_routine,
    const MIRBasicBlock *block,
    size_t block_index,
    const char *name,
    TranspilerCtx *ctx,
    TranspilerSSANameMap *block_ssa_map,
    bool *terminator_emitted,
    char *block_reason,
    size_t block_reason_cap);

bool transpiler_emit_mir_fallthrough_terminator(
    const MIRRoutine *mir_routine,
    const MIRBasicBlock *block,
    size_t block_index,
    const char *name,
    TranspilerCtx *ctx,
    char *block_reason,
    size_t block_reason_cap);

#endif /* PGY_TRANSPILER_MIR_TERMINATOR_EMIT_H */
