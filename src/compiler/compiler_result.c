#include "compiler_internal.h"

#include <stdlib.h>
#include <string.h>

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

bool
compiler_result_bind_artifact_identity(
    CompilerResult *result,
    const PgyVerifiedProjectionPlanRow *plan,
    const char *artifact_kind)
{
    char *kind_copy;

    if (result == NULL || plan == NULL || artifact_kind == NULL
        || (strcmp(artifact_kind, "emitted_c") != 0
            && strcmp(artifact_kind, "emitted_llvm") != 0)
        || !pgy_verified_projection_plan_identity_ready(plan)) {
        return false;
    }
    kind_copy = pergyra_strdup(artifact_kind);
    if (kind_copy == NULL)
        return false;
    free(result->artifact_kind);
    result->artifact_kind = kind_copy;
    result->artifact_plan_revision = plan->projection_plan_revision;
    result->artifact_plan_digest = plan->projection_plan_digest;
    return true;
}

bool
compiler_result_artifact_identity_ready(
    const CompilerResult *result, const char **error_out)
{
    if (error_out != NULL)
        *error_out = NULL;
    if (result == NULL || !result->success) {
        if (error_out != NULL)
            *error_out = "compiler artifact identity: result is not successful";
        return false;
    }
    if (result->artifact_kind == NULL
        || (strcmp(result->artifact_kind, "emitted_c") != 0
            && strcmp(result->artifact_kind, "emitted_llvm") != 0)) {
        if (error_out != NULL)
            *error_out = "compiler artifact identity: artifact kind is missing";
        return false;
    }
    if (result->artifact_plan_revision
            != PGY_VERIFIED_PROJECTION_PLAN_REVISION
        || result->artifact_plan_digest == 0) {
        if (error_out != NULL)
            *error_out =
                "compiler artifact identity: verified plan revision/digest is missing";
        return false;
    }
    return true;
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
    free(result->artifact_kind);
    free(result);
}
