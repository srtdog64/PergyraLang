#include "mir_json_dump_domain_runtime.h"

#include "mir_domain_runtime.h"
#include "mir_json_dump_internal.h"

#include <inttypes.h>

static void
mir_json_emit_domain_participant_role(
    FILE *out, const PgyDomainParticipantRoleFact *fact)
{
    fprintf(out,
        "{\"program_syntax_id\":%" PRIu32
        ",\"owner_syntax_id\":%" PRIu32 ",\"role\":",
        fact->program_syntax_id, fact->owner_syntax_id);
    mir_json_emit_str(out, mir_domain_participant_role_name(fact->role));
    fprintf(out, ",\"field_syntax_id\":%" PRIu32 ",\"owner_name\":",
        fact->field_syntax_id);
    mir_json_emit_str(out, fact->owner_name);
    fputs(",\"field_name\":", out);
    mir_json_emit_str(out, fact->field_name);
    fputs(",\"field_type_name\":", out);
    mir_json_emit_str(out, fact->field_type_name);
    fputc('}', out);
}

static void
mir_json_emit_domain_projection_path_segment(
    FILE *out, const PgyDomainProjectionPathSegmentFact *segment)
{
    fprintf(out, "{\"field_syntax_id\":%" PRIu32 ",\"field_name\":",
        segment->field_syntax_id);
    mir_json_emit_str(out, segment->field_name);
    fputs(",\"field_type_name\":", out);
    mir_json_emit_str(out, segment->field_type_name);
    fputc('}', out);
}

static void
mir_json_emit_domain_projection_member(
    FILE *out, const PgyDomainProjectionMemberAssignmentFact *fact)
{
    fprintf(out,
        "{\"program_syntax_id\":%" PRIu32
        ",\"owner_syntax_id\":%" PRIu32
        ",\"directive_syntax_id\":%" PRIu32 ",\"operation\":",
        fact->program_syntax_id,
        fact->owner_syntax_id,
        fact->directive_syntax_id);
    mir_json_emit_str(out,
        mir_domain_projection_operation_name(fact->operation));
    fprintf(out,
        ",\"projection_slot_syntax_id\":%" PRIu32
        ",\"source_slot_syntax_id\":%" PRIu32
        ",\"target_decl_syntax_id\":%" PRIu32
        ",\"target_field_syntax_id\":%" PRIu32
        ",\"source_decl_syntax_id\":%" PRIu32
        ",\"explicit_map\":%s,\"owner_name\":",
        fact->projection_slot_syntax_id,
        fact->source_slot_syntax_id,
        fact->target_decl_syntax_id,
        fact->target_field_syntax_id,
        fact->source_decl_syntax_id,
        fact->explicit_map ? "true" : "false");
    mir_json_emit_str(out, fact->owner_name);
    fputs(",\"projection_slot_name\":", out);
    mir_json_emit_str(out, fact->projection_slot_name);
    fputs(",\"source_slot_name\":", out);
    mir_json_emit_str(out, fact->source_slot_name);
    fputs(",\"target_field_name\":", out);
    mir_json_emit_str(out, fact->target_field_name);
    fputs(",\"target_field_type_name\":", out);
    mir_json_emit_str(out, fact->target_field_type_name);
    fputs(",\"source_path\":", out);
    mir_json_emit_str(out, fact->source_path);
    fputs(",\"source_leaf_type_name\":", out);
    mir_json_emit_str(out, fact->source_leaf_type_name);
    fputs(",\"source_path_segments\":[", out);
    for (size_t i = 0; i < fact->source_path_segment_count; i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_domain_projection_path_segment(
            out, &fact->source_path_segments[i]);
    }
    fputs("]}", out);
}

void
mir_json_emit_domain_runtime_assignments(
    FILE *out, const MIRProgram *mir)
{
    if (out == NULL || mir == NULL || !mir->has_domain_runtime_facts)
        return;
    fputs(",\"domain_runtime_assignments\":{\"participant_roles\":[", out);
    for (size_t i = 0; i < mir->domain_participant_role_fact_count; i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_domain_participant_role(
            out, &mir->domain_participant_role_facts[i]);
    }
    fputs("],\"projection_members\":[", out);
    for (size_t i = 0;
         i < mir->domain_projection_member_assignment_fact_count; i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_domain_projection_member(
            out, &mir->domain_projection_member_assignment_facts[i]);
    }
    fputs("]}", out);
}
