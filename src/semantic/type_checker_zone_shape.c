#include "type_checker_internal.h"

size_t
type_check_zone_shape_warnings(ASTNode *node, SemanticContext *ctx)
{
    size_t subject_count = count_subject_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count);
    size_t object_count = count_object_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count);
    size_t mutation_rule_count;

    if (subject_count > 4) {
        semantic_warning(ctx, node,
            "Zone '%s' declares %llu subject slots; prefer keeping active subjects to 4 or fewer and model supporting state as objects",
            node->data.zone_decl.name,
            (unsigned long long) subject_count);
    }

    if (subject_count > 1 && object_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has multiple subject slots but no object slots; consider modeling passive support state as objects",
            node->data.zone_decl.name);
    }

    mutation_rule_count = node->data.zone_decl.apply_count
        + node->data.zone_decl.link_count
        + node->data.zone_decl.detach_count
        + node->data.zone_decl.unlink_count
        + node->data.zone_decl.refresh_count
        + node->data.zone_decl.maintained_effect_count
        + node->data.zone_decl.maintained_relation_count
        + node->data.zone_decl.maintained_state_count;

    if (subject_count == 0
        && (mutation_rule_count > 0 || node->data.zone_decl.authority_count > 0)) {
        semantic_warning(ctx, node,
            "Zone '%s' mutates state or declares authority but has no subject slot",
            node->data.zone_decl.name);
    }

    return mutation_rule_count;
}
