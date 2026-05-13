/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR dump and vocabulary owner.
 */

#include "air.h"

#include "../runtime/pgy_runtime_observability_schema.h"
#include "../runtime/pgy_frontier_policy.h"

#include <stdio.h>

static void
air_json_string(FILE *out, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text != NULL ? text : "");
    fputc('"', out);
    while (*p != '\0') {
        switch (*p) {
        case '"':
            fputs("\\\"", out);
            break;
        case '\\':
            fputs("\\\\", out);
            break;
        case '\b':
            fputs("\\b", out);
            break;
        case '\f':
            fputs("\\f", out);
            break;
        case '\n':
            fputs("\\n", out);
            break;
        case '\r':
            fputs("\\r", out);
            break;
        case '\t':
            fputs("\\t", out);
            break;
        default:
            if (*p < 0x20)
                fprintf(out, "\\u%04x", (unsigned int)*p);
            else
                fputc((int)*p, out);
            break;
        }
        p++;
    }
    fputc('"', out);
}

static void
air_json_bool(FILE *out, bool value)
{
    fputs(value ? "true" : "false", out);
}

static void
air_json_string_array(FILE *out, const char *const *items, size_t count)
{
    fputc('[', out);
    for (size_t i = 0; i < count; i++) {
        if (i > 0)
            fputc(',', out);
        air_json_string(out, items[i]);
    }
    fputc(']', out);
}

static void
air_dump_json_observability_schema(FILE *out)
{
    static const char *const surfaces[] = {
        PGY_OBSERVABILITY_SURFACE_LAST,
        PGY_OBSERVABILITY_SURFACE_HISTORY,
        PGY_OBSERVABILITY_SURFACE_ACTIVE,
        PGY_OBSERVABILITY_SURFACE_RECENT,
    };
    static const char *const events[] = {
        PGY_OBSERVABILITY_EVENT_INTENT_ENTER,
        PGY_OBSERVABILITY_EVENT_STEP_BEGIN,
        PGY_OBSERVABILITY_EVENT_BIND,
        PGY_OBSERVABILITY_EVENT_MATERIALIZE,
        PGY_OBSERVABILITY_EVENT_TRANSFER,
        PGY_OBSERVABILITY_EVENT_STEP_OK,
        PGY_OBSERVABILITY_EVENT_FAIL,
        PGY_OBSERVABILITY_EVENT_MIR_RESOURCE,
    };
    static const char *const active_fields[] = {
        PGY_OBSERVABILITY_FIELD_HANDLE,
        PGY_OBSERVABILITY_FIELD_PARENT_HANDLE,
        PGY_OBSERVABILITY_FIELD_TRACE_ID,
        PGY_OBSERVABILITY_FIELD_NAME,
        PGY_OBSERVABILITY_FIELD_TRACE,
        PGY_OBSERVABILITY_FIELD_FAILURE_REASON,
        PGY_OBSERVABILITY_FIELD_STEP_COUNT,
        PGY_OBSERVABILITY_FIELD_FAILED,
        PGY_OBSERVABILITY_FIELD_ACTIVE,
    };
    static const char *const history_fields[] = {
        PGY_OBSERVABILITY_FIELD_NAME,
        PGY_OBSERVABILITY_FIELD_ZONE,
        PGY_OBSERVABILITY_FIELD_PHASE,
        PGY_OBSERVABILITY_FIELD_PARTICIPANT,
        PGY_OBSERVABILITY_FIELD_SLOT,
        PGY_OBSERVABILITY_FIELD_FROM_ZONE,
        PGY_OBSERVABILITY_FIELD_FROM_SLOT,
        PGY_OBSERVABILITY_FIELD_TO_ZONE,
        PGY_OBSERVABILITY_FIELD_TO_SLOT,
        PGY_OBSERVABILITY_FIELD_OK,
        PGY_OBSERVABILITY_FIELD_FAILURE_REASON,
    };
    static const char *const recent_fields[] = {
        PGY_OBSERVABILITY_FIELD_HANDLE,
        PGY_OBSERVABILITY_FIELD_TRACE_ID,
        PGY_OBSERVABILITY_FIELD_NAME,
        PGY_OBSERVABILITY_FIELD_TRACE,
        PGY_OBSERVABILITY_FIELD_FAILURE_REASON,
        PGY_OBSERVABILITY_FIELD_STEP_COUNT,
        PGY_OBSERVABILITY_FIELD_FAILED,
    };

    fputs("\"observability\":{\"abi_schema\":", out);
    air_json_string(out, PGY_OBSERVABILITY_ABI_SCHEMA);
    fputs(",\"trace_schema\":", out);
    air_json_string(out, PGY_OBSERVABILITY_TRACE_SCHEMA);
    fputs(",\"surfaces\":", out);
    air_json_string_array(out, surfaces, sizeof(surfaces) / sizeof(surfaces[0]));
    fputs(",\"event_kinds\":", out);
    air_json_string_array(out, events, sizeof(events) / sizeof(events[0]));
    fputs(",\"active_fields\":", out);
    air_json_string_array(out, active_fields, sizeof(active_fields) / sizeof(active_fields[0]));
    fputs(",\"history_fields\":", out);
    air_json_string_array(out, history_fields, sizeof(history_fields) / sizeof(history_fields[0]));
    fputs(",\"recent_fields\":", out);
    air_json_string_array(out, recent_fields, sizeof(recent_fields) / sizeof(recent_fields[0]));
    fputs("}", out);
}

static unsigned
air_node_line(const ASTNode *node)
{
    return node != NULL ? node->line : 0U;
}

static unsigned
air_node_column(const ASTNode *node)
{
    return node != NULL ? node->column : 0U;
}

static const AIRBoundaryNode *
air_evidence_boundary(const AIRProgram *air, const AIREvidenceNode *evidence)
{
    if (air == NULL || evidence == NULL
        || evidence->boundary_index == SIZE_MAX
        || evidence->boundary_index >= air->boundary_count) {
        return NULL;
    }
    return &air->boundaries[evidence->boundary_index];
}

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
            air->intent_count,
            air->boundary_count,
            air->evidence_count,
            air->drift_count,
            air->strict_evidence ? "yes" : "no",
            air->has_hir_input ? "yes" : "no",
            air->has_rir_input ? "yes" : "no",
            air->has_mir_input ? "yes" : "no");
    fprintf(out, "  evidence hir_routines=%zu hir_cfg=%zu rir_boundaries=%zu rir_authority=%zu mir_cleanup=%zu mir_pin_cleanup=%zu mir_terminator=%zu mir_select_receive=%zu dag_metadata=%zu dag_generic=%zu dag_ability=%zu rir_effect=%zu/%zu rir_relation=%zu/%zu\n",
            air->hir_routine_evidence_count,
            air->hir_cfg_evidence_count,
            air->rir_boundary_evidence_count,
            air->rir_authority_evidence_count,
            air->mir_cleanup_evidence_count,
            air->mir_pin_cleanup_evidence_count,
            air->mir_terminator_evidence_count,
            air->mir_select_receive_evidence_count,
            air->dag_metadata_evidence_count,
            air->dag_generic_evidence_count,
            air->dag_ability_evidence_count,
            air->rir_effect_propagation_evidence_count,
            air->rir_effect_propagation_required_count,
            air->rir_relation_propagation_evidence_count,
            air->rir_relation_propagation_required_count);
    fprintf(out,
            "  runtime_evidence observability_schema=%zu frontier_policy=%zu\n",
            air->observability_schema_evidence_count,
            air->runtime_frontier_policy_evidence_count);
    for (size_t i = 0; i < air->intent_count; i++) {
        const AIRIntentNode *intent = &air->intents[i];
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
    for (size_t i = 0; i < air->boundary_count; i++) {
        const AIRBoundaryNode *boundary = &air->boundaries[i];
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
                boundary->hir_routine_evidence_name != NULL
                    ? boundary->hir_routine_evidence_name
                    : "<none>",
                air_boundary_has_evidence(air, i, AIR_EVIDENCE_HIR_CFG) ? "yes" : "no",
                air_boundary_has_evidence(air, i, AIR_EVIDENCE_RIR_BOUNDARY) ? "yes" : "no",
                boundary->rir_boundary_evidence_scope != NULL
                    ? boundary->rir_boundary_evidence_scope
                    : "<none>",
                air_boundary_has_evidence(air, i, AIR_EVIDENCE_RIR_AUTHORITY) ? "yes" : "no",
                boundary->rir_authority_evidence_name != NULL
                    ? boundary->rir_authority_evidence_name
                    : "<none>");
    }
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
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

void
air_dump_json(const AIRProgram *air, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (air == NULL) {
        fputs("{\"schema\":\"pgy.air.graph.v1\",\"program\":null}\n", out);
        return;
    }

    fputs("{", out);
    fputs("\"schema\":\"pgy.air.graph.v1\",", out);
    fprintf(out,
            "\"summary\":{\"intent_count\":%zu,\"boundary_count\":%zu,\"evidence_count\":%zu,\"drift_count\":%zu,"
            "\"strict_evidence\":",
            air->intent_count,
            air->boundary_count,
            air->evidence_count,
            air->drift_count);
    air_json_bool(out, air->strict_evidence);
    fputs(",\"hir_input\":", out);
    air_json_bool(out, air->has_hir_input);
    fputs(",\"rir_input\":", out);
    air_json_bool(out, air->has_rir_input);
    fputs(",\"mir_input\":", out);
    air_json_bool(out, air->has_mir_input);
    fprintf(out,
            ",\"hir_routine_evidence_count\":%zu,\"hir_cfg_evidence_count\":%zu,"
            "\"rir_boundary_evidence_count\":%zu,\"rir_authority_evidence_count\":%zu,"
            "\"mir_cleanup_evidence_count\":%zu,\"mir_pin_cleanup_evidence_count\":%zu,"
            "\"mir_terminator_evidence_count\":%zu,"
            "\"mir_select_receive_evidence_count\":%zu,"
            "\"dag_metadata_evidence_count\":%zu,\"dag_generic_evidence_count\":%zu,\"dag_ability_evidence_count\":%zu,"
            "\"rir_effect_propagation_required_count\":%zu,\"rir_effect_propagation_evidence_count\":%zu,"
            "\"rir_relation_propagation_required_count\":%zu,\"rir_relation_propagation_evidence_count\":%zu,"
            "\"observability_schema_evidence_count\":%zu,"
            "\"runtime_frontier_policy_evidence_count\":%zu},",
            air->hir_routine_evidence_count,
            air->hir_cfg_evidence_count,
            air->rir_boundary_evidence_count,
            air->rir_authority_evidence_count,
            air->mir_cleanup_evidence_count,
            air->mir_pin_cleanup_evidence_count,
            air->mir_terminator_evidence_count,
            air->mir_select_receive_evidence_count,
            air->dag_metadata_evidence_count,
            air->dag_generic_evidence_count,
            air->dag_ability_evidence_count,
            air->rir_effect_propagation_required_count,
            air->rir_effect_propagation_evidence_count,
            air->rir_relation_propagation_required_count,
            air->rir_relation_propagation_evidence_count,
            air->observability_schema_evidence_count,
            air->runtime_frontier_policy_evidence_count);

    air_dump_json_observability_schema(out);
    fputs(",\"runtime_frontier_policy\":{\"schema\":", out);
    air_json_string(out, PGY_FRONTIER_POLICY_SCHEMA);
    fputs(",\"subject\":", out);
    air_json_string(out, PGY_FRONTIER_POLICY_SUBJECT);
    fprintf(out,
            ",\"pass_limit_fact_count\":%u,"
            "\"overflow_reason_fact_count\":%u,"
            "\"fact_count\":%u}",
            (unsigned)PGY_FRONTIER_PASS_LIMIT_FACT_COUNT,
            (unsigned)PGY_FRONTIER_OVERFLOW_REASON_FACT_COUNT,
            (unsigned)PGY_FRONTIER_POLICY_FACT_COUNT);
    fputs(",\"intents\":[", out);
    for (size_t i = 0; i < air->intent_count; i++) {
        const AIRIntentNode *intent = &air->intents[i];
        if (i > 0)
            fputc(',', out);
        fprintf(out, "{\"id\":%zu,\"owner\":", i);
        air_json_string(out, intent->intent_owner);
        fputs(",\"step\":", out);
        air_json_string(out, intent->step_name);
        fprintf(out, ",\"step_index\":%zu,\"sync\":", intent->step_index);
        air_json_string(out, air_sync_class_name(intent->sync_class));
        fputs(",\"failure\":", out);
        air_json_string(out, air_failure_class_name(intent->failure_class));
        fputs(",\"who_from_intent_default\":", out);
        air_json_bool(out, intent->who_from_intent_default);
        fputs(",\"who_from_on_receiver\":", out);
        air_json_bool(out, intent->who_from_on_receiver);
        fputs(",\"who_from_single_participant\":", out);
        air_json_bool(out, intent->who_from_single_participant);
        fputs(",\"requires_from_action\":", out);
        air_json_bool(out, intent->requires_from_action);
        fputs(",\"causes_from_action\":", out);
        air_json_bool(out, intent->causes_from_action);
        fprintf(out,
                ",\"location\":{\"line\":%u,\"column\":%u}}",
                air_node_line(intent->ast),
                air_node_column(intent->ast));
    }
    fputs("],", out);

    fputs("\"boundaries\":[", out);
    for (size_t i = 0; i < air->boundary_count; i++) {
        const AIRBoundaryNode *boundary = &air->boundaries[i];
        if (i > 0)
            fputc(',', out);
        fprintf(out, "{\"id\":%zu,\"kind\":", i);
        air_json_string(out, air_boundary_kind_name(boundary->kind));
        fputs(",\"owner\":", out);
        air_json_string(out, boundary->owner_name);
        fputs(",\"source\":", out);
        air_json_string(out, boundary->source_name);
        fprintf(out,
                ",\"intent\":%zu,\"step\":%zu,\"sync\":",
                boundary->intent_index,
                boundary->step_index);
        air_json_string(out, air_sync_class_name(boundary->sync_class));
        fputs(",\"authority_required\":", out);
        air_json_bool(out, boundary->authority_required);
        fputs(",\"source_from_intent_default\":", out);
        air_json_bool(out, boundary->source_from_intent_default);
        fputs(",\"source_from_action\":", out);
        air_json_bool(out, boundary->source_from_action);
        fputs(",\"source_from_transfer\":", out);
        air_json_bool(out, boundary->source_from_transfer);
        fputs(",\"authority_from_zone\":", out);
        air_json_bool(out, boundary->authority_from_zone);
        fputs(",\"authority_from_action\":", out);
        air_json_bool(out, boundary->authority_from_action);
        fputs(",\"authority_names\":[", out);
        for (size_t j = 0; j < boundary->authority_name_count; j++) {
            if (j > 0)
                fputc(',', out);
            air_json_string(out, boundary->authority_names[j]);
        }
        fputs("],\"evidence_flags\":{", out);
        fputs("\"hir_routine\":", out);
        air_json_bool(out, air_boundary_has_evidence(
            air, i, AIR_EVIDENCE_HIR_ROUTINE));
        fputs(",\"hir_cfg\":", out);
        air_json_bool(out, air_boundary_has_evidence(
            air, i, AIR_EVIDENCE_HIR_CFG));
        fputs(",\"rir_boundary\":", out);
        air_json_bool(out, air_boundary_has_evidence(
            air, i, AIR_EVIDENCE_RIR_BOUNDARY));
        fputs(",\"rir_authority\":", out);
        air_json_bool(out, air_boundary_has_evidence(
            air, i, AIR_EVIDENCE_RIR_AUTHORITY));
        fputs("},\"location\":{", out);
        fprintf(out,
                "\"line\":%u,\"column\":%u}}",
                air_node_line(boundary->ast),
                air_node_column(boundary->ast));
    }
    fputs("],", out);

    fputs("\"evidence\":[", out);
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        const AIRBoundaryNode *boundary = air_evidence_boundary(air, evidence);
        if (i > 0)
            fputc(',', out);
        fprintf(out, "{\"id\":%zu,\"kind\":", i);
        air_json_string(out, air_evidence_kind_name(evidence->kind));
        if (evidence->boundary_index == SIZE_MAX)
            fputs(",\"boundary\":null,\"provider\":", out);
        else
            fprintf(out, ",\"boundary\":%zu,\"provider\":", evidence->boundary_index);
        air_json_string(out, evidence->provider_name);
        fputs(",\"subject\":", out);
        air_json_string(out, evidence->subject_name);
        fputs(",\"boundary_kind\":", out);
        air_json_string(out, boundary != NULL
                             ? air_boundary_kind_name(boundary->kind)
                             : NULL);
        fputs(",\"boundary_owner\":", out);
        air_json_string(out, boundary != NULL ? boundary->owner_name : NULL);
        fputs(",\"boundary_source\":", out);
        air_json_string(out, boundary != NULL ? boundary->source_name : NULL);
        fprintf(out,
                ",\"fact_count\":%zu,\"fallback_count\":%zu",
                evidence->fact_count,
                evidence->fallback_count);
        fputc('}', out);
    }
    fputs("],", out);

    fputs("\"drifts\":[", out);
    for (size_t i = 0; i < air->drift_count; i++) {
        const AIRDrift *drift = &air->drifts[i];
        if (i > 0)
            fputc(',', out);
        fprintf(out,
                "{\"id\":%zu,\"kind\":",
                i);
        air_json_string(out, air_drift_kind_name(drift->kind));
        if (drift->intent_index == SIZE_MAX)
            fputs(",\"intent\":null", out);
        else
            fprintf(out, ",\"intent\":%zu", drift->intent_index);
        if (drift->boundary_index == SIZE_MAX)
            fputs(",\"boundary\":null,\"message\":", out);
        else
            fprintf(out, ",\"boundary\":%zu,\"message\":", drift->boundary_index);
        air_json_string(out, drift->message);
        fputc('}', out);
    }
    fputs("]}\n", out);
}
