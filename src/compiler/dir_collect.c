#include "dir_internal.h"

#include <stdint.h>
#include <string.h>

static ASTNode *
dir_find_ability_decl_ast(ASTNode *program, const char *name)
{
    if (program == NULL || name == NULL || program->type != AST_PROGRAM)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt != NULL
            && stmt->type == AST_ABILITY_DECL
            && stmt->data.ability_decl.name != NULL
            && strcmp(stmt->data.ability_decl.name, name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static bool
dir_impl_has_method_named(ASTNode *impl, const char *method_name)
{
    if (impl == NULL || impl->type != AST_IMPL_ABILITY || method_name == NULL)
        return false;

    for (size_t i = 0; i < impl->data.impl_ability.method_count; i++) {
        ASTNode *method = impl->data.impl_ability.methods[i];
        if (method != NULL
            && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return true;
        }
    }
    return false;
}

bool
dir_collect_nodes(DIRProgram *dir, ASTNode *program)
{
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *node = program->data.program.statements[i];
        switch (node->type) {
            case AST_CLASS_DECL:
                if (!dir_add_node(dir, DIR_NODE_TYPE, node->data.class_decl.name, node))
                    return false;
                break;
            case AST_TYPE_ALIAS:
                if (!dir_add_node(dir, DIR_NODE_TYPE, node->data.type_alias.name, node))
                    return false;
                break;
            case AST_ENUM_DECL:
                if (!dir_add_node(dir, DIR_NODE_TYPE, node->data.enum_decl.name, node))
                    return false;
                break;
            case AST_ABILITY_DECL:
                if (!dir_add_node(dir, DIR_NODE_ABILITY, node->data.ability_decl.name, node))
                    return false;
                break;
            case AST_ROLE_DECL:
                if (!dir_add_node(dir, DIR_NODE_ROLE, node->data.role_decl.name, node))
                    return false;
                break;
            case AST_PARTY_DECL:
                if (!dir_add_node(dir, DIR_NODE_PARTY, node->data.party_decl.name, node))
                    return false;
                break;
            case AST_ROSTER_DECL:
                if (!dir_add_node(dir, DIR_NODE_SYSTEMIC, node->data.roster_decl.name, node))
                    return false;
                break;
            case AST_WORLD_DECL:
                if (!dir_add_node(dir, DIR_NODE_WORLD, node->data.world_decl.name, node))
                    return false;
                break;
            case AST_RELATION_DECL:
                if (!dir_add_node(dir, DIR_NODE_RELATION, node->data.relation_decl.name, node))
                    return false;
                break;
            case AST_EFFECT_DECL:
                if (!dir_add_node(dir, DIR_NODE_EFFECT, node->data.effect_decl.name, node))
                    return false;
                break;
            case AST_ZONE_DECL:
                if (!dir_add_node(dir, DIR_NODE_ZONE, node->data.zone_decl.name, node))
                    return false;
                break;
            case AST_INTENT_DECL:
                if (!dir_add_node(dir, DIR_NODE_INTENT, node->data.intent_decl.name, node))
                    return false;
                break;
            default:
                break;
        }
    }

    return true;
}

static bool
dir_collect_role_edges(DIRProgram *dir, ASTNode *program, size_t from_id, ASTNode *node)
{
    const char *for_type = type_name(dir, node->data.role_decl.for_type);
    if (for_type != NULL) {
        ssize_t to = dir_find_type_node_by_name(dir, for_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_ROLE_FOR_TYPE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX, "for", for_type))
            return false;
    }

    for (size_t i = 0; i < node->data.role_decl.include_count; i++) {
        ASTNode *inc = node->data.role_decl.includes[i];
        if (inc == NULL)
            continue;
        ssize_t to = dir_find_role_node_by_name(dir, inc->data.include_stmt.role_name);
        if (!dir_add_named_edge(dir, DIR_EDGE_ROLE_INCLUDE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                "include",
                                inc->data.include_stmt.role_name))
            return false;
    }

    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];
        ASTNode *ability_decl = NULL;
        if (impl == NULL)
            continue;
        const char *ability_name =
            (impl->data.impl_ability.ability_ref != NULL
             && impl->data.impl_ability.ability_ref->type == AST_TYPE)
            ? impl->data.impl_ability.ability_ref->data.type.name : NULL;
        ssize_t to = dir_find_ability_node_by_name(dir, ability_name);
        if (!dir_add_named_edge(dir, DIR_EDGE_ROLE_IMPL_ABILITY, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                "impl",
                                ability_name))
            return false;

        ability_decl = dir_find_ability_decl_ast(program, ability_name);
        if (ability_decl != NULL) {
            bool complete = true;
            for (size_t j = 0; j < ability_decl->data.ability_decl.method_count; j++) {
                ASTNode *ability_method = ability_decl->data.ability_decl.methods[j];
                const char *method_name = ability_method != NULL
                    ? ability_method->data.func_decl.name
                    : NULL;
                if (method_name == NULL)
                    continue;
                if (!dir_impl_has_method_named(impl, method_name)) {
                    complete = false;
                    if (!dir_add_named_edge(dir,
                                            DIR_EDGE_ROLE_MISSING_ABILITY_METHOD,
                                            from_id,
                                            to >= 0 ? (size_t)to : SIZE_MAX,
                                            ability_name,
                                            method_name))
                        return false;
                }
            }
            if (complete) {
                if (!dir_add_named_edge(dir,
                                        DIR_EDGE_ROLE_COMPLETES_ABILITY,
                                        from_id,
                                        to >= 0 ? (size_t)to : SIZE_MAX,
                                        "complete",
                                        ability_name))
                    return false;
            }
        }
    }
    return true;
}

static bool
dir_collect_party_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *slot = node->data.party_decl.role_slots[i];
        ssize_t slot_id;
        if (slot == NULL)
            continue;
        slot_id = dir_ensure_qualified_slot_node(dir,
                                                 DIR_NODE_PARTY_SLOT,
                                                 node->data.party_decl.name,
                                                 slot->data.role_slot.slot_name,
                                                 slot);
        if (slot_id < 0)
            return false;
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_PARTY_HAS_SLOT,
                                from_id,
                                (size_t)slot_id,
                                slot->data.role_slot.slot_name,
                                dir->nodes[(size_t)slot_id].name))
            return false;
        for (size_t j = 0; j < slot->data.role_slot.ability_count; j++) {
            ASTNode *ability = slot->data.role_slot.required_abilities[j];
            const char *ability_name = type_name(dir, ability);
            ssize_t to = dir_find_ability_node_by_name(dir, ability_name);
            if (!dir_add_named_edge(dir, DIR_EDGE_PARTY_SLOT_ABILITY, from_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    slot->data.role_slot.slot_name,
                                    ability_name))
                return false;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_PARTY_SLOT_ABILITY,
                                    (size_t)slot_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    slot->data.role_slot.slot_name,
                                    ability_name))
                return false;
        }
    }
    return true;
}

static bool
dir_collect_roster_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    for (size_t i = 0; i < node->data.roster_decl.party_count; i++) {
        ASTNode *slot = node->data.roster_decl.party_slots[i];
        ssize_t to = dir_find_party_node_by_name(dir, slot->data.roster_slot.party_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_SYSTEMIC_PARTY, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot->data.roster_slot.slot_name,
                                slot->data.roster_slot.party_type))
            return false;
    }
    return true;
}

static bool
dir_collect_world_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    for (size_t i = 0; i < node->data.world_decl.roster_count; i++) {
        ASTNode *slot = node->data.world_decl.rosters[i];
        ssize_t to = dir_find_roster_node_by_name(dir, slot->data.world_roster.roster_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_WORLD_SYSTEMIC, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot->data.world_roster.slot_name,
                                slot->data.world_roster.roster_type))
            return false;
    }
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *slot = node->data.world_decl.zones[i];
        ssize_t to = dir_find_zone_node_by_name(dir, slot->data.world_zone.zone_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_WORLD_ZONE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot->data.world_zone.slot_name,
                                slot->data.world_zone.zone_type))
            return false;
    }
    return true;
}

bool
dir_collect_edges_and_intents(DIRProgram *dir, ASTNode *program)
{
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *node = program->data.program.statements[i];
        ssize_t from = -1;
        switch (node->type) {
            case AST_ROLE_DECL:
                from = dir_find_role_node_by_name(dir, node->data.role_decl.name);
                if (from >= 0 && !dir_collect_role_edges(dir, program, (size_t)from, node))
                    return false;
                break;
            case AST_PARTY_DECL:
                from = dir_find_node_by_name_kind(dir, node->data.party_decl.name, DIR_NODE_PARTY);
                if (from >= 0 && !dir_collect_party_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_ROSTER_DECL:
                from = dir_find_roster_node_by_name(dir, node->data.roster_decl.name);
                if (from >= 0 && !dir_collect_roster_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_WORLD_DECL:
                from = dir_find_node_by_name_kind(dir, node->data.world_decl.name, DIR_NODE_WORLD);
                if (from >= 0 && !dir_collect_world_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_RELATION_DECL:
                from = dir_find_relation_node_by_name(dir, node->data.relation_decl.name);
                if (from >= 0
                    && !dir_collect_relation_effect_slot_edges(dir,
                                                               (size_t)from,
                                                               node->data.relation_decl.name,
                                                               node->data.relation_decl.slots,
                                                               node->data.relation_decl.slot_count,
                                                               node->data.relation_decl.refreshes,
                                                               node->data.relation_decl.refresh_count))
                    return false;
                break;
            case AST_EFFECT_DECL:
                from = dir_find_effect_node_by_name(dir, node->data.effect_decl.name);
                if (from >= 0
                    && !dir_collect_relation_effect_slot_edges(dir,
                                                               (size_t)from,
                                                               node->data.effect_decl.name,
                                                               node->data.effect_decl.slots,
                                                               node->data.effect_decl.slot_count,
                                                               node->data.effect_decl.refreshes,
                                                               node->data.effect_decl.refresh_count))
                    return false;
                break;
            case AST_ZONE_DECL:
                from = dir_find_zone_node_by_name(dir, node->data.zone_decl.name);
                if (from >= 0 && !dir_collect_zone_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_INTENT_DECL:
                from = dir_find_node_by_name_kind(dir, node->data.intent_decl.name, DIR_NODE_INTENT);
                if (from >= 0 && !dir_collect_intent_info(dir, (size_t)from, node))
                    return false;
                break;
            default:
                break;
        }
    }
    return true;
}
