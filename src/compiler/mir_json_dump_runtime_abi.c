#include "mir_json_dump_runtime_abi.h"

#include "mir_json_dump_internal.h"

bool
mir_json_emit_instruction_runtime_abi(FILE *out,
                                      const MIRInstruction *inst)
{
    if (out == NULL || inst == NULL || !inst->resource_runtime_fact_present)
        return false;

    const MIRResourceRuntimeRow *row = &inst->resource_runtime_fact;
    fputs(",\"runtime_call_abi\":{\"owner\":\"MIRResource\"", out);
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
    return true;
}
