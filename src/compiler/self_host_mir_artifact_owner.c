/* Installed verified source-to-MIR artifact publication owner. */

#include "self_host_mir_artifact_owner.h"

#include "compiler_process.h"
#include "path_utils.h"
#include "self_host_child_io_authority.h"
#include "self_host_driver.h"

#include <stdio.h>
#include <stdlib.h>

int
driver_materialize_self_host_mir_artifact(const char *launcher_path,
                                          const char *source_path,
                                          const char *output_path,
                                          bool verbose)
{
    const char *child_argv[6];
    char *binary;
    int rc;

    if (source_path == NULL || source_path[0] == '\0') {
        fprintf(stderr, "pgy: self-host MIR emission requires a source path\n");
        return 1;
    }
    if (output_path == NULL || output_path[0] == '\0') {
        fprintf(stderr,
                "pgy: self-host MIR materialization requires an output path\n");
        return 1;
    }

    binary = driver_resolve_self_host_binary(launcher_path);
    if (binary == NULL || !path_file_exists(binary)) {
        fprintf(stderr,
                "pgy: self-host driver is unavailable; run 'make self-host-compiler' or set PGY_SELF_DRIVER_BIN\n");
        free(binary);
        return 1;
    }

    remove(output_path);
    child_argv[0] = binary;
    child_argv[1] = "--emit-mir-json-verified";
    child_argv[2] = source_path;
    child_argv[3] = "-o";
    child_argv[4] = output_path;
    child_argv[5] = NULL;
    driver_authorize_self_host_child_io();
    rc = pgy_exec_argv(child_argv, verbose);
    if (rc != 0)
        fprintf(stderr,
                "pgy: self-host MIR producer failed with code %d\n", rc);
    if (rc == 0 && !path_file_exists(output_path)) {
        fprintf(stderr,
                "pgy: self-host driver reported success without a MIR artifact\n");
        rc = 1;
    }

    free(binary);
    return rc;
}
