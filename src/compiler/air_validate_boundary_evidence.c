/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR boundary evidence shape validation owner.
 */

#include "air_internal.h"

static bool
air_boundary_has_evidence_kind_provider(const AIRProgram *air,
                                        size_t boundary_index,
                                        AIREvidenceKind kind,
                                        const char *provider_name)
{
    if (air == NULL || boundary_index >= air_boundary_node_count(air))
        return false;
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (evidence == NULL)
            continue;
        if (evidence->kind == kind
            && evidence->boundary_index == boundary_index
            && air_name_matches(evidence->provider_name, provider_name)) {
            return true;
        }
    }
    return false;
}

static bool
air_global_evidence_has_provider(const AIRProgram *air,
                                 AIREvidenceKind kind,
                                 const char *provider_name)
{
    if (air == NULL || air_name_is_empty(provider_name))
        return false;
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (evidence == NULL)
            continue;
        if (evidence->kind == kind
            && evidence->boundary_index == SIZE_MAX
            && air_name_matches(evidence->provider_name, provider_name)) {
            return true;
        }
    }
    return false;
}

static bool
air_program_requires_summary_flag_for_evidence(const AIRProgram *air,
                                               AIREvidenceKind kind)
{
    if (air == NULL)
        return false;
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
    case AIR_EVIDENCE_HIR_CFG:
        return air_has_hir_input(air);
    case AIR_EVIDENCE_RIR_BOUNDARY:
    case AIR_EVIDENCE_RIR_AUTHORITY:
        return air_has_rir_input(air);
    default:
        return false;
    }
}

static bool
air_validate_boundary_evidence_base(const AIRProgram *air,
                                    size_t evidence_index,
                                    const AIREvidenceNode **evidence_out,
                                    const AIRBoundaryNode **boundary_out,
                                    char **error_message)
{
    const AIREvidenceNode *evidence;

    if (air == NULL || evidence_index >= air_evidence_node_count(air))
        return false;

    evidence = air_evidence_node_at(air, evidence_index);
    if (evidence == NULL)
        return false;
    if (evidence->boundary_index >= air_boundary_node_count(air)) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu references missing boundary node %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }

    *evidence_out = evidence;
    *boundary_out = air_boundary_node_at(air, evidence->boundary_index);
    if (*boundary_out == NULL)
        return false;
    if (air_program_requires_summary_flag_for_evidence(air, evidence->kind)
        && !air_boundary_has_summary_flag(*boundary_out, evidence->kind)) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu has no matching boundary summary flag",
                                evidence_index);
        return false;
    }
    if (evidence->fact_count == 0) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu has no evidence facts",
                                evidence_index);
        return false;
    }
    if (evidence->fact_count != 1) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu must carry exactly one boundary fact",
                                evidence_index);
        return false;
    }
    if (evidence->fallback_count != 0) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu has fallback evidence facts",
                                evidence_index);
        return false;
    }
    return true;
}

static bool
air_validate_global_evidence_shape(const AIREvidenceNode *evidence,
                                   size_t evidence_index,
                                   char **error_message)
{
    if (evidence->boundary_index != SIZE_MAX) {
        air_set_invariant_error(error_message,
                                "AIR global evidence node %zu is attached to boundary %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }
    return air_validate_global_evidence_node(evidence,
                                             evidence_index,
                                             error_message);
}

static bool
air_validate_boundary_hir_cfg_evidence(const AIRProgram *air,
                                       const AIREvidenceNode *evidence,
                                       const AIRBoundaryNode *boundary,
                                       size_t evidence_index,
                                       char **error_message)
{
    if (air_boundary_requires_hir_evidence(boundary) && boundary->ast == NULL) {
        air_set_invariant_error(error_message,
                                "AIR HIR CFG evidence node %zu has no source AST provenance for boundary %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }
    if (!air_name_matches(evidence->subject_name, boundary->source_name)) {
        air_set_invariant_error(error_message,
                                "AIR HIR CFG evidence node %zu has subject/source mismatch for boundary %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }
    if (!air_boundary_has_evidence_kind_provider(air,
                                                 evidence->boundary_index,
                                                 AIR_EVIDENCE_HIR_ROUTINE,
                                                 evidence->provider_name)) {
        air_set_invariant_error(error_message,
                                "AIR HIR CFG evidence node %zu has no matching HIR routine evidence for boundary %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }
    return true;
}

static bool
air_validate_boundary_rir_authority_evidence(const AIRProgram *air,
                                             const AIREvidenceNode *evidence,
                                             const AIRBoundaryNode *boundary,
                                             size_t evidence_index,
                                             char **error_message)
{
    if (!boundary->authority_required) {
        air_set_invariant_error(error_message,
                                "AIR RIR authority evidence node %zu is attached to non-authority boundary %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }
    if (!air_boundary_has_evidence_kind_provider(air,
                                                 evidence->boundary_index,
                                                 AIR_EVIDENCE_RIR_BOUNDARY,
                                                 evidence->provider_name)) {
        air_set_invariant_error(error_message,
                                "AIR RIR authority evidence node %zu has no matching RIR boundary evidence for boundary %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }
    if (!air_boundary_declares_authority_name(boundary, evidence->subject_name)) {
        air_set_invariant_error(error_message,
                                "AIR RIR authority evidence node %zu has undeclared authority subject for boundary %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }
    return true;
}

static bool
air_validate_boundary_mir_pin_cleanup_evidence(const AIRProgram *air,
                                               const AIREvidenceNode *evidence,
                                               const AIRBoundaryNode *boundary,
                                               size_t evidence_index,
                                               char **error_message)
{
    if (!air_boundary_requires_mir_pin_cleanup_evidence(boundary)) {
        air_set_invariant_error(error_message,
                                "AIR MIR pin cleanup evidence node %zu is attached to non-pin boundary %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }
    if (boundary->ast == NULL) {
        air_set_invariant_error(error_message,
                                "AIR MIR pin cleanup evidence node %zu has no source AST provenance for boundary %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }
    if (air_name_matches(evidence->subject_name, boundary->source_name)) {
        air_set_invariant_error(error_message,
                                "AIR MIR pin cleanup evidence node %zu has no slot anchor subject for boundary %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }
    if (!air_global_evidence_has_provider(air,
                                          AIR_EVIDENCE_MIR_CLEANUP,
                                          evidence->provider_name)) {
        air_set_invariant_error(error_message,
                                "AIR MIR pin cleanup evidence node %zu has no matching MIR cleanup evidence for provider '%s'",
                                evidence_index,
                                evidence->provider_name != NULL
                                    ? evidence->provider_name
                                    : "<null>");
        return false;
    }
    return true;
}

bool
air_evidence_node_matches_boundary_shape(const AIRProgram *air,
                                         size_t evidence_index,
                                         char **error_message)
{
    const AIREvidenceNode *evidence;
    const AIRBoundaryNode *boundary;

    if (air == NULL || evidence_index >= air_evidence_node_count(air))
        return false;

    evidence = air_evidence_node_at(air, evidence_index);
    if (evidence == NULL)
        return false;
    if (air_evidence_kind_is_global(evidence->kind))
        return air_validate_global_evidence_shape(evidence,
                                                  evidence_index,
                                                  error_message);

    if (!air_validate_boundary_evidence_base(air,
                                             evidence_index,
                                             &evidence,
                                             &boundary,
                                             error_message)) {
        return false;
    }

    switch (evidence->kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
        if (!air_name_matches(evidence->subject_name, boundary->source_name)) {
            air_set_invariant_error(error_message,
                                    "AIR HIR routine evidence node %zu has subject/source mismatch for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_HIR_CFG:
        return air_validate_boundary_hir_cfg_evidence(air,
                                                      evidence,
                                                      boundary,
                                                      evidence_index,
                                                      error_message);
    case AIR_EVIDENCE_RIR_BOUNDARY:
        if (!air_boundary_requires_rir_evidence(boundary)) {
            air_set_invariant_error(error_message,
                                    "AIR RIR boundary evidence node %zu is attached to non-RIR boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        if (!air_name_matches(evidence->subject_name, boundary->source_name)) {
            air_set_invariant_error(error_message,
                                    "AIR RIR boundary evidence node %zu has subject/source mismatch for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_RIR_AUTHORITY:
        return air_validate_boundary_rir_authority_evidence(air,
                                                            evidence,
                                                            boundary,
                                                            evidence_index,
                                                            error_message);
    case AIR_EVIDENCE_MIR_PIN_CLEANUP:
        return air_validate_boundary_mir_pin_cleanup_evidence(air,
                                                              evidence,
                                                              boundary,
                                                              evidence_index,
                                                              error_message);
    case AIR_EVIDENCE_DAG_GENERIC:
    case AIR_EVIDENCE_DAG_METADATA:
    case AIR_EVIDENCE_DAG_ABILITY:
    case AIR_EVIDENCE_MIR_CLEANUP:
    case AIR_EVIDENCE_MIR_TERMINATOR:
    case AIR_EVIDENCE_MIR_SELECT_RECEIVE:
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION:
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION:
    case AIR_EVIDENCE_OBSERVABILITY_SCHEMA:
    case AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY:
        return true;
    case AIR_EVIDENCE_KIND_COUNT:
        break;
    }

    return false;
}
