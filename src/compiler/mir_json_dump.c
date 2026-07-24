#include "mir.h"
#include "mir_abi_layout.h"
#include "mir_decl_headers.h"
#include "mir_json_dump_flow.h"
#include "mir_json_dump_decl.h"
#include "mir_json_expression_graph.h"
#include "mir_json_generic_method_specialization.h"
#include "mir_json_dump_internal.h"
#include "mir_json_dump_runtime_abi.h"
#include "mir_parallel_capture_facts.h"

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"

/* --- Lossless MIR JSON serialization (schema pgy.mir.v1) ------------------- */

void
mir_json_emit_str(FILE *out, const char *s)
{
    fputc('"', out);
    if (s != NULL) {
        for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
            switch (*p) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:
                if (*p < 0x20)
                    fprintf(out, "\\u%04x", (unsigned)*p);
                else
                    fputc((int)*p, out);
                break;
            }
        }
    }
    fputc('"', out);
}

void
mir_json_emit_str_or_null(FILE *out, const char *s)
{
    if (s == NULL)
        fputs("null", out);
    else
        mir_json_emit_str(out, s);
}

static void
mir_json_emit_expr_or_null(FILE *out, ASTNode *expr)
{
    char *text;

    if (expr == NULL) {
        fputs("null", out);
        return;
    }
    text = ast_capture_inline(expr);
    mir_json_emit_str_or_null(out, text);
    free(text);
}

static void
mir_json_emit_defer_body(FILE *out, ASTNode *body)
{
    size_t count = ast_block_statement_count(body);

    fputs(",\"defer_body\":[", out);
    for (size_t i = 0; i < count; i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_expr_or_null(out, ast_block_statement(body, i));
    }
    fputc(']', out);
}

static const char *
mir_json_identifier_name(ASTNode *node)
{
    if (node == NULL || node->type != AST_IDENTIFIER)
        return NULL;
    return ast_identifier_name(node);
}

static void
mir_json_emit_match_variant_facts(FILE *out, const MIRInstruction *inst)
{
    ASTNode *pattern;
    ASTNode *callee;
    const char *variant = NULL;

    fputs(",\"match_variant\":", out);
    if (inst == NULL || mir_instruction_match_pattern_count(inst) != 1) {
        fputs("null,\"match_bindings\":[],\"match_binding_types\":[]", out);
        return;
    }

    pattern = mir_instruction_match_pattern_at(inst, 0);
    if (pattern != NULL && pattern->type == AST_CALL) {
        callee = ast_call_callee(pattern);
        variant = mir_json_identifier_name(callee);
    } else {
        const char *name = mir_json_identifier_name(pattern);
        if (name != NULL && strcmp(name, "None") == 0)
            variant = name;
    }

    if (variant == NULL) {
        fputs("null,\"match_bindings\":[],\"match_binding_types\":[]", out);
        return;
    }

    mir_json_emit_str(out, variant);
    fputs(",\"match_bindings\":[", out);
    if (pattern != NULL && pattern->type == AST_CALL) {
        size_t emitted = 0;
        for (size_t i = 0; i < ast_call_arg_count(pattern); i++) {
            const char *binding =
                mir_json_identifier_name(ast_call_argument(pattern, i));
            if (binding == NULL)
                continue;
            if (emitted > 0)
                fputc(',', out);
            mir_json_emit_str(out, binding);
            emitted++;
        }
    }
    fputc(']', out);
    fputs(",\"match_binding_types\":[", out);
    for (size_t i = 0; i < inst->match_binding_type_count; i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_str_or_null(
            out, mir_instruction_match_binding_type_at(inst, i));
    }
    fputc(']', out);
}


static void
mir_json_emit_routine_signature(FILE *out, const MIRRoutine *routine)
{
    if (!mir_routine_has_signature(routine))
        return;
    fputs(",\"generics\":[", out);
    for (size_t g = 0; g < mir_routine_generic_param_count(routine); g++) {
        if (g > 0)
            fputc(',', out);
        mir_json_emit_str_or_null(out,
            mir_routine_generic_param_name(routine, g));
    }
    fputs("],\"params\":[", out);
    for (size_t p = 0; p < mir_routine_param_count(routine); p++) {
        FuncParam *fp = mir_routine_param(routine, p);
        if (p > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        mir_json_emit_str_or_null(out, fp != NULL ? fp->name : NULL);
        fputs(",\"type\":", out);
        mir_json_emit_str_or_null(out,
            mir_routine_param_type_name(routine, p));
        fputs(",\"carriage\":", out);
        mir_json_emit_str(out, mir_param_carriage_name(
            mir_routine_param_carriage(routine, p)));
        fputs(",\"resource\":", out);
        mir_json_emit_str(out, mir_param_resource_kind_name(
            mir_routine_param_resource_kind(routine, p)));
        fputs(",\"pass\":", out);
        mir_json_emit_str(out,
            mir_routine_param_passes_indirect(routine, p)
                ? "indirect"
                : "direct");
        fputc('}', out);
    }
    fputs("],\"return\":", out);
    mir_json_emit_str_or_null(out, mir_routine_return_type_name(routine));
}

/* ABI layout is a MIR-owned fact.  Carry the complete static row across the
 * JSON boundary instead of asking a self-hosted consumer to rediscover it
 * from the source type or a process-local ABI table.  The numeric
 * representation is intentional: it is the enum value hashed by
 * mir_abi_layout_id, so the wire row remains lossless and target-neutral. */
static void
mir_json_emit_instruction_abi_layout(FILE *out, const MIRInstruction *inst)
{
    const MIRTypeLayout *layout = inst != NULL ? inst->type_layout : NULL;
    const char *abi_type_name = inst != NULL && inst->abi_type_name != NULL
        ? inst->abi_type_name
        : layout != NULL ? layout->abi_type_name : NULL;

    fputs(",\"abi_type_name\":", out);
    mir_json_emit_str_or_null(out, abi_type_name);
    fprintf(out, ",\"abi_layout_id\":%u,\"abi_layout_required\":%s",
            inst != NULL ? inst->abi_layout_id : 0,
            layout != NULL ? "true" : "false");
    fputs(",\"abi_layout\":", out);
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

static void
mir_json_emit_instruction(FILE *out, const MIRInstruction *inst)
{
    fprintf(out, "{\"id\":%zu,\"kind\":", inst->id);
    mir_json_emit_str(out, mir_inst_kind_name(inst->kind));
    fputs(",\"name\":", out);
    mir_json_emit_str_or_null(out, inst->name);
    fputs(",\"result\":", out);
    mir_json_emit_str_or_null(out, inst->result_name);
    fputs(",\"arg0\":", out);
    mir_json_emit_str_or_null(out, inst->arg0);
    fputs(",\"arg1\":", out);
    mir_json_emit_str_or_null(out, inst->arg1);
    fputs(",\"slot_anchor\":", out);
    mir_json_emit_str_or_null(out, inst->slot_anchor);
    mir_json_emit_instruction_abi_layout(out, inst);
    fputs(",\"machine_layer\":", out);
    if (inst->machine_layer_fact_present) {
        fputs("{\"operation\":", out);
        mir_json_emit_str(out,
            rir_machine_contact_kind_name(inst->machine_contact_kind));
        fputs(",\"manifest\":", out);
        mir_json_emit_str_or_null(out, inst->machine_layer_manifest_id);
        fputs(",\"physical_grant\":", out);
        mir_json_emit_str_or_null(out, inst->machine_layer_physical_grant_id);
        fprintf(out, ",\"physical_base\":%" PRIu64
                     ",\"physical_size\":%" PRIu64
                     ",\"physical_mode\":",
                inst->machine_layer_physical_base,
                inst->machine_layer_physical_size);
        mir_json_emit_str_or_null(out, inst->machine_layer_physical_mode);
        fputs(",\"runtime_operation\":", out);
        mir_json_emit_str_or_null(out, inst->machine_layer_runtime_operation);
        fprintf(out, ",\"hardware_adequate\":%s,\"authority_required\":%s,\"live_lease_required\":%s}",
            inst->machine_layer_hardware_adequate ? "true" : "false",
            inst->machine_layer_authority_required ? "true" : "false",
            inst->machine_layer_live_lease_required ? "true" : "false");
    } else {
        fputs("null", out);
    }
    /* Keep the semantic contact kind explicit even when the rich owner fact
     * is null.  Self-host MIR consumers can therefore reject a missing
     * machine-layer object instead of guessing from expression spelling. */
    fputs(",\"machine_contact_kind\":", out);
    if (rir_machine_contact_kind_is_present(inst->machine_contact_kind)) {
        mir_json_emit_str(out,
            rir_machine_contact_kind_name(inst->machine_contact_kind));
    } else {
        fputs("null", out);
    }
    fputs(",\"expr0\":", out);
    mir_json_emit_expr_or_null(out, inst->expr0);
    fputs(",\"expr0_graph\":", out);
    mir_json_emit_instruction_expression_graph(out, inst, 0);
    fputs(",\"expr1\":", out);
    mir_json_emit_expr_or_null(out, inst->expr1);
    fputs(",\"expr1_graph\":", out);
    mir_json_emit_instruction_expression_graph(out, inst, 1);
    if (!mir_json_emit_instruction_runtime_abi(out, inst)
        && inst->text_builder_runtime_row != NULL) {
        const MIRTextBuilderRuntimeRow *row =
            inst->text_builder_runtime_row;
        fputs(",\"runtime_call_abi\":{\"owner\":\"TextBuilder\",\"source\":", out);
        mir_json_emit_str_or_null(out, row->source_name);
        fputs(",\"operation\":", out);
        mir_json_emit_str_or_null(out, row->operation);
        fputs(",\"c_symbol\":", out);
        mir_json_emit_str_or_null(out, row->c_inline_fn);
        fputs(",\"llvm_symbol\":", out);
        mir_json_emit_str_or_null(out, row->llvm_export_fn);
        fputs(",\"c_shape\":", out);
        mir_json_emit_str(out,
            mir_text_builder_call_shape_name(row->c_call_shape));
        fputs(",\"llvm_shape\":", out);
        mir_json_emit_str(out,
            mir_text_builder_call_shape_name(row->llvm_call_shape));
        fputc('}', out);
    }
    fputs(",\"speculation\":", out);
    if (inst->has_speculation_safety_fact) {
        fprintf(out, "{\"pure\":%s,\"non_trapping\":%s}",
            inst->speculation_is_pure ? "true" : "false",
            inst->speculation_is_non_trapping ? "true" : "false");
    } else {
        fputs("null", out);
    }
    fputs(",\"source_type\":", out);
    mir_json_emit_str_or_null(out,
        mir_instruction_source_inline_text(inst) != NULL
            ? mir_source_node_type_name((ASTNodeType)
                  mir_instruction_source_node_type_or(inst, AST_PROGRAM))
            : NULL);
    if (mir_instruction_source_is_defer_stmt(inst))
        mir_json_emit_defer_body(out, inst->expr0);
    fputs(",\"match_patterns\":[", out);
    for (size_t p = 0; p < mir_instruction_match_pattern_count(inst); p++) {
        if (p > 0)
            fputc(',', out);
        mir_json_emit_expr_or_null(out,
            mir_instruction_match_pattern_at(inst, p));
    }
    fputc(']', out);
    mir_json_emit_match_variant_facts(out, inst);
    fputs(",\"destructure_element_type\":", out);
    mir_json_emit_str_or_null(out, inst->destructure_element_type_name);
    fputs(",\"destructure_bindings\":[", out);
    for (size_t d = 0; d < mir_instruction_destructure_binding_count(inst);
         d++) {
        if (d > 0)
            fputc(',', out);
        mir_json_emit_str_or_null(out,
            mir_instruction_destructure_binding_name_at(inst, d));
    }
    fputc(']', out);
    fputs(",\"uses\":[", out);
    for (size_t m = 0; m < inst->use_count; m++) {
        if (m > 0)
            fputc(',', out);
        mir_json_emit_str(out, inst->uses[m]);
    }
    fputs("],\"ast\":", out);
    mir_json_emit_str_or_null(out, mir_instruction_source_inline_text(inst));
    fputc('}', out);
}

static void
mir_json_emit_block(FILE *out, const MIRBasicBlock *block, size_t index)
{
    fprintf(out, "{\"id\":%zu,\"reachable\":%s,\"instructions\":[",
            index, block->is_reachable ? "true" : "false");
    for (size_t k = 0;
         k < block->instruction_count && block->instructions != NULL; k++) {
        if (k > 0)
            fputc(',', out);
        mir_json_emit_instruction(out, &block->instructions[k]);
    }
    fputs("]", out);
    if (block->has_succ_true)
        fprintf(out, ",\"succ_true\":%zu", block->succ_true);
    if (block->has_succ_false)
        fprintf(out, ",\"succ_false\":%zu", block->succ_false);
    fputc('}', out);
}

static void
mir_json_emit_source_locals(FILE *out, const MIRRoutine *routine)
{
    fputs(",\"source_locals\":[", out);
    for (size_t s = 0; s < mir_routine_source_local_type_count(routine);
         s++) {
        if (s > 0)
            fputc(',', out);
        fputs("{\"name\":", out);
        mir_json_emit_str_or_null(out,
            mir_routine_source_local_name_at(routine, s));
        fputs(",\"type\":", out);
        mir_json_emit_str_or_null(out,
            mir_routine_source_local_type_name_at(routine, s));
        fputc('}', out);
    }
    fputc(']', out);
}

static void
mir_json_emit_routine(FILE *out, const MIRRoutine *routine)
{
    fputs("{\"name\":", out);
    mir_json_emit_str_or_null(out, routine->name);
    fputs(",\"kind\":", out);
    mir_json_emit_str(out, mir_scope_kind_name(routine->kind));
    fprintf(out, ",\"source_syntax_id\":%u,\"function_param_flow_summary_count\":%zu",
            routine->source_syntax_id,
            routine->function_param_flow_summary_count);
    fputs(",\"function_param_flow_summaries\":[", out);
    for (size_t i = 0; i < routine->function_param_flow_summary_count; i++) {
        if (i > 0)
            fputc(',', out);
        fprintf(out, "{\"parameter_index\":%zu,\"mask\":%u}",
                routine->function_param_flow_summaries[i].parameter_index,
                routine->function_param_flow_summaries[i].mask);
    }
    fputc(']', out);
    if (routine->owner_name != NULL) {
        fputs(",\"owner\":", out);
        mir_json_emit_str(out, routine->owner_name);
    }
    mir_json_emit_routine_signature(out, routine);
    mir_json_emit_resource_flow_symbols(out, routine);
    mir_json_emit_loop_flow_facts(out, routine);
    mir_json_emit_iteration_type_facts(out, routine);
    mir_json_emit_destructure_type_facts(out, routine);
    fputs(",\"blocks\":[", out);
    for (size_t j = 0;
         j < routine->block_count && routine->blocks != NULL; j++) {
        if (j > 0)
            fputc(',', out);
        mir_json_emit_block(out, &routine->blocks[j], j);
    }
    fputc(']', out);
    mir_json_emit_source_locals(out, routine);
    fputc('}', out);
}

static void
mir_json_emit_routines(FILE *out, const MIRProgram *mir)
{
    if (mir == NULL || mir->routines == NULL)
        return;
    for (size_t i = 0; i < mir->routine_count; i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_routine(out, &mir->routines[i]);
    }
}

static const char *
mir_parallel_capture_kind_name(MIRParallelCaptureDispositionKind kind)
{
    switch (kind) {
    case MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY:
        return "snapshot_copy";
    case MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT:
        return "join_index_disjoint";
    case MIR_PARALLEL_CAPTURE_JOIN_READONLY:
        return "join_readonly";
    }
    return "unknown";
}

static void
mir_json_emit_parallel_capture_boundaries(FILE *out, const MIRProgram *mir)
{
    size_t count = mir_parallel_capture_boundary_count(mir);

    for (size_t i = 0; i < count; i++) {
        const MIRParallelCaptureBoundaryFact *boundary =
            mir_parallel_capture_boundary_at(mir, i);
        if (i > 0)
            fputc(',', out);
        fprintf(out,
            "{\"source_stable_id\":%u,\"task_count\":%zu,\"sealed\":%s,\"rows\":[",
            boundary->source_stable_id,
            boundary->task_count,
            boundary->sealed ? "true" : "false");
        for (size_t j = 0; j < boundary->row_count; j++) {
            const MIRParallelCaptureDispositionRow *row = &boundary->rows[j];
            if (j > 0)
                fputc(',', out);
            fputs("{\"name\":", out);
            mir_json_emit_str_or_null(out, row->name);
            fputs(",\"kind\":", out);
            mir_json_emit_str(out,
                mir_parallel_capture_kind_name(row->kind));
            fprintf(out, ",\"writer_task\":%zu}", row->writer_task);
        }
        fputs("]}", out);
    }
}

void
mir_dump_json(const MIRProgram *mir, FILE *out)
{
    if (out == NULL)
        out = stdout;
    fputs("{\"schema\":\"pgy.mir.v1\",\"decls\":[", out);
    mir_json_emit_decls(out, mir);
    fputs("],\"parallel_capture_boundaries\":[", out);
    mir_json_emit_parallel_capture_boundaries(out, mir);
    fputs("],\"generic_method_specializations\":[", out);
    mir_json_emit_generic_method_specializations(out, mir);
    fputs("],\"routines\":[", out);
    mir_json_emit_routines(out, mir);
    fputs("]}\n", out);
}
