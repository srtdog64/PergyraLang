#include "mir_json_dump_domain_topology.h"

#include "mir_decl_headers.h"
#include "mir_domain_topology.h"
#include "mir_json_dump_internal.h"

#include <inttypes.h>
#include <stdbool.h>

static bool
mir_json_domain_topology_required(const MIRProgram *mir)
{
    if (mir == NULL)
        return false;
    if (mir->domain_topology_row_count > 0)
        return true;
    for (size_t i = 0; i < mir->decl_header_count; i++) {
        ASTNodeType ast_type = mir_decl_header_ast_type_or(
            &mir->decl_headers[i], AST_PROGRAM);
        if (ast_type == AST_ZONE_DECL
            || ast_type == AST_EFFECT_DECL
            || ast_type == AST_RELATION_DECL) {
            return true;
        }
    }
    return false;
}

static void
mir_json_emit_topology_name_id(FILE *out, const char *field,
                               const char *name, uint32_t source_id)
{
    fputs(",\"", out);
    fputs(field, out);
    fputs("_name\":", out);
    mir_json_emit_str_or_null(out, name);
    fputs(",\"", out);
    fputs(field, out);
    fprintf(out, "_source_syntax_id\":%" PRIu32, source_id);
}

static void
mir_json_emit_domain_topology_row(FILE *out,
                                  const MIRDomainTopologyRow *row)
{
    fputs("{\"owner_name\":", out);
    mir_json_emit_str_or_null(out, row->owner_name);
    fprintf(out,
            ",\"owner_source_syntax_id\":%" PRIu32
            ",\"source_syntax_id\":%" PRIu32 ",\"kind\":",
            row->owner_source_syntax_id, row->source_syntax_id);
    mir_json_emit_str(out, mir_domain_topology_kind_name(row->kind));
    mir_json_emit_topology_name_id(
        out, "projection_slot", row->projection_slot_name,
        row->projection_slot_source_syntax_id);
    mir_json_emit_topology_name_id(
        out, "source_slot", row->source_slot_name,
        row->source_slot_source_syntax_id);
    mir_json_emit_topology_name_id(
        out, "layer_slot", row->layer_slot_name,
        row->layer_slot_source_syntax_id);
    mir_json_emit_topology_name_id(
        out, "target_slot", row->target_slot_name,
        row->target_slot_source_syntax_id);
    mir_json_emit_topology_name_id(
        out, "left_slot", row->left_slot_name,
        row->left_slot_source_syntax_id);
    mir_json_emit_topology_name_id(
        out, "right_slot", row->right_slot_name,
        row->right_slot_source_syntax_id);
    mir_json_emit_topology_name_id(
        out, "participant_slot", row->participant_slot_name,
        row->participant_slot_source_syntax_id);
    fputc('}', out);
}

void
mir_json_emit_domain_topology(FILE *out, const MIRProgram *mir)
{
    if (!mir_json_domain_topology_required(mir))
        return;
    fprintf(out, ",\"domain_topology\":{\"domain_graph_id\":%" PRIu64
                 ",\"rows\":[",
            mir->domain_graph_id);
    for (size_t i = 0; i < mir->domain_topology_row_count; i++) {
        if (i > 0)
            fputc(',', out);
        mir_json_emit_domain_topology_row(out, &mir->domain_topology_rows[i]);
    }
    fputs("]}", out);
}
