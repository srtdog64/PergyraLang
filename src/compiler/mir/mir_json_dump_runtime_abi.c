#include "mir_json_dump_runtime_abi.h"

#include "mir_json_dump_internal.h"
#include <inttypes.h>

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

/* ABI layout is a MIR-owned fact.  Carry the complete static row across the
 * JSON boundary instead of asking a self-hosted consumer to rediscover it
 * from the source type or a process-local ABI table.  The numeric
 * representation is intentional: it is the enum value hashed by
 * mir_abi_layout_id, so the wire row remains lossless and target-neutral. */
void
mir_json_emit_abi_layout_value(FILE *out, const MIRTypeLayout *layout)
{
    if (layout == NULL) {
        fputs("null", out);
        return;
    }

    fputs("{\"type\":", out);
    mir_json_emit_str_or_null(out, layout->abi_type_name);
    fprintf(out, ",\"size\":%u,\"align\":%u,\"fields\":[",
            layout->size_bytes, layout->align_bytes);
    for (uint16_t i = 0; i < layout->field_count; i++) {
        const MIRFieldLayout *field = &layout->fields[i];
        if (i > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        mir_json_emit_str_or_null(out, field->field_name);
        fprintf(out, ",\"offset\":%u,\"size\":%u,\"align\":%u}",
                field->offset, field->field_size, field->field_align);
    }
    fputs("],\"runtime_fn\":", out);
    mir_json_emit_str_or_null(out, layout->runtime_fn);
    fputs(",\"inner_c_type\":", out);
    mir_json_emit_str_or_null(out, layout->inner_c_type);
    fprintf(out, ",\"representation\":%u,\"discriminant\":",
            (unsigned)layout->representation);
    mir_json_emit_str_or_null(out, layout->discriminant_field_name);
    fprintf(out, ",\"primary_tag\":%" PRId32
                ",\"secondary_tag\":%" PRId32 ",\"niche_none_pattern\":",
            layout->primary_tag_value, layout->secondary_tag_value);
    mir_json_emit_str_or_null(out, layout->niche_none_pattern);
    fputc('}', out);
}
