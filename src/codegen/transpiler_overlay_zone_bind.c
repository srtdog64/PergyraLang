/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend zone effect bind-layer emission owner.
 */

#include "transpiler_overlay_zone_bind.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"

ASTNode *
find_nth_bindable_domain_slot_local(ASTNode **slots, size_t slot_count,
                                    ASTNode **refreshes, size_t refresh_count,
                                    size_t nth)
{
    size_t seen = 0;
    (void)refreshes;
    (void)refresh_count;

    if (slots == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !ast_domain_slot_is_binding(slot)) {
            continue;
        }
        if (seen == nth)
            return slot;
        seen++;
    }

    return NULL;
}

bool
transpiler_find_zone_layer_slot_local(TranspilerCtx *ctx,
                                      ASTNode *zone,
                                      const char *layer_slot_name,
                                      bool is_relation,
                                      ASTNode **slot_out,
                                      const char **layer_type_out)
{
    const char *zone_name;
    TranspilerHostedZoneLayerSlotView layer_view;

    if (slot_out != NULL)
        *slot_out = NULL;
    if (layer_type_out != NULL)
        *layer_type_out = NULL;
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
        ASTNode *slot =
            transpiler_hosted_zone_layer_slot_view_source_ast(&layer_view, i);
        const char *candidate_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && ast_zone_layer_slot_is_relation(slot) == is_relation
            && candidate_name != NULL
            && strcmp(candidate_name, layer_slot_name) == 0) {
            if (slot_out != NULL)
                *slot_out = slot;
            if (layer_type_out != NULL) {
                *layer_type_out =
                    transpiler_hosted_zone_layer_slot_view_type_name(
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
    ASTNode *layer_slot;
    ASTNode *effect_decl;
    ASTNode *target_slot;
    const char *effect_name;
    const char *effect_type_name;

    if (out == NULL || zone == NULL || layer_slot_name == NULL
        || target_slot_name == NULL || ctx == NULL) {
        return;
    }

    if (!transpiler_find_zone_layer_slot_local(ctx, zone, layer_slot_name,
            false, &layer_slot, &effect_type_name)) {
        return;
    }

    effect_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_EFFECT_DECL, effect_type_name);
    if (effect_decl == NULL)
        return;
    effect_name = transpiler_decl_name_local(effect_decl);
    if (effect_name == NULL)
        return;

    size_t effect_slot_count = 0;
    ASTNode **effect_slots = ast_effect_slots(effect_decl, &effect_slot_count);
    size_t effect_refresh_count = 0;
    ASTNode **effect_refreshes =
        ast_effect_refreshes(effect_decl, &effect_refresh_count);
    target_slot = find_nth_bindable_domain_slot_local(effect_slots,
        effect_slot_count, effect_refreshes, effect_refresh_count, 0);
    if (target_slot == NULL)
        return;
    const char *target_binding_name = ast_domain_slot_name(target_slot);
    if (target_binding_name == NULL)
        return;

    if (ast_zone_layer_slot_is_pool(layer_slot)) {
        write_indent(ctx);
        codebuf_write(out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(out, "%s _pgy_%s_instance = (%s){0};\n",
            effect_name,
            layer_slot_name,
            effect_name);
        write_indent(ctx);
        codebuf_write(out, "_pgy_%s_instance.%s = self->%s;\n",
            layer_slot_name,
            target_binding_name,
            target_slot_name);
        for (size_t i = 0; i < effect_refresh_count; i++) {
            ASTNode *refresh = effect_refreshes[i];
            const char *projection_name;
            const char *source_name;

            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            projection_name = ast_zone_refresh_object_slot_name(refresh);
            source_name = ast_zone_refresh_source_slot_name(refresh);
            if (projection_name == NULL || source_name == NULL
                || strcmp(source_name, target_binding_name) != 0) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(out,
                "_pgy_%s_instance.__projection_dirty_%s = true;\n",
                layer_slot_name, projection_name);
            write_indent(ctx);
            codebuf_write(out,
                "_pgy_%s_instance.__projection_ready_%s = false;\n",
                layer_slot_name, projection_name);
        }
        write_indent(ctx);
        codebuf_write(out, "%s_sync(&_pgy_%s_instance);\n",
            effect_name,
            layer_slot_name);
        write_indent(ctx);
        codebuf_write(out, "PGY_EFFECT_POOL_APPLY(self->%s, _pgy_%s_instance);\n",
            layer_slot_name,
            layer_slot_name);
        write_indent(ctx);
        codebuf_write(out, "self->__layer_active_%s = PGY_EFFECT_POOL_ACTIVE_COUNT(self->%s) > 0;\n",
            layer_slot_name,
            layer_slot_name);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(out, "}\n");
        return;
    }

    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name,
        target_binding_name,
        target_slot_name);
    for (size_t i = 0; i < effect_refresh_count; i++) {
        ASTNode *refresh = effect_refreshes[i];
        const char *projection_name;
        const char *source_name;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        projection_name = ast_zone_refresh_object_slot_name(refresh);
        source_name = ast_zone_refresh_source_slot_name(refresh);
        if (projection_name == NULL || source_name == NULL
            || strcmp(source_name, target_binding_name) != 0) {
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
        effect_name,
        layer_slot_name);
}
