/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend zone relation bind-layer emission owner.
 */

#include "transpiler_overlay_zone_relation_bind.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_overlay_zone_bind.h"

void
emit_zone_bind_relation_layer(CodeBuf *out,
                              ASTNode *zone,
                              const char *layer_slot_name,
                              const char *left_slot_name,
                              const char *right_slot_name,
                              TranspilerCtx *ctx)
{
    ASTNode *layer_slot;
    ASTNode *relation_decl;
    ASTNode *left_target;
    ASTNode *right_target;

    if (out == NULL || zone == NULL || layer_slot_name == NULL
        || left_slot_name == NULL || right_slot_name == NULL || ctx == NULL) {
        return;
    }

    layer_slot = NULL;
    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(zone, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && ast_zone_layer_slot_is_relation(slot)
            && ast_zone_layer_slot_name(slot) != NULL
            && strcmp(ast_zone_layer_slot_name(slot), layer_slot_name) == 0) {
            layer_slot = slot;
            break;
        }
    }
    if (layer_slot == NULL)
        return;

    relation_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_RELATION_DECL, ast_zone_layer_slot_layer_type(layer_slot));
    if (relation_decl == NULL)
        return;

    size_t relation_slot_count = 0;
    ASTNode **relation_slots = ast_relation_slots(relation_decl,
        &relation_slot_count);
    size_t relation_refresh_count = 0;
    ASTNode **relation_refreshes =
        ast_relation_refreshes(relation_decl, &relation_refresh_count);
    left_target = find_nth_bindable_domain_slot_local(relation_slots,
        relation_slot_count, relation_refreshes, relation_refresh_count, 0);
    right_target = find_nth_bindable_domain_slot_local(relation_slots,
        relation_slot_count, relation_refreshes, relation_refresh_count, 1);
    if (left_target == NULL || right_target == NULL)
        return;
    const char *left_binding_name = ast_domain_slot_name(left_target);
    const char *right_binding_name = ast_domain_slot_name(right_target);
    if (left_binding_name == NULL || right_binding_name == NULL) {
        return;
    }

    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name, left_binding_name, left_slot_name);
    write_indent(ctx);
    codebuf_write(out, "self->%s.%s = self->%s;\n",
        layer_slot_name, right_binding_name, right_slot_name);
    for (size_t i = 0; i < relation_refresh_count; i++) {
        ASTNode *refresh = relation_refreshes[i];
        const char *projection_name;
        const char *source_name;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        projection_name = ast_zone_refresh_object_slot_name(refresh);
        source_name = ast_zone_refresh_source_slot_name(refresh);
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
        ast_relation_name(relation_decl),
        layer_slot_name);
}
