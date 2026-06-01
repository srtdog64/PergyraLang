#ifndef PGY_RUNTIME_PROCESS_EXIT_H
#define PGY_RUNTIME_PROCESS_EXIT_H

#include <stdint.h>
#include <stdlib.h>

/*
 * Explicit language-level Exit(Int) owner.
 *
 * This is not a panic path: user code requested process termination.
 * Keep raw exit() here so runtime hard-fail auditing can distinguish
 * intentional process exit from internal invariant failure.
 */
static inline void
pgy_runtime_process_exit(int32_t code)
{
    exit((int)code);
}

#endif /* PGY_RUNTIME_PROCESS_EXIT_H */
