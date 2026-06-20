/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR stable JSON graph dump owner.
 */

#include <stdint.h>
#include <stdio.h>

#include "air_internal.h"

#include "../runtime/pgy_frontier_policy.h"
#include "../runtime/pgy_runtime_observability_schema.h"

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
air_json_next_top_level_field(FILE *out, bool *has_previous)
{
    if (has_previous != NULL && *has_previous) {
        fputc(',', out);
        return;
    }
    if (has_previous != NULL)
        *has_previous = true;
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
    size_t boundary_index = air_evidence_node_boundary_index_or(evidence,
                                                                SIZE_MAX);
    if (air == NULL || evidence == NULL
        || boundary_index == SIZE_MAX
        || boundary_index >= air_boundary_node_count(air)) {
        return NULL;
    }
    return air_boundary_node_at(air, boundary_index);
}

static void
air_dump_json_summary(const AIRProgram *air, FILE *out)
{
    fprintf(out,
            "\"summary\":{\"intent_count\":%zu,\"boundary_count\":%zu,\"evidence_count\":%zu,\"drift_count\":%zu,"
            "\"strict_evidence\":",
            air_intent_node_count(air),
            air_boundary_node_count(air),
            air_evidence_node_count(air),
            air_drift_count(air));
    air_json_bool(out, air_requires_strict_evidence(air));
    fputs(",\"hir_input\":", out);
    air_json_bool(out, air_has_hir_input(air));
    fputs(",\"rir_input\":", out);
    air_json_bool(out, air_has_rir_input(air));
    fputs(",\"mir_input\":", out);
    air_json_bool(out, air_has_mir_input(air));
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
            "\"runtime_frontier_policy_evidence_count\":%zu}",
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
            air_evidence_required_count(air, AIR_EVIDENCE_RIR_EFFECT_PROPAGATION),
            air_evidence_summary_count(air, AIR_EVIDENCE_RIR_EFFECT_PROPAGATION),
            air_evidence_required_count(air, AIR_EVIDENCE_RIR_RELATION_PROPAGATION),
            air_evidence_summary_count(air, AIR_EVIDENCE_RIR_RELATION_PROPAGATION),
            air_evidence_summary_count(air, AIR_EVIDENCE_OBSERVABILITY_SCHEMA),
            air_evidence_summary_count(air,
                                       AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY));
}

static void
air_dump_json_frontier_policy(FILE *out)
{
    fputs("\"runtime_frontier_policy\":{\"schema\":", out);
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
}

static void
air_dump_json_intents(const AIRProgram *air, FILE *out)
{
    fputs("\"intents\":[", out);
    for (size_t i = 0; i < air_intent_node_count(air); i++) {
        const AIRIntentNode *intent = air_intent_node_at(air, i);
        if (intent == NULL)
            continue;
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
    fputs("]", out);
}

static void
air_dump_json_boundaries(const AIRProgram *air, FILE *out)
{
    fputs("\"boundaries\":[", out);
    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        const AIRBoundaryNode *boundary = air_boundary_node_at(air, i);
        if (boundary == NULL)
            continue;
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
        for (size_t j = 0;
             j < air_boundary_authority_name_count(boundary);
             j++) {
            if (j > 0)
                fputc(',', out);
            air_json_string(out,
                            air_boundary_authority_name_at(boundary, j));
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
    fputs("]", out);
}

static void
air_dump_json_evidence(const AIRProgram *air, FILE *out)
{
    fputs("\"evidence\":[", out);
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        const AIRBoundaryNode *boundary = air_evidence_boundary(air, evidence);
        size_t boundary_index = air_evidence_node_boundary_index_or(evidence,
                                                                    SIZE_MAX);
        if (evidence == NULL)
            continue;
        if (i > 0)
            fputc(',', out);
        fprintf(out, "{\"id\":%zu,\"kind\":", i);
        air_json_string(out,
                        air_evidence_kind_name(air_evidence_node_kind(evidence)));
        fputs(",\"provider_kind\":", out);
        air_json_string(out,
                        air_evidence_provider_kind_name(
                            air_evidence_node_provider_kind(evidence)));
        fputs(",\"subject_kind\":", out);
        air_json_string(out,
                        air_evidence_subject_kind_name(
                            air_evidence_node_subject_kind(evidence)));
        if (boundary_index == SIZE_MAX)
            fputs(",\"boundary\":null,\"provider\":", out);
        else
            fprintf(out, ",\"boundary\":%zu,\"provider\":", boundary_index);
        air_json_string(out,
                        air_evidence_node_provider_name_or(evidence, NULL));
        fputs(",\"subject\":", out);
        air_json_string(out,
                        air_evidence_node_subject_name_or(evidence, NULL));
        fputs(",\"boundary_kind\":", out);
        air_json_string(out,
                        air_evidence_node_has_boundary_shape(evidence)
                            ? air_boundary_kind_name(
                                air_evidence_node_boundary_kind_or(
                                    evidence,
                                    AIR_BOUNDARY_UNKNOWN))
                            : (boundary != NULL
                                ? air_boundary_kind_name(boundary->kind)
                                : NULL));
        fputs(",\"boundary_owner\":", out);
        air_json_string(out,
                        air_evidence_node_boundary_owner_name_or(
                            evidence,
                            boundary != NULL ? boundary->owner_name : NULL));
        fputs(",\"boundary_source\":", out);
        air_json_string(out,
                        air_evidence_node_boundary_source_name_or(
                            evidence,
                            boundary != NULL ? boundary->source_name : NULL));
        fprintf(out,
                ",\"fact_count\":%zu,\"fallback_count\":%zu",
                air_evidence_node_fact_count(evidence),
                air_evidence_node_fallback_count(evidence));
        fputc('}', out);
    }
    fputs("]", out);
}

static void
air_dump_json_drifts(const AIRProgram *air, FILE *out)
{
    fputs("\"drifts\":[", out);
    for (size_t i = 0; i < air_drift_count(air); i++) {
        const AIRDrift *drift = air_drift_at(air, i);
        if (drift == NULL)
            continue;
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
    fputs("]", out);
}

void
air_dump_json(const AIRProgram *air, FILE *out)
{
    bool has_field = false;

    if (out == NULL)
        out = stdout;
    if (air == NULL) {
        fputs("{\"schema\":\"pgy.air.graph.v1\",\"program\":null}\n", out);
        return;
    }

    fputs("{", out);
    air_json_next_top_level_field(out, &has_field);
    fputs("\"schema\":\"pgy.air.graph.v1\"", out);
    air_json_next_top_level_field(out, &has_field);
    air_dump_json_summary(air, out);
    air_json_next_top_level_field(out, &has_field);
    air_dump_json_observability_schema(out);
    air_json_next_top_level_field(out, &has_field);
    air_dump_json_frontier_policy(out);
    air_json_next_top_level_field(out, &has_field);
    air_dump_json_intents(air, out);
    air_json_next_top_level_field(out, &has_field);
    air_dump_json_boundaries(air, out);
    air_json_next_top_level_field(out, &has_field);
    air_dump_json_evidence(air, out);
    air_json_next_top_level_field(out, &has_field);
    air_dump_json_drifts(air, out);
    fputs("}\n", out);
}
