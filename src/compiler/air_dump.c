/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR human-readable dump owner.
 */

#include "air_internal.h"

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
    fprintf(out, "AIRProgram intents=%zu boundaries=%zu evidence_nodes=%zu drifts=%zu strict_evidence=%s hir_input=%s rir_input=%s mir_input=%s\n",
            air_intent_node_count(air),
            air_boundary_node_count(air),
            air_evidence_node_count(air),
            air_drift_count(air),
            air_requires_strict_evidence(air) ? "yes" : "no",
            air_has_hir_input(air) ? "yes" : "no",
            air_has_rir_input(air) ? "yes" : "no",
            air_has_mir_input(air) ? "yes" : "no");
    fprintf(out, "  evidence hir_routines=%zu hir_cfg=%zu rir_boundaries=%zu rir_authority=%zu mir_cleanup=%zu mir_pin_cleanup=%zu mir_terminator=%zu mir_select_receive=%zu dag_metadata=%zu dag_generic=%zu dag_ability=%zu rir_effect=%zu/%zu rir_relation=%zu/%zu\n",
            air_evidence_summary_count(air, AIR_EVIDENCE_HIR_ROUTINE),
            air_evidence_summary_count(air, AIR_EVIDENCE_HIR_CFG),
            air_evidence_summary_count(air, AIR_EVIDENCE_RIR_BOUNDARY),
            air_evidence_summary_count(air, AIR_EVIDENCE_RIR_AUTHORITY),
            air_evidence_summary_count(air, AIR_EVIDENCE_MIR_CLEANUP),
            air_evidence_summary_count(air, AIR_EVIDENCE_MIR_PIN_CLEANUP),
            air_evidence_summary_count(air, AIR_EVIDENCE_MIR_TERMINATOR),
            air_evidence_summary_count(air, AIR_EVIDENCE_MIR_SELECT_RECEIVE),
            air_evidence_summary_count(air, AIR_EVIDENCE_DAG_METADATA),
            air_evidence_summary_count(air, AIR_EVIDENCE_DAG_GENERIC),
            air_evidence_summary_count(air, AIR_EVIDENCE_DAG_ABILITY),
            air_evidence_summary_count(air, AIR_EVIDENCE_RIR_EFFECT_PROPAGATION),
            air_evidence_required_count(air, AIR_EVIDENCE_RIR_EFFECT_PROPAGATION),
            air_evidence_summary_count(air, AIR_EVIDENCE_RIR_RELATION_PROPAGATION),
            air_evidence_required_count(air, AIR_EVIDENCE_RIR_RELATION_PROPAGATION));
    fprintf(out,
            "  runtime_evidence observability_schema=%zu frontier_policy=%zu\n",
            air_evidence_summary_count(air, AIR_EVIDENCE_OBSERVABILITY_SCHEMA),
            air_evidence_summary_count(air,
                                       AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY));
    for (size_t i = 0; i < air_intent_node_count(air); i++) {
        const AIRIntentNode *intent = air_intent_node_at(air, i);
        if (intent == NULL)
            continue;
        fprintf(out,
                "  intent[%zu] owner=%s step=%s index=%zu sync=%s failure=%s who_from_intent_default=%s who_from_on_receiver=%s who_from_single_participant=%s requires_from_action=%s causes_from_action=%s\n",
                i,
                intent->intent_owner != NULL ? intent->intent_owner : "<anonymous>",
                intent->step_name != NULL ? intent->step_name : "<unnamed>",
                intent->step_index,
                air_sync_class_name(intent->sync_class),
                air_failure_class_name(intent->failure_class),
                intent->who_from_intent_default ? "yes" : "no",
                intent->who_from_on_receiver ? "yes" : "no",
                intent->who_from_single_participant ? "yes" : "no",
                intent->requires_from_action ? "yes" : "no",
                intent->causes_from_action ? "yes" : "no");
    }
    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        const AIRBoundaryNode *boundary = air_boundary_node_at(air, i);
        if (boundary == NULL)
            continue;
        fprintf(out,
                "  boundary[%zu] kind=%s owner=%s source=%s intent=%zu step=%zu sync=%s authority=%s source_from_intent_default=%s source_from_action=%s source_from_transfer=%s authority_from_zone=%s authority_from_action=%s\n",
                i,
                air_boundary_kind_name(boundary->kind),
                boundary->owner_name != NULL ? boundary->owner_name : "<anonymous>",
                boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                boundary->intent_index,
                boundary->step_index,
                air_sync_class_name(boundary->sync_class),
                boundary->authority_required ? "yes" : "no",
                boundary->source_from_intent_default ? "yes" : "no",
                boundary->source_from_action ? "yes" : "no",
                boundary->source_from_transfer ? "yes" : "no",
                boundary->authority_from_zone ? "yes" : "no",
                boundary->authority_from_action ? "yes" : "no");
        fprintf(out,
                "    evidence hir=%s(%s) hir_cfg=%s rir_boundary=%s(%s) rir_authority=%s(%s)\n",
                air_boundary_has_evidence(air, i, AIR_EVIDENCE_HIR_ROUTINE) ? "yes" : "no",
                air_boundary_evidence_provider(air, i, AIR_EVIDENCE_HIR_ROUTINE),
                air_boundary_has_evidence(air, i, AIR_EVIDENCE_HIR_CFG) ? "yes" : "no",
                air_boundary_has_evidence(air, i, AIR_EVIDENCE_RIR_BOUNDARY) ? "yes" : "no",
                air_boundary_evidence_provider(air, i, AIR_EVIDENCE_RIR_BOUNDARY),
                air_boundary_has_evidence(air, i, AIR_EVIDENCE_RIR_AUTHORITY) ? "yes" : "no",
                air_boundary_evidence_subject(air, i, AIR_EVIDENCE_RIR_AUTHORITY));
    }
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (evidence == NULL)
            continue;
        fprintf(out,
                "  evidence_node[%zu] kind=%s boundary=%zu provider=%s subject=%s facts=%zu fallbacks=%zu\n",
                i,
            air_evidence_kind_name(evidence->kind),
            evidence->boundary_index,
                evidence->provider_name != NULL ? evidence->provider_name : "<none>",
                evidence->subject_name != NULL ? evidence->subject_name : "<none>",
                evidence->fact_count,
                evidence->fallback_count);
    }
}
