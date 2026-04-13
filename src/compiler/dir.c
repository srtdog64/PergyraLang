#include "dir.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../common/string_compat.h"

static bool
append_node(DIRNode **nodes, size_t *count, DIRNode node)
{
    DIRNode *grown = realloc(*nodes, (*count + 1) * sizeof(DIRNode));
    if (grown == NULL)
        return false;
    grown[*count] = node;
    *nodes = grown;
    (*count)++;
    return true;
}

static char *
dir_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    return result;
}

static bool
append_edge(DIREdge **edges, size_t *count, DIREdge edge)
{
    DIREdge *grown = realloc(*edges, (*count + 1) * sizeof(DIREdge));
    if (grown == NULL)
        return false;
    grown[*count] = edge;
    *edges = grown;
    (*count)++;
    return true;
}

static bool
append_name(const char ***items, size_t *count, const char *name)
{
    const char **grown;
    if (name == NULL)
        return true;
    grown = realloc((void *)*items, (*count + 1) * sizeof(const char *));
    if (grown == NULL)
        return false;
    grown[*count] = name;
    *items = grown;
    (*count)++;
    return true;
}

static bool
append_intent_info(DIRIntentInfo **items, size_t *count, DIRIntentInfo info)
{
    DIRIntentInfo *grown = realloc(*items, (*count + 1) * sizeof(DIRIntentInfo));
    if (grown == NULL)
        return false;
    grown[*count] = info;
    *items = grown;
    (*count)++;
    return true;
}

static bool
append_intent_participant(DIRIntentParticipant **items,
                          size_t *count,
                          DIRIntentParticipant participant)
{
    DIRIntentParticipant *grown = realloc(*items, (*count + 1) * sizeof(DIRIntentParticipant));
    if (grown == NULL)
        return false;
    grown[*count] = participant;
    *items = grown;
    (*count)++;
    return true;
}

static bool
append_intent_step(DIRIntentStep **items, size_t *count, DIRIntentStep step)
{
    DIRIntentStep *grown = realloc(*items, (*count + 1) * sizeof(DIRIntentStep));
    if (grown == NULL)
        return false;
    grown[*count] = step;
    *items = grown;
    (*count)++;
    return true;
}

static bool
dir_track_owned_name(DIRProgram *dir, char *name)
{
    char **grown;

    if (dir == NULL || name == NULL)
        return false;

    grown = realloc(dir->owned_names, (dir->owned_name_count + 1) * sizeof(char *));
    if (grown == NULL)
        return false;
    grown[dir->owned_name_count] = name;
    dir->owned_names = grown;
    dir->owned_name_count++;
    return true;
}

static const char *
dir_own_string_fmt(DIRProgram *dir, const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *owned;

    if (dir == NULL || fmt == NULL)
        return NULL;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    owned = malloc((size_t)length + 1);
    if (owned == NULL) {
        va_end(args);
        return NULL;
    }
    vsnprintf(owned, (size_t)length + 1, fmt, args);
    va_end(args);

    if (!dir_track_owned_name(dir, owned)) {
        free(owned);
        return NULL;
    }
    return owned;
}

static ssize_t
dir_find_node_by_name_kind(const DIRProgram *dir, const char *name, DIRNodeKind kind)
{
    if (dir == NULL || name == NULL)
        return -1;

    for (size_t i = 0; i < dir->node_count; i++) {
        if (dir->nodes[i].kind == kind
            && dir->nodes[i].name != NULL
            && strcmp(dir->nodes[i].name, name) == 0) {
            return (ssize_t)i;
        }
    }

    return -1;
}

static ssize_t
dir_find_slot_node(const DIRProgram *dir, DIRNodeKind kind, const char *owner_name, const char *slot_name)
{
    const char *qualified_name;
    char *scratch = NULL;
    ssize_t found;

    if (dir == NULL || owner_name == NULL || slot_name == NULL)
        return -1;

    scratch = dir_strdup_fmt("%s.%s", owner_name, slot_name);
    if (scratch == NULL)
        return -1;
    qualified_name = scratch;
    found = dir_find_node_by_name_kind(dir, qualified_name, kind);
    free(scratch);
    return found;
}

static ssize_t
dir_find_any_node_by_name(const DIRProgram *dir, const char *name)
{
    if (dir == NULL || name == NULL)
        return -1;

    for (size_t i = 0; i < dir->node_count; i++) {
        if (dir->nodes[i].name != NULL && strcmp(dir->nodes[i].name, name) == 0)
            return (ssize_t)i;
    }

    return -1;
}

static ssize_t
dir_find_type_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_TYPE);
}

static ssize_t
dir_find_ability_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_ABILITY);
}

static ssize_t
dir_find_role_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_ROLE);
}

static ssize_t
dir_find_party_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_PARTY);
}

static ssize_t
dir_find_roster_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_SYSTEMIC);
}

static ssize_t
dir_find_zone_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_ZONE);
}

static ssize_t
dir_find_effect_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_EFFECT);
}

static ssize_t
dir_find_relation_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_RELATION);
}

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

static bool
dir_add_node(DIRProgram *dir, DIRNodeKind kind, const char *name, ASTNode *ast)
{
    DIRNode node;
    node.id = dir->node_count;
    node.kind = kind;
    node.name = name;
    node.ast = ast;
    return append_node(&dir->nodes, &dir->node_count, node);
}

static ssize_t
dir_ensure_qualified_slot_node(DIRProgram *dir,
                               DIRNodeKind kind,
                               const char *owner_name,
                               const char *slot_name,
                               ASTNode *ast)
{
    const char *qualified_name;
    ssize_t existing;

    if (dir == NULL || owner_name == NULL || slot_name == NULL)
        return -1;

    existing = dir_find_slot_node(dir, kind, owner_name, slot_name);
    if (existing >= 0)
        return existing;

    qualified_name = dir_own_string_fmt(dir, "%s.%s", owner_name, slot_name);
    if (qualified_name == NULL)
        return -1;
    if (!dir_add_node(dir, kind, qualified_name, ast))
        return -1;
    return (ssize_t)(dir->node_count - 1);
}

static bool
dir_add_named_edge(DIRProgram *dir,
                   DIREdgeKind kind,
                   size_t from_node_id,
                   size_t to_node_id,
                   const char *label,
                   const char *target_name)
{
    DIREdge edge;
    edge.kind = kind;
    edge.from_node_id = from_node_id;
    edge.to_node_id = to_node_id;
    edge.label = label;
    edge.target_name = target_name;
    return append_edge(&dir->edges, &dir->edge_count, edge);
}

static const char *
type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return NULL;
    if (type_node->type == AST_TYPE)
        return type_node->data.type.name;
    return NULL;
}

static bool
dir_domain_slot_is_projection(ASTNode *slot)
{
    return slot != NULL
        && slot->type == AST_DOMAIN_SLOT
        && !slot->data.domain_slot.is_subject
        && !slot->data.domain_slot.is_vessel;
}

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
        return true;

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

static bool
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
    const char *for_type = type_name(node->data.role_decl.for_type);
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
            const char *ability_name = type_name(ability);
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
        if (!dir_add_named_edge(dir, DIR_EDGE_SYSTEMIC_PARTY, from_id,
                                dir_find_party_node_by_name(dir, slot->data.roster_slot.party_type) >= 0
                                    ? (size_t)dir_find_party_node_by_name(dir, slot->data.roster_slot.party_type)
                                    : SIZE_MAX,
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

static bool
dir_collect_zone_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.slots[i];
        const char *target = type_name(slot->data.domain_slot.type);
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

static bool
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
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        if (!dir_domain_slot_is_projection(slot))
            continue;
        target = type_name(slot->data.domain_slot.type);
        to = dir_find_type_node_by_name(dir, target);
        slot_node_id = dir_ensure_qualified_slot_node(dir,
                                                      DIR_NODE_PROJECTION_SLOT,
                                                      owner_name,
                                                      slot->data.domain_slot.slot_name,
                                                      slot);
        if (slot_node_id < 0)
            return false;
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
    }

    for (size_t i = 0; i < refresh_count; i++) {
        if (!dir_add_projection_contract_edges(dir, from_id, owner_name, refreshes[i]))
            return false;
    }

    return true;
}

static bool
dir_collect_intent_info(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    DIRIntentInfo info;
    memset(&info, 0, sizeof(info));
    info.node_id = from_id;

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *inv = node->data.intent_decl.involves[i];
        DIRIntentParticipant participant;
        participant.alias = inv->data.intent_involves.alias;
        participant.subject_type_name = type_name(inv->data.intent_involves.subject_type);
        {
            ssize_t to = dir_find_any_node_by_name(dir, participant.subject_type_name);
            participant.subject_type_node_id = to >= 0 ? (size_t)to : SIZE_MAX;
        }
        if (!append_intent_participant(&info.participants, &info.participant_count, participant))
            goto oom;
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_INTENT_PARTICIPANT_TYPE,
                                from_id,
                                participant.subject_type_node_id,
                                participant.alias,
                                participant.subject_type_name))
            goto oom;
    }

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step_node = node->data.intent_decl.steps[i];
        DIRIntentStep step;
        memset(&step, 0, sizeof(step));
        step.index = i;
        step.name = step_node->data.intent_step.name;
        step.where_type_name = type_name(step_node->data.intent_step.where_type);
        {
            ssize_t to = dir_find_zone_node_by_name(dir, step.where_type_name);
            step.where_type_node_id = to >= 0 ? (size_t)to : SIZE_MAX;
        }
        step.using_alias = step_node->data.intent_step.using_expr != NULL
            && step_node->data.intent_step.using_expr->type == AST_IDENTIFIER
            ? step_node->data.intent_step.using_expr->data.identifier.name
            : NULL;
        step.predecessor_step_name = (i > 0 && node->data.intent_decl.steps[i - 1] != NULL)
            ? node->data.intent_decl.steps[i - 1]->data.intent_step.name
            : NULL;
        step.predecessor_step_index = i > 0 ? (i - 1) : SIZE_MAX;
        step.transfer_from_alias = step_node->data.intent_step.transfer_from_alias;
        step.transfer_to_alias = step_node->data.intent_step.transfer_to_alias;
        step.causes_effect_name = step_node->data.intent_step.causes_effect;
        {
            ssize_t to = dir_find_effect_node_by_name(dir, step.causes_effect_name);
            step.causes_effect_node_id = to >= 0 ? (size_t)to : SIZE_MAX;
        }
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_INTENT_STEP_ZONE,
                                from_id,
                                step.where_type_node_id,
                                step.name,
                                step.where_type_name))
            goto oom;
        if (step.causes_effect_name != NULL) {
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_CAUSES,
                                    from_id,
                                    step.causes_effect_node_id,
                                    step.name,
                                    step.causes_effect_name))
                goto oom;
        }
        if (step.predecessor_step_name != NULL) {
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_DEPENDS_ON,
                                    from_id,
                                    from_id,
                                    step.predecessor_step_name,
                                    step.name))
                goto oom;
        }
        for (size_t j = 0; j < step_node->data.intent_step.who_count; j++) {
            if (!append_name(&step.who_names, &step.who_count, step_node->data.intent_step.who_names[j]))
                goto oom;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_WHO,
                                    from_id,
                                    from_id,
                                    step.name,
                                    step_node->data.intent_step.who_names[j]))
                goto oom;
        }
        for (size_t j = 0; j < step_node->data.intent_step.required_ability_count; j++) {
            ASTNode *ability_ref = step_node->data.intent_step.required_abilities[j];
            const char *ability_name = (ability_ref != NULL && ability_ref->type == AST_TYPE)
                ? ability_ref->data.type.name : NULL;
            if (ability_name == NULL)
                continue;
            if (!append_name(&step.required_abilities,
                             &step.required_ability_count,
                             ability_name))
                goto oom;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_REQUIRES,
                                    from_id,
                                    dir_find_ability_node_by_name(dir, ability_name) >= 0
                                        ? (size_t)dir_find_ability_node_by_name(dir, ability_name)
                                        : SIZE_MAX,
                                    step.name,
                                    ability_name))
                goto oom;
        }
        for (size_t j = 0; j < step_node->data.intent_step.authorized_by_count; j++) {
            if (!append_name(&step.authorized_by,
                             &step.authorized_by_count,
                             step_node->data.intent_step.authorized_by[j]))
                goto oom;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_AUTHORIZED_BY,
                                    from_id,
                                    from_id,
                                    step.name,
                                    step_node->data.intent_step.authorized_by[j]))
                goto oom;
        }
        if (!append_intent_step(&info.steps, &info.step_count, step))
            goto oom;
    }

    if (!append_intent_info(&dir->intents, &dir->intent_count, info))
        goto oom;
    return true;

oom:
    free(info.participants);
    if (info.steps != NULL) {
        for (size_t i = 0; i < info.step_count; i++) {
            free((void *)info.steps[i].who_names);
            free((void *)info.steps[i].required_abilities);
            free((void *)info.steps[i].authorized_by);
        }
    }
    free(info.steps);
    return false;
}

static bool
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

DIRProgram *
dir_lower(ASTNode *annotated_ast, char **error_message)
{
    DIRProgram *dir;

    if (error_message != NULL)
        *error_message = NULL;
    if (annotated_ast == NULL || annotated_ast->type != AST_PROGRAM) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("DIR lowering requires AST_PROGRAM root");
        return NULL;
    }

    dir = calloc(1, sizeof(DIRProgram));
    if (dir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return NULL;
    }

    if (!dir_collect_nodes(dir, annotated_ast) || !dir_collect_edges_and_intents(dir, annotated_ast)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        dir_destroy(dir);
        return NULL;
    }

    return dir;
}

void
dir_destroy(DIRProgram *dir)
{
    if (dir == NULL)
        return;
    if (dir->intents != NULL) {
        for (size_t i = 0; i < dir->intent_count; i++) {
            free(dir->intents[i].participants);
            for (size_t j = 0; j < dir->intents[i].step_count; j++) {
                free((void *)dir->intents[i].steps[j].who_names);
                free((void *)dir->intents[i].steps[j].required_abilities);
                free((void *)dir->intents[i].steps[j].authorized_by);
            }
            free(dir->intents[i].steps);
        }
    }
    if (dir->owned_names != NULL) {
        for (size_t i = 0; i < dir->owned_name_count; i++)
            free(dir->owned_names[i]);
    }
    free(dir->nodes);
    free(dir->edges);
    free(dir->intents);
    free(dir->owned_names);
    free(dir);
}

const char *
dir_node_kind_name(DIRNodeKind kind)
{
    switch (kind) {
        case DIR_NODE_TYPE: return "type";
        case DIR_NODE_ABILITY: return "ability";
        case DIR_NODE_ROLE: return "role";
        case DIR_NODE_PARTY: return "party";
        case DIR_NODE_PARTY_SLOT: return "party-slot";
        case DIR_NODE_SYSTEMIC: return "roster";
        case DIR_NODE_WORLD: return "world";
        case DIR_NODE_RELATION: return "relation";
        case DIR_NODE_EFFECT: return "effect";
        case DIR_NODE_ZONE: return "zone";
        case DIR_NODE_ZONE_SLOT: return "zone-slot";
        case DIR_NODE_PROJECTION_SLOT: return "projection-slot";
        case DIR_NODE_AUTHORITY_SLOT: return "authority-slot";
        case DIR_NODE_INTENT: return "intent";
        default: return "unknown";
    }
}

const char *
dir_edge_kind_name(DIREdgeKind kind)
{
    switch (kind) {
        case DIR_EDGE_ROLE_FOR_TYPE: return "role-for";
        case DIR_EDGE_ROLE_INCLUDE: return "role-include";
        case DIR_EDGE_ROLE_IMPL_ABILITY: return "role-impl";
        case DIR_EDGE_ROLE_COMPLETES_ABILITY: return "role-complete";
        case DIR_EDGE_ROLE_MISSING_ABILITY_METHOD: return "role-missing-method";
        case DIR_EDGE_PARTY_HAS_SLOT: return "party-has-slot";
        case DIR_EDGE_PARTY_SLOT_ABILITY: return "party-slot";
        case DIR_EDGE_SYSTEMIC_PARTY: return "roster-party";
        case DIR_EDGE_WORLD_SYSTEMIC: return "world-roster";
        case DIR_EDGE_WORLD_ZONE: return "world-zone";
        case DIR_EDGE_ZONE_HAS_SLOT: return "zone-has-slot";
        case DIR_EDGE_ZONE_SLOT_TYPE: return "zone-slot";
        case DIR_EDGE_OWNER_HAS_PROJECTION_SLOT: return "owner-has-projection-slot";
        case DIR_EDGE_PROJECTION_SLOT_TYPE: return "projection-slot-type";
        case DIR_EDGE_PROJECTION_SLOT_SOURCE: return "projection-slot-source";
        case DIR_EDGE_ZONE_HAS_AUTHORITY_SLOT: return "zone-has-authority-slot";
        case DIR_EDGE_AUTHORITY_SLOT_SUBJECT: return "authority-slot-subject";
        case DIR_EDGE_ZONE_LAYER_TYPE: return "zone-layer";
        case DIR_EDGE_ZONE_AUTHORITY_ABILITY: return "zone-authority";
        case DIR_EDGE_ZONE_STATE_LAYER: return "zone-state";
        case DIR_EDGE_INTENT_PARTICIPANT_TYPE: return "intent-participant";
        case DIR_EDGE_INTENT_STEP_ZONE: return "intent-step-zone";
        case DIR_EDGE_INTENT_STEP_WHO: return "intent-step-who";
        case DIR_EDGE_INTENT_STEP_REQUIRES: return "intent-step-requires";
        case DIR_EDGE_INTENT_STEP_AUTHORIZED_BY: return "intent-step-authorized-by";
        case DIR_EDGE_INTENT_STEP_CAUSES: return "intent-step-causes";
        case DIR_EDGE_INTENT_STEP_DEPENDS_ON: return "intent-step-depends-on";
        default: return "unknown";
    }
}

bool
dir_validate(const DIRProgram *dir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (dir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("DIR program is null");
        return false;
    }

    for (size_t i = 0; i < dir->edge_count; i++) {
        const DIREdge *edge = &dir->edges[i];
        if (edge->from_node_id >= dir->node_count) {
            if (error_message != NULL)
                *error_message = dir_strdup_fmt("DIR edge[%zu] has invalid from_node_id", i);
            return false;
        }
        if (edge->to_node_id != SIZE_MAX && edge->to_node_id >= dir->node_count) {
            if (error_message != NULL)
                *error_message = dir_strdup_fmt("DIR edge[%zu] has invalid to_node_id", i);
            return false;
        }
    }

    for (size_t i = 0; i < dir->intent_count; i++) {
        const DIRIntentInfo *intent = &dir->intents[i];
        if (intent->node_id >= dir->node_count) {
            if (error_message != NULL)
                *error_message = dir_strdup_fmt("DIR intent[%zu] has invalid node id", i);
            return false;
        }
        for (size_t j = 0; j < intent->participant_count; j++) {
            const DIRIntentParticipant *participant = &intent->participants[j];
            if (participant->subject_type_node_id == SIZE_MAX
                || participant->subject_type_node_id >= dir->node_count) {
                if (error_message != NULL) {
                    *error_message = dir_strdup_fmt(
                        "DIR intent[%zu] participant '%s' is unresolved",
                        i,
                        participant->alias != NULL ? participant->alias : "-");
                }
                return false;
            }
        }
        for (size_t j = 0; j < intent->step_count; j++) {
            const DIRIntentStep *step = &intent->steps[j];
            if (step->index != j) {
                if (error_message != NULL)
                    *error_message = dir_strdup_fmt("DIR intent[%zu] step[%zu] has unstable index", i, j);
                return false;
            }
            if (step->where_type_name != NULL
                && (step->where_type_node_id == SIZE_MAX
                    || step->where_type_node_id >= dir->node_count)) {
                if (error_message != NULL) {
                    *error_message = dir_strdup_fmt(
                        "DIR intent[%zu] step '%s' has unresolved where zone",
                        i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if (step->predecessor_step_name != NULL && j == 0) {
                if (error_message != NULL) {
                    *error_message = dir_strdup_fmt(
                        "DIR intent[%zu] first step '%s' cannot have predecessor",
                        i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if (step->predecessor_step_name != NULL
                && step->predecessor_step_index >= j) {
                if (error_message != NULL) {
                    *error_message = dir_strdup_fmt(
                        "DIR intent[%zu] step '%s' has invalid predecessor index",
                        i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
        }
    }

    return true;
}

static void
dir_dump_resolved_id(FILE *out, size_t node_id)
{
    if (node_id == SIZE_MAX)
        fputc('-', out);
    else
        fprintf(out, "%zu", node_id);
}

void
dir_dump(const DIRProgram *dir, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (dir == NULL) {
        fprintf(out, "DIR: (null)\n");
        return;
    }

    fprintf(out, "DIR Program\n  nodes: %zu\n  edges: %zu\n  intents: %zu\n",
            dir->node_count, dir->edge_count, dir->intent_count);

    for (size_t i = 0; i < dir->node_count; i++) {
        fprintf(out, "  node[%02zu] %-8s %s\n",
                i,
                dir_node_kind_name(dir->nodes[i].kind),
                dir->nodes[i].name != NULL ? dir->nodes[i].name : "(anonymous)");
    }

    for (size_t i = 0; i < dir->edge_count; i++) {
        const DIREdge *edge = &dir->edges[i];
        fprintf(out, "  edge[%02zu] %-14s from=%zu label=%s target=%s resolved=",
                i,
                dir_edge_kind_name(edge->kind),
                edge->from_node_id,
                edge->label != NULL ? edge->label : "-",
                edge->target_name != NULL ? edge->target_name : "-");
        dir_dump_resolved_id(out, edge->to_node_id);
        fputc('\n', out);
    }

    for (size_t i = 0; i < dir->intent_count; i++) {
        const DIRIntentInfo *intent = &dir->intents[i];
        const DIRNode *node = &dir->nodes[intent->node_id];
        fprintf(out, "  intent[%02zu] %s participants=%zu steps=%zu\n",
                i,
                node->name != NULL ? node->name : "(anonymous)",
                intent->participant_count,
                intent->step_count);
        for (size_t j = 0; j < intent->participant_count; j++) {
            const DIRIntentParticipant *p = &intent->participants[j];
            fprintf(out, "    participant %-12s type=%s resolved=",
                    p->alias != NULL ? p->alias : "-",
                    p->subject_type_name != NULL ? p->subject_type_name : "-");
            dir_dump_resolved_id(out, p->subject_type_node_id);
            fputc('\n', out);
        }
        for (size_t j = 0; j < intent->step_count; j++) {
            const DIRIntentStep *step = &intent->steps[j];
            fprintf(out, "    step[%02zu] %-12s where=%s resolved=",
                    step->index,
                    step->name != NULL ? step->name : "-",
                    step->where_type_name != NULL ? step->where_type_name : "-");
            dir_dump_resolved_id(out, step->where_type_node_id);
            fprintf(out, " using=%s causes=%s",
                    step->using_alias != NULL ? step->using_alias : "-",
                    step->causes_effect_name != NULL ? step->causes_effect_name : "-");
            if (step->predecessor_step_name != NULL) {
                fprintf(out, " depends-on=%s",
                        step->predecessor_step_name);
            }
            if (step->transfer_from_alias != NULL || step->transfer_to_alias != NULL) {
                fprintf(out, " transfer=%s->%s",
                        step->transfer_from_alias != NULL ? step->transfer_from_alias : "-",
                        step->transfer_to_alias != NULL ? step->transfer_to_alias : "-");
            }
            fputc('\n', out);
            for (size_t k = 0; k < step->who_count; k++)
                fprintf(out, "      who[%zu] %s\n", k, step->who_names[k]);
            for (size_t k = 0; k < step->required_ability_count; k++)
                fprintf(out, "      requires[%zu] %s\n", k, step->required_abilities[k]);
            for (size_t k = 0; k < step->authorized_by_count; k++)
                fprintf(out, "      authorized_by[%zu] %s\n", k, step->authorized_by[k]);
        }
    }
}
