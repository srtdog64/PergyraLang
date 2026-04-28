#include "compiler.h"
#include "compiler_toolchain.h"
#include "path_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../codegen/transpiler.h"
#include "../common/string_compat.h"

#ifndef PGY_SRC_DIR
#define PGY_SRC_DIR "src"
#endif

#ifndef PGY_RUNTIME_DIR
#define PGY_RUNTIME_DIR "src/runtime"
#endif

#ifndef PGY_RUNTIME_LIB_C
#define PGY_RUNTIME_LIB_C "src/runtime/pgy_runtime_lib.c"
#endif

#ifdef PGY_LLVM_ENABLED
#include "../codegen/llvm_backend.h"
#endif

#ifdef _WIN32
#define PGY_CFLAGS_THREAD_LIB "-lwinpthread"
#define PGY_CFLAGS_THREAD_FLAG "-pthread"
#else
#define PGY_CFLAGS_THREAD_LIB "-lpthread"
#define PGY_CFLAGS_THREAD_FLAG "-pthread"
#endif

static CompilerResult *
compiler_result_create(void)
{
    return calloc(1, sizeof(CompilerResult));
}

static CompilerResult *
compiler_error(const char *message)
{
    CompilerResult *result = compiler_result_create();
    if (result == NULL)
        return NULL;

    result->success = false;
    result->exit_code = 1;
    result->error_message = pergyra_strdup(message);
    return result;
}

/* Variant that attaches a stable diagnostic code (e.g. "PGY_LLVM_SPEC_LIMIT").
 * `code` ownership transfers: caller must pass either NULL or a heap-allocated
 * string (typically `pergyra_strdup` of the source ctx code). `code == NULL`
 * is equivalent to `compiler_error(message)`. */
static CompilerResult *
compiler_error_with_code(const char *message, const char *code)
{
    CompilerResult *result = compiler_error(message);
    if (result == NULL)
        return NULL;
    if (code != NULL)
        result->error_code = pergyra_strdup(code);
    return result;
}

/* Full variant that also propagates the `cause_ir` and `fix_source`
 * routing tags. All three hint strings are optional; passing NULL omits
 * the corresponding field. Strings are strdup'd into owning storage; the
 * backend error ctx holds non-owning static literals. */
static CompilerResult *
compiler_error_full(const char *message, const char *code,
                    const char *cause_ir, const char *fix_source)
{
    CompilerResult *result = compiler_error_with_code(message, code);
    if (result == NULL)
        return NULL;
    if (cause_ir != NULL)
        result->error_cause_ir = pergyra_strdup(cause_ir);
    if (fix_source != NULL)
        result->error_fix_source = pergyra_strdup(fix_source);
    return result;
}

static CompilerResult *
compiler_success(const char *output_c_path, const char *output_binary_path)
{
    CompilerResult *result = compiler_result_create();
    if (result == NULL)
        return NULL;

    result->success = true;
    result->c_output_path = output_c_path != NULL ? pergyra_strdup(output_c_path) : NULL;
    result->binary_path = output_binary_path != NULL ? pergyra_strdup(output_binary_path) : NULL;
    return result;
}

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

#ifdef PGY_LLVM_ENABLED

CompilerResult *
compiler_emit_llvm_ir(const CompilerIRBundle *bundle, const char *module_name)
{
    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        return compiler_error("IR bundle is incomplete");
    }

    LLVMGenResult *gen = llvm_codegen_from_mir(bundle->mir, module_name);
    if (gen == NULL)
        return compiler_error("Out of memory");

    if (!gen->success) {
        CompilerResult *result = compiler_error_full(
            gen->error_message != NULL
                ? gen->error_message
                : "LLVM codegen failed",
            gen->error_code,
            gen->error_cause_ir,
            gen->error_fix_source);
        llvm_gen_result_destroy(gen);
        return result;
    }

    /* Print IR to stdout */
    if (gen->ir_text != NULL)
        printf("%s", gen->ir_text);

    llvm_gen_result_destroy(gen);
    return compiler_success(NULL, NULL);
}

CompilerResult *
compiler_emit_llvm_ir_to_file(const CompilerIRBundle *bundle,
                              const char *module_name,
                              const char *output_ir_path)
{
    if (!pgy_path_is_safe(output_ir_path))
        return compiler_error("Unsafe characters in file path");
    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        return compiler_error("IR bundle is incomplete");
    }

    LLVMGenResult *gen = llvm_codegen_from_mir(bundle->mir, module_name);
    if (gen == NULL)
        return compiler_error("Out of memory");

    if (!gen->success) {
        CompilerResult *result = compiler_error_full(
            gen->error_message != NULL
                ? gen->error_message
                : "LLVM codegen failed",
            gen->error_code,
            gen->error_cause_ir,
            gen->error_fix_source);
        llvm_gen_result_destroy(gen);
        return result;
    }

    FILE *out = fopen(output_ir_path, "w");
    if (out == NULL) {
        llvm_gen_result_destroy(gen);
        return compiler_error("Cannot open LLVM IR output file");
    }

    if (gen->ir_text != NULL)
        fputs(gen->ir_text, out);
    fclose(out);

    llvm_gen_result_destroy(gen);
    return compiler_success(output_ir_path, NULL);
}

CompilerResult *
compiler_build_native_llvm(const CompilerIRBundle *bundle,
                           const char *output_obj_path,
                           const char *output_binary_path,
                           bool verbose,
                           PgyOptProfile opt_profile)
{
    char *runtime_obj_path = NULL;
    double phase_start;
    CompilerResult *result = NULL;
    bool compiled_runtime = false;
    bool uses_intent_observability = false;

    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        return compiler_error("IR bundle is incomplete");
    }
    if (verbose)
        printf("pgy: LLVM codegen -> %s\n", output_obj_path);

    compiler_debug_llvm_host_stage("codegen_begin");
    phase_start = compiler_now_seconds();
    LLVMGenResult *gen = llvm_codegen_to_object_from_mir(
        bundle->mir,
        "pergyra_module",
        output_obj_path,
        opt_profile == PGY_OPT_RELEASE);
    if (gen == NULL)
        return compiler_error("Out of memory");

    if (!gen->success) {
        CompilerResult *error_result = compiler_error_full(
            gen->error_message != NULL
                ? gen->error_message
                : "LLVM codegen failed",
            gen->error_code,
            gen->error_cause_ir,
            gen->error_fix_source);
        if (error_result != NULL)
            error_result->backend_timings.codegen = compiler_now_seconds() - phase_start;
        llvm_gen_result_destroy(gen);
        return error_result;
    }
    uses_intent_observability = gen->uses_intent_observability;
    llvm_gen_result_destroy(gen);
    compiler_debug_llvm_host_stage("codegen_done");

    /* Link object file with GCC + runtime library */
    if (!pgy_path_is_safe(output_binary_path) ||
        !pgy_path_is_safe(output_obj_path)) {
        return compiler_error("Unsafe characters in file path");
    }

    runtime_obj_path = compiler_runtime_prebuilt_object_path(
        opt_profile,
        uses_intent_observability);
    bool using_prebuilt_runtime = (runtime_obj_path != NULL);
    if (runtime_obj_path == NULL) {
        runtime_obj_path = compiler_runtime_cache_object_path(
            opt_profile,
            uses_intent_observability);
    }
    if (runtime_obj_path == NULL)
        return compiler_error("Out of memory");
    if (!pgy_path_is_safe(runtime_obj_path)) {
        free(runtime_obj_path);
        return compiler_error("Unsafe characters in file path");
    }

    const char *opt_flag = (opt_profile == PGY_OPT_RELEASE) ? "-O3" : "-O0";
    const char *intent_observability_flag =
        uses_intent_observability
            ? "-DPGY_INTENT_OBSERVABILITY_ENABLED=1"
            : "-DPGY_INTENT_OBSERVABILITY_ENABLED=0";
    const char *cc = pgy_detect_c_compiler();
    const char *cc_target = pgy_cc_extra_target_flag();
    result = compiler_success(output_obj_path, output_binary_path);
    if (result == NULL) {
        free(runtime_obj_path);
        return NULL;
    }
    result->backend_timings.codegen = compiler_now_seconds() - phase_start;
    compiler_debug_llvm_host_stage("runtime_prepare");
#ifdef _WIN32
    const char *compile_runtime_argv[24];
    int compile_runtime_argc = 0;
    compile_runtime_argv[compile_runtime_argc++] = cc;
    if (cc_target != NULL)
        compile_runtime_argv[compile_runtime_argc++] = cc_target;
    compile_runtime_argv[compile_runtime_argc++] = "-std=c11";
    compile_runtime_argv[compile_runtime_argc++] = "-Wall";
    compile_runtime_argv[compile_runtime_argc++] = "-Wno-unused-value";
    compile_runtime_argv[compile_runtime_argc++] = "-Wno-parentheses-equality";
    compile_runtime_argv[compile_runtime_argc++] = "-Wno-c23-extensions";
    compile_runtime_argv[compile_runtime_argc++] = "-Wno-format-truncation";
    compile_runtime_argv[compile_runtime_argc++] = PGY_CFLAGS_THREAD_FLAG;
    compile_runtime_argv[compile_runtime_argc++] = opt_flag;
    compile_runtime_argv[compile_runtime_argc++] = intent_observability_flag;
    compile_runtime_argv[compile_runtime_argc++] = "-DPGY_LLVM_ENABLED";
    compile_runtime_argv[compile_runtime_argc++] = "-I";
    compile_runtime_argv[compile_runtime_argc++] = PGY_SRC_DIR;
    compile_runtime_argv[compile_runtime_argc++] = "-c";
    compile_runtime_argv[compile_runtime_argc++] = PGY_RUNTIME_LIB_C;
    compile_runtime_argv[compile_runtime_argc++] = "-o";
    compile_runtime_argv[compile_runtime_argc++] = runtime_obj_path;
    compile_runtime_argv[compile_runtime_argc] = NULL;
#else
    const char *compile_runtime_argv[20];
    int compile_runtime_argc = 0;
    compile_runtime_argv[compile_runtime_argc++] = cc;
    if (cc_target != NULL)
        compile_runtime_argv[compile_runtime_argc++] = cc_target;
    compile_runtime_argv[compile_runtime_argc++] = "-std=c11";
    compile_runtime_argv[compile_runtime_argc++] = "-Wall";
    compile_runtime_argv[compile_runtime_argc++] = opt_flag;
    compile_runtime_argv[compile_runtime_argc++] = "-fopenmp";
    compile_runtime_argv[compile_runtime_argc++] = intent_observability_flag;
    compile_runtime_argv[compile_runtime_argc++] = "-DPGY_LLVM_ENABLED";
    compile_runtime_argv[compile_runtime_argc++] = "-I";
    compile_runtime_argv[compile_runtime_argc++] = PGY_SRC_DIR;
    compile_runtime_argv[compile_runtime_argc++] = "-c";
    compile_runtime_argv[compile_runtime_argc++] = PGY_RUNTIME_LIB_C;
    compile_runtime_argv[compile_runtime_argc++] = "-o";
    compile_runtime_argv[compile_runtime_argc++] = runtime_obj_path;
    compile_runtime_argv[compile_runtime_argc] = NULL;
#endif
    phase_start = compiler_now_seconds();
    if (using_prebuilt_runtime && !compiler_runtime_cache_is_fresh(runtime_obj_path)) {
        result->success = false;
        result->exit_code = 1;
        free(result->error_message);
        result->error_message = pergyra_strdup(
            "Prebuilt LLVM runtime object is stale or missing");
        result->backend_timings.native_compile = compiler_now_seconds() - phase_start;
        free(runtime_obj_path);
        return result;
    }
    if (!using_prebuilt_runtime && !compiler_runtime_cache_is_fresh(runtime_obj_path)) {
        compiler_debug_llvm_host_stage("runtime_compile");
        int rc = pgy_exec_argv(compile_runtime_argv, verbose);
        compiled_runtime = true;
        if (rc != 0) {
            result->success = false;
            result->exit_code = rc;
            free(result->error_message);
            result->error_message = pergyra_strdup("LLVM runtime compilation failed");
            result->backend_timings.native_compile = compiler_now_seconds() - phase_start;
            remove(runtime_obj_path);
            free(runtime_obj_path);
            return result;
        }
    }
    result->backend_timings.native_compile = compiler_now_seconds() - phase_start;

    compiler_debug_llvm_host_stage("link_begin");
    phase_start = compiler_now_seconds();
    int rc;
#ifdef _WIN32
    {
        const char *link_argv[22];
        int link_argc = 0;
        link_argv[link_argc++] = cc;
        if (cc_target != NULL)
            link_argv[link_argc++] = cc_target;
        link_argv[link_argc++] = "-std=c11";
        link_argv[link_argc++] = opt_flag;
        link_argv[link_argc++] = PGY_CFLAGS_THREAD_FLAG;
        link_argv[link_argc++] = "-mconsole";
        link_argv[link_argc++] = "-DPGY_LLVM_ENABLED";
        link_argv[link_argc++] = "-I";
        link_argv[link_argc++] = PGY_SRC_DIR;
        link_argv[link_argc++] = "-o";
        link_argv[link_argc++] = output_binary_path;
        link_argv[link_argc++] = output_obj_path;
        link_argv[link_argc++] = runtime_obj_path;
        link_argv[link_argc++] = PGY_CFLAGS_THREAD_LIB;
        link_argv[link_argc++] = "-lm";
        link_argv[link_argc] = NULL;
        rc = pgy_exec_argv(link_argv, verbose);
    }
#else
    {
        const char *link_argv[22];
        int link_argc = 0;
        link_argv[link_argc++] = cc;
        if (cc_target != NULL)
            link_argv[link_argc++] = cc_target;
        link_argv[link_argc++] = "-std=c11";
        link_argv[link_argc++] = opt_flag;
        if (opt_profile == PGY_OPT_RELEASE) {
            link_argv[link_argc++] = "-march=native";
            link_argv[link_argc++] = "-mtune=native";
        }
        link_argv[link_argc++] = "-fopenmp";
        if (compiler_should_use_lld())
            link_argv[link_argc++] = "-fuse-ld=lld";
        link_argv[link_argc++] = "-no-pie";
        link_argv[link_argc++] = "-Wl,--build-id=none";
        link_argv[link_argc++] = "-DPGY_LLVM_ENABLED";
        link_argv[link_argc++] = "-I";
        link_argv[link_argc++] = PGY_SRC_DIR;
        link_argv[link_argc++] = "-o";
        link_argv[link_argc++] = output_binary_path;
        link_argv[link_argc++] = output_obj_path;
        link_argv[link_argc++] = runtime_obj_path;
        link_argv[link_argc++] = PGY_CFLAGS_THREAD_LIB;
        link_argv[link_argc++] = "-lm";
        link_argv[link_argc] = NULL;
        rc = pgy_exec_argv(link_argv, verbose);
    }
#endif
    if (rc != 0) {
        result->success = false;
        result->exit_code = rc;
        free(result->error_message);
        result->error_message = pergyra_strdup("LLVM link failed");
        result->backend_timings.link = compiler_now_seconds() - phase_start;
        if (compiled_runtime)
            remove(runtime_obj_path);
        free(runtime_obj_path);
        return result;
    }
    result->backend_timings.link = compiler_now_seconds() - phase_start;

    compiler_debug_llvm_host_stage("link_done");
    free(runtime_obj_path);
    compiler_debug_llvm_host_stage("return");
    return result;
}

#endif /* PGY_LLVM_ENABLED */

#ifndef PGY_LLVM_ENABLED

CompilerResult *
compiler_emit_llvm_ir(const CompilerIRBundle *bundle, const char *module_name)
{
    (void)bundle;
    (void)module_name;
    return compiler_error("LLVM backend not available in this build");
}

CompilerResult *
compiler_emit_llvm_ir_to_file(const CompilerIRBundle *bundle,
                              const char *module_name,
                              const char *output_ir_path)
{
    (void)bundle;
    (void)module_name;
    (void)output_ir_path;
    return compiler_error("LLVM backend not available in this build");
}

CompilerResult *
compiler_build_native_llvm(const CompilerIRBundle *bundle,
                           const char *output_obj_path,
                           const char *output_binary_path,
                           bool verbose,
                           PgyOptProfile opt_profile)
{
    (void)bundle;
    (void)output_obj_path;
    (void)output_binary_path;
    (void)verbose;
    (void)opt_profile;
    return compiler_error("LLVM backend not available in this build");
}

#endif /* !PGY_LLVM_ENABLED */

void
compiler_result_destroy(CompilerResult *result)
{
    if (result == NULL)
        return;

    free(result->error_message);
    free(result->error_code);
    free(result->error_cause_ir);
    free(result->error_fix_source);
    free(result->c_output_path);
    free(result->binary_path);
    free(result);
}
