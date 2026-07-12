#ifndef PGY_RUNTIME_CHANNEL_LIFECYCLE_INLINE_H
#define PGY_RUNTIME_CHANNEL_LIFECYCLE_INLINE_H

#include <stdbool.h>
#include <stdio.h>

static inline void
pgy_channel_require_operable(bool ch_is_null, bool ch_uninitialized,
                             bool out_is_null, const char *op)
{
    const char *what;

    if (!ch_is_null && !ch_uninitialized && !out_is_null)
        return;
    what = ch_is_null ? "null channel"
         : out_is_null ? "null output pointer"
                       : "uninitialized channel";
    fprintf(stderr, "%s channel lifecycle violation: op=%s on %s\n",
            PGY_RUNTIME_PANIC_PREFIX, op, what);
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_LIFECYCLE_STATE,
                      PGY_RUNTIME_PANIC_REASON_INVALID_LIFECYCLE_STATE);
}

#endif /* PGY_RUNTIME_CHANNEL_LIFECYCLE_INLINE_H */
