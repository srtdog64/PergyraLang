#include "rir_internal.h"

#include <stdio.h>

static void
json_write_str(FILE *out, const char *s)
{
    if (s == NULL) {
        fputs("null", out);
        return;
    }
    fputc('"', out);
    for (const char *p = s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out); break;
        case '\r': fputs("\\r", out); break;
        case '\t': fputs("\\t", out); break;
        default:   fputc(*p, out); break;
        }
    }
    fputc('"', out);
}

static void
rir_dump_json_fact(FILE *out, const RIRFact *fact)
{
    fprintf(out,
        "      {\"kind\":\"%s\",\"resource\":\"%s\",\"name\":",
        rir_fact_kind_name(fact->kind),
        rir_resource_kind_name(fact->resource_kind));
    json_write_str(out, fact->name);
    fputs(",\"slot_anchor\":", out);
    json_write_str(out, fact->slot_anchor);
    fputs(",\"arg0\":", out);
    json_write_str(out, fact->arg0);
    fputs(",\"arg1\":", out);
    json_write_str(out, fact->arg1);
    fprintf(out, ",\"state\":\"%s\",\"flow_identity\":%s",
        rir_resource_state_name(fact->state),
        fact->has_flow_identity ? "true" : "false");
    if (fact->has_flow_identity) {
        fprintf(out,
                ",\"stable_index\":%zu,\"declaration_syntax_id\":%u",
                fact->stable_index,
                fact->declaration_syntax_id);
    }
    fputc('}', out);
}

static void
rir_dump_json_resource_flow_symbol(FILE *out,
                                   const RIRResourceFlowSymbol *symbol)
{
    fprintf(out,
            "      {\"stable_index\":%zu,\"declaration_syntax_id\":%u"
            ",\"line\":%u,\"column\":%u,\"symbol_kind\":%u"
            ",\"is_parameter\":%s,\"parameter_index\":%zu,\"name\":",
            symbol->stable_index,
            symbol->declaration_syntax_id,
            symbol->line,
            symbol->column,
            symbol->symbol_kind,
            symbol->is_parameter ? "true" : "false",
            symbol->parameter_index);
    json_write_str(out, symbol->name);
    fputc('}', out);
}

static void
rir_dump_json_op(FILE *out, const RIROp *op)
{
    fprintf(out, "      {\"kind\":\"%s\",\"subject\":",
        rir_op_kind_name(op->kind));
    json_write_str(out, op->subject);
    fputs(",\"slot_anchor\":", out);
    json_write_str(out, op->slot_anchor);
    fputs(",\"arg0\":", out);
    json_write_str(out, op->arg0);
    fputs(",\"arg1\":", out);
    json_write_str(out, op->arg1);
    fprintf(out, ",\"machine_contact\":\"%s\"",
        rir_machine_contact_kind_name(op->machine_contact_kind));
    fputc('}', out);
}

static void
rir_dump_json_summary(FILE *out, const RIRStateSummary *summary)
{
    fputs("      {\"name\":", out);
    json_write_str(out, summary->name);
    fputs(",\"slot_anchor\":", out);
    json_write_str(out, summary->slot_anchor);
    fprintf(out, ",\"kind\":\"%s\",\"resource\":\"%s\",",
        rir_fact_kind_name(summary->origin_kind),
        rir_resource_kind_name(summary->resource_kind));
    fprintf(out, "\"initial_state\":\"%s\",\"final_state\":\"%s\",",
        rir_resource_state_name(summary->initial_state),
        rir_resource_state_name(summary->final_state));
    fputs("\"last_op\":", out);
    json_write_str(out, summary->last_op_name);
    fprintf(out, ",\"has_error\":%s,\"flow_identity\":%s",
        summary->has_transition_error ? "true" : "false",
        summary->has_flow_identity ? "true" : "false");
    if (summary->has_flow_identity) {
        fprintf(out,
                ",\"stable_index\":%zu,\"declaration_syntax_id\":%u",
                summary->stable_index,
                summary->declaration_syntax_id);
    }
    fputc('}', out);
}

void
rir_dump_json(const RIRProgram *rir, FILE *out)
{
    RIRScopeInventory inventory;

    if (out == NULL)
        out = stdout;
    if (rir == NULL) {
        fputs("{\"error\":\"RIR program is null\"}\n", out);
        return;
    }

    rir_scope_inventory_from_program(rir, &inventory);
    fputs("{\n  \"rir_version\": 1,\n", out);
    fprintf(out, "  \"scope_count\": %zu,\n", inventory.count);
    fputs("  \"scopes\": [\n", out);

    for (size_t i = 0; i < inventory.count; i++) {
        const RIRScope *scope = rir_scope_inventory_get(&inventory, i);
        size_t fact_count;
        size_t op_count;
        size_t summary_count;
        if (scope == NULL)
            continue;
        fact_count = rir_scope_fact_count(scope);
        op_count = rir_scope_op_count(scope);
        summary_count = rir_scope_state_summary_count(scope);
        if (i > 0)
            fputs(",\n", out);

        fprintf(out,
            "    {\n"
            "      \"index\": %zu,\n"
            "      \"kind\": \"%s\",\n"
            "      \"source_syntax_id\": %u,\n"
            "      \"resource_identity_verified\": %s,\n"
            "      \"resource_flow_symbol_count\": %zu,\n"
            "      \"function_param_flow_summary_count\": %zu,\n"
            "      \"name\": ",
            i,
            rir_scope_kind_name(rir_scope_kind(scope)),
            scope->source_syntax_id,
            scope->resource_identity_verified ? "true" : "false",
            rir_scope_resource_flow_symbol_count(scope),
            rir_scope_function_param_flow_summary_count(scope));
        json_write_str(out, rir_scope_name(scope));
        fputs(",\n      \"owner\": ", out);
        json_write_str(out, rir_scope_owner_name(scope));
        fprintf(out, ",\n      \"fact_count\": %zu,\n"
                     "      \"op_count\": %zu,\n",
                fact_count, op_count);
        fputs("      \"resource_flow_symbols\": [\n", out);
        for (size_t j = 0;
             j < rir_scope_resource_flow_symbol_count(scope); j++) {
            const RIRResourceFlowSymbol *symbol =
                rir_scope_resource_flow_symbol_at(scope, j);
            if (j > 0)
                fputs(",\n", out);
            if (symbol != NULL)
                rir_dump_json_resource_flow_symbol(out, symbol);
            else
                fputs("null", out);
        }
        fputs("\n      ],\n      \"facts\": [\n", out);
        for (size_t j = 0; j < fact_count; j++) {
            const RIRFact *fact = rir_scope_fact_at(scope, j);
            if (j > 0)
                fputs(",\n", out);
            if (fact != NULL)
                rir_dump_json_fact(out, fact);
            else
                fputs("null", out);
        }
        fputs("\n      ],\n      \"ops\": [\n", out);
        for (size_t j = 0; j < op_count; j++) {
            const RIROp *op = rir_scope_op_at(scope, j);
            if (j > 0)
                fputs(",\n", out);
            if (op != NULL)
                rir_dump_json_op(out, op);
            else
                fputs("null", out);
        }
        fputs("\n      ],\n      \"summaries\": [\n", out);
        for (size_t j = 0; j < summary_count; j++) {
            const RIRStateSummary *summary =
                rir_scope_state_summary_at(scope, j);
            if (j > 0)
                fputs(",\n", out);
            if (summary != NULL)
                rir_dump_json_summary(out, summary);
            else
                fputs("null", out);
        }
        fprintf(out, "\n      ],\n"
                     "      \"has_state_errors\": %s\n"
                     "    }",
                rir_scope_has_state_errors(scope) ? "true" : "false");
    }

    fputs("\n  ]\n}\n", out);
}
