#include "mir_json_dump_runtime_abi.h"

#include "mir_json_dump_internal.h"

static void
mir_json_emit_runtime_abi_row(FILE *out,
                              const MIRResourceRuntimeRow *row)
{
    fputs("{\"owner\":\"MIRResource\"", out);
    fputs(",\"id\":", out);
    fprintf(out, "%u", row->runtime_call_abi_id);
    fputs(",\"domain\":", out);
    mir_json_emit_str_or_null(out, row->domain);
    fputs(",\"type\":", out);
    mir_json_emit_str_or_null(out, row->abi_type_name);
    fputs(",\"operation\":", out);
    mir_json_emit_str_or_null(out, row->resource_op_name);
    fputs(",\"symbol\":", out);
    mir_json_emit_str_or_null(out, row->runtime_fn);
    fputs(",\"target_kind\":", out);
    mir_json_emit_str_or_null(out, row->target_kind);
    fputs(",\"materialization\":", out);
    mir_json_emit_str_or_null(out, row->materialization);
    fputs(",\"call_shape\":", out);
    mir_json_emit_str_or_null(out, row->call_shape);
    fputc('}', out);
}

bool
mir_json_emit_instruction_runtime_abi(FILE *out,
                                      const MIRInstruction *inst)
{
    if (out == NULL || inst == NULL
        || (!inst->resource_runtime_fact_present
            && inst->resource_runtime_aux_fact_count == 0))
        return false;

    if (inst->resource_runtime_fact_present) {
        fputs(",\"runtime_call_abi\":", out);
        mir_json_emit_runtime_abi_row(out, &inst->resource_runtime_fact);
    }
    if (inst->resource_runtime_aux_fact_count > 0) {
        fputs(",\"runtime_call_abi_aux\":[", out);
        for (size_t i = 0; i < inst->resource_runtime_aux_fact_count; i++) {
            if (i > 0)
                fputc(',', out);
            mir_json_emit_runtime_abi_row(
                out, &inst->resource_runtime_aux_facts[i]);
        }
        fputc(']', out);
    }
    return true;
}
