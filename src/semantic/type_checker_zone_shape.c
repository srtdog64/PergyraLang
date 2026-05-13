#include "type_checker_internal.h"

static const char *ZONE_SHAPE_SUBJECT_HEAVY_WARNING =
    "Zone '%s' declares %llu subject slots; if some entries are passive business data, model them as object/vessel support state instead of identity-bearing state-transition subjects";

static const char *ZONE_SHAPE_NO_PASSIVE_OBJECT_WARNING =
    "Zone '%s' has multiple subject slots but no object slots; consider a zone-first shape where passive support state uses objects/vessels and only state-transition actors remain subjects";

static const char *ZONE_SHAPE_NO_SUBJECT_MUTATION_WARNING =
    "Zone '%s' mutates state or declares authority but has no subject slot";

size_t
type_check_zone_shape_warnings(ASTNode *node, SemanticContext *ctx)
{
    ASTNode **slots;
    size_t slot_count;
    size_t refresh_count;
    size_t apply_count;
    size_t link_count;
    size_t detach_count;
    size_t unlink_count;
    size_t maintained_effect_count;
    size_t maintained_relation_count;
    size_t maintained_state_count;
    size_t authority_count;
    size_t subject_count;
    size_t object_count;
    size_t mutation_rule_count;

    slots = ast_zone_slots(node, &slot_count);
    ast_zone_refreshes(node, &refresh_count);
    ast_zone_applies(node, &apply_count);
    ast_zone_links(node, &link_count);
    ast_zone_detaches(node, &detach_count);
    ast_zone_unlinks(node, &unlink_count);
    ast_zone_maintained_effects(node, &maintained_effect_count);
    ast_zone_maintained_relations(node, &maintained_relation_count);
    ast_zone_maintained_states(node, &maintained_state_count);
    ast_zone_authorities(node, &authority_count);
    subject_count = count_subject_domain_slots(slots, slot_count);
    object_count = count_object_domain_slots(slots, slot_count);

    if (subject_count > 4) {
        semantic_warning(ctx, node,
            ZONE_SHAPE_SUBJECT_HEAVY_WARNING,
            ast_zone_name(node),
            (unsigned long long) subject_count);
    }

    if (subject_count > 1 && object_count == 0) {
        semantic_warning(ctx, node,
            ZONE_SHAPE_NO_PASSIVE_OBJECT_WARNING,
            ast_zone_name(node));
    }

    mutation_rule_count = apply_count
        + link_count
        + detach_count
        + unlink_count
        + refresh_count
        + maintained_effect_count
        + maintained_relation_count
        + maintained_state_count;

    if (subject_count == 0
        && (mutation_rule_count > 0 || authority_count > 0)) {
        semantic_warning(ctx, node,
            ZONE_SHAPE_NO_SUBJECT_MUTATION_WARNING,
            ast_zone_name(node));
    }

    return mutation_rule_count;
}
