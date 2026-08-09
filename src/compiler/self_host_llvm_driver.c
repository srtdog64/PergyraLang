#include "self_host_llvm_driver.h"

#include "compiler_process.h"
#include "path_utils.h"
#include "self_host_driver.h"
#include "self_host_mir_artifact_owner.h"

#include <stdio.h>
#include <stdlib.h>

int
driver_materialize_self_host_llvm_artifacts(
    const char *launcher_path,
    const char *source_path,
    const char *mir_output_path,
    const char *llvm_output_path,
    bool verbose)
{
    const char *backend_argv[6];
    char *binary;
    int rc;

    if (source_path == NULL || source_path[0] == '\0' ||
        mir_output_path == NULL || mir_output_path[0] == '\0' ||
        llvm_output_path == NULL || llvm_output_path[0] == '\0') {
        fprintf(stderr,
                "pgy: self-host LLVM materialization requires source, MIR, and LLVM paths\n");
        return 1;
    }
    remove(llvm_output_path);
    rc = driver_materialize_self_host_mir_artifact(
        launcher_path, source_path, mir_output_path, verbose);
    if (rc != 0)
        return rc;

    binary = driver_resolve_self_host_binary(launcher_path);
    if (binary == NULL || !path_file_exists(binary)) {
        fprintf(stderr,
                "pgy: self-host driver is unavailable; run 'make self-host-compiler' or set PGY_SELF_DRIVER_BIN\n");
        free(binary);
        return 1;
    }

    backend_argv[0] = binary;
    backend_argv[1] = "--mir-json-backend=llvm";
    backend_argv[2] = mir_output_path;
    backend_argv[3] = "-o";
    backend_argv[4] = llvm_output_path;
    backend_argv[5] = NULL;
    rc = pgy_exec_argv(backend_argv, verbose);
    if (rc != 0) {
        fprintf(stderr,
                "pgy: self-host LLVM projector failed with code %d\n", rc);
        goto done;
    }
    if (!path_file_exists(llvm_output_path)) {
        fprintf(stderr,
                "pgy: self-host driver reported success without an LLVM artifact\n");
        rc = 1;
        goto done;
    }

done:
    free(binary);
    return rc;
}
