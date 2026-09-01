#include "self_host_source_stdout_owner.h"

#include "self_host_driver.h"

#include <stdio.h>
#include <string.h>

int
driver_run_self_host_source_stdout(const char *launcher_path,
                                   const char *mode,
                                   const char *source_path)
{
    char *args[2];

    if (mode == NULL
        || (strcmp(mode, "--tokens") != 0 && strcmp(mode, "--ast") != 0
            && strcmp(mode, "--tokens-json-diagnostic-verified") != 0
            && strcmp(mode, "--ast-json-diagnostic-verified") != 0
            && strcmp(mode, "--emit-capability-manifest-verified") != 0
            && strcmp(mode, "--emit-dir-verified") != 0)) {
        fprintf(stderr, "pgy: unknown self-host source stdout mode\n");
        return 1;
    }
    if (source_path == NULL || source_path[0] == '\0') {
        fprintf(stderr, "pgy: self-host source stdout requires a source path\n");
        return 1;
    }
    args[0] = (char *)mode;
    args[1] = (char *)source_path;
    return driver_run_self_host_command(launcher_path, 2, args);
}
