#include "self_host_llvm_ir_stdout_owner.h"

#include "compiler_transient_artifact_workspace.h"
#include "self_host_llvm_driver.h"

#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int
driver_stream_llvm_ir_file_to_stdout(const char *path)
{
    unsigned char buffer[16384];
    FILE *input = fopen(path, "rb");
    size_t count;

    if (input == NULL) {
        fprintf(stderr, "pgy: could not open the verified LLVM IR artifact\n");
        return 1;
    }
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        fprintf(stderr, "pgy: could not select binary LLVM IR stdout\n");
        fclose(input);
        return 1;
    }
#endif
    while ((count = fread(buffer, 1, sizeof(buffer), input)) != 0) {
        if (fwrite(buffer, 1, count, stdout) != count) {
            fprintf(stderr, "pgy: failed while writing verified LLVM IR stdout\n");
            fclose(input);
            return 1;
        }
    }
    if (ferror(input) || fclose(input) != 0 || fflush(stdout) != 0) {
        fprintf(stderr, "pgy: failed while completing verified LLVM IR stdout\n");
        return 1;
    }
    return 0;
}

int
driver_write_self_host_llvm_ir_stdout(const char *launcher_path,
                                      const char *source_path,
                                      bool emit_json_diagnostic)
{
    CompilerTransientArtifactWorkspace workspace;
    int rc;

    if (source_path == NULL || source_path[0] == '\0') {
        fprintf(stderr, "pgy: self-host LLVM IR stdout requires a source path\n");
        return 1;
    }
    if (!compiler_transient_artifact_workspace_open(
            source_path, ".", ".mir.json", ".ll", &workspace)) {
        fprintf(stderr,
                "pgy: could not create a private LLVM IR stdout workspace\n");
        return 1;
    }
    rc = driver_materialize_self_host_llvm_artifact(
        launcher_path, source_path, workspace.secondary_path, false,
        emit_json_diagnostic);
    if (rc == 0)
        rc = driver_stream_llvm_ir_file_to_stdout(workspace.secondary_path);
    compiler_transient_artifact_workspace_close(&workspace);
    return rc;
}
