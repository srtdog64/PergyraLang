/* Runtime check for pgy_runtime_process_utf8_argv. Prints each argument after
 * argv[0] as hex bytes so the smoke can compare them with the UTF-8 spelling.
 * PGY_TEST_INCLUDE_WINDOWS_H compiles the owner after <windows.h> and
 * <shellapi.h>, as the native inline runtime does, to prove its Win32
 * declarations stay compatible with the system headers. */
#if defined(_WIN32) && defined(PGY_TEST_INCLUDE_WINDOWS_H)
#include <windows.h>
#include <shellapi.h>
#endif
#include <stdio.h>
#include <string.h>

#include "runtime/pgy_runtime_process_utf8_argv.h"

int
main(int argc, char **argv)
{
    int utf8_argc = 0;
    char **utf8_argv = NULL;

    if (!pgy_runtime_process_utf8_argv(argc, argv, &utf8_argc, &utf8_argv)) {
        puts("conversion failed");
        return 2;
    }
    for (int i = 1; i < utf8_argc; i++) {
        printf("arg%d", i);
        for (const unsigned char *p = (const unsigned char *)utf8_argv[i];
             *p != '\0'; ++p)
            printf(" %02X", *p);
        printf("\n");
    }
    return 0;
}
