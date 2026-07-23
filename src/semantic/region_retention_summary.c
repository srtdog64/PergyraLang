#include "region_retention_summary.h"

#include "builtin_kind.h"

bool
semantic_region_retention_summary_for_builtin(
    uint32_t builtin_kind,
    size_t argument_index,
    PgyRegionRetentionKind *kind_out)
{
    if (kind_out != NULL)
        *kind_out = PGY_REGION_RETENTION_UNKNOWN;
    if (argument_index != 0)
        return false;
    if (builtin_kind != (uint32_t)BUILTIN_PRINT)
        return false;
    if (kind_out != NULL)
        *kind_out = PGY_REGION_RETENTION_BORROWED_FOR_CALL;
    return true;
}
