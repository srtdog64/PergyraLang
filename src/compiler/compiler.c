#include "compiler_internal.h"
#include "compiler_toolchain.h"
#include "path_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../codegen/transpiler.h"
#include "../common/string_compat.h"

static int
invoke_c_backend(const CompilerIRBundle *bundle,
                 const char *output_c_path,
                 char **error_message,
                 char **error_code,
                 char **error_cause_ir,
                 char **error_fix_source,
                 bool *uses_intent_observability)
{
    TranspileResult *transpile_result;
    if (error_code != NULL)
        *error_code = NULL;
    if (error_cause_ir != NULL)
        *error_cause_ir = NULL;
    if (error_fix_source != NULL)
        *error_fix_source = NULL;
    if (error_message != NULL)
        *error_message = NULL;
    if (uses_intent_observability != NULL)
        *uses_intent_observability = false;
    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("IR bundle is incomplete");
        return 1;
    }

    transpile_result = transpile_from_mir(bundle->mir, output_c_path);
    if (transpile_result == NULL) {
        *error_message = pergyra_strdup("Out of memory");
        return 1;
    }

    if (!transpile_result->success) {
        *error_message = transpile_result->error_message != NULL
            ? pergyra_strdup(transpile_result->error_message)
            : pergyra_strdup("C backend failed");
        if (error_code != NULL && transpile_result->error_code != NULL)
            *error_code = pergyra_strdup(transpile_result->error_code);
        if (error_cause_ir != NULL && transpile_result->error_cause_ir != NULL)
            *error_cause_ir = pergyra_strdup(transpile_result->error_cause_ir);
        if (error_fix_source != NULL && transpile_result->error_fix_source != NULL)
            *error_fix_source = pergyra_strdup(transpile_result->error_fix_source);
        transpile_result_destroy(transpile_result);
        return 1;
    }

    if (uses_intent_observability != NULL)
        *uses_intent_observability = transpile_result->uses_intent_observability;

    transpile_result_destroy(transpile_result);
    return 0;
}

CompilerResult *
compiler_emit_c(const CompilerIRBundle *bundle, const char *output_c_path)
{
    char *error_message = NULL;
    char *error_code = NULL;
    char *error_cause_ir = NULL;
    char *error_fix_source = NULL;
    int rc = invoke_c_backend(bundle, output_c_path, &error_message,
                              &error_code, &error_cause_ir,
                              &error_fix_source, NULL);
    if (rc != 0) {
        CompilerResult *result = compiler_error_full(
            error_message != NULL ? error_message : "C backend failed",
            error_code, error_cause_ir, error_fix_source);
        free(error_message);
        free(error_code);
        free(error_cause_ir);
        free(error_fix_source);
        return result;
    }

    return compiler_success(output_c_path, NULL);
}

CompilerResult *
compiler_build_native(const CompilerIRBundle *bundle,
                      const char *output_c_path,
                      const char *output_binary_path,
                      bool verbose,
                      PgyOptProfile opt_profile)
{
    char *error_message = NULL;
    char *error_code = NULL;
    char *error_cause_ir = NULL;
    char *error_fix_source = NULL;
    char *output_obj_path = NULL;
    double phase_start = compiler_now_seconds();
    bool uses_intent_observability = false;
    int rc = invoke_c_backend(bundle, output_c_path, &error_message,
                              &error_code, &error_cause_ir,
                              &error_fix_source, &uses_intent_observability);
    if (rc != 0) {
        CompilerResult *result = compiler_error_full(
            error_message != NULL ? error_message : "C backend failed",
            error_code, error_cause_ir, error_fix_source);
        if (result != NULL)
            result->backend_timings.codegen = compiler_now_seconds() - phase_start;
        free(error_message);
        free(error_code);
        free(error_cause_ir);
        free(error_fix_source);
        return result;
    }

    output_obj_path = path_replace_extension(output_binary_path, ".o");
    if (output_obj_path == NULL)
        return compiler_error("Out of memory");

    if (!pgy_path_is_safe(output_c_path) ||
        !pgy_path_is_safe(output_binary_path) ||
        !pgy_path_is_safe(output_obj_path)) {
        free(output_obj_path);
        return compiler_error("Unsafe characters in file path");
    }

    const char *opt_flag = (opt_profile == PGY_OPT_RELEASE) ? "-O3" : "-O0";
    const char *intent_observability_flag =
        uses_intent_observability
            ? "-DPGY_INTENT_OBSERVABILITY_ENABLED=1"
            : "-DPGY_INTENT_OBSERVABILITY_ENABLED=0";
    CompilerResult *result = NULL;
    const char *cc = pgy_detect_c_compiler();
    const char *cc_target = pgy_cc_extra_target_flag();
    {
        const char *compile_argv[28];
        int ci = 0;
#ifdef _WIN32
        const char *link_argv[28];
        int li = 0;
#endif
        compile_argv[ci++] = cc;
        if (cc_target != NULL) compile_argv[ci++] = cc_target;
        compile_argv[ci++] = "-std=c11";
        compile_argv[ci++] = "-Wall";
        compile_argv[ci++] = "-Wno-unused-function";
#ifdef _WIN32
        compile_argv[ci++] = "-Wno-unused-value";
        compile_argv[ci++] = "-Wno-parentheses-equality";
        compile_argv[ci++] = "-Wno-c23-extensions";
        compile_argv[ci++] = "-Wno-format-truncation";
        compile_argv[ci++] = PGY_CFLAGS_THREAD_FLAG;
#endif
        compile_argv[ci++] = opt_flag;
#ifndef _WIN32
        compile_argv[ci++] = "-fopenmp";
#endif
        compile_argv[ci++] = intent_observability_flag;
        compile_argv[ci++] = "-I";
        compile_argv[ci++] = PGY_SRC_DIR;
        compile_argv[ci++] = "-I";
        compile_argv[ci++] = PGY_RUNTIME_DIR;
        compile_argv[ci++] = "-c";
        compile_argv[ci++] = output_c_path;
        compile_argv[ci++] = "-o";
        compile_argv[ci++] = output_obj_path;
        compile_argv[ci] = NULL;

#ifdef _WIN32
        link_argv[li++] = cc;
        if (cc_target != NULL) link_argv[li++] = cc_target;
        link_argv[li++] = "-std=c11";
        link_argv[li++] = "-Wall";
        link_argv[li++] = opt_flag;
        link_argv[li++] = PGY_CFLAGS_THREAD_FLAG;
        link_argv[li++] = output_obj_path;
        link_argv[li++] = "-o";
        link_argv[li++] = output_binary_path;
        link_argv[li++] = PGY_CFLAGS_THREAD_LIB;
        link_argv[li++] = "-lm";
        link_argv[li] = NULL;
#endif

    result = compiler_success(output_c_path, output_binary_path);
    if (result == NULL) {
        free(output_obj_path);
        return NULL;
    }
    result->backend_timings.codegen = compiler_now_seconds() - phase_start;

    phase_start = compiler_now_seconds();
    rc = pgy_exec_argv(compile_argv, verbose);
    if (rc != 0) {
        result->success = false;
        result->exit_code = rc;
        free(result->error_message);
        result->error_message = pergyra_strdup("Native compilation failed");
        result->backend_timings.native_compile = compiler_now_seconds() - phase_start;
        remove(output_obj_path);
        free(output_obj_path);
        return result;
    }
    result->backend_timings.native_compile = compiler_now_seconds() - phase_start;

    phase_start = compiler_now_seconds();
#ifndef _WIN32
    /* Linux: rebuild link_argv with extra flags (lld, build-id, fopenmp) */
    {
        const char *lnk[20];
        int lc = 0;
        lnk[lc++] = cc;
        if (cc_target != NULL) lnk[lc++] = cc_target;
        lnk[lc++] = "-std=c11";
        lnk[lc++] = "-Wall";
        lnk[lc++] = opt_flag;
        lnk[lc++] = "-fopenmp";
        if (compiler_should_use_lld())
            lnk[lc++] = "-fuse-ld=lld";
        lnk[lc++] = "-Wl,--build-id=none";
        lnk[lc++] = output_obj_path;
        lnk[lc++] = "-o";
        lnk[lc++] = output_binary_path;
        lnk[lc++] = PGY_CFLAGS_THREAD_LIB;
        lnk[lc++] = "-lm";
        lnk[lc] = NULL;
        rc = pgy_exec_argv(lnk, verbose);
    }
#else
    rc = pgy_exec_argv(link_argv, verbose);
#endif
    } /* close compile/link argv scope */
    if (rc != 0) {
        result->success = false;
        result->exit_code = rc;
        free(result->error_message);
        result->error_message = pergyra_strdup("Native link failed");
        result->backend_timings.link = compiler_now_seconds() - phase_start;
        remove(output_obj_path);
        free(output_obj_path);
        return result;
    }
    result->backend_timings.link = compiler_now_seconds() - phase_start;

    remove(output_obj_path);
    free(output_obj_path);
    return result;
}

int
compiler_run_binary(const char *binary_path, bool verbose)
{
    if (!pgy_path_is_safe(binary_path))
        return -1;

    char *resolved_path = path_resolve_runnable_binary(binary_path);
    if (resolved_path == NULL || !pgy_path_is_safe(resolved_path)) {
        free(resolved_path);
        return -1;
    }

    char safe_path[512];
#ifdef _WIN32
    pgy_win32_normalize_exec_path(resolved_path, safe_path, sizeof(safe_path));
#else
    if (resolved_path[0] == '/' || strncmp(resolved_path, "./", 2) == 0) {
        snprintf(safe_path, sizeof(safe_path), "%s", resolved_path);
    } else {
        snprintf(safe_path, sizeof(safe_path), "./%s", resolved_path);
    }
#endif

    const char *run_argv[] = { safe_path, NULL };

    if (verbose) {
        printf("--- output ---\n");
        fflush(stdout);
    }
    int rc = pgy_exec_argv(run_argv, verbose);
    if (verbose) {
        printf("--- end ---\n");
        fflush(stdout);
    }
    free(resolved_path);
    return rc;
}
