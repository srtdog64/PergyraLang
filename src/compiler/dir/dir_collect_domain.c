#include "dir_internal.h"
#include "parser/ast_api.h"

#include <stdint.h>
#include <string.h>

static ASTNode *
dir_domain_owner_slot(ASTNode *owner, const char *slot_name, bool layer)
{
    ASTNode **slots = NULL;
    size_t slot_count = 0;

    if (owner == NULL || slot_name == NULL)
        return NULL;
    if (layer) {
        if (owner->type != AST_ZONE_DECL)
            return NULL;
        slots = ast_zone_layer_slots(owner, &slot_count);
        for (size_t i = 0; slots != NULL && i < slot_count; i++) {
            if (slots[i] != NULL
                && ast_zone_layer_slot_name(slots[i]) != NULL
                && strcmp(ast_zone_layer_slot_name(slots[i]), slot_name) == 0) {
                return slots[i];
            }
        }
        return NULL;
    }

    switch (owner->type) {
    case AST_ZONE_DECL:
        slots = ast_zone_slots(owner, &slot_count);
        break;
    case AST_RELATION_DECL:
        slots = ast_relation_slots(owner, &slot_count);
        break;
    case AST_EFFECT_DECL:
        slots = ast_effect_slots(owner, &slot_count);
        break;
    default:
        return NULL;
    }
    for (size_t i = 0; slots != NULL && i < slot_count; i++) {
        if (slots[i] != NULL && ast_domain_slot_name(slots[i]) != NULL
            && strcmp(ast_domain_slot_name(slots[i]), slot_name) == 0) {
            return slots[i];
        }
    }
    return NULL;
}

static uint32_t
dir_domain_owner_slot_syntax_id(ASTNode *owner,
                                const char *slot_name,
                                bool layer)
{
    ASTNode *slot = dir_domain_owner_slot(owner, slot_name, layer);
    return slot != NULL ? ast_node_stable_id(slot) : 0;
}

static bool
dir_add_projection_contract_edges(DIRProgram *dir,
                                  size_t owner_id,
                                  const char *owner_name,
                                  ASTNode *refresh)
{
    DIRDomainTopologyRow topology = {0};
    ASTNode *owner;
    ssize_t projection_slot_id;
    ssize_t source_slot_id;
    const char *source_name;

    if (dir == NULL || owner_name == NULL || refresh == NULL || refresh->type != AST_ZONE_REFRESH)
        return false;

    projection_slot_id = dir_find_slot_node(dir,
                                            DIR_NODE_PROJECTION_SLOT,
                                            owner_name,
                                            ast_zone_refresh_object_slot_name(refresh));
    if (projection_slot_id < 0)
        return dir_failf(
            dir,
            "DIR projection contract for '%s' is missing target projection slot '%s'.\n"
            "Reason:\n"
            "- refresh/publish/bind declared a projection target that was not materialized as a DIR projection slot\n"
            "Fix:\n"
            "- declare the projection slot on '%s' before lowering\n"
            "- or fix the projection target name so DIR can resolve it",
            owner_name,
            ast_zone_refresh_object_slot_name(refresh) != NULL
                ? ast_zone_refresh_object_slot_name(refresh) : "(unnamed)",
            owner_name);

    source_name = ast_zone_refresh_source_slot_name(refresh);
    source_slot_id = dir_find_slot_node(dir,
                                        DIR_NODE_ZONE_SLOT,
                                        owner_name,
                                        source_name);
    if (source_slot_id < 0) {
        source_slot_id = dir_find_slot_node(dir,
                                            DIR_NODE_PROJECTION_SLOT,
                                            owner_name,
                                            source_name);
    }
    if (source_slot_id < 0)
        return dir_failf(
            dir,
            "DIR projection contract for '%s' is missing source slot '%s'.\n"
            "Reason:\n"
            "- refresh/publish/bind declared a source that was not materialized as a DIR zone/projection slot\n"
            "Fix:\n"
            "- declare the source slot on '%s' before lowering\n"
            "- or fix the projection source name so DIR can resolve it",
            owner_name,
            source_name != NULL ? source_name : "(unnamed)",
            owner_name);

    if (!dir_add_named_edge(dir,
                            DIR_EDGE_PROJECTION_SLOT_SOURCE,
                            (size_t)projection_slot_id,
                            source_slot_id >= 0 ? (size_t)source_slot_id : SIZE_MAX,
                            ast_zone_refresh_derives_target_kind(refresh)
                                ? "bind"
                                : (ast_zone_refresh_requires_dto(refresh)
                                       ? "publish"
                                       : "refresh"),
                            source_name))
        return false;

    if (owner_id != SIZE_MAX) {
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_OWNER_HAS_PROJECTION_SLOT,
                                owner_id,
                                (size_t)projection_slot_id,
                                ast_zone_refresh_object_slot_name(refresh),
                                dir->nodes[(size_t)projection_slot_id].name))
            return false;
    }

    owner = owner_id < dir->node_count ? dir->nodes[owner_id].ast : NULL;
    topology.owner_node_id = owner_id;
    topology.owner_source_syntax_id = owner_id < dir->node_count
        ? dir->nodes[owner_id].source_syntax_id : 0;
    topology.source_syntax_id = ast_node_stable_id(refresh);
    topology.kind = ast_zone_refresh_derives_target_kind(refresh)
        ? DIR_DOMAIN_TOPOLOGY_PROJECTION_BIND
        : (ast_zone_refresh_requires_dto(refresh)
            ? DIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH
            : DIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH);
    topology.projection_slot_name =
        ast_zone_refresh_object_slot_name(refresh);
    topology.projection_slot_source_syntax_id =
        dir_domain_owner_slot_syntax_id(
            owner, topology.projection_slot_name, false);
    topology.source_slot_name = source_name;
    topology.source_slot_source_syntax_id =
        dir_domain_owner_slot_syntax_id(
            owner, topology.source_slot_name, false);
    topology.participant_slot_name =
        ast_zone_refresh_participant_slot_name(refresh);
    topology.participant_slot_source_syntax_id =
        dir_domain_owner_slot_syntax_id(
            owner, topology.participant_slot_name, false);
    if (!dir_add_domain_topology_row(dir, topology))
        return false;

    return true;
}

static bool
dir_add_zone_effect_topology(DIRProgram *dir,
                             size_t owner_id,
                             ASTNode *zone,
                             ASTNode *directive,
                             DIRDomainTopologyKind kind)
{
    DIRDomainTopologyRow row = {0};

    if (dir == NULL || zone == NULL || directive == NULL
        || (kind != DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT
            && kind != DIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT)
        || owner_id >= dir->node_count)
        return false;
    row.owner_node_id = owner_id;
    row.owner_source_syntax_id = dir->nodes[owner_id].source_syntax_id;
    row.source_syntax_id = ast_node_stable_id(directive);
    row.kind = kind;
    row.layer_slot_name = ast_zone_effect_slot_name(directive);
    row.layer_slot_source_syntax_id = dir_domain_owner_slot_syntax_id(
        zone, row.layer_slot_name, true);
    row.target_slot_name = ast_zone_effect_target_slot_name(directive);
    row.target_slot_source_syntax_id = dir_domain_owner_slot_syntax_id(
        zone, row.target_slot_name, false);
    row.participant_slot_name =
        ast_zone_directive_participant_slot_name(directive);
    row.participant_slot_source_syntax_id = dir_domain_owner_slot_syntax_id(
        zone, row.participant_slot_name, false);
    return dir_add_domain_topology_row(dir, row);
}

static bool
dir_add_zone_link_topology(DIRProgram *dir,
                           size_t owner_id,
                           ASTNode *zone,
                           ASTNode *directive)
{
    DIRDomainTopologyRow row = {0};

    if (dir == NULL || zone == NULL || directive == NULL
        || owner_id >= dir->node_count)
        return false;
    row.owner_node_id = owner_id;
    row.owner_source_syntax_id = dir->nodes[owner_id].source_syntax_id;
    row.source_syntax_id = ast_node_stable_id(directive);
    row.kind = DIR_DOMAIN_TOPOLOGY_LINK_RELATION;
    row.layer_slot_name = ast_zone_relation_slot_name(directive);
    row.layer_slot_source_syntax_id = dir_domain_owner_slot_syntax_id(
        zone, row.layer_slot_name, true);
    row.left_slot_name = ast_zone_relation_left_slot_name(directive);
    row.left_slot_source_syntax_id = dir_domain_owner_slot_syntax_id(
        zone, row.left_slot_name, false);
    row.right_slot_name = ast_zone_relation_right_slot_name(directive);
    row.right_slot_source_syntax_id = dir_domain_owner_slot_syntax_id(
        zone, row.right_slot_name, false);
    row.participant_slot_name =
        ast_zone_directive_participant_slot_name(directive);
    row.participant_slot_source_syntax_id = dir_domain_owner_slot_syntax_id(
        zone, row.participant_slot_name, false);
    return dir_add_domain_topology_row(dir, row);
}

bool
dir_collect_zone_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    size_t slot_count = 0;
    ASTNode **slots = ast_zone_slots(node, &slot_count);
    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(node, &layer_slot_count);
    size_t authority_count = 0;
    ASTNode **authorities = ast_zone_authorities(node, &authority_count);
    size_t refresh_count = 0;
    ASTNode **refreshes = ast_zone_refreshes(node, &refresh_count);
    size_t state_count = 0;
    ASTNode **states = ast_zone_states(node, &state_count);
    size_t apply_count = 0;
    ASTNode **applies = ast_zone_applies(node, &apply_count);
    size_t maintained_effect_count = 0;
    ASTNode **maintained_effects = ast_zone_maintained_effects(
        node, &maintained_effect_count);
    size_t link_count = 0;
    ASTNode **links = ast_zone_links(node, &link_count);

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *slot_name = ast_domain_slot_name(slot);
        const char *target = type_name(dir, ast_domain_slot_type(slot));
        ssize_t to = dir_find_type_node_by_name(dir, target);
        ssize_t slot_node_id;
        bool is_projection = dir_domain_slot_is_projection(slot);
        DIRNodeKind slot_kind = is_projection ? DIR_NODE_PROJECTION_SLOT : DIR_NODE_ZONE_SLOT;

        slot_node_id = dir_ensure_qualified_slot_node(dir,
                                                      slot_kind,
                                                      ast_zone_name(node),
                                                      slot_name,
                                                      ast_node_stable_id(node),
                                                      slot);
        if (slot_node_id < 0)
            return false;
        if (!dir_add_named_edge(dir,
                                is_projection
                                    ? DIR_EDGE_OWNER_HAS_PROJECTION_SLOT
                                    : DIR_EDGE_ZONE_HAS_SLOT,
                                from_id,
                                (size_t)slot_node_id,
                                slot_name,
                                dir->nodes[(size_t)slot_node_id].name))
            return false;
        if (!dir_add_named_edge(dir, DIR_EDGE_ZONE_SLOT_TYPE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot_name,
                                target))
            return false;
        if (!dir_add_named_edge(dir,
                                is_projection
                                    ? DIR_EDGE_PROJECTION_SLOT_TYPE
                                    : DIR_EDGE_ZONE_SLOT_TYPE,
                                (size_t)slot_node_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot_name,
                                target))
            return false;
    }
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        ssize_t to = ast_zone_layer_slot_is_relation(slot)
            ? dir_find_relation_node_by_name(dir, ast_zone_layer_slot_layer_type(slot))
            : dir_find_effect_node_by_name(dir, ast_zone_layer_slot_layer_type(slot));
        if (!dir_add_named_edge(dir, DIR_EDGE_ZONE_LAYER_TYPE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                ast_zone_layer_slot_name(slot),
                                ast_zone_layer_slot_layer_type(slot)))
            return false;
    }
    for (size_t i = 0; i < authority_count; i++) {
        ASTNode *auth = authorities[i];
        const char *subject_slot = ast_zone_authority_subject_slot_name(auth);
        ssize_t auth_slot_id = dir_ensure_qualified_slot_node(dir,
                                                              DIR_NODE_AUTHORITY_SLOT,
                                                              ast_zone_name(node),
                                                              subject_slot,
                                                              ast_node_stable_id(node),
                                                              auth);
        ssize_t subject_slot_id = dir_find_slot_node(dir,
                                                     DIR_NODE_ZONE_SLOT,
                                                     ast_zone_name(node),
                                                     subject_slot);
        if (auth_slot_id < 0)
            return false;
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_ZONE_HAS_AUTHORITY_SLOT,
                                from_id,
                                (size_t)auth_slot_id,
                                subject_slot,
                                dir->nodes[(size_t)auth_slot_id].name))
            return false;
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_AUTHORITY_SLOT_SUBJECT,
                                (size_t)auth_slot_id,
                                subject_slot_id >= 0 ? (size_t)subject_slot_id : SIZE_MAX,
                                subject_slot,
                                subject_slot))
            return false;
        for (size_t j = 0; j < ast_zone_authority_ability_count(auth); j++) {
            ASTNode *ability_ref = ast_zone_authority_required_ability(auth, j);
            const char *ability_name = ast_type_name(ability_ref);
            if (ability_name == NULL)
                continue;
            ssize_t to = dir_find_ability_node_by_name(dir, ability_name);
            if (!dir_add_named_edge(dir, DIR_EDGE_ZONE_AUTHORITY_ABILITY, from_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    subject_slot,
                                    ability_name))
                return false;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_ZONE_AUTHORITY_ABILITY,
                                    (size_t)auth_slot_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    subject_slot,
                                    ability_name))
                return false;
        }
    }
    for (size_t i = 0; i < refresh_count; i++) {
        if (!dir_add_projection_contract_edges(dir,
                                               from_id,
                                               ast_zone_name(node),
                                               refreshes[i]))
            return false;
    }
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        const char *layer = ast_zone_state_layer_slot_name(state);
        ssize_t to = -1;
        for (size_t j = 0; j < layer_slot_count; j++) {
            ASTNode *slot = layer_slots[j];
            if (slot != NULL
                && strcmp(ast_zone_layer_slot_name(slot), layer) == 0) {
                to = (ssize_t)from_id;
                break;
            }
        }
        if (!dir_add_named_edge(dir, DIR_EDGE_ZONE_STATE_LAYER, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                ast_zone_state_name(state),
                                layer))
            return false;
    }
    for (size_t i = 0; i < apply_count; i++) {
        if (ast_zone_effect_slot_name(applies[i]) == NULL
            || ast_zone_effect_target_slot_name(applies[i]) == NULL) {
            return false;
        }
        if (!dir_add_zone_effect_topology(
                dir, from_id, node, applies[i],
                DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT)) {
            return false;
        }
    }
    for (size_t i = 0; i < maintained_effect_count; i++) {
        if (!dir_add_zone_effect_topology(
                dir, from_id, node, maintained_effects[i],
                DIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT)) {
            return false;
        }
    }
    for (size_t i = 0; i < link_count; i++) {
        if (!dir_add_zone_link_topology(dir, from_id, node, links[i]))
            return false;
    }
    return true;
}

bool
dir_collect_relation_effect_slot_edges(DIRProgram *dir,
                                       size_t from_id,
                                       const char *owner_name,
                                       uint32_t owner_source_syntax_id,
                                       ASTNode **slots,
                                       size_t slot_count,
                                       ASTNode **refreshes,
                                       size_t refresh_count)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *target;
        ssize_t to;
        ssize_t slot_node_id;
        bool is_projection;
        DIRNodeKind slot_kind;
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        target = type_name(dir, ast_domain_slot_type(slot));
        to = dir_find_type_node_by_name(dir, target);
        is_projection = dir_domain_slot_is_projection(slot);
        slot_kind = is_projection ? DIR_NODE_PROJECTION_SLOT : DIR_NODE_ZONE_SLOT;
        slot_node_id = dir_ensure_qualified_slot_node(dir,
                                                      slot_kind,
                                                      owner_name,
                                                      ast_domain_slot_name(slot),
                                                      owner_source_syntax_id,
                                                      slot);
        if (slot_node_id < 0)
            return false;
        if (is_projection) {
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_OWNER_HAS_PROJECTION_SLOT,
                                    from_id,
                                    (size_t)slot_node_id,
                                    ast_domain_slot_name(slot),
                                    dir->nodes[(size_t)slot_node_id].name))
                return false;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_PROJECTION_SLOT_TYPE,
                                    (size_t)slot_node_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    ast_domain_slot_name(slot),
                                    target))
                return false;
        } else {
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_ZONE_SLOT_TYPE,
                                    (size_t)slot_node_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    ast_domain_slot_name(slot),
                                    target))
                return false;
        }
    }

    for (size_t i = 0; i < refresh_count; i++) {
        if (!dir_add_projection_contract_edges(dir, from_id, owner_name, refreshes[i]))
            return false;
    }

    return true;
}
