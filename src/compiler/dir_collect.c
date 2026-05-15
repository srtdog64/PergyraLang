#include "dir_internal.h"
#include "parser/ast_api.h"

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
            && ast_ability_name(stmt) != NULL
            && strcmp(ast_ability_name(stmt), name) == 0) {
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

    for (size_t i = 0; i < ast_impl_ability_method_count(impl); i++) {
        ASTNode *method = ast_impl_ability_method(impl, i);
        const char *declared_name = ast_declaration_name(method);
        if (method != NULL
            && method->type == AST_FUNC_DECL
            && declared_name != NULL
            && strcmp(declared_name, method_name) == 0) {
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
                if (!dir_add_node(dir, DIR_NODE_TYPE, ast_class_name(node), node))
                    return false;
                break;
            case AST_TYPE_ALIAS:
                if (!dir_add_node(dir, DIR_NODE_TYPE, ast_type_alias_name(node), node))
                    return false;
                break;
            case AST_ENUM_DECL:
                if (!dir_add_node(dir, DIR_NODE_TYPE, ast_enum_name(node), node))
                    return false;
                break;
            case AST_ABILITY_DECL:
                if (!dir_add_node(dir, DIR_NODE_ABILITY, ast_ability_name(node), node))
                    return false;
                break;
            case AST_ROLE_DECL:
                if (!dir_add_node(dir, DIR_NODE_ROLE, ast_role_name(node), node))
                    return false;
                break;
            case AST_PARTY_DECL:
                if (!dir_add_node(dir, DIR_NODE_PARTY, ast_party_name(node), node))
                    return false;
                break;
            case AST_ROSTER_DECL:
                if (!dir_add_node(dir, DIR_NODE_SYSTEMIC, ast_roster_name(node), node))
                    return false;
                break;
            case AST_WORLD_DECL:
                if (!dir_add_node(dir, DIR_NODE_WORLD, ast_world_name(node), node))
                    return false;
                break;
            case AST_RELATION_DECL:
                if (!dir_add_node(dir, DIR_NODE_RELATION, ast_relation_name(node), node))
                    return false;
                break;
            case AST_EFFECT_DECL:
                if (!dir_add_node(dir, DIR_NODE_EFFECT, ast_effect_name(node), node))
                    return false;
                break;
            case AST_ZONE_DECL:
                if (!dir_add_node(dir, DIR_NODE_ZONE, ast_zone_name(node), node))
                    return false;
                break;
            case AST_INTENT_DECL:
                if (!dir_add_node(dir, DIR_NODE_INTENT, ast_intent_decl_name(node), node))
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
    const char *for_type = type_name(dir, ast_role_for_type(node));
    if (for_type != NULL) {
        ssize_t to = dir_find_type_node_by_name(dir, for_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_ROLE_FOR_TYPE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX, "for", for_type))
            return false;
    }

    for (size_t i = 0; i < ast_role_include_count(node); i++) {
        ASTNode *inc = ast_role_include(node, i);
        const char *role_name = ast_include_role_name(inc);
        if (role_name == NULL)
            continue;
        ssize_t to = dir_find_role_node_by_name(dir, role_name);
        if (!dir_add_named_edge(dir, DIR_EDGE_ROLE_INCLUDE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                "include",
                                role_name))
            return false;
    }

    for (size_t i = 0; i < ast_role_impl_count(node); i++) {
        ASTNode *impl = ast_role_impl(node, i);
        ASTNode *ability_decl = NULL;
        if (impl == NULL)
            continue;
        const char *ability_name = ast_impl_ability_name(impl);
        ssize_t to = dir_find_ability_node_by_name(dir, ability_name);
        if (!dir_add_named_edge(dir, DIR_EDGE_ROLE_IMPL_ABILITY, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                "impl",
                                ability_name))
            return false;

        ability_decl = dir_find_ability_decl_ast(program, ability_name);
        if (ability_decl != NULL) {
            bool complete = true;
            for (size_t j = 0; j < ast_ability_method_count(ability_decl); j++) {
                ASTNode *ability_method = ast_ability_method(ability_decl, j);
                const char *method_name = ability_method != NULL
                    ? ast_declaration_name(ability_method)
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
    for (size_t i = 0; i < ast_party_role_count(node); i++) {
        ASTNode *slot = ast_party_role(node, i);
        const char *slot_name = ast_role_slot_name(slot);
        size_t ability_count = ast_role_slot_required_ability_count(slot);
        ssize_t slot_id;
        if (slot == NULL)
            continue;
        slot_id = dir_ensure_qualified_slot_node(dir,
                                                 DIR_NODE_PARTY_SLOT,
                                                 ast_party_name(node),
                                                 slot_name,
                                                 slot);
        if (slot_id < 0)
            return false;
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_PARTY_HAS_SLOT,
                                from_id,
                                (size_t)slot_id,
                                slot_name,
                                dir->nodes[(size_t)slot_id].name))
            return false;
        for (size_t j = 0; j < ability_count; j++) {
            ASTNode *ability = ast_role_slot_required_ability(slot, j);
            const char *ability_name = type_name(dir, ability);
            ssize_t to = dir_find_ability_node_by_name(dir, ability_name);
            if (!dir_add_named_edge(dir, DIR_EDGE_PARTY_SLOT_ABILITY, from_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    slot_name,
                                    ability_name))
                return false;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_PARTY_SLOT_ABILITY,
                                    (size_t)slot_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    slot_name,
                                    ability_name))
                return false;
        }
    }
    return true;
}

static bool
dir_collect_roster_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    for (size_t i = 0; i < ast_roster_party_count(node); i++) {
        ASTNode *slot = ast_roster_party(node, i);
        const char *party_type = ast_roster_slot_party_type(slot);
        const char *slot_name = ast_roster_slot_name(slot);
        ssize_t to = dir_find_party_node_by_name(dir, party_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_SYSTEMIC_PARTY, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot_name, party_type))
            return false;
    }
    return true;
}

static bool
dir_collect_world_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    size_t roster_count = 0;
    ASTNode **rosters = ast_world_rosters(node, &roster_count);
    for (size_t i = 0; i < roster_count; i++) {
        ASTNode *slot = rosters[i];
        const char *type_name = ast_world_roster_type_name(slot);
        const char *slot_name = ast_world_roster_slot_name(slot);
        ssize_t to = dir_find_roster_node_by_name(dir, type_name);
        if (!dir_add_named_edge(dir, DIR_EDGE_WORLD_SYSTEMIC, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot_name,
                                type_name))
            return false;
    }
    size_t zone_count = 0;
    ASTNode **zones = ast_world_zones(node, &zone_count);
    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *slot = zones[i];
        const char *type_name = ast_world_zone_type_name(slot);
        const char *slot_name = ast_world_zone_slot_name(slot);
        ssize_t to = dir_find_zone_node_by_name(dir, type_name);
        if (!dir_add_named_edge(dir, DIR_EDGE_WORLD_ZONE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot_name,
                                type_name))
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
                from = dir_find_role_node_by_name(dir, ast_role_name(node));
                if (from >= 0 && !dir_collect_role_edges(dir, program, (size_t)from, node))
                    return false;
                break;
            case AST_PARTY_DECL:
                from = dir_find_node_by_name_kind(dir, ast_party_name(node), DIR_NODE_PARTY);
                if (from >= 0 && !dir_collect_party_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_ROSTER_DECL:
                from = dir_find_roster_node_by_name(dir, ast_roster_name(node));
                if (from >= 0 && !dir_collect_roster_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_WORLD_DECL:
                from = dir_find_node_by_name_kind(dir, ast_world_name(node), DIR_NODE_WORLD);
                if (from >= 0 && !dir_collect_world_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_RELATION_DECL:
                from = dir_find_relation_node_by_name(dir, ast_relation_name(node));
                if (from >= 0) {
                    size_t slot_count = 0;
                    size_t refresh_count = 0;
                    ASTNode **slots = ast_relation_slots(node, &slot_count);
                    ASTNode **refreshes = ast_relation_refreshes(node, &refresh_count);
                    if (!dir_collect_relation_effect_slot_edges(dir,
                                                                (size_t)from,
                                                                ast_relation_name(node),
                                                                slots,
                                                                slot_count,
                                                                refreshes,
                                                                refresh_count))
                        return false;
                }
                break;
            case AST_EFFECT_DECL:
                from = dir_find_effect_node_by_name(dir, ast_effect_name(node));
                if (from >= 0) {
                    size_t slot_count = 0;
                    size_t refresh_count = 0;
                    ASTNode **slots = ast_effect_slots(node, &slot_count);
                    ASTNode **refreshes = ast_effect_refreshes(node, &refresh_count);
                    if (!dir_collect_relation_effect_slot_edges(dir,
                                                                (size_t)from,
                                                                ast_effect_name(node),
                                                                slots,
                                                                slot_count,
                                                                refreshes,
                                                                refresh_count))
                        return false;
                }
                break;
            case AST_ZONE_DECL:
                from = dir_find_zone_node_by_name(dir, ast_zone_name(node));
                if (from >= 0 && !dir_collect_zone_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_INTENT_DECL:
                from = dir_find_node_by_name_kind(
                    dir, ast_intent_decl_name(node), DIR_NODE_INTENT);
                if (from >= 0 && !dir_collect_intent_info(dir, (size_t)from, node))
                    return false;
                break;
            default:
                break;
        }
    }
    return true;
}
