#ifndef PGY_RUNTIME_INTENT_IDENTITY_H
#define PGY_RUNTIME_INTENT_IDENTITY_H

#include <stdint.h>
#include <stddef.h>

/* Public handles are authority-bearing int32 ABI values, not recyclable
 * registry indexes. No internal generation can protect a stale public caller
 * that supplies only that value. Exhaustion therefore latches at zero instead
 * of recycling an identity; trace IDs remain a separate observational field.
 * The registry mutex owns serialization of this issuance operation. */
static inline int32_t
pgy_intent_issue_handle(int32_t *next_handle)
{
    int32_t issued;

    if (next_handle == NULL || *next_handle <= 0)
        return 0;
    issued = *next_handle;
    *next_handle = issued == INT32_MAX ? 0 : issued + 1;
    return issued;
}

#endif /* PGY_RUNTIME_INTENT_IDENTITY_H */
