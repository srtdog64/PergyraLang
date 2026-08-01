#include "compiler_internal.h"
#include "compiler_toolchain.h"

#include <stdlib.h>

#include "../common/string_compat.h"

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

CompilerResult *
compiler_compile_link_self_host_llvm_artifact(
    const char *input_llvm_path,
    const char *output_binary_path,
    bool verbose,
    PgyOptProfile opt_profile)
{
    const char *compile_link_argv[16];
    int argc = 0;
    int rc;
    double phase_start;
    CompilerResult *result;
    PgyLlvmIrCompilerSelection clang_selection;

    if (input_llvm_path == NULL || output_binary_path == NULL
        || !pgy_path_is_safe(input_llvm_path)
        || !pgy_path_is_safe(output_binary_path)) {
        return compiler_error(
            "Unsafe or missing self-host LLVM artifact path");
    }
    if (!pgy_select_llvm_ir_compiler(&clang_selection))
        return compiler_error("Unable to detect an LLVM IR-capable clang");

    compile_link_argv[argc++] = clang_selection.cc;
    if (clang_selection.target_flag != NULL)
        compile_link_argv[argc++] = clang_selection.target_flag;
    compile_link_argv[argc++] = "-x";
    compile_link_argv[argc++] = "ir";
    compile_link_argv[argc++] = input_llvm_path;
    compile_link_argv[argc++] =
        opt_profile == PGY_OPT_RELEASE ? "-O3" : "-O0";
    compile_link_argv[argc++] = "-o";
    compile_link_argv[argc++] = output_binary_path;
    compile_link_argv[argc] = NULL;

    result = compiler_success(input_llvm_path, output_binary_path);
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
            "Self-host LLVM artifact compilation/link failed");
    }
    return result;
}
