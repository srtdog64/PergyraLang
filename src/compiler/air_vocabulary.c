/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR stable vocabulary owner.
 */

#include "air.h"

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
    case AIR_DRIFT_EFFECT_PROPAGATION_MISSING: return "effect_propagation_missing";
    case AIR_DRIFT_RELATION_PROPAGATION_MISSING: return "relation_propagation_missing";
    case AIR_DRIFT_DAG_DEAD_END_PRESENT: return "dag_dead_end_present";
    }
    return "invalid";
}

const char *
air_evidence_kind_name(AIREvidenceKind kind)
{
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE: return "hir_routine";
    case AIR_EVIDENCE_HIR_CFG: return "hir_cfg";
    case AIR_EVIDENCE_RIR_BOUNDARY: return "rir_boundary";
    case AIR_EVIDENCE_RIR_AUTHORITY: return "rir_authority";
    case AIR_EVIDENCE_MIR_CLEANUP: return "mir_cleanup";
    case AIR_EVIDENCE_MIR_PIN_CLEANUP: return "mir_pin_cleanup";
    case AIR_EVIDENCE_MIR_TERMINATOR: return "mir_terminator";
    case AIR_EVIDENCE_MIR_SELECT_RECEIVE: return "mir_select_receive";
    case AIR_EVIDENCE_DAG_METADATA: return "dag_metadata";
    case AIR_EVIDENCE_DAG_GENERIC: return "dag_generic";
    case AIR_EVIDENCE_DAG_ABILITY: return "dag_ability";
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION: return "rir_effect_propagation";
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION: return "rir_relation_propagation";
    case AIR_EVIDENCE_OBSERVABILITY_SCHEMA: return "observability_schema";
    case AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY: return "runtime_frontier_policy";
    case AIR_EVIDENCE_KIND_COUNT: break;
    }
    return "invalid";
}
