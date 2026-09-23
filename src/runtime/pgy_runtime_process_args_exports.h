#ifndef PGY_RUNTIME_PROCESS_ARGS_EXPORTS_H
#define PGY_RUNTIME_PROCESS_ARGS_EXPORTS_H

#include <stdint.h>

#include "pgy_runtime_process_utf8_argv.h"

/*
 * Process argument snapshot owner for LLVM-linked runtime binaries.
 * argv remains borrowed from main() (or, on Windows, is the UTF-8 vector
 * pgy_runtime_process_utf8_argv built); Args() returns an owned Array<String>.
 */
static int32_t pgy_runtime_export_argc = 0;
static char **pgy_runtime_export_argv = NULL;
static int pgy_runtime_export_argv_utf8 = 1;

void
pgy_args_init(int32_t argc, char **argv)
{
    int utf8_argc = 0;
    char **utf8_argv = NULL;

    pgy_runtime_export_argv_utf8 = pgy_runtime_process_utf8_argv(
        (int)argc, argv, &utf8_argc, &utf8_argv);
    pgy_runtime_export_argc = (int32_t)utf8_argc;
    pgy_runtime_export_argv = utf8_argv;
}

/* The direct-MIR LLVM route's main() stores its process arguments through
 * this call, and only when the program reads Args(), so a command line that
 * is not valid UTF-16 panics here instead of at the Args() call. */
void
pgy_runtime_process_utf8_argv_export(int32_t argc, char **argv,
                                     int32_t *argc_out, char ***argv_out)
{
    int utf8_argc = 0;
    char **utf8_argv = NULL;

    if (!pgy_runtime_process_utf8_argv((int)argc, argv, &utf8_argc,
                                       &utf8_argv))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          PGY_RUNTIME_PANIC_REASON_PROCESS_ARGS_NOT_UTF16);
    *argc_out = (int32_t)utf8_argc;
    *argv_out = utf8_argv;
}

PgyArray_String
pgy_args(void)
{
    int32_t count;

    /* Process arguments are an ambient fingerprinting/exfiltration surface, so
     * reading them is gated on PGY_CAP_ENV (same fail-closed discipline as
     * read-file/now/random). pgy_args_init (startup infra) is not gated. */
    pgy_cap_require_export(PGY_CAP_ENV, "args");
    if (!pgy_runtime_export_argv_utf8)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          PGY_RUNTIME_PANIC_REASON_PROCESS_ARGS_NOT_UTF16);
    count =
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
