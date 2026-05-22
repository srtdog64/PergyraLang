#ifndef PGY_MIR_LOWER_PUBLIC_API_H
#define PGY_MIR_LOWER_PUBLIC_API_H

#include "mir.h"

MIRProgram *mir_lower(const HIRProgram *hir,
                      const RIRProgram *rir,
                      char **error_message);

#endif /* PGY_MIR_LOWER_PUBLIC_API_H */
