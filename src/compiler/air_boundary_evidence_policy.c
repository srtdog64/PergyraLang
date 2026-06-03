/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR boundary evidence policy owner. Validation and drift passes consume this
 * table instead of re-stating boundary/evidence requirements locally.
 */

#include "air_internal.h"

typedef struct
{
    bool requires_hir_routine;
    bool requires_hir_cfg;
    bool requires_rir_boundary;
} AIRBoundaryEvidencePolicy;

static const AIRBoundaryEvidencePolicy kBoundaryEvidencePolicies[] = {
    [AIR_BOUNDARY_UNKNOWN] = { false, false, false },
    [AIR_BOUNDARY_ZONE] = { true, false, true },
    [AIR_BOUNDARY_WORLD] = { true, false, true },
    [AIR_BOUNDARY_PARALLEL] = { true, true, true },
    [AIR_BOUNDARY_IO] = { true, true, true },
    [AIR_BOUNDARY_CHANNEL] = { true, true, true },
    [AIR_BOUNDARY_EXECUTION] = { true, true, false },
};

static const AIRBoundaryEvidencePolicy *
air_boundary_evidence_policy(AIRBoundaryKind kind)
{
    if ((int)kind < 0
        || (size_t)kind >= sizeof(kBoundaryEvidencePolicies)
            / sizeof(kBoundaryEvidencePolicies[0])) {
        return NULL;
    }
    return &kBoundaryEvidencePolicies[kind];
}

bool
air_boundary_requires_hir_routine_evidence(const AIRBoundaryNode *boundary)
{
    const AIRBoundaryEvidencePolicy *policy;

    if (boundary == NULL)
        return false;
    policy = air_boundary_evidence_policy(boundary->kind);
    return policy != NULL && policy->requires_hir_routine;
}

bool
air_boundary_requires_hir_evidence(const AIRBoundaryNode *boundary)
{
    const AIRBoundaryEvidencePolicy *policy;

    if (boundary == NULL)
        return false;
    policy = air_boundary_evidence_policy(boundary->kind);
    return policy != NULL && policy->requires_hir_cfg;
}

bool
air_boundary_requires_hir_cfg_for_program(const AIRProgram *air,
                                          const AIRBoundaryNode *boundary)
{
    if (boundary == NULL)
        return false;
    if (air_boundary_requires_hir_evidence(boundary))
        return true;
    return air_has_hir_input(air)
        && (boundary->kind == AIR_BOUNDARY_ZONE
            || boundary->kind == AIR_BOUNDARY_WORLD);
}

bool
air_boundary_requires_rir_evidence(const AIRBoundaryNode *boundary)
{
    const AIRBoundaryEvidencePolicy *policy;

    if (boundary == NULL)
        return false;
    policy = air_boundary_evidence_policy(boundary->kind);
    return policy != NULL && policy->requires_rir_boundary;
}

bool
air_boundary_requires_mir_pin_cleanup_evidence(const AIRBoundaryNode *boundary)
{
    return boundary != NULL
        && boundary->kind == AIR_BOUNDARY_EXECUTION
        && air_name_matches(boundary->source_name, "pin");
}
