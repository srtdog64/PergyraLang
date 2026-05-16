/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR boundary summary-flag validation owner.
 */

#include "air_internal.h"

bool
air_validate_boundary_summary_shape(const AIRProgram *air,
                                    size_t boundary_index,
                                    char **error_message)
{
    const AIRBoundaryNode *boundary;

    if (air == NULL || boundary_index >= air->boundary_count)
        return false;
    boundary = &air->boundaries[boundary_index];
    if (boundary->has_hir_routine_evidence
        && air_name_is_empty(boundary->hir_routine_evidence_name)) {
        air_set_invariant_error(error_message,
                                "AIR boundary node %zu has HIR evidence without provenance",
                                boundary_index);
        return false;
    }
    if (boundary->has_hir_cfg_evidence
        && !boundary->has_hir_routine_evidence) {
        air_set_invariant_error(error_message,
                                "AIR boundary node %zu has HIR CFG evidence without routine evidence",
                                boundary_index);
        return false;
    }
    if (boundary->has_rir_boundary_evidence
        && air_name_is_empty(boundary->rir_boundary_evidence_scope)) {
        air_set_invariant_error(error_message,
                                "AIR boundary node %zu has RIR boundary evidence without provenance",
                                boundary_index);
        return false;
    }
    if (boundary->has_rir_authority_evidence
        && !boundary->has_rir_boundary_evidence) {
        air_set_invariant_error(error_message,
                                "AIR boundary node %zu has RIR authority evidence without boundary evidence",
                                boundary_index);
        return false;
    }
    if (boundary->has_rir_authority_evidence
        && !boundary->authority_required) {
        air_set_invariant_error(error_message,
                                "AIR boundary node %zu has RIR authority evidence on non-authority boundary",
                                boundary_index);
        return false;
    }
    if (boundary->has_rir_authority_evidence
        && air_name_is_empty(boundary->rir_authority_evidence_name)) {
        air_set_invariant_error(error_message,
                                "AIR boundary node %zu has RIR authority evidence without provenance",
                                boundary_index);
        return false;
    }
    if (boundary->has_rir_authority_evidence
        && !air_boundary_declares_authority_name(
            boundary,
            boundary->rir_authority_evidence_name)) {
        air_set_invariant_error(error_message,
                                "AIR boundary node %zu has RIR authority evidence for undeclared participant",
                                boundary_index);
        return false;
    }
    return true;
}
