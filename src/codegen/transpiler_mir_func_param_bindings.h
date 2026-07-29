#ifndef PGY_TRANSPILER_MIR_FUNC_PARAM_BINDINGS_H
#define PGY_TRANSPILER_MIR_FUNC_PARAM_BINDINGS_H

#include <stdbool.h>

#include "../compiler/mir.h"
#include "transpiler.h"

bool transpiler_register_mir_func_param_bindings(
    TranspilerCtx *ctx,
    const MIRRoutine *mir_routine,
    const char *function_name,
    bool is_method,
    bool mir_active);

#endif /* PGY_TRANSPILER_MIR_FUNC_PARAM_BINDINGS_H */
