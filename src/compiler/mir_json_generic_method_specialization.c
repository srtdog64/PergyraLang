#include "mir_json_generic_method_specialization.h"

#include "mir_generic_method_specialization.h"
#include "mir_json_dump_internal.h"

void
mir_json_emit_generic_method_specializations(FILE *out,
                                             const MIRProgram *mir)
{
    size_t count = mir_generic_method_specialization_count(mir);

    for (size_t i = 0; i < count; i++) {
        const MIRGenericMethodSpecializationFact *fact =
            mir_generic_method_specialization_at(mir, i);
        if (i > 0)
            fputc(',', out);
        fputs("{\"source_call_syntax_id\":", out);
        fprintf(out, "%u", fact->source_call_syntax_id);
        fputs(",\"owner\":", out);
        mir_json_emit_str_or_null(out, fact->owner_name);
        fputs(",\"method\":", out);
        mir_json_emit_str_or_null(out, fact->method_name);
        fputs(",\"symbol\":", out);
        mir_json_emit_str_or_null(out, fact->specialized_name);
        fputs(",\"generic_params\":[", out);
        for (size_t j = 0; j < fact->binding_count; j++) {
            if (j > 0)
                fputc(',', out);
            mir_json_emit_str_or_null(out, fact->generic_param_names[j]);
        }
        fputs("],\"actual_types\":[", out);
        for (size_t j = 0; j < fact->binding_count; j++) {
            if (j > 0)
                fputc(',', out);
            mir_json_emit_str_or_null(out, fact->actual_type_names[j]);
        }
        fputs("]}", out);
    }
}
