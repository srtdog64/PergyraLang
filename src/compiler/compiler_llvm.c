#include "compiler_internal.h"
#include "compiler_toolchain.h"

#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"

#ifdef PGY_LLVM_ENABLED
#include "../codegen/llvm_backend.h"

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

#else

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

#endif
