#include "self_host_llvm_driver.h"

#include "compiler_process.h"
#include "path_utils.h"
#include "self_host_artifact_process_owner.h"
#include "self_host_driver.h"

#include <stdio.h>
#include <stdlib.h>

int
driver_materialize_self_host_llvm_artifact(
    const char *launcher_path,
    const char *source_path,
    const char *llvm_output_path,
    bool verbose,
    bool emit_json_diagnostic)
{
    const char *intent_argv[6];
    char *binary;
    int rc;

    if (source_path == NULL || source_path[0] == '\0' ||
        llvm_output_path == NULL || llvm_output_path[0] == '\0') {
        fprintf(stderr,
                "pgy: self-host LLVM materialization requires source and LLVM paths\n");
        return 1;
    }
    remove(llvm_output_path);
    binary = driver_resolve_self_host_binary(launcher_path);
    if (binary == NULL || !path_file_exists(binary)) {
        fprintf(stderr,
                "pgy: self-host driver is unavailable; run 'make self-host-compiler' or set PGY_SELF_DRIVER_BIN\n");
        free(binary);
        return 1;
    }

    intent_argv[0] = binary;
    intent_argv[1] = emit_json_diagnostic
        ? "--emit-source-llvm-ir-json-diagnostic-verified"
        : "--emit-source-llvm-ir-verified";
    intent_argv[2] = source_path;
    intent_argv[3] = "-o";
    intent_argv[4] = llvm_output_path;
    intent_argv[5] = NULL;
    rc = driver_run_self_host_artifact_process(
        intent_argv, verbose, emit_json_diagnostic);
    if (rc != 0) {
        if (!emit_json_diagnostic)
            fprintf(stderr,
                    "pgy: self-host source LLVM intent failed with code %d\n",
                    rc);
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
