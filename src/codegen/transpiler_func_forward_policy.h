#ifndef PGY_SRC_CODEGEN_TRANSPILER_FUNC_FORWARD_POLICY_H
#define PGY_SRC_CODEGEN_TRANSPILER_FUNC_FORWARD_POLICY_H

#include "transpiler.h"

/* Program stages at which a free-function prototype can be spelled in C, in
 * emission order. A prototype goes out at the first stage where every
 * signature type is nameable, so it precedes every hosted body emitted after
 * that stage:
 *   EARLY           before nominal layouts (runtime types, class typedefs)
 *   NOMINAL_LAYOUTS after class/enum layouts, before role/party/roster/
 *                   relation/effect/zone/world bodies (domain typedefs only)
 *   AFTER_ZONES     after zone layouts and bodies, before world bodies
 *   HOSTED_BODIES   after world layouts, before class/subject method bodies
 * NONE keeps only the late file-scope prototype (generic or unsupported
 * signatures, and constructed names without an owned specialization). */
typedef enum {
    TRANSPILER_FUNC_FORWARD_STAGE_NONE = 0,
    TRANSPILER_FUNC_FORWARD_STAGE_EARLY,
    TRANSPILER_FUNC_FORWARD_STAGE_NOMINAL_LAYOUTS,
    TRANSPILER_FUNC_FORWARD_STAGE_AFTER_ZONES,
    TRANSPILER_FUNC_FORWARD_STAGE_HOSTED_BODIES
} TranspilerFuncForwardStage;

TranspilerFuncForwardStage transpiler_func_forward_stage(TranspilerCtx *ctx,
                                                         ASTNode *func);
/* One stage per function, computed once per program. NULL when there are no
 * functions or on allocation failure (the backend error is set). */
TranspilerFuncForwardStage *transpiler_func_forward_stages_create(
    TranspilerCtx *ctx, ASTNode **functions, size_t function_count);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_FUNC_FORWARD_POLICY_H */
