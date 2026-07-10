/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend public entry/result lifecycle.
 */

#include <stdio.h>
#include <stdlib.h>

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_program.h"
#include "../common/string_compat.h"
#include "../compiler/verified_projection_plan.h"

static TranspileResult *
transpile_mir_only(const MIRProgram *mir, const char *output_path)
{
    TranspileResult *result = calloc(1, sizeof(TranspileResult));
    TranspilerCtx *ctx;
    PgyVerifiedProjectionPlanRow observability_plan;
    const char *projection_error = NULL;

    if (result == NULL)
        return NULL;

    ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        result->success = false;
        result->error_message = pergyra_strdup("Out of memory");
        return result;
    }

    ctx->mir = mir;
    if (!pgy_verified_projection_plan_intent_observability(
            mir, PGY_PROJECTION_TARGET_C, &observability_plan,
            &projection_error)) {
        transpiler_set_mir_inventory_missing(ctx, "%s",
            projection_error != NULL ? projection_error
                                     : "verified projection plan failed");
    } else {
        ctx->uses_intent_observability =
            observability_plan.disposition == PGY_PROJECTION_MATERIALIZE;
        emit_program(ctx);
    }

    if (ctx->backend_error != NULL) {
        result->success = false;
        result->error_message = pergyra_strdup(ctx->backend_error);
        if (ctx->backend_error_code != NULL)
            result->error_code = pergyra_strdup(ctx->backend_error_code);
        if (ctx->backend_error_cause_ir != NULL)
            result->error_cause_ir = pergyra_strdup(ctx->backend_error_cause_ir);
        if (ctx->backend_error_fix_source != NULL)
            result->error_fix_source = pergyra_strdup(ctx->backend_error_fix_source);
        transpiler_ctx_destroy(ctx);
        return result;
    }

    if (output_path != NULL) {
        if (!codebuf_dump_file(ctx->out, output_path)) {
            char msg[512];
            snprintf(msg, sizeof(msg), "Cannot write output file: %s", output_path);
            result->success = false;
            result->error_message = pergyra_strdup(msg);
            transpiler_ctx_destroy(ctx);
            return result;
        }
    }

    result->success = true;
    result->uses_intent_observability = ctx->uses_intent_observability;
    transpiler_ctx_destroy(ctx);
    return result;
}

TranspileResult *
transpile_from_mir(const MIRProgram *mir, const char *output_path)
{
    return transpile_mir_only(mir, output_path);
}

TranspileResult *
transpile_with_mir(const HIRProgram *hir, const MIRProgram *mir, const char *output_path)
{
    (void)hir;
    if (mir == NULL) {
        TranspileResult *result = calloc(1, sizeof(TranspileResult));
        if (result != NULL) {
            result->success = false;
            result->error_message = pergyra_strdup("MIR-only C backend: missing MIR program");
        }
        return result;
    }
    return transpile_mir_only(mir, output_path);
}

void
transpile_result_destroy(TranspileResult *res)
{
    if (res == NULL)
        return;
    free(res->error_message);
    free(res->error_code);
    free(res->error_cause_ir);
    free(res->error_fix_source);
    free(res);
}
