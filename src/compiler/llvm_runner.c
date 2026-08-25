/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "llvm_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "compiler.h"
#include "compiler_toolchain.h"
#include "compiler_transient_artifact_workspace.h"
#include "driver_app.h"
#include "path_utils.h"
#include "self_host_llvm_driver.h"

static bool
llvm_path_has_suffix(const char *path, const char *suffix)
{
    size_t path_len;
    size_t suffix_len;

    if (path == NULL || suffix == NULL)
        return false;
    path_len = strlen(path);
    suffix_len = strlen(suffix);
    if (path_len < suffix_len)
        return false;
    return strcmp(path + (path_len - suffix_len), suffix) == 0;
}

static bool
llvm_path_has_object_suffix(const char *path)
{
    return llvm_path_has_suffix(path, ".o") || llvm_path_has_suffix(path, ".obj");
}

static char *
llvm_resolve_runnable_binary_path(const char *binary_path, bool do_run)
{
    if (binary_path == NULL)
        return NULL;
    if (!do_run || !llvm_path_has_object_suffix(binary_path))
        return pergyra_strdup(binary_path);
#ifdef _WIN32
    return path_replace_extension(binary_path, ".exe");
#else
    return pergyra_strdup(binary_path);
#endif
}

int
llvm_runner_execute_installed_self_host_llvm(
    const char *launcher_path,
    const DriverFlags *flags,
    CompilerBackendTimings *backend_timings)
{
    CompilerTransientArtifactWorkspace workspace;
    char *requested_path;
    char *binary_path;
    CompilerResult *result;
    int materialize_rc;
    int exit_code = 0;

    if (backend_timings != NULL)
        memset(backend_timings, 0, sizeof(*backend_timings));
    if (flags == NULL || flags->source_path == NULL) {
        fprintf(stderr, "pgy: self-host LLVM compile requires a source path\n");
        return 1;
    }
    if (!compiler_transient_artifact_workspace_open(
            flags->source_path, ".", ".mir.json", ".ll", &workspace)) {
        fprintf(stderr,
                "pgy: could not create a private self-host LLVM artifact workspace\n");
        return 1;
    }
    requested_path = flags->output_path != NULL
        ? pergyra_strdup(flags->output_path)
        : path_default_binary(flags->source_path);
    binary_path = llvm_resolve_runnable_binary_path(
        requested_path, flags->do_run
    );
    if (requested_path == NULL || binary_path == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        free(requested_path);
        free(binary_path);
        compiler_transient_artifact_workspace_close(&workspace);
        return 1;
    }
    if (flags->do_run && llvm_path_has_object_suffix(requested_path)) {
        fprintf(stderr, "pgy: warning: output path '%s' looks like object; "
                        "run target is '%s'\n",
                requested_path, binary_path);
    }
    free(requested_path);
    if (!pgy_path_is_safe(binary_path)) {
        fprintf(stderr, "pgy: unsafe self-host LLVM output path\n");
        compiler_transient_artifact_workspace_close(&workspace);
        free(binary_path);
        return 1;
    }
    if (path_file_exists(binary_path) && remove(binary_path) != 0) {
        fprintf(stderr,
                "pgy: could not remove the stale self-host LLVM output\n");
        compiler_transient_artifact_workspace_close(&workspace);
        free(binary_path);
        return 1;
    }
    if (flags->verbose)
        printf("pgy: self-host LLVM artifacts → %s, %s\n",
               workspace.primary_path, workspace.secondary_path);
    materialize_rc = driver_materialize_self_host_llvm_artifact(
        launcher_path, flags->source_path, workspace.secondary_path,
        flags->verbose);
    if (materialize_rc != 0) {
        compiler_transient_artifact_workspace_close(&workspace);
        free(binary_path);
        return materialize_rc;
    }

    result = compiler_compile_link_self_host_llvm_artifact(
        workspace.secondary_path, binary_path, flags->verbose,
        flags->opt_profile);
    compiler_transient_artifact_workspace_close(&workspace);
    if (result == NULL || !result->success) {
        const char *message = result != NULL && result->error_message != NULL
            ? result->error_message : "out of memory";
        fprintf(stderr, "pgy: self-host LLVM compile failed: %s\n", message);
        if (backend_timings != NULL && result != NULL)
            *backend_timings = result->backend_timings;
        compiler_result_destroy(result);
        free(binary_path);
        return 1;
    }
    if (backend_timings != NULL)
        *backend_timings = result->backend_timings;
    printf("pgy: compiled (self-host LLVM) → %s\n", binary_path);
    if (flags->do_run) {
        exit_code = compiler_run_binary(binary_path, flags->verbose);
        if (exit_code != 0)
            fprintf(stderr, "pgy: program exited with code %d\n", exit_code);
    }
    compiler_result_destroy(result);
    free(binary_path);
    return exit_code;
}

#ifdef PGY_LLVM_ENABLED

int
llvm_runner_execute(const DriverFlags *flags,
                    const CompilerIRBundle *bundle,
                    const PgyAirVerification *air,
                    CompilerBackendTimings *backend_timings)
{
    if (backend_timings != NULL)
        memset(backend_timings, 0, sizeof(*backend_timings));

    if (flags->emit_llvm_ir) {
        CompilerResult *result = flags->output_path != NULL
            ? compiler_emit_llvm_ir_to_file(bundle, air, "pergyra_module", flags->output_path)
            : compiler_emit_llvm_ir(bundle, air, "pergyra_module");
        const char *identity_error = NULL;
        bool identity_ready = result != NULL
            && result->success
            && compiler_result_artifact_identity_ready(result, &identity_error);
        if (result == NULL || !result->success || !identity_ready) {
            const char *msg  = result != NULL && result->error_message != NULL
                ? result->error_message
                : (identity_error != NULL ? identity_error : "out of memory");
            const char *code = result != NULL ? result->error_code : NULL;
            const char *cause = result != NULL ? result->error_cause_ir : NULL;
            const char *fix   = result != NULL ? result->error_fix_source : NULL;
            if (flags->diag_format == DIAG_FORMAT_JSON) {
                const char *stage = driver_route_stage("backend_llvm_emit", code);
                driver_emit_single_diag_json_full(stage, code, cause, fix, msg);
            } else {
                fprintf(stderr, "pgy: LLVM IR generation failed: %s\n", msg);
            }
            compiler_result_destroy(result);
            return 1;
        }

        if (flags->output_path != NULL)
            printf("pgy: wrote %s\n", flags->output_path);
        compiler_result_destroy(result);
        return 0;
    }

    char *bin_path = flags->output_path != NULL
        ? pergyra_strdup(flags->output_path)
        : path_default_binary(flags->source_path);
    bool output_looks_object = llvm_path_has_object_suffix(bin_path);
    char *runnable_bin_path = llvm_resolve_runnable_binary_path(bin_path, flags->do_run);
    char *obj_path = bin_path != NULL
        ? (llvm_path_has_object_suffix(bin_path)
           ? pergyra_strdup(bin_path)
           : path_replace_extension(bin_path, ".o"))
        : NULL;
    CompilerResult *result;

    if (bin_path == NULL || obj_path == NULL || runnable_bin_path == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        free(bin_path);
        free(obj_path);
        free(runnable_bin_path);
        return 1;
    }
    if (flags->do_run && output_looks_object) {
        fprintf(stderr, "pgy: warning: output path '%s' looks like object; "
                        "run target is '%s'\n",
                bin_path, runnable_bin_path);
    }

    result = compiler_build_native_llvm(bundle, air, obj_path, runnable_bin_path, flags->verbose,
                                        flags->opt_profile);
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
            const char *stage = driver_route_stage("backend_llvm_native", code);
            driver_emit_single_diag_json_full(stage, code, cause, fix, msg);
        } else {
            fprintf(stderr, "pgy: LLVM compile failed: %s\n", msg);
        }
        if (backend_timings != NULL && result != NULL)
            *backend_timings = result->backend_timings;
        compiler_result_destroy(result);
        free(obj_path);
        free(bin_path);
        free(runnable_bin_path);
        return 1;
    }
    if (backend_timings != NULL)
        *backend_timings = result->backend_timings;

    printf("pgy: compiled (LLVM) → %s\n", runnable_bin_path);
    int exit_code = 0;
    if (flags->do_run) {
        exit_code = compiler_run_binary(runnable_bin_path, flags->verbose);
        if (exit_code != 0)
            fprintf(stderr, "pgy: program exited with code %d\n", exit_code);
    }

    compiler_result_destroy(result);
    free(obj_path);
    free(bin_path);
    free(runnable_bin_path);
    return exit_code;
}

#else /* !PGY_LLVM_ENABLED */

int
llvm_runner_execute(const DriverFlags *flags,
                    const CompilerIRBundle *bundle,
                    const PgyAirVerification *air,
                    CompilerBackendTimings *backend_timings)
{
    (void)flags;
    (void)bundle;
    (void)air;
    (void)backend_timings;
    /* One spelling for the C-only fact: the driver guard and every harness
     * skip detection key on this phrase, and a second spelling turned a
     * declared SKIP into a FAIL on the C-only platforms. */
    fprintf(stderr, "pgy: this build was compiled without LLVM backend support\n");
    return 1;
}

#endif
