#include "region_retention_summary.h"

#include "builtin_kind.h"

typedef struct PgyBuiltinArgumentRetentionRow {
    uint32_t builtin_kind;
    size_t argument_index;
    PgyRegionRetentionKind retention;
} PgyBuiltinArgumentRetentionRow;

#define PGY_RETENTION_ARGUMENT_ANY ((size_t)-1)
#define PGY_BUILTIN_ARGUMENT_RETENTION(identity, source_name, argument, kind) \
    { (uint32_t)BUILTIN_##identity, (argument), (kind) },
static const PgyBuiltinArgumentRetentionRow builtin_argument_retention_rows[] = {
#include "builtin_argument_retention_registry.def"
};
#undef PGY_BUILTIN_ARGUMENT_RETENTION
#undef PGY_RETENTION_ARGUMENT_ANY

bool
semantic_region_retention_summary_for_builtin(
    uint32_t builtin_kind,
    size_t argument_index,
    PgyRegionRetentionKind *kind_out)
{
    size_t row;

    if (kind_out != NULL)
        *kind_out = PGY_REGION_RETENTION_UNKNOWN;
    for (row = 0;
         row < sizeof(builtin_argument_retention_rows)
                 / sizeof(builtin_argument_retention_rows[0]);
         row++) {
        const PgyBuiltinArgumentRetentionRow *candidate =
            &builtin_argument_retention_rows[row];
        if (candidate->builtin_kind != builtin_kind ||
            (candidate->argument_index != (size_t)-1 &&
             candidate->argument_index != argument_index))
            continue;
        if (kind_out != NULL)
            *kind_out = candidate->retention;
        return true;
    }
    return false;
}
