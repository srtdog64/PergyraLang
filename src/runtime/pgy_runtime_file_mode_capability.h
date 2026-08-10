#ifndef PGY_RUNTIME_FILE_MODE_CAPABILITY_H
#define PGY_RUNTIME_FILE_MODE_CAPABILITY_H

/* One owner for the FileOpen mode -> ambient capability contract. Semantic
 * analysis uses it for literal modes; both native runtime twins use it for the
 * concrete mode at the effect boundary. Unknown/dynamic modes fail closed by
 * requiring both capabilities. */

#include <stdint.h>

#include "pgy_runtime_capability.h"

static inline uint32_t
pgy_file_mode_capability_mask(const char *mode)
{
    uint32_t mask = PGY_CAP_NONE;

    if (mode == NULL || mode[0] == '\0')
        return PGY_CAP_IO_READ | PGY_CAP_IO_WRITE;
    for (const char *p = mode; *p != '\0'; p++) {
#define PGY_FILE_MODE_CAPABILITY_CAP_IO_READ PGY_CAP_IO_READ
#define PGY_FILE_MODE_CAPABILITY_CAP_IO_WRITE PGY_CAP_IO_WRITE
#define PGY_FILE_MODE_CAPABILITY_CAP_IO_READ_WRITE \
    (PGY_CAP_IO_READ | PGY_CAP_IO_WRITE)
#define PGY_FILE_MODE_CAPABILITY(character, capability_identity)             \
        if (*p == (character))                                               \
            mask |= PGY_FILE_MODE_CAPABILITY_##capability_identity;
#include "pgy_file_mode_capability.def"
#undef PGY_FILE_MODE_CAPABILITY
#undef PGY_FILE_MODE_CAPABILITY_CAP_IO_READ_WRITE
#undef PGY_FILE_MODE_CAPABILITY_CAP_IO_WRITE
#undef PGY_FILE_MODE_CAPABILITY_CAP_IO_READ
    }
    return mask != PGY_CAP_NONE
        ? mask
        : PGY_CAP_IO_READ | PGY_CAP_IO_WRITE;
}

#endif /* PGY_RUNTIME_FILE_MODE_CAPABILITY_H */
