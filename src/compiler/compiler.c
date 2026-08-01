#include "compiler_internal.h"
#include "compiler_toolchain.h"
#include "path_utils.h"
#include "verified_projection_plan.h"
#include "verified_region_plan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../codegen/transpiler.h"
#include "../common/string_compat.h"

static int
invoke_c_backend(const CompilerIRBundle *bundle,
                 const PgyAirVerification *air,
                 const char *output_c_path,
                 char **error_message,
                 char **error_code,
                 char **error_cause_ir,
                 char **error_fix_source,
                 PgyVerifiedProjectionPlanRow *projection_plan_out,
                 bool *uses_intent_observability)
{
    TranspileResult *transpile_result;
    PgyVerifiedProjectionPlanRow projection_plan;
    PgyVerifiedParallelCapturePlan parallel_capture_plan = {0};
    const char *projection_error = NULL;
    if (error_code != NULL)
        *error_code = NULL;
    if (error_cause_ir != NULL)
        *error_cause_ir = NULL;
    if (error_fix_source != NULL)
        *error_fix_source = NULL;
    if (error_message != NULL)
        *error_message = NULL;
    if (projection_plan_out != NULL)
        memset(projection_plan_out, 0, sizeof(*projection_plan_out));
    if (uses_intent_observability != NULL)
        *uses_intent_observability = false;
    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL
        || air == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("IR bundle is incomplete");
        return 1;
    }

    if (!pgy_verified_projection_plan_intent_observability_with_air(
            air, bundle->mir, PGY_PROJECTION_TARGET_C,
            &projection_plan, &projection_error)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                projection_error != NULL
                    ? projection_error
                    : "verified projection plan failed");
        return 1;
    }
    if (projection_plan_out != NULL)
        *projection_plan_out = projection_plan;

    PgySpawnLanePlan spawn_lane_plan;
    if (!pgy_verified_spawn_lane_plan_from_air(air, &spawn_lane_plan,
            &projection_error)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                projection_error != NULL
                    ? projection_error
                    : "verified spawn-lane plan failed");
        return 1;
    }

    if (!pgy_verified_parallel_capture_plan_from_air(
            air, bundle->mir, &parallel_capture_plan, &projection_error)) {
        pgy_verified_spawn_lane_plan_dispose(&spawn_lane_plan);
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                projection_error != NULL
                    ? projection_error
                    : "verified parallel-capture plan failed");
        return 1;
    }

    /* The legacy inline runtime carries no M:N materialization, so a movable
     * spawn-lane row is refused HERE, before codegen -- compile-time
     * fail-closed (docs/194 R1), not a runtime null-handle panic. */
    {
        /* Same predicate as the link step below: inline iff the opt-out is
         * exactly "1". */
        const char *force_inline_env = getenv("PGY_RUNTIME_INLINE");
        bool runtime_inline = force_inline_env != NULL
            && force_inline_env[0] == '1';
        if (runtime_inline) {
            for (size_t i = 0; i < spawn_lane_plan.row_count; i++) {
                if (spawn_lane_plan.rows[i].lane
                    != PGY_LANE_MOVABLE_SCHEDULER) {
                    continue;
                }
                pgy_verified_spawn_lane_plan_dispose(&spawn_lane_plan);
                pgy_verified_parallel_capture_plan_dispose(
                    &parallel_capture_plan);
                if (error_message != NULL) {
                    *error_message = pergyra_strdup(
                        "movable execution lane requires the extern runtime; "
                        "unset PGY_RUNTIME_INLINE");
                }
                return 1;
            }
        }
    }

    transpile_result = transpile_from_mir_with_projection_plans(
        bundle->mir, &projection_plan, &parallel_capture_plan,
        &spawn_lane_plan, bundle->region_plan, output_c_path);
    pgy_verified_spawn_lane_plan_dispose(&spawn_lane_plan);
    pgy_verified_parallel_capture_plan_dispose(&parallel_capture_plan);
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
compiler_emit_c(const CompilerIRBundle *bundle,
                const PgyAirVerification *air,
                const char *output_c_path)
{
    char *error_message = NULL;
    char *error_code = NULL;
    char *error_cause_ir = NULL;
    char *error_fix_source = NULL;
    PgyVerifiedProjectionPlanRow projection_plan = {0};
    int rc = invoke_c_backend(bundle, air, output_c_path, &error_message,
                              &error_code, &error_cause_ir,
                              &error_fix_source, &projection_plan, NULL);
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

    CompilerResult *result = compiler_success(output_c_path, NULL);
    if (result == NULL)
        return NULL;
    if (!compiler_result_bind_artifact_identity(
            result, &projection_plan, "emitted_c")) {
        compiler_result_destroy(result);
        return compiler_error(
            "C artifact identity could not bind the verified projection plan");
    }
    return result;
}

CompilerResult *
compiler_build_native(const CompilerIRBundle *bundle,
                      const PgyAirVerification *air,
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
    PgyVerifiedProjectionPlanRow projection_plan = {0};
    int rc = invoke_c_backend(bundle, air, output_c_path, &error_message,
                              &error_code, &error_cause_ir,
                              &error_fix_source, &projection_plan,
                              &uses_intent_observability);
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
    PgyCCompilerSelection cc_selection;
    if (!pgy_select_c_compiler(&cc_selection))
        return compiler_error("Unable to detect C compiler");
    const char *cc = cc_selection.cc;
    const char *cc_target = cc_selection.target_flag;

    /* Extern-runtime mode: emitted C parses only runtime prototypes
     * (PGY_RUNTIME_DECLS_ONLY) and links the separately-compiled runtime
     * object, instead of re-inlining ~9k lines of runtime per build -- the
     * measured 91% of emitted-C compile time (docs/189 C14 / WO-RED2). Opt out
     * with PGY_RUNTIME_INLINE=1 to force self-contained inline emission. */
    const char *force_inline_env = getenv("PGY_RUNTIME_INLINE");
    bool extern_runtime = !(force_inline_env != NULL && force_inline_env[0] == '1');
    char *runtime_obj_path = NULL;
    if (extern_runtime) {
        const char *obj_err = NULL;
        runtime_obj_path = compiler_runtime_object_ensure(
            opt_profile, uses_intent_observability, verbose, &obj_err);
        if (runtime_obj_path == NULL) {
            free(output_obj_path);
            return compiler_error(obj_err != NULL ? obj_err
                                  : "Runtime object unavailable");
        }
    }
    {
        const char *compile_argv[32];
        int ci = 0;
#ifdef _WIN32
        const char *link_argv[32];
        int li = 0;
#endif
        compile_argv[ci++] = cc;
        if (cc_target != NULL) compile_argv[ci++] = cc_target;
        compile_argv[ci++] = "-std=c11";
        compile_argv[ci++] = "-Wall";
        compile_argv[ci++] = "-Wno-unused-function";
#ifdef __APPLE__
        compile_argv[ci++] = "-D_DARWIN_C_SOURCE";
        compile_argv[ci++] = "-D_XOPEN_SOURCE=700";
#elif !defined(_WIN32)
        compile_argv[ci++] = "-D_POSIX_C_SOURCE=200809L";
        compile_argv[ci++] = "-D_XOPEN_SOURCE=700";
#endif
#ifdef _WIN32
        compile_argv[ci++] = "-Wno-unused-value";
        compile_argv[ci++] = "-Wno-parentheses-equality";
        compile_argv[ci++] = "-Wno-c23-extensions";
        compile_argv[ci++] = "-Wno-format-truncation";
        compile_argv[ci++] = PGY_CFLAGS_THREAD_FLAG;
#endif
        compile_argv[ci++] = opt_flag;
        /* Define signed integer overflow as two's-complement wraparound so the
         * C backend matches the LLVM backend (which wraps) instead of letting
         * the optimizer assume overflow cannot happen. Without this the two
         * backends diverge on overflowing integer arithmetic. */
        compile_argv[ci++] = "-fwrapv";
        /* Do not let type-based alias analysis miscompile slot/witness
         * pointer punning; keep parity with the LLVM runtime build. */
        compile_argv[ci++] = "-fno-strict-aliasing";
#if !defined(_WIN32) && !defined(__APPLE__)
        compile_argv[ci++] = "-fopenmp";
#endif
        compile_argv[ci++] = intent_observability_flag;
        if (extern_runtime)
            compile_argv[ci++] = "-DPGY_RUNTIME_DECLS_ONLY";
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
        if (runtime_obj_path != NULL)
            link_argv[li++] = runtime_obj_path;
        link_argv[li++] = "-o";
        link_argv[li++] = output_binary_path;
        link_argv[li++] = PGY_CFLAGS_THREAD_LIB;
        link_argv[li++] = "-lm";
        link_argv[li] = NULL;
#endif

    result = compiler_success(output_c_path, output_binary_path);
    if (result == NULL) {
        free(output_obj_path);
        free(runtime_obj_path);
        return NULL;
    }
    if (!compiler_result_bind_artifact_identity(
            result, &projection_plan, "emitted_c")) {
        compiler_result_destroy(result);
        free(output_obj_path);
        free(runtime_obj_path);
        return compiler_error(
            "C artifact identity could not bind the verified projection plan");
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
        free(runtime_obj_path);
        return result;
    }
    result->backend_timings.native_compile = compiler_now_seconds() - phase_start;

    phase_start = compiler_now_seconds();
#ifndef _WIN32
    /* POSIX: rebuild link_argv with extra flags where the host compiler supports them. */
    {
        const char *lnk[24];
        int lc = 0;
        lnk[lc++] = cc;
        if (cc_target != NULL) lnk[lc++] = cc_target;
        lnk[lc++] = "-std=c11";
        lnk[lc++] = "-Wall";
        lnk[lc++] = opt_flag;
#ifndef __APPLE__
        lnk[lc++] = "-fopenmp";
        if (compiler_should_use_lld())
            lnk[lc++] = "-fuse-ld=lld";
        lnk[lc++] = "-Wl,--build-id=none";
#endif
        lnk[lc++] = output_obj_path;
        if (runtime_obj_path != NULL)
            lnk[lc++] = runtime_obj_path;
        lnk[lc++] = "-o";
        lnk[lc++] = output_binary_path;
#ifndef __APPLE__
        lnk[lc++] = PGY_CFLAGS_THREAD_LIB;
#endif
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
        free(runtime_obj_path);
        return result;
    }
    result->backend_timings.link = compiler_now_seconds() - phase_start;

    remove(output_obj_path);
    free(output_obj_path);
    free(runtime_obj_path);
    return result;
}

CompilerResult *
compiler_compile_link_self_host_c_artifact(const char *input_c_path,
                                           const char *output_binary_path,
                                           bool verbose,
                                           PgyOptProfile opt_profile)
{
    const char *compile_link_argv[40];
    int argc = 0;
    int rc;
    double phase_start;
    CompilerResult *result;
    PgyCCompilerSelection cc_selection;

    if (input_c_path == NULL || output_binary_path == NULL
        || !pgy_path_is_safe(input_c_path)
        || !pgy_path_is_safe(output_binary_path)) {
        return compiler_error("Unsafe or missing self-host C artifact path");
    }
    if (!pgy_select_c_compiler(&cc_selection))
        return compiler_error("Unable to detect C compiler");

    compile_link_argv[argc++] = cc_selection.cc;
    if (cc_selection.target_flag != NULL)
        compile_link_argv[argc++] = cc_selection.target_flag;
    compile_link_argv[argc++] = "-x";
    compile_link_argv[argc++] = "c";
    compile_link_argv[argc++] = "-std=c11";
    compile_link_argv[argc++] = "-Wall";
    compile_link_argv[argc++] = "-Wno-unused-function";
#ifdef __APPLE__
    compile_link_argv[argc++] = "-D_DARWIN_C_SOURCE";
    compile_link_argv[argc++] = "-D_XOPEN_SOURCE=700";
#elif !defined(_WIN32)
    compile_link_argv[argc++] = "-D_POSIX_C_SOURCE=200809L";
    compile_link_argv[argc++] = "-D_XOPEN_SOURCE=700";
    compile_link_argv[argc++] = "-D_DEFAULT_SOURCE";
#endif
#ifdef _WIN32
    compile_link_argv[argc++] = "-Wno-unused-value";
    compile_link_argv[argc++] = "-Wno-parentheses-equality";
    compile_link_argv[argc++] = "-Wno-c23-extensions";
    compile_link_argv[argc++] = "-Wno-format-truncation";
#endif
    compile_link_argv[argc++] = PGY_CFLAGS_THREAD_FLAG;
    compile_link_argv[argc++] =
        opt_profile == PGY_OPT_RELEASE ? "-O3" : "-O0";
    compile_link_argv[argc++] = "-fwrapv";
    compile_link_argv[argc++] = "-fno-strict-aliasing";
#if !defined(_WIN32) && !defined(__APPLE__)
    compile_link_argv[argc++] = "-fopenmp";
    if (compiler_should_use_lld())
        compile_link_argv[argc++] = "-fuse-ld=lld";
    compile_link_argv[argc++] = "-Wl,--build-id=none";
#endif
    compile_link_argv[argc++] = "-I";
    compile_link_argv[argc++] = PGY_SRC_DIR;
    compile_link_argv[argc++] = "-I";
    compile_link_argv[argc++] = PGY_RUNTIME_DIR;
    compile_link_argv[argc++] = input_c_path;
    compile_link_argv[argc++] = "-o";
    compile_link_argv[argc++] = output_binary_path;
    compile_link_argv[argc++] = PGY_CFLAGS_THREAD_LIB;
    compile_link_argv[argc++] = "-lm";
    compile_link_argv[argc] = NULL;

    result = compiler_success(input_c_path, output_binary_path);
    if (result == NULL)
        return NULL;
    phase_start = compiler_now_seconds();
    rc = pgy_exec_argv(compile_link_argv, verbose);
    result->backend_timings.native_compile =
        compiler_now_seconds() - phase_start;
    if (rc != 0) {
        result->success = false;
        result->exit_code = rc;
        free(result->error_message);
        result->error_message = pergyra_strdup(
            "Self-host C artifact compilation/link failed");
    }
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
