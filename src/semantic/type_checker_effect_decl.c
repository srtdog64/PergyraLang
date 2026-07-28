#include "type_checker_internal.h"

bool
type_check_effect_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_effect = ctx->current_effect;
    ASTNode **slots;
    ASTNode **refreshes;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t slot_count;
    size_t refresh_count;
    size_t shared_count;
    size_t method_count;
    slots = ast_effect_slots(node, &slot_count);
    refreshes = ast_effect_refreshes(node, &refresh_count);
    shared_fields = ast_effect_shared_fields(node, &shared_count);
    methods = ast_effect_methods(node, &method_count);
    ctx->current_effect = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        ast_effect_name(node),
        SYMBOL_EFFECT,
        shared_fields,
        shared_count,
        methods,
        method_count,
        "effect");

    ok = type_check_domain_slots(slots, slot_count, ctx, "Effect") && ok;
    ok = type_check_domain_slot_initializers(slots, slot_count, ctx,
        "effect") && ok;
    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        if (refresh == NULL)
            continue;
        const char *action_name =
            ast_zone_refresh_derives_target_kind(refresh) ? "bind"
            : (ast_zone_refresh_requires_dto(refresh) ? "publish" : "refresh");
        ok = type_check_projection_contract(slots, slot_count, node, "Effect",
            ast_effect_name(node), refresh,
            ast_zone_refresh_object_slot_name(refresh),
            ast_zone_refresh_source_slot_name(refresh), ctx,
            action_name) && ok;
    }
    size_t target_count = count_bindable_domain_slots(
        slots,
        slot_count,
        refreshes,
        refresh_count);
    if (target_count == 0) {
        semantic_warning(ctx, node,
            "Effect '%s' should declare at least one target slot.\n"
            "Reason:\n"
            "- declaration propagation edge is effect '%s' -> target slot\n"
            "- effect contracts need a concrete target surface to apply/maintain against\n"
            "- without a target slot, effect propagation and authority checks stay underspecified\n"
            "- branch/join or zone/world propagation cannot preserve an effect target path without a bindable target slot\n"
            "Fix:\n"
            "- use 'for name: Type'\n"
            "- or declare a bindable subject/object target slot",
            ast_effect_name(node),
            ast_effect_name(node));
    } else if (target_count > 1) {
        semantic_warning(ctx, node,
            "Effect '%s' currently declares multiple target slots.\n"
            "Reason:\n"
            "- effect is currently treated as a focused layer on one target path\n"
            "- multiple targets usually mean relation or zone-state coordination instead\n"
            "Fix:\n"
            "- keep one effect target slot\n"
            "- or move the multi-target shape into relation/zone",
            ast_effect_name(node));
    }
    ctx->current_effect = saved_effect;
    return ok && !ctx->has_error;
}
