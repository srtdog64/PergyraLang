/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR boundary evidence shape validation owner.
 */

#include "air_internal.h"

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
    AIREvidenceKind kind;
    size_t boundary_index;
    size_t fact_count;
    size_t fallback_count;

    if (air == NULL || evidence_index >= air_evidence_node_count(air))
        return false;

    evidence = air_evidence_node_at(air, evidence_index);
    if (evidence == NULL)
        return false;
    kind = air_evidence_node_kind(evidence);
    boundary_index = air_evidence_node_boundary_index_or(evidence, SIZE_MAX);
    fact_count = air_evidence_node_fact_count(evidence);
    fallback_count = air_evidence_node_fallback_count(evidence);
    if (boundary_index >= air_boundary_node_count(air)) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu references missing boundary node %zu",
                                evidence_index,
                                boundary_index);
        return false;
    }

    *evidence_out = evidence;
    *boundary_out = air_boundary_node_at(air, boundary_index);
    if (*boundary_out == NULL)
        return false;
    if (air_evidence_node_has_boundary_shape(evidence)) {
        if (air_evidence_node_boundary_kind_or(evidence,
                                               AIR_BOUNDARY_UNKNOWN)
            != (*boundary_out)->kind) {
            air_set_invariant_error(error_message,
                                    "AIR boundary evidence node %zu has boundary kind drift",
                                    evidence_index);
            return false;
        }
        if (!air_name_matches(
                air_evidence_node_boundary_owner_name_or(evidence, NULL),
                (*boundary_out)->owner_name)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary evidence node %zu has boundary owner drift",
                                    evidence_index);
            return false;
        }
        if (!air_name_matches(
                air_evidence_node_boundary_source_name_or(evidence, NULL),
                (*boundary_out)->source_name)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary evidence node %zu has boundary source drift",
                                    evidence_index);
            return false;
        }
    }
    if (air_program_requires_summary_flag_for_evidence(air, kind)
        && !air_boundary_has_summary_flag(*boundary_out, kind)) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu has no matching boundary summary flag",
                                evidence_index);
        return false;
    }
    if (fact_count == 0) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu has no evidence facts",
                                evidence_index);
        return false;
    }
    if (fact_count != 1) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu must carry exactly one boundary fact",
                                evidence_index);
        return false;
    }
    if (fallback_count != 0) {
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
    size_t boundary_index = air_evidence_node_boundary_index_or(evidence,
                                                                SIZE_MAX);
    if (boundary_index != SIZE_MAX) {
        air_set_invariant_error(error_message,
                                "AIR global evidence node %zu is attached to boundary %zu",
                                evidence_index,
                                boundary_index);
        return false;
    }
    if (air_evidence_node_has_boundary_shape(evidence)) {
        air_set_invariant_error(error_message,
                                "AIR global evidence node %zu carries boundary shape",
                                evidence_index);
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
    size_t boundary_index = air_evidence_node_boundary_index_or(evidence,
                                                                SIZE_MAX);
    const char *provider_name =
        air_evidence_node_provider_name_or(evidence, NULL);
    const char *subject_name =
        air_evidence_node_subject_name_or(evidence, NULL);

    if (air_boundary_requires_hir_evidence(boundary) && boundary->ast == NULL) {
        air_set_invariant_error(error_message,
                                "AIR HIR CFG evidence node %zu has no source AST provenance for boundary %zu",
                                evidence_index,
                                boundary_index);
        return false;
    }
    if (!air_name_matches(subject_name, boundary->source_name)) {
        air_set_invariant_error(error_message,
                                "AIR HIR CFG evidence node %zu has subject/source mismatch for boundary %zu",
                                evidence_index,
                                boundary_index);
        return false;
    }
    if (!air_boundary_has_evidence_kind_provider(air,
                                                 boundary_index,
                                                 AIR_EVIDENCE_HIR_ROUTINE,
                                                 provider_name)) {
        air_set_invariant_error(error_message,
                                "AIR HIR CFG evidence node %zu has no matching HIR routine evidence for boundary %zu",
                                evidence_index,
                                boundary_index);
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
    size_t boundary_index = air_evidence_node_boundary_index_or(evidence,
                                                                SIZE_MAX);
    const char *provider_name =
        air_evidence_node_provider_name_or(evidence, NULL);
    const char *subject_name =
        air_evidence_node_subject_name_or(evidence, NULL);

    if (!boundary->authority_required) {
        air_set_invariant_error(error_message,
                                "AIR RIR authority evidence node %zu is attached to non-authority boundary %zu",
                                evidence_index,
                                boundary_index);
        return false;
    }
    if (!air_boundary_has_evidence_kind_provider(air,
                                                 boundary_index,
                                                 AIR_EVIDENCE_RIR_BOUNDARY,
                                                 provider_name)) {
        air_set_invariant_error(error_message,
                                "AIR RIR authority evidence node %zu has no matching RIR boundary evidence for boundary %zu",
                                evidence_index,
                                boundary_index);
        return false;
    }
    if (!air_boundary_declares_authority_name(boundary, subject_name)) {
        air_set_invariant_error(error_message,
                                "AIR RIR authority evidence node %zu has undeclared authority subject for boundary %zu",
                                evidence_index,
                                boundary_index);
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
    size_t boundary_index = air_evidence_node_boundary_index_or(evidence,
                                                                SIZE_MAX);
    const char *provider_name =
        air_evidence_node_provider_name_or(evidence, NULL);
    const char *subject_name =
        air_evidence_node_subject_name_or(evidence, NULL);

    if (!air_boundary_requires_mir_pin_cleanup_evidence(boundary)) {
        air_set_invariant_error(error_message,
                                "AIR MIR pin cleanup evidence node %zu is attached to non-pin boundary %zu",
                                evidence_index,
                                boundary_index);
        return false;
    }
    if (boundary->ast == NULL) {
        air_set_invariant_error(error_message,
                                "AIR MIR pin cleanup evidence node %zu has no source AST provenance for boundary %zu",
                                evidence_index,
                                boundary_index);
        return false;
    }
    if (air_name_matches(subject_name, boundary->source_name)) {
        air_set_invariant_error(error_message,
                                "AIR MIR pin cleanup evidence node %zu has no slot anchor subject for boundary %zu",
                                evidence_index,
                                boundary_index);
        return false;
    }
    if (!air_has_global_evidence_provider(air,
                                          AIR_EVIDENCE_MIR_CLEANUP,
                                          provider_name)) {
        air_set_invariant_error(error_message,
                                "AIR MIR pin cleanup evidence node %zu has no matching MIR cleanup evidence for provider '%s'",
                                evidence_index,
                                provider_name != NULL ? provider_name : "<null>");
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
    AIREvidenceKind kind;
    size_t boundary_index;
    const char *subject_name;

    if (air == NULL || evidence_index >= air_evidence_node_count(air))
        return false;

    evidence = air_evidence_node_at(air, evidence_index);
    if (evidence == NULL)
        return false;
    kind = air_evidence_node_kind(evidence);
    if (air_evidence_kind_is_global(kind))
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

    boundary_index = air_evidence_node_boundary_index_or(evidence, SIZE_MAX);
    subject_name = air_evidence_node_subject_name_or(evidence, NULL);
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
        if (!air_name_matches(subject_name, boundary->source_name)) {
            air_set_invariant_error(error_message,
                                    "AIR HIR routine evidence node %zu has subject/source mismatch for boundary %zu",
                                    evidence_index,
                                    boundary_index);
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
                                    boundary_index);
            return false;
        }
        if (!air_name_matches(subject_name, boundary->source_name)) {
            air_set_invariant_error(error_message,
                                    "AIR RIR boundary evidence node %zu has subject/source mismatch for boundary %zu",
                                    evidence_index,
                                    boundary_index);
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
