#ifndef PGY_RUNTIME_PROCESS_ARGS_EXPORTS_H
#define PGY_RUNTIME_PROCESS_ARGS_EXPORTS_H

#include <stdint.h>

/*
 * Process argument snapshot owner for LLVM-linked runtime binaries.
 * argv remains borrowed from main(); Args() returns an owned Array<String>.
 */
static int32_t pgy_runtime_export_argc = 0;
static char **pgy_runtime_export_argv = NULL;

void
pgy_args_init(int32_t argc, char **argv)
{
    pgy_runtime_export_argc = argc;
    pgy_runtime_export_argv = argv;
}

PgyArray_String
pgy_args(void)
{
    int32_t count =
        (pgy_runtime_export_argc > 1 && pgy_runtime_export_argv != NULL)
            ? pgy_runtime_export_argc - 1
            : 0;
    PgyArray_String out = pgy_array_new_String((size_t)count);

    for (int32_t i = 1; i < pgy_runtime_export_argc; i++) {
        const char *arg = (pgy_runtime_export_argv != NULL
                           && pgy_runtime_export_argv[i] != NULL)
            ? pgy_runtime_export_argv[i]
            : "";
        char *owned = pgy_runtime_lib_strdup(arg);
        if (owned == NULL) {
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                              PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
        }
        pgy_array_push_String(&out, owned);
    }

    return out;
}

#endif /* PGY_RUNTIME_PROCESS_ARGS_EXPORTS_H */
