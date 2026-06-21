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
air_compression_budget_name(AIRCompressionBudget budget)
{
    switch (budget) {
    case AIR_COMPRESSION_UNKNOWN: return "unknown";
    case AIR_COMPRESSION_RETAIN: return "retain";
    case AIR_COMPRESSION_SUMMARIZE: return "summarize";
    case AIR_COMPRESSION_ERASE: return "erase";
    case AIR_COMPRESSION_FORBID: return "forbid";
    }
    return "invalid";
}

const char *
air_retain_cause_name(AIRRetainCause cause)
{
    switch (cause) {
    case AIR_RETAIN_CAUSE_NONE: return "none";
    case AIR_RETAIN_CAUSE_INHERENT: return "inherent";
    case AIR_RETAIN_CAUSE_POLICY: return "policy";
    case AIR_RETAIN_CAUSE_UNPROVEN: return "unproven";
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
    case AIR_DRIFT_COMPRESSION_RESIDUE_MISMATCH:
        return "compression_residue_mismatch";
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

const char *
air_evidence_provider_kind_name(AIREvidenceProviderKind kind)
{
    switch (kind) {
    case AIR_EVIDENCE_PROVIDER_UNKNOWN: return "unknown";
    case AIR_EVIDENCE_PROVIDER_HIR: return "hir";
    case AIR_EVIDENCE_PROVIDER_RIR: return "rir";
    case AIR_EVIDENCE_PROVIDER_MIR: return "mir";
    case AIR_EVIDENCE_PROVIDER_DAG: return "dag";
    case AIR_EVIDENCE_PROVIDER_RUNTIME: return "runtime";
    case AIR_EVIDENCE_PROVIDER_COUNT: break;
    }
    return "invalid";
}

const char *
air_evidence_subject_kind_name(AIREvidenceSubjectKind kind)
{
    switch (kind) {
    case AIR_EVIDENCE_SUBJECT_UNKNOWN: return "unknown";
    case AIR_EVIDENCE_SUBJECT_ROUTINE: return "routine";
    case AIR_EVIDENCE_SUBJECT_CFG: return "cfg";
    case AIR_EVIDENCE_SUBJECT_BOUNDARY: return "boundary";
    case AIR_EVIDENCE_SUBJECT_AUTHORITY: return "authority";
    case AIR_EVIDENCE_SUBJECT_CLEANUP: return "cleanup";
    case AIR_EVIDENCE_SUBJECT_PIN_CLEANUP: return "pin_cleanup";
    case AIR_EVIDENCE_SUBJECT_TERMINATOR: return "terminator";
    case AIR_EVIDENCE_SUBJECT_SELECT_RECEIVE: return "select_receive";
    case AIR_EVIDENCE_SUBJECT_METADATA: return "metadata";
    case AIR_EVIDENCE_SUBJECT_GENERIC: return "generic";
    case AIR_EVIDENCE_SUBJECT_ABILITY: return "ability";
    case AIR_EVIDENCE_SUBJECT_EFFECT_PROPAGATION: return "effect_propagation";
    case AIR_EVIDENCE_SUBJECT_RELATION_PROPAGATION: return "relation_propagation";
    case AIR_EVIDENCE_SUBJECT_OBSERVABILITY_SCHEMA: return "observability_schema";
    case AIR_EVIDENCE_SUBJECT_FRONTIER_POLICY: return "frontier_policy";
    case AIR_EVIDENCE_SUBJECT_COUNT: break;
    }
    return "invalid";
}
