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
    char tmp_dir[1024];
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

    if (!c_runner_make_private_tmpdir(tmpdir, tmp_dir, sizeof(tmp_dir))) {
        driver_emit_stage_fail(flags, "backend_c_native",
            "could not create a private temporary directory",
            "PGY_C_RUNNER_TMPDIR_UNAVAILABLE: the C backend needs a directory "
            "only this process can write to, and could not create one under "
            "the temporary root. "
            "Reason: the temporary root is missing, full, or not writable. "
            "Fix: set TMPDIR (TMP/TEMP on Windows) to a writable directory.");
        return 1;
    }

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
        /* Inside the private directory the name can stay readable: nobody
         * else can get in to race it. */
        snprintf(tmp_c, sizeof(tmp_c), "%s/%s.c", tmp_dir, stem);
    }

    bin_path = flags->output_path != NULL
        ? pergyra_strdup(flags->output_path)
        : path_default_binary(flags->source_path);
    if (bin_path == NULL) {
        fprintf(stderr, "pgy: out of memory\n");
        remove(tmp_c);
        pgy_rmdir(tmp_dir);
        return 1;
    }

    if (flags->verbose)
        printf("pgy: generating C → %s\n", tmp_c);

    result = compiler_build_native(bundle, tmp_c, bin_path, flags->verbose,
                                   flags->opt_profile);
    remove(tmp_c);
    pgy_rmdir(tmp_dir);

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
