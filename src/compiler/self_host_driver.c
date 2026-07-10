#include "self_host_driver.h"

#include "compiler_process.h"
#include "path_utils.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
self_host_driver_binary(const char *launcher_path)
{
    const char *override = getenv("PGY_SELF_DRIVER_BIN");
    char *directory;
    char *candidate;
    char *resolved;

    if (override != NULL && override[0] != '\0')
        return path_resolve_runnable_binary(override);

    directory = path_dirname_dup(launcher_path);
    if (directory == NULL)
        return NULL;
    candidate = path_join_dup(directory, "pgy-self-driver");
    free(directory);
    if (candidate == NULL)
        return NULL;
    resolved = path_resolve_runnable_binary(candidate);
    free(candidate);
    return resolved;
}

int
driver_run_self_host_command(const char *launcher_path, int argc, char *argv[])
{
    const char *child_argv[4];
    char *binary;
    bool mir_json_mode;
    bool manifest_mode;
    int child_argc = 0;
    int rc;

    if (argc < 1) {
        fprintf(stderr,
                "pgy: --self-driver requires a source path or --fixture-manifest\n");
        return 1;
    }
    mir_json_mode = strcmp(argv[0], "--mir-json") == 0;
    manifest_mode = strcmp(argv[0], "--fixture-manifest") == 0
        || strcmp(argv[0], "--mir-fixture-manifest") == 0;
    if (mir_json_mode && argc != 2) {
        fprintf(stderr,
                "pgy: --self-driver --mir-json requires one MIR JSON path\n");
        return 1;
    }
    if (manifest_mode && argc != 1) {
        fprintf(stderr,
                "pgy: --self-driver manifest mode does not accept extra arguments\n");
        return 1;
    }
    if (!mir_json_mode && !manifest_mode
        && (argc > 2
            || (argc == 2
                && strcmp(argv[1], "--emit-c-verified") != 0))) {
        fprintf(stderr,
                "pgy: --self-driver supports <source.pgy> [--emit-c-verified] or --mir-json <file>\n");
        return 1;
    }

    binary = self_host_driver_binary(launcher_path);
    if (binary == NULL || !path_file_exists(binary)) {
        fprintf(stderr,
                "pgy: self-host driver is unavailable; run 'make self-host-compiler' or set PGY_SELF_DRIVER_BIN\n");
        free(binary);
        return 1;
    }

    child_argv[child_argc++] = binary;
    child_argv[child_argc++] = argv[0];
    if (mir_json_mode)
        child_argv[child_argc++] = argv[1];
    else if (!manifest_mode)
        child_argv[child_argc++] = "--emit-c-verified";
    child_argv[child_argc] = NULL;
    rc = pgy_exec_argv(child_argv, false);
    free(binary);
    return rc;
}
