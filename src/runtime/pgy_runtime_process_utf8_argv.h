#ifndef PGY_RUNTIME_PROCESS_UTF8_ARGV_H
#define PGY_RUNTIME_PROCESS_UTF8_ARGV_H

#include <stdlib.h>
#ifdef _WIN32
#include <wchar.h>
#endif

#include "pgy_runtime_panic_contract.h"

#define PGY_RUNTIME_PANIC_REASON_PROCESS_ARGS_NOT_UTF16 \
    "process arguments are not valid UTF-16"

#ifdef _WIN32
/* The four Win32 calls this owner needs, declared compatibly with
 * <windows.h>/<shellapi.h> so a generated unit that never included them does
 * not take their macros (min, max, CreateFile, ...) along with them. */
__declspec(dllimport) wchar_t *__stdcall GetCommandLineW(void);
__declspec(dllimport) wchar_t **__stdcall CommandLineToArgvW(const wchar_t *,
                                                              int *);
__declspec(dllimport) int __stdcall WideCharToMultiByte(
    unsigned int, unsigned long, const wchar_t *, int, char *, int,
    const char *, int *);
__declspec(dllimport) void *__stdcall LocalFree(void *);
#define PGY_RUNTIME_CODE_PAGE_UTF8 65001u
#define PGY_RUNTIME_WC_ERR_INVALID_CHARS 0x80ul
#endif

/*
 * Process argument text owner. A String is UTF-8 everywhere else, but on
 * Windows main() receives argv in the ANSI code page (CP949 on a Korean
 * system), so `args_probe 한글` handed Args() four bytes of CP949. There the
 * arguments are read again from the UTF-16 command line and converted once at
 * startup; elsewhere argv already holds the bytes the program was given.
 *
 * Returns 1 with *argc_out and *argv_out set. Returns 0, leaving main()'s argv in
 * place, when the command line cannot be read or holds text that is not valid
 * UTF-16; the Args() reader panics on that, so a program that never reads its
 * arguments still starts. An allocation failure panics here. The converted
 * vector lives for the process.
 */
static inline int
pgy_runtime_process_utf8_argv(int argc, char **argv, int *argc_out,
                              char ***argv_out)
{
#ifdef _WIN32
    int wide_count = 0;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    char **utf8;

    *argc_out = argc;
    *argv_out = argv;
    if (wide == NULL || wide_count < 0)
        return 0;
    utf8 = (char **)calloc((size_t)wide_count + 1, sizeof(char *));
    if (utf8 == NULL) {
        LocalFree(wide);
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }
    for (int i = 0; i < wide_count; i++) {
        int bytes = WideCharToMultiByte(PGY_RUNTIME_CODE_PAGE_UTF8,
                                        PGY_RUNTIME_WC_ERR_INVALID_CHARS,
                                        wide[i], -1, NULL, 0, NULL, NULL);
        if (bytes <= 0) {
            for (int j = 0; j < i; j++)
                free(utf8[j]);
            free(utf8);
            LocalFree(wide);
            return 0;
        }
        utf8[i] = (char *)malloc((size_t)bytes);
        if (utf8[i] == NULL) {
            LocalFree(wide);
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                              PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
        }
        (void)WideCharToMultiByte(PGY_RUNTIME_CODE_PAGE_UTF8,
                                  PGY_RUNTIME_WC_ERR_INVALID_CHARS, wide[i], -1,
                                  utf8[i], bytes, NULL, NULL);
    }
    LocalFree(wide);
    *argc_out = wide_count;
    *argv_out = utf8;
    return 1;
#else
    *argc_out = argc;
    *argv_out = argv;
    return 1;
#endif
}

#endif /* PGY_RUNTIME_PROCESS_UTF8_ARGV_H */
