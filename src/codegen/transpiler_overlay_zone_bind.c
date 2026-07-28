/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend zone effect bind-layer emission owner.
 */

#include "transpiler_overlay_zone_bind.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_provenance_emit.h"
#include "transpiler_projection.h"

bool
transpiler_find_zone_layer_slot_local(TranspilerCtx *ctx,
                                      ASTNode *zone,
                                      const char *layer_slot_name,
                                      bool is_relation,
                                      const char **layer_type_out,
                                      bool *is_pool_out)
{
    const char *zone_name;
    TranspilerHostedZoneLayerSlotView layer_view;

    if (layer_type_out != NULL)
        *layer_type_out = NULL;
    if (is_pool_out != NULL)
        *is_pool_out = false;
    if (ctx == NULL || zone == NULL || layer_slot_name == NULL)
        return false;

    zone_name = transpiler_decl_name_local(zone);
    layer_view = transpiler_hosted_zone_layer_slot_view_from_decl(
        ctx, zone_name, zone);
    if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone layer-slot bind metadata for '%s'",
            zone_name != NULL ? zone_name : "(anonymous-zone)");
        return false;
    }

    for (size_t i = 0; i < layer_view.count; i++) {
        const char *candidate_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (transpiler_hosted_zone_layer_slot_view_is_relation(
                &layer_view, i) == is_relation
            && candidate_name != NULL
            && strcmp(candidate_name, layer_slot_name) == 0) {
            if (layer_type_out != NULL) {
                *layer_type_out =
                    transpiler_hosted_zone_layer_slot_view_type_name(
                        &layer_view, i);
            }
            if (is_pool_out != NULL) {
                *is_pool_out =
                    transpiler_hosted_zone_layer_slot_view_is_pool(
                        &layer_view, i);
            }
            return true;
        }
    }

    return false;
}

void
emit_zone_bind_effect_layer(CodeBuf *out, ASTNode *zone,
                            const char *layer_slot_name,
                            const char *target_slot_name, TranspilerCtx *ctx)
{
    ASTNode *effect_decl;
    const char *effect_name;
    const char *effect_type_name;
    const char *target_binding_name;
    const PgyDomainParticipantRoleFact *bearer_fact;
    bool layer_is_pool = false;

    if (out == NULL || zone == NULL || layer_slot_name == NULL
        || target_slot_name == NULL || ctx == NULL) {
        return;
    }

    if (!transpiler_find_zone_layer_slot_local(ctx, zone, layer_slot_name,
            false, &effect_type_name, &layer_is_pool)) {
        return;
    }

    effect_decl = transpiler_find_named_decl_local(
        ctx, AST_EFFECT_DECL, effect_type_name);
    if (effect_decl == NULL)
        return;
    effect_name = transpiler_decl_name_local(effect_decl);
    if (effect_name == NULL)
        return;

    TranspilerHostedZoneRefreshView effect_refresh_view =
        transpiler_hosted_zone_refresh_view_from_decl(ctx, effect_name,
                                                      effect_decl);
    TranspilerHostedDomainSlotView effect_slot_view =
        transpiler_hosted_domain_slot_view_from_decl(ctx, effect_name,
                                                     effect_decl);
    if (transpiler_hosted_zone_refresh_view_missing_mir_metadata(
            &effect_refresh_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone effect bind refresh metadata for '%s'",
            effect_name);
        return;
    }
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(
            &effect_slot_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing zone effect bind domain-slot metadata for '%s'",
            effect_name);
        return;
    }
    bearer_fact = transpiler_require_domain_participant_role_fact(
        ctx, effect_name, PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER);
    if (bearer_fact == NULL)
        return;
    target_binding_name = bearer_fact->field_name;

    if (layer_is_pool) {
        write_indent_to(out, ctx->indent);
        codebuf_write(out, "{\n");
        ctx->indent++;
        write_indent_to(out, ctx->indent);
        codebuf_write(out, "%s _pgy_%s_instance = (%s){0};\n",
            effect_name,
            layer_slot_name,
            effect_name);
        write_indent_to(out, ctx->indent);
        codebuf_write(out, "_pgy_%s_instance.%s = self->%s;\n",
            layer_slot_name,
            target_binding_name,
            target_slot_name);
        for (size_t i = 0; i < effect_refresh_view.count; i++) {
            const char *projection_name;
            const char *source_name;

            projection_name =
                transpiler_hosted_zone_refresh_view_object_slot_name(
                    &effect_refresh_view, i);
            source_name =
                transpiler_hosted_zone_refresh_view_source_slot_name(
                    &effect_refresh_view, i);
            if (projection_name == NULL || source_name == NULL
                || strcmp(source_name, target_binding_name) != 0) {
                continue;
            }
            write_indent_to(out, ctx->indent);
            codebuf_write(out,
                "_pgy_%s_instance.__projection_dirty_%s = true;\n",
                layer_slot_name, projection_name);
            write_indent_to(out, ctx->indent);
            codebuf_write(out,
                "_pgy_%s_instance.__projection_ready_%s = false;\n",
                layer_slot_name, projection_name);
        }
        write_indent_to(out, ctx->indent);
        codebuf_write(out, "%s_sync(&_pgy_%s_instance);\n",
            effect_name,
            layer_slot_name);
        write_indent_to(out, ctx->indent);
        codebuf_write(out, "PGY_EFFECT_POOL_APPLY(self->%s, _pgy_%s_instance);\n",
            layer_slot_name,
            layer_slot_name);
        write_indent_to(out, ctx->indent);
        codebuf_write(out, "self->__layer_active_%s = PGY_EFFECT_POOL_ACTIVE_COUNT(self->%s) > 0;\n",
            layer_slot_name,
            layer_slot_name);
        ctx->indent--;
        write_indent_to(out, ctx->indent);
        codebuf_write(out, "}\n");
        return;
    }

    write_indent_to(out, ctx->indent);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name,
        target_binding_name,
        target_slot_name);
    for (size_t i = 0; i < effect_refresh_view.count; i++) {
        const char *projection_name;
        const char *source_name;

        projection_name =
            transpiler_hosted_zone_refresh_view_object_slot_name(
                &effect_refresh_view, i);
        source_name =
            transpiler_hosted_zone_refresh_view_source_slot_name(
                &effect_refresh_view, i);
        if (projection_name == NULL || source_name == NULL
            || strcmp(source_name, target_binding_name) != 0) {
            continue;
        }
        write_indent_to(out, ctx->indent);
        codebuf_write(out,
            "self->%s.__projection_dirty_%s = true;\n",
            layer_slot_name, projection_name);
        write_indent_to(out, ctx->indent);
        codebuf_write(out,
            "self->%s.__projection_ready_%s = false;\n",
            layer_slot_name, projection_name);
    }
    write_indent_to(out, ctx->indent);
    codebuf_write(out, "%s_sync(&self->%s);\n",
        effect_name,
        layer_slot_name);
}
