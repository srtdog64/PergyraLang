#include "self_host_machine_manifest_artifact_owner.h"

#include "compiler_process.h"
#include "path_utils.h"
#include "self_host_child_io_authority.h"
#include "self_host_driver.h"

#include <stdio.h>
#include <stdlib.h>

int
driver_write_self_host_machine_manifest(const char *launcher_path)
{
    const char *child_argv[4];
    char *binary = driver_resolve_self_host_binary(launcher_path);
    char *manifest_path;
    int rc;

    if (binary == NULL || !path_file_exists(binary)) {
        fprintf(stderr,
                "pgy: self-host driver is unavailable; run 'make self-host-compiler' or set PGY_SELF_DRIVER_BIN\n");
        free(binary);
        return 1;
    }
    manifest_path = path_replace_extension(
        binary, ".machine-layer-manifest.json");
    if (manifest_path == NULL || !path_file_exists(manifest_path)) {
        fprintf(stderr,
                "pgy: installed self-host machine manifest companion is unavailable\n");
        free(manifest_path);
        free(binary);
        return 1;
    }

    child_argv[0] = binary;
    child_argv[1] = "--emit-machine-manifest-verified";
    child_argv[2] = manifest_path;
    child_argv[3] = NULL;
    driver_authorize_self_host_child_io();
    rc = pgy_exec_argv(child_argv, false);
    free(manifest_path);
    free(binary);
    return rc;
}
