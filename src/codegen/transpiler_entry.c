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
transpile_mir_only(const MIRProgram *mir,
                   const PgyVerifiedProjectionPlanRow *projection_plan,
                   const PgyVerifiedParallelCapturePlan *parallel_capture_plan,
                   const PgySpawnLanePlan *spawn_lane_plan,
                   const PgyRegionPlan *region_plan,
                   const char *output_path)
{
    TranspileResult *result = calloc(1, sizeof(TranspileResult));
    TranspilerCtx *ctx;
    PgyVerifiedProjectionPlanRow observability_plan;

    if (result == NULL)
        return NULL;

    ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        result->success = false;
        result->error_message = pergyra_strdup("Out of memory");
        return result;
    }

    ctx->mir = mir;
    ctx->projection_plan = projection_plan;
    ctx->parallel_capture_plan = parallel_capture_plan;
    ctx->spawn_lane_plan = spawn_lane_plan;
    ctx->region_plan = region_plan;
    if (projection_plan == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "%s", "C backend: verified projection plan required");
    } else if (!projection_plan->verified
               || projection_plan->target != PGY_PROJECTION_TARGET_C) {
        transpiler_set_mir_inventory_missing(ctx, "%s",
            "C backend: projection plan is not verified for C");
    } else if (!pgy_verified_projection_plan_identity_ready(projection_plan)) {
        transpiler_set_mir_inventory_missing(ctx, "%s",
            "C backend: verified projection plan identity is invalid");
    } else if (projection_plan->air_certificate_schema == NULL
               || projection_plan->air_certificate_fingerprint == 0) {
        transpiler_set_mir_inventory_missing(ctx, "%s",
            "C backend: AIR evidence certificate is missing from projection plan");
    } else if (projection_plan->target_capability_fingerprint == 0) {
        transpiler_set_mir_inventory_missing(ctx, "%s",
            "C backend: target capability fingerprint is missing");
    } else if (projection_plan->machine_layer_manifest_fingerprint == 0) {
        transpiler_set_mir_inventory_missing(ctx, "%s",
            "C backend: machine-layer manifest fingerprint is missing");
    } else if (projection_plan->machine_layer_physical_manifest_fingerprint == 0) {
        transpiler_set_mir_inventory_missing(ctx, "%s",
            "C backend: physical machine declaration fingerprint is missing");
    } else if (spawn_lane_plan == NULL || !spawn_lane_plan->verified
               || spawn_lane_plan->revision != PGY_SPAWN_LANE_PLAN_REVISION) {
        transpiler_set_mir_inventory_missing(ctx, "%s",
            "C backend: verified spawn-lane plan required");
    } else if (parallel_capture_plan == NULL
               || !pgy_verified_parallel_capture_plan_identity_ready(
                      parallel_capture_plan)) {
        transpiler_set_mir_inventory_missing(ctx, "%s",
            "C backend: verified parallel-capture plan required");
    } else {
        observability_plan = *projection_plan;
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
    return transpile_mir_only(mir, NULL, NULL, NULL, NULL, output_path);
}

TranspileResult *
transpile_from_mir_with_projection_plan(
    const MIRProgram *mir,
    const PgyVerifiedProjectionPlanRow *projection_plan,
    const PgySpawnLanePlan *spawn_lane_plan,
    const char *output_path)
{
    return transpile_mir_only(mir, projection_plan, NULL,
                              spawn_lane_plan, NULL, output_path);
}

TranspileResult *
transpile_from_mir_with_projection_plans(
    const MIRProgram *mir,
    const PgyVerifiedProjectionPlanRow *projection_plan,
    const PgyVerifiedParallelCapturePlan *parallel_capture_plan,
    const PgySpawnLanePlan *spawn_lane_plan,
    const PgyRegionPlan *region_plan,
    const char *output_path)
{
    return transpile_mir_only(mir, projection_plan, parallel_capture_plan,
                              spawn_lane_plan, region_plan, output_path);
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
    return transpile_mir_only(mir, NULL, NULL, NULL, NULL, output_path);
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
