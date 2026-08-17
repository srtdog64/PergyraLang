#include "mir_json_dump_intent_execution.h"

#include "mir.h"
#include "mir_intent_execution.h"
#include "mir_json_dump_internal.h"

#include <inttypes.h>

static bool
mir_json_has_intent_execution(const MIRProgram *mir)
{
    if (mir == NULL)
        return false;
    for (size_t i = 0; i < mir->routine_count; i++) {
        if (mir_routine_has_admitted_intent_execution_plan(
                &mir->routines[i])) {
            return true;
        }
    }
    return false;
}

static void
mir_json_emit_intent_compensation(
    FILE *out,
    const MIRIntentCompensationFact *fact)
{
    fprintf(out,
        "{\"transition_id\":%" PRIu32
        ",\"expression_syntax_id\":%" PRIu32
        ",\"instruction_block_id\":%zu"
        ",\"instruction_id\":%zu"
        ",\"graph_root_id\":%zu"
        ",\"graph_digest\":%" PRIu32
        ",\"call_target_name\":",
        fact->transition_id,
        fact->expression_syntax_id,
        fact->instruction_block_id,
        fact->instruction_id,
        fact->graph_root_id,
        fact->graph_digest);
    mir_json_emit_str_or_null(out, fact->call_target_name);
    fprintf(out, ",\"call_target_syntax_id\":%" PRIu32 "}",
            fact->call_target_syntax_id);
}

static void
mir_json_emit_intent_step(FILE *out,
                          const MIRIntentStepTransitionFact *row)
{
    fprintf(out,
        "{\"transition_id\":%" PRIu32
        ",\"routine_syntax_id\":%" PRIu32
        ",\"step_syntax_id\":%" PRIu32
        ",\"step_name\":",
        row->transition_id, row->routine_syntax_id, row->step_syntax_id);
    mir_json_emit_str_or_null(out, row->step_name);
    fprintf(out,
        ",\"has_predecessor\":%s"
        ",\"predecessor_transition_id\":%" PRIu32
        ",\"predecessor_step_syntax_id\":%" PRIu32
        ",\"predecessor_step_name\":",
        row->has_predecessor ? "true" : "false",
        row->predecessor_transition_id,
        row->predecessor_step_syntax_id);
    mir_json_emit_str_or_null(out, row->predecessor_step_name);
    fprintf(out,
        ",\"action_syntax_id\":%" PRIu32
        ",\"outcome_instruction_block_id\":%zu"
        ",\"outcome_instruction_id\":%zu"
        ",\"outcome_result_name\":",
        row->action_syntax_id,
        row->outcome_instruction_block_id,
        row->outcome_instruction_id);
    mir_json_emit_str_or_null(out, row->outcome_result_name);
    fputs(",\"outcome_type_name\":", out);
    mir_json_emit_str_or_null(out, row->outcome_type_name);
    fputs(",\"outcome_enum_name\":", out);
    mir_json_emit_str_or_null(out, row->outcome_enum_name);
    fprintf(out,
        ",\"outcome_enum_syntax_id\":%" PRIu32
        ",\"branch_block_id\":%zu"
        ",\"branch_instruction_id\":%zu"
        ",\"success_variant_index\":%zu"
        ",\"success_variant_name\":",
        row->outcome_enum_syntax_id,
        row->branch_block_id,
        row->branch_instruction_id,
        row->success.variant_index);
    mir_json_emit_str_or_null(out, row->success.variant_name);
    fputs(",\"success_payload_name\":", out);
    mir_json_emit_str_or_null(out, row->success.payload_name);
    fputs(",\"success_payload_type_name\":", out);
    mir_json_emit_str_or_null(out, row->success.payload_type_name);
    fprintf(out,
        ",\"success_payload_decl_syntax_id\":%" PRIu32
        ",\"success_successor_block_id\":%zu"
        ",\"failure_variant_index\":%zu"
        ",\"failure_variant_name\":",
        row->success.payload_decl_syntax_id,
        row->success.successor_block_id,
        row->failure.variant_index);
    mir_json_emit_str_or_null(out, row->failure.variant_name);
    fputs(",\"failure_payload_name\":", out);
    mir_json_emit_str_or_null(out, row->failure.payload_name);
    fputs(",\"failure_payload_type_name\":", out);
    mir_json_emit_str_or_null(out, row->failure.payload_type_name);
    fprintf(out,
        ",\"failure_payload_decl_syntax_id\":%" PRIu32
        ",\"failure_successor_block_id\":%zu"
        ",\"completion_block_id\":%zu"
        ",\"completion_instruction_id\":%zu"
        ",\"compensations\":[",
        row->failure.payload_decl_syntax_id,
        row->failure.successor_block_id,
        row->completion_block_id,
        row->completion_instruction_id);
    for (size_t i = 0; i < row->compensation_count; i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_intent_compensation(out, &row->compensations[i]);
    }
    fputs("],\"where_zone_name\":", out);
    mir_json_emit_str_or_null(out, row->where_zone_name);
    fprintf(out, ",\"where_zone_syntax_id\":%u",
            row->where_zone_syntax_id);
    fputc('}', out);
}

static void
mir_json_emit_intent_terminal(
    FILE *out,
    const MIRIntentTerminalTransitionFact *row)
{
    fprintf(out,
        "{\"terminal_transition_id\":%" PRIu32
        ",\"routine_syntax_id\":%" PRIu32
        ",\"role\":",
        row->terminal_transition_id, row->routine_syntax_id);
    mir_json_emit_str(out, mir_intent_terminal_role_name(row->role));
    fprintf(out,
        ",\"source_transition_id\":%" PRIu32
        ",\"source_step_syntax_id\":%" PRIu32
        ",\"source_step_name\":",
        row->source_transition_id, row->source_step_syntax_id);
    mir_json_emit_str_or_null(out, row->source_step_name);
    fprintf(out, ",\"source_variant_index\":%zu,\"source_variant_name\":",
            row->source_variant_index);
    mir_json_emit_str_or_null(out, row->source_variant_name);
    fputs(",\"source_payload_name\":", out);
    mir_json_emit_str_or_null(out, row->source_payload_name);
    fputs(",\"source_payload_type_name\":", out);
    mir_json_emit_str_or_null(out, row->source_payload_type_name);
    fprintf(out,
        ",\"source_payload_decl_syntax_id\":%" PRIu32
        ",\"result_instruction_block_id\":%zu"
        ",\"result_instruction_id\":%zu"
        ",\"result_definition_name\":",
        row->source_payload_decl_syntax_id,
        row->result_instruction_block_id,
        row->result_instruction_id);
    mir_json_emit_str_or_null(out, row->result_definition_name);
    fputs(",\"result_type_name\":", out);
    mir_json_emit_str_or_null(out, row->result_type_name);
    fputs(",\"result_enum_name\":", out);
    mir_json_emit_str_or_null(out, row->result_enum_name);
    fprintf(out,
        ",\"result_enum_syntax_id\":%" PRIu32
        ",\"result_variant_index\":%zu"
        ",\"result_variant_name\":",
        row->result_enum_syntax_id,
        row->result_variant_index);
    mir_json_emit_str_or_null(out, row->result_variant_name);
    fputs(",\"result_payload_name\":", out);
    mir_json_emit_str_or_null(out, row->result_payload_name);
    fputs(",\"result_payload_type_name\":", out);
    mir_json_emit_str_or_null(out, row->result_payload_type_name);
    fprintf(out,
        ",\"result_payload_decl_syntax_id\":%" PRIu32
        ",\"expression_syntax_id\":%" PRIu32
        ",\"graph_root_id\":%zu"
        ",\"graph_digest\":%" PRIu32 "}",
        row->result_payload_decl_syntax_id,
        row->expression_syntax_id,
        row->graph_root_id,
        row->graph_digest);
}

void
mir_json_emit_intent_execution(FILE *out, const MIRProgram *mir)
{
    bool first = true;

    if (out == NULL || !mir_json_has_intent_execution(mir))
        return;
    fputs(",\"intent_execution\":{\"schema\":", out);
    mir_json_emit_str(out, PGY_MIR_INTENT_EXECUTION_SCHEMA);
    fprintf(out, ",\"plan_digest\":%" PRIu32 ",\"steps\":[",
            mir_intent_execution_program_digest(mir));
    for (size_t r = 0; r < mir->routine_count; r++) {
        const MIRRoutine *routine = &mir->routines[r];
        if (!mir_routine_has_admitted_intent_execution_plan(routine))
            continue;
        for (size_t i = 0; i < routine->intent_step_transition_count; i++) {
            if (!first)
                fputc(',', out);
            first = false;
            mir_json_emit_intent_step(
                out, &routine->intent_step_transitions[i]);
        }
    }
    fputs("],\"terminals\":[", out);
    first = true;
    for (size_t r = 0; r < mir->routine_count; r++) {
        const MIRRoutine *routine = &mir->routines[r];
        if (!mir_routine_has_admitted_intent_execution_plan(routine))
            continue;
        for (size_t i = 0;
             i < routine->intent_terminal_transition_count; i++) {
            if (!first)
                fputc(',', out);
            first = false;
            mir_json_emit_intent_terminal(
                out, &routine->intent_terminal_transitions[i]);
        }
    }
    fputs("]}", out);
}
