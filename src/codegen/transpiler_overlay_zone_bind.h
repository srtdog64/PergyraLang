#ifndef PGY_TRANSPILER_OVERLAY_ZONE_BIND_H
#define PGY_TRANSPILER_OVERLAY_ZONE_BIND_H

#include "parser/ast_api.h"

static ASTNode *
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

static void
emit_zone_bind_effect_layer(CodeBuf *out, ASTNode *zone, const char *layer_slot_name,
                            const char *target_slot_name, TranspilerCtx *ctx)
{
    ASTNode *layer_slot;
    ASTNode *effect_decl;
    ASTNode *target_slot;

    if (out == NULL || zone == NULL || layer_slot_name == NULL
        || target_slot_name == NULL || ctx == NULL) {
        return;
    }

    layer_slot = NULL;
    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(zone, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && !ast_zone_layer_slot_is_relation(slot)
            && ast_zone_layer_slot_name(slot) != NULL
            && strcmp(ast_zone_layer_slot_name(slot), layer_slot_name) == 0) {
            layer_slot = slot;
            break;
        }
    }
    if (layer_slot == NULL)
        return;

    effect_decl = find_effect_decl(ctx, ast_zone_layer_slot_layer_type(layer_slot));
    if (effect_decl == NULL)
        return;

    size_t effect_slot_count = 0;
    ASTNode **effect_slots = ast_effect_slots(effect_decl, &effect_slot_count);
    size_t effect_refresh_count = 0;
    ASTNode **effect_refreshes =
        ast_effect_refreshes(effect_decl, &effect_refresh_count);
    target_slot = find_nth_bindable_domain_slot_local(effect_slots,
        effect_slot_count, effect_refreshes, effect_refresh_count, 0);
    const char *target_binding_name = ast_domain_slot_name(target_slot);
    if (target_slot == NULL || target_binding_name == NULL)
        return;

    if (ast_zone_layer_slot_is_pool(layer_slot)) {
        write_indent(ctx);
        codebuf_write(out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(out, "%s _pgy_%s_instance = (%s){0};\n",
            ast_effect_name(effect_decl),
            layer_slot_name,
            ast_effect_name(effect_decl));
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
            ast_effect_name(effect_decl),
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
        ast_effect_name(effect_decl),
        layer_slot_name);
}

#endif
