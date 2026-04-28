#include "compiler_internal.h"

#include <stdlib.h>

#include "../common/string_compat.h"

static CompilerResult *
compiler_result_create(void)
{
    return calloc(1, sizeof(CompilerResult));
}

CompilerResult *
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

CompilerResult *
compiler_error_with_code(const char *message, const char *code)
{
    CompilerResult *result = compiler_error(message);
    if (result == NULL)
        return NULL;
    if (code != NULL)
        result->error_code = pergyra_strdup(code);
    return result;
}

CompilerResult *
compiler_error_full(const char *message,
                    const char *code,
                    const char *cause_ir,
                    const char *fix_source)
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

CompilerResult *
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
