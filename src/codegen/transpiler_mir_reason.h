#ifndef PGY_TRANSPILER_MIR_REASON_H
#define PGY_TRANSPILER_MIR_REASON_H

#include <stddef.h>

void transpiler_mir_reasonf(char *reason, size_t reason_cap,
                            const char *fmt, ...);

#endif /* PGY_TRANSPILER_MIR_REASON_H */
