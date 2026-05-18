#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

bool
type_check_zone_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_zone = ctx->current_zone;
    const char *prev_module_path = ctx->current_module_path;
    const char *zone_name = ast_zone_name(node);
    ASTNode **slots;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t slot_count;
    size_t shared_count;
    size_t method_count;
    size_t authority_count;
    size_t mutation_rule_count;
    ctx->current_zone = node;
    if (node->origin_path != NULL)
        ctx->current_module_path = node->origin_path;
    slots = ast_zone_slots(node, &slot_count);
    shared_fields = ast_zone_shared_fields(node, &shared_count);
    methods = ast_zone_methods(node, &method_count);
    ast_zone_authorities(node, &authority_count);

    bool ok = type_check_overlay_decl_common(node, ctx,
        zone_name,
        SYMBOL_ZONE,
        shared_fields,
        shared_count,
        methods,
        method_count,
        "zone");

    ok = type_check_domain_slots(slots, slot_count, ctx, "Zone") && ok;
    ok = type_check_domain_slot_initializers(slots, slot_count, ctx, "zone") && ok;
    mutation_rule_count = type_check_zone_shape_warnings(node, ctx);
    type_check_zone_authorities(node, ctx);

    if (mutation_rule_count > 0 && authority_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has lifecycle-changing rules but no explicit authority set",
            zone_name);
    }

    type_check_zone_layer_slots(node, ctx);
    type_check_zone_lifecycle_mutations(node, ctx);
    type_check_zone_projection_rules(node, ctx);
    type_check_zone_lifecycle_maintenance(node, ctx);
    type_check_zone_state_aliases(node, ctx);
    ctx->current_zone = saved_zone;
    ctx->current_module_path = prev_module_path;
    return ok && !ctx->has_error;
}
