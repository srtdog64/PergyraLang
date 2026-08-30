#include "compiler_internal.h"
#include "compiler_release_artifact_policy.h"
#include "compiler_toolchain.h"
#include "verified_projection_plan.h"

#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"

#ifdef PGY_LLVM_ENABLED
#include "../codegen/llvm_backend.h"

CompilerResult *
compiler_emit_llvm_ir(const CompilerIRBundle *bundle,
                      const AIRProgram *air,
                      const char *module_name)
{
    PgyVerifiedProjectionPlanRow projection_plan;
    const char *projection_error = NULL;
    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL
        || air == NULL) {
        return compiler_error("IR bundle is incomplete");
    }

    if (!pgy_verified_projection_plan_intent_observability_with_air(
            air, bundle->mir, PGY_PROJECTION_TARGET_LLVM,
            &projection_plan, &projection_error)) {
        return compiler_error(projection_error != NULL
            ? projection_error : "verified projection plan failed");
    }
    PgySpawnLanePlan spawn_lane_plan;
    PgyVerifiedParallelCapturePlan parallel_capture_plan = {0};
    if (!pgy_verified_spawn_lane_plan_from_air(air, &spawn_lane_plan,
            &projection_error)) {
        return compiler_error(projection_error != NULL
            ? projection_error : "verified spawn-lane plan failed");
    }

    if (!pgy_verified_parallel_capture_plan_from_air(
            air, bundle->mir, &parallel_capture_plan, &projection_error)) {
        pgy_verified_spawn_lane_plan_dispose(&spawn_lane_plan);
        return compiler_error(projection_error != NULL
            ? projection_error : "verified parallel-capture plan failed");
    }

    LLVMGenResult *gen = llvm_codegen_from_mir_with_projection_plans(
        bundle->mir, &projection_plan, &parallel_capture_plan,
        &spawn_lane_plan, bundle->region_plan, module_name);
    pgy_verified_spawn_lane_plan_dispose(&spawn_lane_plan);
    pgy_verified_parallel_capture_plan_dispose(&parallel_capture_plan);
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
    CompilerResult *result = compiler_success(NULL, NULL);
    if (result == NULL)
        return NULL;
    if (!compiler_result_bind_artifact_identity(
            result, &projection_plan, "emitted_llvm")) {
        compiler_result_destroy(result);
        return compiler_error(
            "LLVM artifact identity could not bind the verified projection plan");
    }
    return result;
}

CompilerResult *
compiler_emit_llvm_ir_to_file(const CompilerIRBundle *bundle,
                              const AIRProgram *air,
                              const char *module_name,
                              const char *output_ir_path)
{
    PgyVerifiedProjectionPlanRow projection_plan;
    const char *projection_error = NULL;
    if (!pgy_path_is_safe(output_ir_path))
        return compiler_error("Unsafe characters in file path");
    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL
        || air == NULL) {
        return compiler_error("IR bundle is incomplete");
    }

    if (!pgy_verified_projection_plan_intent_observability_with_air(
            air, bundle->mir, PGY_PROJECTION_TARGET_LLVM,
            &projection_plan, &projection_error)) {
        return compiler_error(projection_error != NULL
            ? projection_error : "verified projection plan failed");
    }
    PgySpawnLanePlan spawn_lane_plan;
    PgyVerifiedParallelCapturePlan parallel_capture_plan = {0};
    if (!pgy_verified_spawn_lane_plan_from_air(air, &spawn_lane_plan,
            &projection_error)) {
        return compiler_error(projection_error != NULL
            ? projection_error : "verified spawn-lane plan failed");
    }

    if (!pgy_verified_parallel_capture_plan_from_air(
            air, bundle->mir, &parallel_capture_plan, &projection_error)) {
        pgy_verified_spawn_lane_plan_dispose(&spawn_lane_plan);
        return compiler_error(projection_error != NULL
            ? projection_error : "verified parallel-capture plan failed");
    }

    LLVMGenResult *gen = llvm_codegen_from_mir_with_projection_plans(
        bundle->mir, &projection_plan, &parallel_capture_plan,
        &spawn_lane_plan, bundle->region_plan, module_name);
    pgy_verified_spawn_lane_plan_dispose(&spawn_lane_plan);
    pgy_verified_parallel_capture_plan_dispose(&parallel_capture_plan);
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
    CompilerResult *result = compiler_success(output_ir_path, NULL);
    if (result == NULL)
        return NULL;
    if (!compiler_result_bind_artifact_identity(
            result, &projection_plan, "emitted_llvm")) {
        compiler_result_destroy(result);
        return compiler_error(
            "LLVM artifact identity could not bind the verified projection plan");
    }
    return result;
}

CompilerResult *
compiler_build_native_llvm(const CompilerIRBundle *bundle,
                           const AIRProgram *air,
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
    const char *runtime_error = NULL;
    PgyVerifiedProjectionPlanRow projection_plan;
    PgyVerifiedParallelCapturePlan parallel_capture_plan = {0};
    const char *projection_error = NULL;

    if (bundle == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL
        || air == NULL) {
        return compiler_error("IR bundle is incomplete");
    }
    if (!pgy_verified_projection_plan_intent_observability_with_air(
            air, bundle->mir, PGY_PROJECTION_TARGET_LLVM,
            &projection_plan, &projection_error)) {
        return compiler_error(projection_error != NULL
            ? projection_error : "verified projection plan failed");
    }
    PgySpawnLanePlan spawn_lane_plan;
    if (!pgy_verified_spawn_lane_plan_from_air(air, &spawn_lane_plan,
            &projection_error)) {
        return compiler_error(projection_error != NULL
            ? projection_error : "verified spawn-lane plan failed");
    }
    if (!pgy_verified_parallel_capture_plan_from_air(
            air, bundle->mir, &parallel_capture_plan, &projection_error)) {
        pgy_verified_spawn_lane_plan_dispose(&spawn_lane_plan);
        return compiler_error(projection_error != NULL
            ? projection_error : "verified parallel-capture plan failed");
    }
    if (verbose)
        printf("pgy: LLVM codegen -> %s\n", output_obj_path);

    compiler_debug_llvm_host_stage("codegen_begin");
    phase_start = compiler_now_seconds();
    LLVMGenResult *gen = llvm_codegen_to_object_from_mir_with_projection_plans(
        bundle->mir, &projection_plan, &parallel_capture_plan,
        &spawn_lane_plan,
        bundle->region_plan,
        "pergyra_module",
        output_obj_path,
        opt_profile == PGY_OPT_RELEASE);
    pgy_verified_spawn_lane_plan_dispose(&spawn_lane_plan);
    pgy_verified_parallel_capture_plan_dispose(&parallel_capture_plan);
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

    const char *opt_flag = (opt_profile == PGY_OPT_RELEASE) ? "-O3" : "-O0";
    const char *release_artifact_flag =
        compiler_release_artifact_link_flag(opt_profile);
    PgyCCompilerSelection cc_selection;
    if (!pgy_select_c_compiler(&cc_selection))
        return compiler_error("Unable to detect C compiler");
    const char *cc = cc_selection.cc;
    const char *cc_target = cc_selection.target_flag;
    result = compiler_success(output_obj_path, output_binary_path);
    if (result == NULL)
        return NULL;
    if (!compiler_result_bind_artifact_identity(
            result, &projection_plan, "emitted_llvm")) {
        compiler_result_destroy(result);
        return compiler_error(
            "LLVM artifact identity could not bind the verified projection plan");
    }
    result->backend_timings.codegen = compiler_now_seconds() - phase_start;
    compiler_debug_llvm_host_stage("runtime_prepare");
    phase_start = compiler_now_seconds();
    runtime_obj_path = compiler_llvm_runtime_object_ensure(
        opt_profile, uses_intent_observability, verbose,
        &compiled_runtime, &runtime_error);
    if (runtime_obj_path == NULL) {
        result->success = false;
        result->exit_code = 1;
        free(result->error_message);
        result->error_message = pergyra_strdup(runtime_error != NULL
            ? runtime_error : "LLVM runtime object unavailable");
        result->backend_timings.native_compile = compiler_now_seconds() - phase_start;
        return result;
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
        if (release_artifact_flag != NULL)
            link_argv[link_argc++] = release_artifact_flag;
        link_argv[link_argc++] = PGY_CFLAGS_THREAD_FLAG;
        link_argv[link_argc++] = "-mconsole";
        link_argv[link_argc++] = "-DPGY_LLVM_ENABLED";
        link_argv[link_argc++] = "-I";
        link_argv[link_argc++] = PGY_SRC_DIR;
        link_argv[link_argc++] = "-o";
        link_argv[link_argc++] = output_binary_path;
        link_argv[link_argc++] = output_obj_path;
        link_argv[link_argc++] = runtime_obj_path;
#ifndef __APPLE__
        link_argv[link_argc++] = PGY_CFLAGS_THREAD_LIB;
#endif
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
        if (release_artifact_flag != NULL)
            link_argv[link_argc++] = release_artifact_flag;
        if (opt_profile == PGY_OPT_RELEASE) {
            link_argv[link_argc++] = "-march=native";
            link_argv[link_argc++] = "-mtune=native";
        }
#ifndef __APPLE__
        link_argv[link_argc++] = "-fopenmp";
        if (compiler_should_use_lld())
            link_argv[link_argc++] = "-fuse-ld=lld";
        link_argv[link_argc++] = "-no-pie";
        link_argv[link_argc++] = "-Wl,--build-id=none";
#endif
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
compiler_emit_llvm_ir(const CompilerIRBundle *bundle,
                      const AIRProgram *air,
                      const char *module_name)
{
    (void)bundle;
    (void)air;
    (void)module_name;
    return compiler_error("this build was compiled without LLVM backend support");
}

CompilerResult *
compiler_emit_llvm_ir_to_file(const CompilerIRBundle *bundle,
                              const AIRProgram *air,
                              const char *module_name,
                              const char *output_ir_path)
{
    (void)bundle;
    (void)air;
    (void)module_name;
    (void)output_ir_path;
    return compiler_error("this build was compiled without LLVM backend support");
}

CompilerResult *
compiler_build_native_llvm(const CompilerIRBundle *bundle,
                           const AIRProgram *air,
                           const char *output_obj_path,
                           const char *output_binary_path,
                           bool verbose,
                           PgyOptProfile opt_profile)
{
    (void)bundle;
    (void)air;
    (void)output_obj_path;
    (void)output_binary_path;
    (void)verbose;
    (void)opt_profile;
    return compiler_error("this build was compiled without LLVM backend support");
}

#endif
