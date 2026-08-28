#include "self_host_debug_driver.h"

#include "compiler_process.h"
#include "path_utils.h"
#include "self_host_child_io_authority.h"
#include "self_host_driver.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

int
driver_run_self_host_debug_session(const char *launcher_path,
                                   const char *source_path)
{
    const char *child_argv[4];
    char *binary;
    char *canonical_source_path;
    int rc;

    if (source_path == NULL || source_path[0] == '\0') {
        fprintf(stderr, "pgy debug: source path is required\n");
        return 1;
    }
    canonical_source_path =
        driver_self_host_source_identity_path_dup(source_path);
    if (canonical_source_path == NULL) {
        fprintf(stderr,
                "pgy debug: could not canonicalize source identity\n");
        return 1;
    }
    binary = driver_resolve_self_host_binary(launcher_path);
    if (binary == NULL || !path_file_exists(binary)) {
        fprintf(stderr,
                "pgy: self-host driver is unavailable; run 'make self-host-compiler' or set PGY_SELF_DRIVER_BIN\n");
        free(canonical_source_path);
        free(binary);
        return 1;
    }

    child_argv[0] = binary;
    child_argv[1] = "--debug-session";
    child_argv[2] = canonical_source_path;
    child_argv[3] = NULL;
    driver_authorize_self_host_child_io();
    rc = pgy_exec_argv(child_argv, false);
    if (rc != 0)
        fprintf(stderr,
                "pgy debug: self-host debug session failed (exit %d)\n",
                rc);

    free(canonical_source_path);
    free(binary);
    return rc;
}
