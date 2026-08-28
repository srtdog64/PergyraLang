#include "self_host_fmt_driver.h"

#include "compiler_process.h"
#include "path_utils.h"
#include "self_host_child_io_authority.h"
#include "self_host_driver.h"

#include <stdio.h>
#include <stdlib.h>

#define PGY_FMT_DIAGNOSTIC_STDOUT_LIMIT (1024u * 1024u)
#define PGY_FMT_TIMEOUT_MILLIS 300000u

int
driver_materialize_self_host_format_artifact(const char *launcher_path,
                                             const char *source_path,
                                             const char *output_path)
{
    const char *child_argv[6];
    char *binary;
    unsigned char *child_stdout = NULL;
    size_t child_stdout_length = 0;
    int rc;

    if (source_path == NULL || source_path[0] == '\0' ||
        output_path == NULL || output_path[0] == '\0') {
        fprintf(stderr,
                "pgy fmt: source and temporary output paths are required\n");
        return 1;
    }
    binary = driver_resolve_self_host_binary(launcher_path);
    if (binary == NULL || !path_file_exists(binary)) {
        fprintf(stderr,
                "pgy: self-host driver is unavailable; run 'make self-host-compiler' or set PGY_SELF_DRIVER_BIN\n");
        free(binary);
        return 1;
    }
    if (path_file_exists(output_path)) {
        fprintf(stderr,
                "pgy fmt: private output path already exists\n");
        free(binary);
        return 1;
    }

    child_argv[0] = binary;
    child_argv[1] = "--format-source-verified";
    child_argv[2] = source_path;
    child_argv[3] = "-o";
    child_argv[4] = output_path;
    child_argv[5] = NULL;
    driver_authorize_self_host_child_io();
    rc = pgy_exec_argv_capture_stdout(
        child_argv, PGY_FMT_DIAGNOSTIC_STDOUT_LIMIT,
        PGY_FMT_TIMEOUT_MILLIS, &child_stdout, &child_stdout_length);
    if (rc != 0) {
        if (child_stdout_length > 0) {
            (void)fwrite(child_stdout, 1, child_stdout_length, stderr);
            if (child_stdout[child_stdout_length - 1] != '\n')
                fputc('\n', stderr);
        }
        if (rc == PGY_EXEC_CAPTURE_TIMEOUT)
            fprintf(stderr, "pgy fmt: self-host formatter timed out\n");
        else if (rc == PGY_EXEC_CAPTURE_OUTPUT_LIMIT)
            fprintf(stderr,
                    "pgy fmt: self-host formatter exceeded its diagnostic limit\n");
        else if (rc == PGY_EXEC_CAPTURE_CRASHED)
            fprintf(stderr, "pgy fmt: self-host formatter child crashed\n");
        else if (rc < 0)
            fprintf(stderr,
                    "pgy fmt: failed to capture self-host formatter diagnostics\n");
        else
            fprintf(stderr,
                    "pgy fmt: self-host formatter failed (exit %d)\n", rc);
        if (rc < 0) rc = 1;
    } else if (child_stdout_length != 0) {
        fprintf(stderr,
                "pgy fmt: self-host formatter returned unexpected stdout\n");
        rc = 1;
    } else if (!path_file_exists(output_path)) {
        fprintf(stderr,
                "pgy fmt: self-host formatter returned without an artifact\n");
        rc = 1;
    }

    free(child_stdout);
    free(binary);
    return rc;
}
