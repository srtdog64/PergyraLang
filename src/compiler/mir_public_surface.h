#ifndef PGY_MIR_PUBLIC_SURFACE_H
#define PGY_MIR_PUBLIC_SURFACE_H

#include "mir.h"

/* Public MIR validation/inventory surface. Declarations live in mir.h unless
 * the function is an owner-local inventory primitive shared by MIR owners. */
void mir_count_non_cfg_body_fallback_inventory(const MIRProgram *mir,
                                               size_t *fallback_total,
                                               size_t *fallback_routines);

#endif /* PGY_MIR_PUBLIC_SURFACE_H */
