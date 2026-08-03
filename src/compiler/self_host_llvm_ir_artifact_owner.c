#include "self_host_llvm_ir_artifact_owner.h"

#include "compiler_toolchain.h"
#include "compiler_transient_artifact_workspace.h"
#include "path_utils.h"
#include "self_host_llvm_driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
driver_publish_self_host_llvm_ir_file(const char *launcher_path,
                                      const char *source_path,
                                      const char *output_path)
{
    CompilerTransientArtifactWorkspace workspace;
    char *output_directory;
    int rc;

    if (source_path == NULL || source_path[0] == '\0' ||
        output_path == NULL || output_path[0] == '\0') {
        fprintf(stderr,
                "pgy: self-host LLVM IR emission requires source and output paths\n");
        return 1;
    }
    if (strcmp(source_path, output_path) == 0) {
        fprintf(stderr,
                "pgy: self-host LLVM IR output must differ from its source\n");
        return 1;
    }
    if (!pgy_path_is_safe(output_path)) {
        fprintf(stderr, "pgy: unsafe self-host LLVM IR output path\n");
        return 1;
    }
    if (path_file_exists(output_path) && remove(output_path) != 0) {
        fprintf(stderr,
                "pgy: could not remove the stale self-host LLVM IR output\n");
        return 1;
    }

    output_directory = path_dirname_dup(output_path);
    if (output_directory == NULL ||
        !compiler_transient_artifact_workspace_open(
            source_path, output_directory, ".mir.json", ".ll", &workspace)) {
        fprintf(stderr,
                "pgy: could not create a private LLVM IR publication workspace\n");
        free(output_directory);
        return 1;
    }
    free(output_directory);

    rc = driver_materialize_self_host_llvm_artifacts(
        launcher_path, source_path, workspace.primary_path,
        workspace.secondary_path, false);
    if (rc == 0 && rename(workspace.secondary_path, output_path) != 0) {
        fprintf(stderr, "pgy: could not publish the self-host LLVM IR artifact\n");
        rc = 1;
    }
    compiler_transient_artifact_workspace_close(&workspace);
    if (rc == 0)
        printf("pgy: wrote %s\n", output_path);
    return rc;
}
