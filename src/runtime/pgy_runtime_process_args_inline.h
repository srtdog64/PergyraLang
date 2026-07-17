#ifndef PGY_RUNTIME_PROCESS_ARGS_INLINE_H
#define PGY_RUNTIME_PROCESS_ARGS_INLINE_H

#include <stdint.h>
#include "pgy_runtime_linkage.h"

/*
 * Process argument snapshot owner for C-backend generated binaries.
 * argv remains borrowed from main(); Args() returns an owned Array<String>.
 */
PGY_RT_GLOBAL int32_t pgy_runtime_argc
#ifndef PGY_RUNTIME_DECLS_ONLY
    = 0
#endif
;
PGY_RT_GLOBAL char **pgy_runtime_argv
#ifndef PGY_RUNTIME_DECLS_ONLY
    = NULL
#endif
;

PGY_RT_DECL void
pgy_args_init(int32_t argc, char **argv)
#ifndef PGY_RUNTIME_DECLS_ONLY
{
    pgy_runtime_argc = argc;
    pgy_runtime_argv = argv;
}
#else
;
#endif

PGY_RT_DECL PgyArray_String
pgy_args(void)
#ifndef PGY_RUNTIME_DECLS_ONLY
{
    int32_t count;

    /* Gated on PGY_CAP_ENV: process arguments are an ambient fingerprinting
     * surface. Twin of the gate in pgy_runtime_process_args_exports.h. Only the
     * reader is gated; pgy_args_init (startup infra) is not. */
    pgy_cap_require_export(PGY_CAP_ENV, "args");
    count = (pgy_runtime_argc > 1 && pgy_runtime_argv != NULL)
        ? pgy_runtime_argc - 1
        : 0;
    PgyArray_String out = pgy_array_new_String((size_t)count);

    for (int32_t i = 1; i < pgy_runtime_argc; i++) {
        const char *arg = (pgy_runtime_argv != NULL && pgy_runtime_argv[i] != NULL)
            ? pgy_runtime_argv[i]
            : "";
        char *owned = pgy_runtime_strdup(arg);
        if (owned == NULL) {
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                              PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
        }
        pgy_array_push_String(&out, owned);
    }

    return out;
}
#else
;
#endif

#endif /* PGY_RUNTIME_PROCESS_ARGS_INLINE_H */
