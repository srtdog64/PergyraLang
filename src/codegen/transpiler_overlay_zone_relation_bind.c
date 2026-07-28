/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend zone relation bind-layer emission owner.
 */

#include "transpiler_overlay_zone_relation_bind.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_provenance_emit.h"
#include "transpiler_overlay_zone_bind.h"
#include "transpiler_projection.h"

void
emit_zone_bind_relation_layer(CodeBuf *out,
                              ASTNode *zone,
                              const char *layer_slot_name,
                              const char *left_slot_name,
                              const char *right_slot_name,
                              TranspilerCtx *ctx)
{
    ASTNode *relation_decl;
    const char *relation_name;
    const char *relation_type_name;
    const char *left_binding_name;
    const char *right_binding_name;
    const PgyDomainParticipantRoleFact *source_fact;
    const PgyDomainParticipantRoleFact *target_fact;

    if (out == NULL || zone == NULL || layer_slot_name == NULL
        || left_slot_name == NULL || right_slot_name == NULL || ctx == NULL) {
        return;
    }

    if (!transpiler_find_zone_layer_slot_local(ctx, zone, layer_slot_name,
            true, &relation_type_name, NULL)) {
        return;
    }

    relation_decl = transpiler_find_named_decl_local(
        ctx, AST_RELATION_DECL, relation_type_name);
    if (relation_decl == NULL)
        return;
    relation_name = transpiler_decl_name_local(relation_decl);
    if (relation_name == NULL)
        return;

    TranspilerHostedZoneRefreshView relation_refresh_view =
        transpiler_hosted_zone_refresh_view_from_decl(ctx, relation_name,
                                                      relation_decl);
    TranspilerHostedDomainSlotView relation_slot_view =
        transpiler_hosted_domain_slot_view_from_decl(ctx, relation_name,
                                                     relation_decl);
    if (transpiler_hosted_zone_refresh_view_missing_mir_metadata(
            &relation_refresh_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone relation bind refresh metadata for '%s'",
            relation_name);
        return;
    }
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
            &relation_slot_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone relation bind domain-slot metadata for '%s'",
            relation_name);
        return;
    }
    source_fact = transpiler_require_domain_participant_role_fact(
        ctx, relation_name, PGY_DOMAIN_PARTICIPANT_RELATION_SOURCE);
    target_fact = transpiler_require_domain_participant_role_fact(
        ctx, relation_name, PGY_DOMAIN_PARTICIPANT_RELATION_TARGET);
    if (source_fact == NULL || target_fact == NULL) {
        return;
    }
    left_binding_name = source_fact->field_name;
    right_binding_name = target_fact->field_name;

    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name, left_binding_name, left_slot_name);
    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name, right_binding_name, right_slot_name);
    for (size_t i = 0; i < relation_refresh_view.count; i++) {
        const char *projection_name;
        const char *source_name;

        projection_name =
            transpiler_hosted_zone_refresh_view_object_slot_name(
                &relation_refresh_view, i);
        source_name =
            transpiler_hosted_zone_refresh_view_source_slot_name(
                &relation_refresh_view, i);
        if (projection_name == NULL || source_name == NULL)
            continue;
        if (strcmp(source_name, left_binding_name) != 0
            && strcmp(source_name, right_binding_name) != 0) {
            continue;
        }
        write_indent(ctx);
        codebuf_write(out,
            "self->%s.__projection_dirty_%s = true;\n",
            layer_slot_name, projection_name);
        write_indent(ctx);
        codebuf_write(out,
            "self->%s.__projection_ready_%s = false;\n",
            layer_slot_name, projection_name);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(&self->%s);\n",
        relation_name,
        layer_slot_name);
}
