#include "dir_internal.h"

#include <stdint.h>
#include <string.h>

static bool
dir_add_projection_contract_edges(DIRProgram *dir,
                                  size_t owner_id,
                                  const char *owner_name,
                                  ASTNode *refresh)
{
    ssize_t projection_slot_id;
    ssize_t source_slot_id;
    const char *source_name;

    if (dir == NULL || owner_name == NULL || refresh == NULL || refresh->type != AST_ZONE_REFRESH)
        return false;

    projection_slot_id = dir_find_slot_node(dir,
                                            DIR_NODE_PROJECTION_SLOT,
                                            owner_name,
                                            refresh->data.zone_refresh.object_slot_name);
    if (projection_slot_id < 0)
        return dir_failf(
            "DIR projection contract for '%s' is missing target projection slot '%s'.\n"
            "Reason:\n"
            "- refresh/publish/bind declared a projection target that was not materialized as a DIR projection slot\n"
            "Fix:\n"
            "- declare the projection slot on '%s' before lowering\n"
            "- or fix the projection target name so DIR can resolve it",
            owner_name,
            refresh->data.zone_refresh.object_slot_name != NULL
                ? refresh->data.zone_refresh.object_slot_name : "(unnamed)",
            owner_name);

    source_name = refresh->data.zone_refresh.source_slot_name;
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
                            refresh->data.zone_refresh.derive_target_kind
                                ? "bind"
                                : (refresh->data.zone_refresh.requires_dto
                                       ? "publish"
                                       : "refresh"),
                            source_name))
        return false;

    if (owner_id != SIZE_MAX) {
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_OWNER_HAS_PROJECTION_SLOT,
                                owner_id,
                                (size_t)projection_slot_id,
                                refresh->data.zone_refresh.object_slot_name,
                                dir->nodes[(size_t)projection_slot_id].name))
            return false;
    }

    return true;
}

bool
dir_collect_zone_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.slots[i];
        const char *target = type_name(dir, slot->data.domain_slot.type);
        ssize_t to = dir_find_type_node_by_name(dir, target);
        ssize_t slot_node_id;
        bool is_projection = dir_domain_slot_is_projection(slot);
        DIRNodeKind slot_kind = is_projection ? DIR_NODE_PROJECTION_SLOT : DIR_NODE_ZONE_SLOT;

        slot_node_id = dir_ensure_qualified_slot_node(dir,
                                                      slot_kind,
                                                      node->data.zone_decl.name,
                                                      slot->data.domain_slot.slot_name,
                                                      slot);
        if (slot_node_id < 0)
            return false;
        if (!dir_add_named_edge(dir,
                                is_projection
                                    ? DIR_EDGE_OWNER_HAS_PROJECTION_SLOT
                                    : DIR_EDGE_ZONE_HAS_SLOT,
                                from_id,
                                (size_t)slot_node_id,
                                slot->data.domain_slot.slot_name,
                                dir->nodes[(size_t)slot_node_id].name))
            return false;
        if (!dir_add_named_edge(dir, DIR_EDGE_ZONE_SLOT_TYPE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot->data.domain_slot.slot_name,
                                target))
            return false;
        if (!dir_add_named_edge(dir,
                                is_projection
                                    ? DIR_EDGE_PROJECTION_SLOT_TYPE
                                    : DIR_EDGE_ZONE_SLOT_TYPE,
                                (size_t)slot_node_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot->data.domain_slot.slot_name,
                                target))
            return false;
    }
    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        ssize_t to = slot->data.zone_layer_slot.is_relation
            ? dir_find_relation_node_by_name(dir, slot->data.zone_layer_slot.layer_type)
            : dir_find_effect_node_by_name(dir, slot->data.zone_layer_slot.layer_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_ZONE_LAYER_TYPE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot->data.zone_layer_slot.slot_name,
                                slot->data.zone_layer_slot.layer_type))
            return false;
    }
    for (size_t i = 0; i < node->data.zone_decl.authority_count; i++) {
        ASTNode *auth = node->data.zone_decl.authorities[i];
        ssize_t auth_slot_id = dir_ensure_qualified_slot_node(dir,
                                                              DIR_NODE_AUTHORITY_SLOT,
                                                              node->data.zone_decl.name,
                                                              auth->data.zone_authority.subject_slot_name,
                                                              auth);
        ssize_t subject_slot_id = dir_find_slot_node(dir,
                                                     DIR_NODE_ZONE_SLOT,
                                                     node->data.zone_decl.name,
                                                     auth->data.zone_authority.subject_slot_name);
        if (auth_slot_id < 0)
            return false;
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_ZONE_HAS_AUTHORITY_SLOT,
                                from_id,
                                (size_t)auth_slot_id,
                                auth->data.zone_authority.subject_slot_name,
                                dir->nodes[(size_t)auth_slot_id].name))
            return false;
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_AUTHORITY_SLOT_SUBJECT,
                                (size_t)auth_slot_id,
                                subject_slot_id >= 0 ? (size_t)subject_slot_id : SIZE_MAX,
                                auth->data.zone_authority.subject_slot_name,
                                auth->data.zone_authority.subject_slot_name))
            return false;
        for (size_t j = 0; j < auth->data.zone_authority.ability_count; j++) {
            ASTNode *ability_ref = auth->data.zone_authority.required_abilities[j];
            const char *ability_name = (ability_ref != NULL && ability_ref->type == AST_TYPE)
                ? ability_ref->data.type.name : NULL;
            if (ability_name == NULL)
                continue;
            ssize_t to = dir_find_ability_node_by_name(dir, ability_name);
            if (!dir_add_named_edge(dir, DIR_EDGE_ZONE_AUTHORITY_ABILITY, from_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    auth->data.zone_authority.subject_slot_name,
                                    ability_name))
                return false;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_ZONE_AUTHORITY_ABILITY,
                                    (size_t)auth_slot_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    auth->data.zone_authority.subject_slot_name,
                                    ability_name))
                return false;
        }
    }
    for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++) {
        if (!dir_add_projection_contract_edges(dir,
                                               from_id,
                                               node->data.zone_decl.name,
                                               node->data.zone_decl.refreshes[i]))
            return false;
    }
    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        const char *layer = state->data.zone_state.layer_slot_name;
        ssize_t to = -1;
        for (size_t j = 0; j < node->data.zone_decl.layer_slot_count; j++) {
            ASTNode *slot = node->data.zone_decl.layer_slots[j];
            if (slot != NULL
                && strcmp(slot->data.zone_layer_slot.slot_name, layer) == 0) {
                to = (ssize_t)from_id;
                break;
            }
        }
        if (!dir_add_named_edge(dir, DIR_EDGE_ZONE_STATE_LAYER, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                state->data.zone_state.state_name,
                                layer))
            return false;
    }
    return true;
}

bool
dir_collect_relation_effect_slot_edges(DIRProgram *dir,
                                       size_t from_id,
                                       const char *owner_name,
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
        target = type_name(dir, slot->data.domain_slot.type);
        to = dir_find_type_node_by_name(dir, target);
        is_projection = dir_domain_slot_is_projection(slot);
        slot_kind = is_projection ? DIR_NODE_PROJECTION_SLOT : DIR_NODE_ZONE_SLOT;
        slot_node_id = dir_ensure_qualified_slot_node(dir,
                                                      slot_kind,
                                                      owner_name,
                                                      slot->data.domain_slot.slot_name,
                                                      slot);
        if (slot_node_id < 0)
            return false;
        if (is_projection) {
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_OWNER_HAS_PROJECTION_SLOT,
                                    from_id,
                                    (size_t)slot_node_id,
                                    slot->data.domain_slot.slot_name,
                                    dir->nodes[(size_t)slot_node_id].name))
                return false;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_PROJECTION_SLOT_TYPE,
                                    (size_t)slot_node_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    slot->data.domain_slot.slot_name,
                                    target))
                return false;
        } else {
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_ZONE_SLOT_TYPE,
                                    (size_t)slot_node_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    slot->data.domain_slot.slot_name,
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
