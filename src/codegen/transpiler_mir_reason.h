#ifndef PGY_TRANSPILER_MIR_REASON_H
#define PGY_TRANSPILER_MIR_REASON_H

#include <stdarg.h>
#include <stdio.h>

static void
transpiler_mir_reasonf(char *reason, size_t reason_cap,
                       const char *fmt, ...)
{
    va_list ap;
    int written;

    if (reason == NULL || reason_cap == 0 || fmt == NULL)
        return;
    va_start(ap, fmt);
    written = vsnprintf(reason, reason_cap, fmt, ap);
    va_end(ap);
    if (written < 0)
        reason[0] = '\0';
}

#endif /* PGY_TRANSPILER_MIR_REASON_H */
