#include "compiler.h"
#include "path_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>   /* _spawnvp */
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

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
#else
#define PGY_CFLAGS_THREAD_LIB "-lpthread"
#endif

/* -----------------------------------------------------------------
 * Safe process execution (no shell — immune to command injection)
 *
 * argv must be NULL-terminated.
 * Returns process exit code, or -1 on failure.
 * ----------------------------------------------------------------- */
static int
pgy_exec_argv(const char *const argv[], bool verbose)
{
    if (verbose) {
        printf("pgy:");
        for (const char *const *p = argv; *p; p++)
            printf(" %s", *p);
        printf("\n");
    }

#ifdef _WIN32
    intptr_t rc = _spawnvp(_P_WAIT, argv[0], argv);
    return (int)rc;
#else
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return -1;
#endif
}

/* Validate a path contains no shell metacharacters */
static bool
pgy_path_is_safe(const char *path)
{
    for (const char *p = path; *p; p++) {
        switch (*p) {
        case ';': case '&': case '|': case '`':
        case '$': case '(': case ')': case '{':
        case '}': case '<': case '>': case '!':
        case '\n': case '\r':
            return false;
        default:
            break;
        }
    }
    return true;
}

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
invoke_c_backend(const CompilerIRBundle *bundle, const char *output_c_path, char **error_message)
{
    TranspileResult *transpile_result;
    if (error_message != NULL)
        *error_message = NULL;
    if (bundle == NULL || bundle->hir == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("IR bundle is incomplete");
        return 1;
    }

    transpile_result = transpile_with_mir(bundle->hir, bundle->mir, output_c_path);
    if (transpile_result == NULL) {
        *error_message = pergyra_strdup("Out of memory");
        return 1;
    }

    if (!transpile_result->success) {
        *error_message = transpile_result->error_message != NULL
            ? pergyra_strdup(transpile_result->error_message)
            : pergyra_strdup("C backend failed");
        transpile_result_destroy(transpile_result);
        return 1;
    }

    transpile_result_destroy(transpile_result);
    return 0;
}

CompilerResult *
compiler_emit_c(const CompilerIRBundle *bundle, const char *output_c_path)
{
    char *error_message = NULL;
    int rc = invoke_c_backend(bundle, output_c_path, &error_message);
    if (rc != 0) {
        CompilerResult *result = compiler_error(error_message != NULL
            ? error_message
            : "C backend failed");
        free(error_message);
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
    int rc = invoke_c_backend(bundle, output_c_path, &error_message);
    if (rc != 0) {
        CompilerResult *result = compiler_error(error_message != NULL
            ? error_message
            : "C backend failed");
        free(error_message);
        return result;
    }

    if (!pgy_path_is_safe(output_c_path) ||
        !pgy_path_is_safe(output_binary_path)) {
        return compiler_error("Unsafe characters in file path");
    }

    const char *opt_flag = (opt_profile == PGY_OPT_RELEASE) ? "-O3" : "-O0";
#ifdef _WIN32
    const char *gcc_argv[] = {
        "gcc", "-std=c11", "-Wall", opt_flag,
        "-I", PGY_SRC_DIR,
        "-I", PGY_RUNTIME_DIR,
        output_c_path,
        "-o", output_binary_path,
        PGY_CFLAGS_THREAD_LIB,
        "-lm",
        NULL
    };
#else
    const char *gcc_argv[] = {
        "gcc", "-std=c11", "-Wall", opt_flag, "-fopenmp",
        "-I", PGY_SRC_DIR,
        "-I", PGY_RUNTIME_DIR,
        output_c_path,
        "-o", output_binary_path,
        PGY_CFLAGS_THREAD_LIB,
        "-lm",
        NULL
    };
#endif

    rc = pgy_exec_argv(gcc_argv, verbose);
    if (rc != 0) {
        CompilerResult *result = compiler_error("Native compilation failed");
        if (result != NULL) {
            result->exit_code = rc;
            result->c_output_path = pergyra_strdup(output_c_path);
            result->binary_path = pergyra_strdup(output_binary_path);
        }
        return result;
    }

    return compiler_success(output_c_path, output_binary_path);
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
    snprintf(safe_path, sizeof(safe_path), "%s", resolved_path);
    for (char *p = safe_path; *p; p++) {
        if (*p == '/') *p = '\\';
    }
#else
    if (resolved_path[0] == '/' || strncmp(resolved_path, "./", 2) == 0) {
        snprintf(safe_path, sizeof(safe_path), "%s", resolved_path);
    } else {
        snprintf(safe_path, sizeof(safe_path), "./%s", resolved_path);
    }
#endif

    const char *run_argv[] = { safe_path, NULL };

    printf("--- output ---\n");
    int rc = pgy_exec_argv(run_argv, verbose);
    printf("--- end ---\n");
    free(resolved_path);
    return rc;
}

#ifdef PGY_LLVM_ENABLED

CompilerResult *
compiler_emit_llvm_ir(const CompilerIRBundle *bundle, const char *module_name)
{
    if (bundle == NULL || bundle->hir == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        return compiler_error("IR bundle is incomplete");
    }

    LLVMGenResult *gen = llvm_codegen_with_mir(bundle->hir, bundle->mir, module_name);
    if (gen == NULL)
        return compiler_error("Out of memory");

    if (!gen->success) {
        CompilerResult *result = compiler_error(gen->error_message != NULL
            ? gen->error_message
            : "LLVM codegen failed");
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
    if (bundle == NULL || bundle->hir == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        return compiler_error("IR bundle is incomplete");
    }

    LLVMGenResult *gen = llvm_codegen_with_mir(bundle->hir, bundle->mir, module_name);
    if (gen == NULL)
        return compiler_error("Out of memory");

    if (!gen->success) {
        CompilerResult *result = compiler_error(gen->error_message != NULL
            ? gen->error_message
            : "LLVM codegen failed");
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
    if (bundle == NULL || bundle->hir == NULL || bundle->dir == NULL
        || bundle->rir == NULL || bundle->mir == NULL) {
        return compiler_error("IR bundle is incomplete");
    }
    if (verbose)
        printf("pgy: LLVM codegen → %s\n", output_obj_path);

    LLVMGenResult *gen = llvm_codegen_to_object_with_mir(bundle->hir,
                                                         bundle->mir,
                                                         "pergyra_module",
                                                         output_obj_path,
                                                         opt_profile == PGY_OPT_RELEASE);
    if (gen == NULL)
        return compiler_error("Out of memory");

    if (!gen->success) {
        CompilerResult *result = compiler_error(gen->error_message != NULL
            ? gen->error_message
            : "LLVM codegen failed");
        llvm_gen_result_destroy(gen);
        return result;
    }
    llvm_gen_result_destroy(gen);

    /* Link object file with GCC + runtime library */
    if (!pgy_path_is_safe(output_binary_path) ||
        !pgy_path_is_safe(output_obj_path)) {
        return compiler_error("Unsafe characters in file path");
    }

    const char *opt_flag = (opt_profile == PGY_OPT_RELEASE) ? "-O3" : "-O0";
    const char *link_argv_release[] = {
        "gcc", "-std=c11", opt_flag, "-march=native", "-mtune=native",
#ifndef _WIN32
        "-fopenmp",
#endif
#ifndef _WIN32
        "-no-pie",
#endif
        "-DPGY_LLVM_ENABLED",
        "-I", PGY_SRC_DIR,
        "-o", output_binary_path, output_obj_path,
        PGY_RUNTIME_LIB_C,
        PGY_CFLAGS_THREAD_LIB,
        "-lm",
        NULL
    };
    const char *link_argv_dev[] = {
        "gcc", "-std=c11", opt_flag,
#ifndef _WIN32
        "-fopenmp",
#endif
#ifndef _WIN32
        "-no-pie",
#endif
        "-DPGY_LLVM_ENABLED",
        "-I", PGY_SRC_DIR,
        "-o", output_binary_path, output_obj_path,
        PGY_RUNTIME_LIB_C,
        PGY_CFLAGS_THREAD_LIB,
        "-lm",
        NULL
    };
    const char *const *link_argv =
        (opt_profile == PGY_OPT_RELEASE) ? link_argv_release : link_argv_dev;

    int rc = pgy_exec_argv(link_argv, verbose);
    if (rc != 0) {
        CompilerResult *result = compiler_error("LLVM link failed");
        if (result != NULL) {
            result->exit_code = rc;
            result->binary_path = pergyra_strdup(output_binary_path);
        }
        return result;
    }

    return compiler_success(output_obj_path, output_binary_path);
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
    free(result->c_output_path);
    free(result->binary_path);
    free(result);
}
