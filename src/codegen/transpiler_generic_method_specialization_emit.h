#ifndef PGY_TRANSPILER_GENERIC_METHOD_SPECIALIZATION_EMIT_H
#define PGY_TRANSPILER_GENERIC_METHOD_SPECIALIZATION_EMIT_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_emit_generic_method_specialization_forwards(
    TranspilerCtx *ctx,
    const char *host_name,
    const MIRDeclMethod *method_meta,
    const MIRRoutine *method_routine,
    bool pointer_self);
bool transpiler_emit_generic_method_specialization_bodies(
    TranspilerCtx *ctx,
    const MIRRoutine *method_routine);

#endif /* PGY_TRANSPILER_GENERIC_METHOD_SPECIALIZATION_EMIT_H */
