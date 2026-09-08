#ifndef PGY_TRANSPILER_MIR_PHI_TYPE_OWNER_H
#define PGY_TRANSPILER_MIR_PHI_TYPE_OWNER_H

#include "transpiler.h"

typedef struct {
    bool is_phi;
    const char *type_name;
} TranspilerMIRPhiType;

/* One projection per routine. Types borrow MIR-owned strings. Unknown or
 * crossed incoming facts fail before a phi can use a spelling-based type. */
bool transpiler_mir_phi_types_from_incomings(
    TranspilerCtx *ctx, const MIRRoutine *routine,
    const char *const *names, size_t count, TranspilerMIRPhiType *out);

#endif
