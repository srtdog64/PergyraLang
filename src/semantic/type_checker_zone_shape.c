#include "type_checker_internal.h"

static const char *ZONE_SHAPE_SUBJECT_HEAVY_WARNING =
    "Zone '%s' declares %llu subject slots; if some entries are passive business data, model them as object/vessel support state instead of authority-bearing subjects";

static const char *ZONE_SHAPE_NO_PASSIVE_OBJECT_WARNING =
    "Zone '%s' has multiple subject slots but no object slots; consider a zone-first shape where passive support state uses objects/vessels and only state-transition actors remain subjects";

static const char *ZONE_SHAPE_NO_SUBJECT_MUTATION_WARNING =
    "Zone '%s' mutates state or declares authority but has no subject slot";

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
            ZONE_SHAPE_SUBJECT_HEAVY_WARNING,
            node->data.zone_decl.name,
            (unsigned long long) subject_count);
    }

    if (subject_count > 1 && object_count == 0) {
        semantic_warning(ctx, node,
            ZONE_SHAPE_NO_PASSIVE_OBJECT_WARNING,
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
            ZONE_SHAPE_NO_SUBJECT_MUTATION_WARNING,
            node->data.zone_decl.name);
    }

    return mutation_rule_count;
}
