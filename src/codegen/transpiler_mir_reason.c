/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR reason-buffer formatting helper.
 */

#include "transpiler_mir_reason.h"

#include <stdarg.h>
#include <stdio.h>

void
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
