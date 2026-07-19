#include "mir_json_dump_flow.h"

#include "mir_json_dump_internal.h"

void
mir_json_emit_resource_flow_symbols(FILE *out, const MIRRoutine *routine)
{
    size_t count = routine != NULL
        ? routine->resource_flow_symbol_count : 0;

    fprintf(out, ",\"resource_flow_symbol_count\":%zu"
                 ",\"resource_flow_symbols\":[", count);
    for (size_t i = 0; i < count; i++) {
        const MIRResourceFlowSymbol *symbol =
            &routine->resource_flow_symbols[i];
        if (i > 0)
            fputc(',', out);
        fprintf(out, "{\"stable_index\":%zu,\"declaration_syntax_id\":%u"
                     ",\"line\":%u,\"column\":%u,\"symbol_kind\":%u"
                     ",\"is_parameter\":%s,\"parameter_index\":%zu"
                     ",\"name\":",
                symbol->stable_index,
                symbol->declaration_syntax_id,
                symbol->line,
                symbol->column,
                symbol->symbol_kind,
                symbol->is_parameter ? "true" : "false",
                symbol->parameter_index);
        mir_json_emit_str_or_null(out, symbol->name);
        fputc('}', out);
    }
    fputc(']', out);
}

void
mir_json_emit_loop_flow_facts(FILE *out, const MIRRoutine *routine)
{
    size_t summary_count = routine != NULL
        ? routine->loop_flow_summary_count : 0;
    size_t state_count = routine != NULL
        ? routine->loop_flow_state_count : 0;

    fprintf(out, ",\"loop_flow_summary_count\":%zu"
                 ",\"loop_flow_summaries\":[", summary_count);
    for (size_t i = 0; i < summary_count; i++) {
        const PgyLoopFlowSummaryFact *summary =
            &routine->loop_flow_summaries[i];
        if (i > 0)
            fputc(',', out);
        fprintf(out,
                "{\"loop_syntax_id\":%u,\"kind\":\"%s\""
                ",\"effect_base\":%u,\"effect_delta\":%u"
                ",\"flags\":%u,\"entry_state_start\":%zu"
                ",\"entry_state_count\":%zu"
                ",\"exit_state_start\":%zu,\"exit_state_count\":%zu}",
                summary->loop_syntax_id,
                summary->kind == 1u ? "for" : "while",
                summary->effect_base,
                summary->effect_delta,
                summary->flags,
                summary->entry_state_start,
                summary->entry_state_count,
                summary->exit_state_start,
                summary->exit_state_count);
    }
    fprintf(out, "],\"loop_flow_state_count\":%zu,\"loop_flow_states\":[",
            state_count);
    for (size_t i = 0; i < state_count; i++) {
        const PgyLoopFlowStateFact *state = &routine->loop_flow_states[i];
        if (i > 0)
            fputc(',', out);
        fprintf(out,
                "{\"stable_index\":%zu,\"is_consumed\":%s"
                ",\"is_used\":%s,\"access_mask\":%u"
                ",\"slot_state\":%d,\"semantic_state\":%d"
                ",\"pool_id\":%d}",
                state->stable_index,
                state->is_consumed ? "true" : "false",
                state->is_used ? "true" : "false",
                (unsigned)state->access_mask,
                state->slot_state,
                state->semantic_state,
                state->pool_id);
    }
    fputc(']', out);
}

void
mir_json_emit_iteration_type_facts(FILE *out, const MIRRoutine *routine)
{
    size_t count = routine != NULL ? routine->iteration_type_fact_count : 0;
    fprintf(out, ",\"iteration_type_fact_count\":%zu"
                 ",\"iteration_type_facts\":[", count);
    for (size_t i = 0; i < count; i++) {
        const MIRIterationTypeFact *fact = &routine->iteration_type_facts[i];
        if (i > 0)
            fputc(',', out);
        fprintf(out, "{\"function_syntax_id\":%u"
                     ",\"iteration_syntax_id\":%u"
                     ",\"binding_type\":",
                fact->function_syntax_id, fact->iteration_syntax_id);
        mir_json_emit_str_or_null(out, fact->binding_type_name);
        fputs(",\"iterable_type\":", out);
        mir_json_emit_str_or_null(out, fact->iterable_type_name);
        fprintf(out, ",\"collection_hoisted\":%s}",
                fact->collection_hoisted ? "true" : "false");
    }
    fputc(']', out);
}

void
mir_json_emit_destructure_type_facts(FILE *out, const MIRRoutine *routine)
{
    size_t count = routine != NULL ? routine->destructure_type_fact_count : 0;
    fprintf(out, ",\"destructure_type_fact_count\":%zu"
                 ",\"destructure_type_facts\":[", count);
    for (size_t i = 0; i < count; i++) {
        const MIRDestructureTypeFact *fact =
            &routine->destructure_type_facts[i];
        if (i > 0)
            fputc(',', out);
        fprintf(out, "{\"function_syntax_id\":%u"
                     ",\"destructure_syntax_id\":%u"
                     ",\"binding_index\":%zu"
                     ",\"binding_count\":%zu"
                     ",\"binding_type\":",
                fact->function_syntax_id,
                fact->destructure_syntax_id,
                fact->binding_index,
                fact->binding_count);
        mir_json_emit_str_or_null(out, fact->binding_type_name);
        fputc('}', out);
    }
    fputc(']', out);
}
