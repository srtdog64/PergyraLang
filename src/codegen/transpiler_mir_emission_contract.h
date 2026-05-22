#ifndef PGY_TRANSPILER_MIR_EMISSION_CONTRACT_H
#define PGY_TRANSPILER_MIR_EMISSION_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>

#include "../compiler/mir.h"
#include "../parser/ast.h"
#include "transpiler_context.h"

bool transpiler_can_emit_function_from_mir_with_reason(
    const TranspilerCtx *ctx,
    const ASTNode *func_decl,
    const MIRRoutine **mir_routine_out,
    char *reason,
    size_t reason_cap);

bool transpiler_can_emit_intent_cleanup_from_mir_with_reason(
    const TranspilerCtx *ctx,
    const ASTNode *intent_decl,
    const MIRRoutine **mir_routine_out,
    char *reason,
    size_t reason_cap);

#endif /* PGY_TRANSPILER_MIR_EMISSION_CONTRACT_H */
