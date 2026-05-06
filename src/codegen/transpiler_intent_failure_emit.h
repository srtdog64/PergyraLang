#ifndef PGY_SRC_CODEGEN_TRANSPILER_INTENT_FAILURE_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_INTENT_FAILURE_EMIT_H

#include "transpiler.h"

void emit_intent_step_condition_failure(CodeBuf *out,
                                        TranspilerCtx *ctx,
                                        const char *condition_expr,
                                        const char *phase,
                                        const char *step_name,
                                        const char *intent_name,
                                        bool emit_cleanup_from_mir,
                                        size_t cleanup_block);
#endif /* PGY_SRC_CODEGEN_TRANSPILER_INTENT_FAILURE_EMIT_H */
