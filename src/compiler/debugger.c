#include "debugger.h"

#include "self_host_debug_driver.h"

#include <stdio.h>
#include <string.h>

int
driver_run_debug_command(const char *launcher_path, int argc, char *argv[])
{
    const char *path = NULL;
    int i;

    for (i = 0; i < argc; i++) {
        if (i == 0 && strcmp(argv[i], "debug") == 0)
            continue;
        if (argv[i][0] != '-') {
            path = argv[i];
            break;
        }
    }
    if (path == NULL) {
        fprintf(stderr, "Usage: pgy debug <file.pgy>\n");
        return 1;
    }
    return driver_run_self_host_debug_session(launcher_path, path);
}
