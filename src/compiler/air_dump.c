/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR dump and vocabulary owner.
 */

#include "air.h"

#include <stdio.h>

void
air_dump(const AIRProgram *air, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (air == NULL) {
        fprintf(out, "AIRProgram(null)\n");
        return;
    }
    fprintf(out, "AIRProgram intents=%zu boundaries=%zu drifts=%zu strict_evidence=%s\n",
            air->intent_count,
            air->boundary_count,
            air->drift_count,
            air->strict_evidence ? "yes" : "no");
    fprintf(out, "  evidence hir_routines=%zu hir_cfg=%zu rir_boundaries=%zu rir_authority=%zu\n",
            air->hir_routine_evidence_count,
            air->hir_cfg_evidence_count,
            air->rir_boundary_evidence_count,
            air->rir_authority_evidence_count);
    for (size_t i = 0; i < air->intent_count; i++) {
        const AIRIntentNode *intent = &air->intents[i];
        fprintf(out,
                "  intent[%zu] owner=%s step=%s index=%zu sync=%s failure=%s\n",
                i,
                intent->intent_owner != NULL ? intent->intent_owner : "<anonymous>",
                intent->step_name != NULL ? intent->step_name : "<unnamed>",
                intent->step_index,
                air_sync_class_name(intent->sync_class),
                air_failure_class_name(intent->failure_class));
    }
    for (size_t i = 0; i < air->boundary_count; i++) {
        const AIRBoundaryNode *boundary = &air->boundaries[i];
        fprintf(out,
                "  boundary[%zu] kind=%s owner=%s source=%s intent=%zu step=%zu sync=%s authority=%s\n",
                i,
                air_boundary_kind_name(boundary->kind),
                boundary->owner_name != NULL ? boundary->owner_name : "<anonymous>",
                boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                boundary->intent_index,
                boundary->step_index,
                air_sync_class_name(boundary->sync_class),
                boundary->authority_required ? "yes" : "no");
        fprintf(out,
                "    evidence hir=%s(%s) hir_cfg=%s rir_boundary=%s(%s) rir_authority=%s(%s)\n",
                boundary->has_hir_routine_evidence ? "yes" : "no",
                boundary->hir_routine_evidence_name != NULL
                    ? boundary->hir_routine_evidence_name
                    : "<none>",
                boundary->has_hir_cfg_evidence ? "yes" : "no",
                boundary->has_rir_boundary_evidence ? "yes" : "no",
                boundary->rir_boundary_evidence_scope != NULL
                    ? boundary->rir_boundary_evidence_scope
                    : "<none>",
                boundary->has_rir_authority_evidence ? "yes" : "no",
                boundary->rir_authority_evidence_name != NULL
                    ? boundary->rir_authority_evidence_name
                    : "<none>");
    }
}

const char *
air_sync_class_name(AIRSyncClass sync_class)
{
    switch (sync_class) {
    case AIR_SYNC_UNKNOWN: return "unknown";
    case AIR_SYNC_SYNC: return "sync";
    case AIR_SYNC_ASYNC: return "async";
    case AIR_SYNC_EITHER: return "either";
    }
    return "invalid";
}

const char *
air_failure_class_name(AIRFailureClass failure_class)
{
    switch (failure_class) {
    case AIR_FAILURE_UNKNOWN: return "unknown";
    case AIR_FAILURE_RECOVERABLE: return "recoverable";
    case AIR_FAILURE_FATAL: return "fatal";
    case AIR_FAILURE_COMPENSABLE: return "compensable";
    }
    return "invalid";
}

const char *
air_boundary_kind_name(AIRBoundaryKind kind)
{
    switch (kind) {
    case AIR_BOUNDARY_UNKNOWN: return "unknown";
    case AIR_BOUNDARY_ZONE: return "zone";
    case AIR_BOUNDARY_WORLD: return "world";
    case AIR_BOUNDARY_PARALLEL: return "parallel";
    case AIR_BOUNDARY_IO: return "io";
    case AIR_BOUNDARY_CHANNEL: return "channel";
    case AIR_BOUNDARY_EXECUTION: return "execution";
    }
    return "invalid";
}

const char *
air_drift_kind_name(AIRDriftKind kind)
{
    switch (kind) {
    case AIR_DRIFT_NONE: return "none";
    case AIR_DRIFT_SYNC_ASYNC_CONFLICT: return "sync_async_conflict";
    case AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING: return "boundary_evidence_missing";
    }
    return "invalid";
}
