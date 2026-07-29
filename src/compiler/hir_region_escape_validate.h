#ifndef PGY_HIR_REGION_ESCAPE_VALIDATE_H
#define PGY_HIR_REGION_ESCAPE_VALIDATE_H

#include "hir.h"

bool hir_validate_region_escape_facts(
    const HIRProgram *hir,
    const HIRRoutineInventory *inventory,
    char **error_message);

#endif /* PGY_HIR_REGION_ESCAPE_VALIDATE_H */
