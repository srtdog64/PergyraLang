/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "c_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "compiler.h"
#include "compiler_transient_artifact_workspace.h"
#include "driver_app.h"
#include "driver_diag.h"
#include "path_utils.h"
#include "self_host_driver.h"

int
c_runner_execute_installed_self_host_c(
    const char *launcher_path,
    const DriverFlags *flags,
    CompilerBackendTimings *backend_timings)
{
    CompilerTransientArtifactWorkspace workspace;
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
    if (!compiler_transient_artifact_workspace_open(
            flags->source_path, ".", ".c", NULL, &workspace)) {
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
        compiler_transient_artifact_workspace_close(&workspace);
        return 1;
    }

    if (flags->verbose)
        printf("pgy: self-host C artifact → %s\n", workspace.primary_path);
    materialize_rc = driver_materialize_self_host_c_artifact(
        launcher_path, flags->source_path, workspace.primary_path,
        flags->verbose, flags->diag_format == DIAG_FORMAT_JSON);
    if (materialize_rc != 0) {
        compiler_transient_artifact_workspace_close(&workspace);
        free(binary_path);
        return materialize_rc;
    }

    result = compiler_compile_link_self_host_c_artifact(
        workspace.primary_path, binary_path, flags->verbose, flags->opt_profile);
    compiler_transient_artifact_workspace_close(&workspace);
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
    CompilerTransientArtifactWorkspace workspace;
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

    if (!compiler_transient_artifact_workspace_open(
            flags->source_path, tmpdir, ".c", NULL, &workspace)) {
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
        compiler_transient_artifact_workspace_close(&workspace);
        return 1;
    }

    if (flags->verbose)
        printf("pgy: generating C → %s\n", workspace.primary_path);

    result = compiler_build_native(bundle, air, workspace.primary_path, bin_path, flags->verbose,
                                   flags->opt_profile);
    compiler_transient_artifact_workspace_close(&workspace);

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
