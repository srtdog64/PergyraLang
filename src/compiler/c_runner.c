/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "c_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include "../common/string_compat.h"
#include "compiler.h"
#include "driver_app.h"
#include "path_utils.h"

int
c_runner_execute(const DriverFlags *flags,
                 const CompilerIRBundle *bundle,
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

        result = compiler_emit_c(bundle, output_c);
        if (result == NULL || !result->success) {
            const char *msg   = result != NULL ? result->error_message : "out of memory";
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
    char tmp_c[1024];
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

    {
        const char *base = flags->source_path;
        const char *sep = strrchr(base, '/');
#ifdef _WIN32
        const char *bsep = strrchr(base, '\\');
        if (bsep != NULL && (sep == NULL || bsep > sep))
            sep = bsep;
#endif
        if (sep != NULL)
            base = sep + 1;

        const char *dot = strrchr(base, '.');
        size_t blen = dot ? (size_t)(dot - base) : strlen(base);
        char stem[256];
        if (blen > sizeof(stem) - 1)
            blen = sizeof(stem) - 1;
        memcpy(stem, base, blen);
        stem[blen] = '\0';
        snprintf(tmp_c, sizeof(tmp_c), "%s/_pgy_%s_%u.c",
                 tmpdir, stem, (unsigned)getpid());
    }

    bin_path = flags->output_path != NULL
        ? pergyra_strdup(flags->output_path)
        : path_default_binary(flags->source_path);
    if (bin_path == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        return 1;
    }

    if (flags->verbose)
        printf("pgy: generating C → %s\n", tmp_c);

    result = compiler_build_native(bundle, tmp_c, bin_path, flags->verbose,
                                   flags->opt_profile);
    remove(tmp_c);

    if (result == NULL || !result->success) {
        const char *msg   = result != NULL ? result->error_message : "out of memory";
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
