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
#include "path_utils.h"

#ifdef PGY_LLVM_ENABLED

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
llvm_runner_execute(const DriverFlags *flags,
                    const CompilerIRBundle *bundle,
                    CompilerBackendTimings *backend_timings)
{
    if (backend_timings != NULL)
        memset(backend_timings, 0, sizeof(*backend_timings));

    if (flags->emit_llvm_ir) {
        CompilerResult *result = flags->output_path != NULL
            ? compiler_emit_llvm_ir_to_file(bundle, "pergyra_module", flags->output_path)
            : compiler_emit_llvm_ir(bundle, "pergyra_module");
        if (result == NULL || !result->success) {
            fprintf(stderr, "pgy: LLVM IR generation failed: %s\n",
                    result != NULL ? result->error_message : "out of memory");
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

    result = compiler_build_native_llvm(bundle, obj_path, runnable_bin_path, flags->verbose,
                                        flags->opt_profile);
    if (result == NULL || !result->success) {
        fprintf(stderr, "pgy: LLVM compile failed: %s\n",
                result != NULL ? result->error_message : "out of memory");
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
                    CompilerBackendTimings *backend_timings)
{
    (void)flags;
    (void)bundle;
    (void)backend_timings;
    fprintf(stderr, "pgy: LLVM backend not available in this build\n");
    return 1;
}

#endif
