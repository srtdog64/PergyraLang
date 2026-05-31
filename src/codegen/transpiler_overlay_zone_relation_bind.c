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
    ASTNode *relation_decl;
    ASTNode *left_target;
    ASTNode *right_target;
    const char *relation_name;
    const char *relation_type_name;

    if (out == NULL || zone == NULL || layer_slot_name == NULL
        || left_slot_name == NULL || right_slot_name == NULL || ctx == NULL) {
        return;
    }

    if (!transpiler_find_zone_layer_slot_local(ctx, zone, layer_slot_name,
            true, NULL, &relation_type_name)) {
        return;
    }

    relation_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_RELATION_DECL, relation_type_name);
    if (relation_decl == NULL)
        return;
    relation_name = transpiler_decl_name_local(relation_decl);
    if (relation_name == NULL)
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
        relation_name,
        layer_slot_name);
}
