#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>

#include "../codegen/transpiler.h"
#include "../common/string_compat.h"

#ifdef PGY_LLVM_ENABLED
#include "../codegen/llvm_backend.h"
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
invoke_c_backend(ASTNode *ast, const char *output_c_path, char **error_message)
{
    TranspileResult *transpile_result = transpile(ast, output_c_path);
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
compiler_emit_c(ASTNode *ast, const char *output_c_path)
{
    char *error_message = NULL;
    int rc = invoke_c_backend(ast, output_c_path, &error_message);
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
compiler_build_native(ASTNode *ast,
                      const char *output_c_path,
                      const char *output_binary_path,
                      bool verbose)
{
    char *error_message = NULL;
    int rc = invoke_c_backend(ast, output_c_path, &error_message);
    if (rc != 0) {
        CompilerResult *result = compiler_error(error_message != NULL
            ? error_message
            : "C backend failed");
        free(error_message);
        return result;
    }

    char command[1024];
    snprintf(command, sizeof(command),
             "gcc -std=c11 -Wall -O2 "
             "-I src "
             "-I src/runtime "
             "%s "
             "-o %s "
             "-lpthread",
             output_c_path, output_binary_path);

    if (verbose)
        printf("pgy: %s\n", command);

    rc = system(command);
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
    char run_command[512];

#ifdef _WIN32
    snprintf(run_command, sizeof(run_command), "%s", binary_path);
    for (char *p = run_command; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }
#else
    snprintf(run_command, sizeof(run_command), "./%s", binary_path);
#endif

    if (verbose)
        printf("pgy: running %s\n", binary_path);

    printf("--- output ---\n");
    int rc = system(run_command);
    printf("--- end ---\n");
    return rc;
}

#ifdef PGY_LLVM_ENABLED

CompilerResult *
compiler_emit_llvm_ir(ASTNode *ast, const char *module_name)
{
    LLVMGenResult *gen = llvm_codegen(ast, module_name);
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
compiler_build_native_llvm(ASTNode *ast,
                           const char *output_obj_path,
                           const char *output_binary_path,
                           bool verbose)
{
    if (verbose)
        printf("pgy: LLVM codegen → %s\n", output_obj_path);

    LLVMGenResult *gen = llvm_codegen_to_object(ast, "pergyra_module",
                                                 output_obj_path);
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
    char command[2048];
    snprintf(command, sizeof(command),
             "gcc -std=c11 -O2 "
             "-DPGY_LLVM_ENABLED "
             "-I src "
             "-o %s %s "
             "src/runtime/pgy_runtime_lib.c "
             "-lpthread",
             output_binary_path, output_obj_path);

    if (verbose)
        printf("pgy: %s\n", command);

    int rc = system(command);
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
