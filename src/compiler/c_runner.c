/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "c_runner.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define getpid _getpid
#define pgy_mkdir_private(path) _mkdir(path)
#define pgy_rmdir(path)         _rmdir(path)
#else
#include <unistd.h>
#define pgy_mkdir_private(path) mkdir((path), 0700)
#define pgy_rmdir(path)         rmdir((path))
#endif

#include "../common/string_compat.h"
#include "compiler.h"
#include "driver_app.h"
#include "driver_diag.h"
#include "path_utils.h"
#include "self_host_driver.h"

/*
 * The generated C used to land at $TMPDIR/_pgy_<stem>_<pid>.c -- a name any
 * other process on the host can predict. On a shared /tmp that is the classic
 * symlink race: plant a symlink at the name we are about to write, and the
 * compiler writes its output through it, into whatever file the invoking user
 * can touch.
 *
 * Randomizing the file name alone does not fix it (the attacker only has to
 * win the race once). What fixes it is writing inside a directory nobody else
 * can enter: mkdir is atomic and refuses an existing path, so the create
 * either wins outright or we pick a new name; the 0700 mode then means no
 * other user can plant anything inside it. On Windows %TEMP% is already
 * per-user, but the same code holds -- the atomic create still refuses a
 * squatted name.
 *
 * The nonce only avoids collisions and denial of service; the security comes
 * from mkdir's atomicity and the mode.
 */
static bool
c_runner_make_private_tmpdir(const char *base, char *out, size_t out_cap)
{
    unsigned long nonce = (unsigned long)getpid()
        ^ ((unsigned long)time(NULL) << 8);

    for (int attempt = 0; attempt < 64; attempt++) {
        int written = snprintf(out, out_cap, "%s/pgy-%lx",
                               base, nonce + (unsigned long)attempt * 2654435761UL);
        if (written < 0 || (size_t)written >= out_cap)
            return false;
        if (pgy_mkdir_private(out) == 0)
            return true;
        if (errno != EEXIST)
            return false;
    }
    return false;
}

typedef struct
{
    char directory[1024];
    char c_path[1024];
    bool active;
} CRunnerCArtifactWorkspace;

static bool
c_runner_c_artifact_workspace_open(const char *source_path,
                                   const char *base_directory,
                                   CRunnerCArtifactWorkspace *workspace)
{
    const char *base;
    const char *sep;
    const char *dot;
    size_t stem_length;
    char stem[256];
    int written;

    if (source_path == NULL || base_directory == NULL || workspace == NULL)
        return false;
    memset(workspace, 0, sizeof(*workspace));
    if (!c_runner_make_private_tmpdir(
            base_directory, workspace->directory,
            sizeof(workspace->directory))) {
        return false;
    }

    base = source_path;
    sep = strrchr(base, '/');
#ifdef _WIN32
    {
        const char *backslash = strrchr(base, '\\');
        if (backslash != NULL && (sep == NULL || backslash > sep))
            sep = backslash;
    }
#endif
    if (sep != NULL)
        base = sep + 1;
    dot = strrchr(base, '.');
    stem_length = dot != NULL ? (size_t)(dot - base) : strlen(base);
    if (stem_length > sizeof(stem) - 1)
        stem_length = sizeof(stem) - 1;
    memcpy(stem, base, stem_length);
    stem[stem_length] = '\0';
    written = snprintf(workspace->c_path, sizeof(workspace->c_path),
                       "%s/%s.c", workspace->directory, stem);
    if (written < 0 || (size_t)written >= sizeof(workspace->c_path)) {
        pgy_rmdir(workspace->directory);
        memset(workspace, 0, sizeof(*workspace));
        return false;
    }
    workspace->active = true;
    return true;
}

static void
c_runner_c_artifact_workspace_close(CRunnerCArtifactWorkspace *workspace)
{
    if (workspace == NULL || !workspace->active)
        return;
    remove(workspace->c_path);
    pgy_rmdir(workspace->directory);
    memset(workspace, 0, sizeof(*workspace));
}

int
c_runner_execute_installed_self_host_c(
    const char *launcher_path,
    const DriverFlags *flags,
    CompilerBackendTimings *backend_timings)
{
    CRunnerCArtifactWorkspace workspace;
    char *binary_path;
    CompilerResult *result;
    int materialize_rc;

    if (backend_timings != NULL)
        memset(backend_timings, 0, sizeof(*backend_timings));
    if (flags == NULL || flags->source_path == NULL) {
        fprintf(stderr, "pgy: self-host C compile requires a source path\n");
        return 1;
    }
    /* The Pergyra runtime denies absolute file-I/O paths unless the caller
     * explicitly grants that authority.  Keep this compiler-owned transient
     * artifact relative instead of weakening the child process sandbox. */
    if (!c_runner_c_artifact_workspace_open(
            flags->source_path, ".", &workspace)) {
        driver_emit_stage_fail(flags, "backend_c_self_host_artifact",
            "could not create a private self-host C artifact workspace",
            "PGY_C_RUNNER_TMPDIR_UNAVAILABLE: the installed self-host C path "
            "requires a private temporary directory. Fix: set TMPDIR "
            "(TMP/TEMP on Windows) to a writable directory.");
        return 1;
    }
    binary_path = flags->output_path != NULL
        ? pergyra_strdup(flags->output_path)
        : path_default_binary(flags->source_path);
    if (binary_path == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        c_runner_c_artifact_workspace_close(&workspace);
        return 1;
    }

    if (flags->verbose)
        printf("pgy: self-host C artifact → %s\n", workspace.c_path);
    materialize_rc = driver_materialize_self_host_c_artifact(
        launcher_path, flags->source_path, workspace.c_path, flags->verbose);
    if (materialize_rc != 0) {
        c_runner_c_artifact_workspace_close(&workspace);
        free(binary_path);
        return materialize_rc;
    }

    result = compiler_compile_link_self_host_c_artifact(
        workspace.c_path, binary_path, flags->verbose, flags->opt_profile);
    c_runner_c_artifact_workspace_close(&workspace);
    if (result == NULL || !result->success) {
        const char *message = result != NULL && result->error_message != NULL
            ? result->error_message : "out of memory";
        fprintf(stderr, "pgy: self-host C compile failed: %s\n", message);
        if (backend_timings != NULL && result != NULL)
            *backend_timings = result->backend_timings;
        compiler_result_destroy(result);
        free(binary_path);
        return 1;
    }
    if (backend_timings != NULL)
        *backend_timings = result->backend_timings;
    printf("pgy: compiled → %s\n", binary_path);
    int exit_code = 0;
    if (flags->do_run) {
        exit_code = compiler_run_binary(binary_path, flags->verbose);
        if (exit_code != 0)
            fprintf(stderr, "pgy: program exited with code %d\n", exit_code);
    }
    compiler_result_destroy(result);
    free(binary_path);
    return exit_code;
}

int
c_runner_execute(const DriverFlags *flags,
                 const CompilerIRBundle *bundle,
                 const PgyAirVerification *air,
                 CompilerBackendTimings *backend_timings)
{
    if (backend_timings != NULL)
        memset(backend_timings, 0, sizeof(*backend_timings));

    if (flags->emit_c_only) {
        char *output_c = flags->output_path != NULL
            ? pergyra_strdup(flags->output_path)
            : path_replace_extension(flags->source_path, ".c");
CompilerResult *result;

        if (output_c == NULL) {
            fprintf(stderr, "pgy: out of memory\n");
            return 1;
        }

        if (flags->verbose)
            printf("pgy: generating C → %s\n", output_c);

        result = compiler_emit_c(bundle, air, output_c);
        const char *identity_error = NULL;
        bool identity_ready = result != NULL
            && result->success
            && compiler_result_artifact_identity_ready(result, &identity_error);
        if (result == NULL || !result->success || !identity_ready) {
            const char *msg   = result != NULL && result->error_message != NULL
                ? result->error_message
                : (identity_error != NULL ? identity_error : "out of memory");
            const char *code  = result != NULL ? result->error_code : NULL;
            const char *cause = result != NULL ? result->error_cause_ir : NULL;
            const char *fix   = result != NULL ? result->error_fix_source : NULL;
            if (flags->diag_format == DIAG_FORMAT_JSON) {
                const char *stage = driver_route_stage("backend_c_emit", code);
                driver_emit_single_diag_json_full(stage, code, cause, fix, msg);
            } else {
                fprintf(stderr, "pgy: C generation failed: %s\n", msg);
            }
            compiler_result_destroy(result);
            free(output_c);
            return 1;
        }

        printf("pgy: wrote %s\n", output_c);
        compiler_result_destroy(result);
        free(output_c);
        return 0;
    }

    /* Full C pipeline: HIR → .c → GCC → binary */
    CRunnerCArtifactWorkspace workspace;
    char *bin_path;
    CompilerResult *result;
    const char *tmpdir = getenv("TMPDIR");

    if (tmpdir == NULL) tmpdir = getenv("TMP");
    if (tmpdir == NULL) tmpdir = getenv("TEMP");
#ifdef _WIN32
    if (tmpdir == NULL) tmpdir = ".";
#else
    if (tmpdir == NULL) tmpdir = "/tmp";
#endif

    if (!c_runner_c_artifact_workspace_open(
            flags->source_path, tmpdir, &workspace)) {
        driver_emit_stage_fail(flags, "backend_c_native",
            "could not create a private temporary directory",
            "PGY_C_RUNNER_TMPDIR_UNAVAILABLE: the C backend needs a directory "
            "only this process can write to, and could not create one under "
            "the temporary root. "
            "Reason: the temporary root is missing, full, or not writable. "
            "Fix: set TMPDIR (TMP/TEMP on Windows) to a writable directory.");
        return 1;
    }

    bin_path = flags->output_path != NULL
        ? pergyra_strdup(flags->output_path)
        : path_default_binary(flags->source_path);
    if (bin_path == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        c_runner_c_artifact_workspace_close(&workspace);
        return 1;
    }

    if (flags->verbose)
        printf("pgy: generating C → %s\n", workspace.c_path);

    result = compiler_build_native(bundle, air, workspace.c_path, bin_path, flags->verbose,
                                   flags->opt_profile);
    c_runner_c_artifact_workspace_close(&workspace);

    const char *identity_error = NULL;
    bool identity_ready = result != NULL
        && result->success
        && compiler_result_artifact_identity_ready(result, &identity_error);
    if (result == NULL || !result->success || !identity_ready) {
        const char *msg   = result != NULL && result->error_message != NULL
            ? result->error_message
            : (identity_error != NULL ? identity_error : "out of memory");
        const char *code  = result != NULL ? result->error_code : NULL;
        const char *cause = result != NULL ? result->error_cause_ir : NULL;
        const char *fix   = result != NULL ? result->error_fix_source : NULL;
        if (flags->diag_format == DIAG_FORMAT_JSON) {
            const char *stage = driver_route_stage("backend_c_native", code);
            driver_emit_single_diag_json_full(stage, code, cause, fix, msg);
        } else {
            fprintf(stderr, "pgy: compile failed: %s\n", msg);
        }
        if (backend_timings != NULL && result != NULL)
            *backend_timings = result->backend_timings;
        compiler_result_destroy(result);
        free(bin_path);
        return 1;
    }
    if (backend_timings != NULL)
        *backend_timings = result->backend_timings;

    printf("pgy: compiled → %s\n", bin_path);
    int exit_code = 0;
    if (flags->do_run) {
        exit_code = compiler_run_binary(bin_path, flags->verbose);
        if (exit_code != 0)
            fprintf(stderr, "pgy: program exited with code %d\n", exit_code);
    }

    compiler_result_destroy(result);
    free(bin_path);
    return exit_code;
}
