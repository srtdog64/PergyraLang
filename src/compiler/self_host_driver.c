#include "self_host_driver.h"

#include "compiler_process.h"
#include "path_utils.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The driver is a Pergyra program, so its IO runs through the runtime policy
 * in pgy_runtime_lib_file_path_core.h, which denies absolute paths. That
 * default targets *compiled user programs*; the delegated driver is the
 * compiler, reading and writing exactly the paths named on pgy's own command
 * line. Without this grant `pgy --emit-c /abs/path.pgy` died at exit 1 with no
 * diagnostic: the read was denied and pgy_read_file maps a denial to "".
 * An operator-declared PGY_IO_ROOT is left alone -- that sandbox must fail
 * closed, not be widened here. */
static void
driver_authorize_self_host_child_io(void)
{
    const char *root = getenv("PGY_IO_ROOT");
    if (root != NULL && root[0] != '\0')
        return;
#ifdef _WIN32
    (void)_putenv_s("PGY_IO_ALLOW_ABSOLUTE", "1");
#else
    (void)setenv("PGY_IO_ALLOW_ABSOLUTE", "1", 1);
#endif
}

char *
driver_resolve_self_host_binary(const char *launcher_path)
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
    bool mir_producer_mode;
    bool mir_canonicalize_mode;
    int child_argc = 0;
    int rc;

    if (argc < 1) {
        fprintf(stderr,
                "pgy: --self-driver requires a source path or read-only compiler mode\n");
        return 1;
    }
    mir_json_mode = strcmp(argv[0], "--mir-json") == 0;
    mir_producer_mode = strcmp(argv[0], "--emit-mir-json-verified") == 0;
    mir_canonicalize_mode = strcmp(argv[0], "--canonicalize-mir-json") == 0;
    if (mir_json_mode && argc != 2) {
        fprintf(stderr,
                "pgy: --self-driver --mir-json requires one MIR JSON path\n");
        return 1;
    }
    if (mir_producer_mode && argc != 2) {
        fprintf(stderr,
                "pgy: --self-driver --emit-mir-json-verified requires one source path\n");
        return 1;
    }
    if (mir_canonicalize_mode && argc != 2) {
        fprintf(stderr,
                "pgy: --self-driver --canonicalize-mir-json requires one MIR JSON path\n");
        return 1;
    }
    if (!mir_json_mode && !mir_producer_mode && !mir_canonicalize_mode
        && (argc > 2
            || (argc == 2
                && strcmp(argv[1], "--emit-c-verified") != 0))) {
        fprintf(stderr,
                "pgy: --self-driver supports <source.pgy> [--emit-c-verified], --emit-mir-json-verified <source.pgy>, --canonicalize-mir-json <file>, or --mir-json <file>\n");
        return 1;
    }

    binary = driver_resolve_self_host_binary(launcher_path);
    if (binary == NULL || !path_file_exists(binary)) {
        fprintf(stderr,
                "pgy: self-host driver is unavailable; run 'make self-host-compiler' or set PGY_SELF_DRIVER_BIN\n");
        free(binary);
        return 1;
    }

    child_argv[child_argc++] = binary;
    child_argv[child_argc++] = argv[0];
    if (mir_json_mode || mir_producer_mode || mir_canonicalize_mode)
        child_argv[child_argc++] = argv[1];
    else
        child_argv[child_argc++] = "--emit-c-verified";
    child_argv[child_argc] = NULL;
    driver_authorize_self_host_child_io();
    rc = pgy_exec_argv(child_argv, false);
    free(binary);
    return rc;
}

int
driver_run_self_host_mir_json(const char *launcher_path,
                              const char *source_path)
{
    char *args[2];

    if (source_path == NULL || source_path[0] == '\0') {
        fprintf(stderr, "pgy: self-host MIR emission requires a source path\n");
        return 1;
    }
    args[0] = (char *)"--emit-mir-json-verified";
    args[1] = (char *)source_path;
    return driver_run_self_host_command(launcher_path, 2, args);
}

int
driver_materialize_self_host_c_artifact(const char *launcher_path,
                                        const char *source_path,
                                        const char *output_path,
                                        bool verbose)
{
    const char *child_argv[5];
    char *binary;
    int rc;

    if (source_path == NULL || source_path[0] == '\0') {
        fprintf(stderr, "pgy: self-host C emission requires a source path\n");
        return 1;
    }
    if (output_path == NULL || output_path[0] == '\0') {
        fprintf(stderr, "pgy: self-host C materialization requires an output path\n");
        return 1;
    }

    binary = driver_resolve_self_host_binary(launcher_path);
    if (binary == NULL || !path_file_exists(binary)) {
        fprintf(stderr,
                "pgy: self-host driver is unavailable; run 'make self-host-compiler' or set PGY_SELF_DRIVER_BIN\n");
        free(binary);
        return 1;
    }

    child_argv[0] = binary;
    child_argv[1] = "--emit-c-artifact-verified";
    child_argv[2] = source_path;
    child_argv[3] = output_path;
    child_argv[4] = NULL;
    driver_authorize_self_host_child_io();
    rc = pgy_exec_argv(child_argv, verbose);
    /* The driver can fail before it has anything to say -- a denied source
     * read, say -- so never let a delegated failure surface as a bare exit
     * code with no observable cause. */
    if (rc != 0)
        fprintf(stderr,
                "pgy: self-host driver failed (exit %d) emitting C for %s -> %s\n",
                rc, source_path, output_path);
    if (rc == 0 && !path_file_exists(output_path)) {
        fprintf(stderr,
                "pgy: self-host driver reported success without a C artifact\n");
        rc = 1;
    }

    free(binary);
    return rc;
}

int
driver_run_self_host_c_emit_artifact(const char *launcher_path,
                                     const char *source_path,
                                     const char *output_path,
                                     bool verbose)
{
    const char *effective_output = output_path;
    char *derived_output = NULL;
    int rc;

    if (source_path == NULL || source_path[0] == '\0') {
        fprintf(stderr, "pgy: self-host C emission requires a source path\n");
        return 1;
    }
    if (effective_output == NULL || effective_output[0] == '\0') {
        derived_output = path_replace_extension(source_path, ".c");
        effective_output = derived_output;
    }
    if (effective_output == NULL) {
        fprintf(stderr, "pgy: could not derive self-host C output path\n");
        return 1;
    }

    rc = driver_materialize_self_host_c_artifact(
        launcher_path, source_path, effective_output, verbose);
    if (rc == 0)
        printf("pgy: wrote %s\n", effective_output);

    free(derived_output);
    return rc;
}
