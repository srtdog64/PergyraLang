/*
 * AIR-bound projection of MIR parallel-capture facts.
 *
 * The MIR capture inventory remains the semantic owner.  This file owns only
 * the verified carrier consumed by C and LLVM, so a backend cannot silently
 * re-query the MIR table after the AIR admission boundary.
 */

#include "verified_projection_plan.h"

#include <stdlib.h>
#include <string.h>

#include "air.h"
#include "air_evidence_certificate.h"
#include "mir_parallel_capture_facts.h"
#include "../parser/ast_api.h"

static uint64_t
capture_plan_mix_byte(uint64_t hash, uint8_t byte)
{
    hash ^= (uint64_t)byte;
    return hash * UINT64_C(1099511628211);
}

static uint64_t
capture_plan_mix_u32(uint64_t hash, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++)
        hash = capture_plan_mix_byte(hash,
            (uint8_t)((value >> (i * 8)) & 0xffu));
    return hash;
}

static uint64_t
capture_plan_mix_u64(uint64_t hash, uint64_t value)
{
    for (unsigned i = 0; i < 8; i++)
        hash = capture_plan_mix_byte(hash,
            (uint8_t)((value >> (i * 8)) & UINT64_C(0xff)));
    return hash;
}

static uint64_t
capture_plan_mix_bool(uint64_t hash, bool value)
{
    return capture_plan_mix_byte(hash, value ? 1u : 0u);
}

static uint64_t
capture_plan_mix_text(uint64_t hash, const char *text)
{
    if (text == NULL)
        return capture_plan_mix_byte(hash, 0);
    hash = capture_plan_mix_byte(hash, 1);
    while (*text != '\0')
        hash = capture_plan_mix_byte(hash, (uint8_t)*text++);
    return capture_plan_mix_byte(hash, 0);
}

static PgyProjectionDisposition
capture_plan_disposition(MIRParallelCaptureDispositionKind kind)
{
    switch (kind) {
    case MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY:
        return PGY_PROJECTION_MATERIALIZE;
    case MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT:
    case MIR_PARALLEL_CAPTURE_JOIN_READONLY:
        return PGY_PROJECTION_RETAIN;
    }
    return PGY_PROJECTION_REJECT;
}

static bool
capture_plan_kind_valid(MIRParallelCaptureDispositionKind kind)
{
    return kind == MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY
        || kind == MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT
        || kind == MIR_PARALLEL_CAPTURE_JOIN_READONLY;
}

uint64_t
pgy_verified_parallel_capture_plan_digest(
    const PgyVerifiedParallelCapturePlan *plan)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    if (plan == NULL)
        return 0;
    hash = capture_plan_mix_u32(hash, plan->revision);
    hash = capture_plan_mix_u64(hash, plan->air_certificate_fingerprint);
    hash = capture_plan_mix_u64(hash, (uint64_t)plan->row_count);
    for (size_t i = 0; i < plan->row_count; i++) {
        const PgyVerifiedParallelCaptureRow *row = &plan->rows[i];
        hash = capture_plan_mix_u32(hash, row->source_stable_id);
        hash = capture_plan_mix_text(hash, row->name);
        hash = capture_plan_mix_u32(hash, (uint32_t)row->kind);
        hash = capture_plan_mix_u64(hash, (uint64_t)row->writer_task);
        hash = capture_plan_mix_u32(hash, (uint32_t)row->disposition);
    }
    return capture_plan_mix_bool(hash, plan->verified);
}

bool
pgy_verified_parallel_capture_plan_identity_ready(
    const PgyVerifiedParallelCapturePlan *plan)
{
    return plan != NULL
        && plan->verified
        && plan->revision == PGY_VERIFIED_PARALLEL_CAPTURE_PLAN_REVISION
        && plan->air_certificate_fingerprint != 0
        && plan->digest != 0
        && plan->digest == pgy_verified_parallel_capture_plan_digest(plan);
}

static bool
capture_plan_air_boundary_present(const PgyAirVerification *air,
                                  uint32_t source_stable_id)
{
    if (air == NULL || source_stable_id == 0)
        return false;
    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        const AIRBoundaryNode *boundary = air_boundary_node_at(air, i);
        if (boundary == NULL || boundary->kind != AIR_BOUNDARY_PARALLEL
            || boundary->ast == NULL)
            continue;
        if (ast_node_stable_id(boundary->ast) == source_stable_id)
            return true;
    }
    return false;
}

bool
pgy_verified_parallel_capture_plan_from_air(
    const PgyAirVerification *air,
    const MIRProgram *mir,
    PgyVerifiedParallelCapturePlan *plan_out,
    const char **error_out)
{
    const char *certificate_error = NULL;
    char *mir_error = NULL;
    size_t row_count = 0;

    if (error_out != NULL)
        *error_out = NULL;
    if (plan_out == NULL) {
        if (error_out != NULL)
            *error_out = "parallel capture plan: missing output plan";
        return false;
    }
    memset(plan_out, 0, sizeof(*plan_out));
    if (!pgy_air_evidence_certificate_ready(air, &certificate_error)) {
        if (error_out != NULL)
            *error_out = certificate_error != NULL
                ? certificate_error
                : "parallel capture plan: AIR certificate is missing";
        return false;
    }
    if (mir == NULL) {
        if (error_out != NULL)
            *error_out = "parallel capture plan: missing MIR program";
        return false;
    }
    if (!mir_validate_parallel_capture_facts(mir, &mir_error)) {
        free(mir_error);
        if (error_out != NULL)
            *error_out =
                "parallel capture plan: MIR capture facts are invalid";
        return false;
    }

    /* Every MIR capture boundary must have certified AIR evidence.  AIR may
     * contain a parallel boundary with no captures; that is a valid empty
     * carrier and must not invent a semantic row. */
    for (size_t i = 0; i < mir_parallel_capture_boundary_count(mir); i++) {
        const MIRParallelCaptureBoundaryFact *boundary =
            mir_parallel_capture_boundary_at(mir, i);
        if (boundary == NULL
            || !capture_plan_air_boundary_present(
                   air, boundary->source_stable_id)) {
            if (error_out != NULL)
                *error_out =
                    "parallel capture plan: MIR boundary lacks AIR evidence";
            return false;
        }
        if (boundary->row_count > SIZE_MAX - row_count) {
            if (error_out != NULL)
                *error_out = "parallel capture plan: row inventory overflow";
            return false;
        }
        row_count += boundary->row_count;
    }
    if (row_count > 0) {
        if (row_count > SIZE_MAX / sizeof(*plan_out->rows)) {
            if (error_out != NULL)
                *error_out = "parallel capture plan: row allocation overflow";
            return false;
        }
        plan_out->rows = calloc(row_count, sizeof(*plan_out->rows));
        if (plan_out->rows == NULL) {
            if (error_out != NULL)
                *error_out = "parallel capture plan: out of memory";
            return false;
        }
    }
    plan_out->revision = PGY_VERIFIED_PARALLEL_CAPTURE_PLAN_REVISION;
    plan_out->air_certificate_fingerprint =
        air->verification_certificate_fingerprint;
    plan_out->row_count = row_count;
    plan_out->verified = true;

    size_t out_index = 0;
    for (size_t i = 0; i < mir_parallel_capture_boundary_count(mir); i++) {
        const MIRParallelCaptureBoundaryFact *boundary =
            mir_parallel_capture_boundary_at(mir, i);
        for (size_t j = 0; j < boundary->row_count; j++) {
            const MIRParallelCaptureDispositionRow *source =
                &boundary->rows[j];
            PgyVerifiedParallelCaptureRow *row =
                &plan_out->rows[out_index++];
            if (!capture_plan_kind_valid(source->kind)) {
                pgy_verified_parallel_capture_plan_dispose(plan_out);
                if (error_out != NULL)
                    *error_out =
                        "parallel capture plan: unknown MIR disposition kind";
                return false;
            }
            row->source_stable_id = boundary->source_stable_id;
            row->name = source->name;
            row->kind = source->kind;
            row->writer_task = source->writer_task;
            row->disposition = capture_plan_disposition(source->kind);
        }
    }
    plan_out->digest = pgy_verified_parallel_capture_plan_digest(plan_out);
    return true;
}

void
pgy_verified_parallel_capture_plan_dispose(
    PgyVerifiedParallelCapturePlan *plan)
{
    if (plan == NULL)
        return;
    free(plan->rows);
    memset(plan, 0, sizeof(*plan));
}

const PgyVerifiedParallelCaptureRow *
pgy_verified_parallel_capture_disposition_find(
    const PgyVerifiedParallelCapturePlan *plan,
    const MIRParallelCaptureBoundaryFact *boundary,
    const char *name,
    MIRParallelCaptureDispositionKind kind)
{
    if (!pgy_verified_parallel_capture_plan_identity_ready(plan)
        || boundary == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < plan->row_count; i++) {
        const PgyVerifiedParallelCaptureRow *row = &plan->rows[i];
        if (row->source_stable_id == boundary->source_stable_id
            && row->kind == kind && row->name != NULL
            && strcmp(row->name, name) == 0)
            return row;
    }
    return NULL;
}
