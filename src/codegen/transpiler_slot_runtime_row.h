#ifndef PGY_TRANSPILER_SLOT_RUNTIME_ROW_H
#define PGY_TRANSPILER_SLOT_RUNTIME_ROW_H

#include <stdbool.h>

#include "transpiler.h"

const char *transpiler_slot_runtime_fn(TranspilerCtx *ctx,
                                       bool secure,
                                       const char *inner_type,
                                       const char *operation);

#endif /* PGY_TRANSPILER_SLOT_RUNTIME_ROW_H */
