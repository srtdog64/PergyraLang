#include "type_checker_internal.h"

bool
type_check_effect_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_effect = ctx->current_effect;
    ctx->current_effect = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.effect_decl.name,
        SYMBOL_EFFECT,
        node->data.effect_decl.shared_fields,
        node->data.effect_decl.shared_count,
        node->data.effect_decl.methods,
        node->data.effect_decl.method_count,
        "effect");

    ok = type_check_domain_slots(node->data.effect_decl.slots,
        node->data.effect_decl.slot_count, ctx, "Effect") && ok;
    ok = type_check_domain_slot_initializers(node->data.effect_decl.slots,
        node->data.effect_decl.slot_count, ctx, "effect") && ok;
    for (size_t i = 0; i < node->data.effect_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.effect_decl.refreshes[i];
        if (refresh == NULL)
            continue;
        const char *action_name =
            refresh->data.zone_refresh.derive_target_kind ? "bind"
            : (refresh->data.zone_refresh.requires_dto ? "publish" : "refresh");
        ok = type_check_projection_contract(node->data.effect_decl.slots,
            node->data.effect_decl.slot_count, "Effect",
            node->data.effect_decl.name, refresh,
            refresh->data.zone_refresh.object_slot_name,
            refresh->data.zone_refresh.source_slot_name, ctx,
            action_name) && ok;
    }
    size_t target_count = count_bindable_domain_slots(
        node->data.effect_decl.slots,
        node->data.effect_decl.slot_count,
        node->data.effect_decl.refreshes,
        node->data.effect_decl.refresh_count);
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
            node->data.effect_decl.name,
            node->data.effect_decl.name);
    } else if (target_count > 1) {
        semantic_warning(ctx, node,
            "Effect '%s' currently declares multiple target slots.\n"
            "Reason:\n"
            "- effect is currently treated as a focused layer on one target path\n"
            "- multiple targets usually mean relation or zone-state coordination instead\n"
            "Fix:\n"
            "- keep one effect target slot\n"
            "- or move the multi-target shape into relation/zone",
            node->data.effect_decl.name);
    }
    ctx->current_effect = saved_effect;
    return ok && !ctx->has_error;
}
