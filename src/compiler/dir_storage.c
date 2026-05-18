#include "dir_internal.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
dir_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    size_t next;

    if (capacity == NULL || elem_size == 0)
        return false;
    if (*capacity == 0) {
        next = initial;
    } else {
        if (*capacity > SIZE_MAX / 2)
            return false;
        next = *capacity * 2;
    }
    if (next > SIZE_MAX / elem_size)
        return false;
    *capacity = next;
    return true;
}

static bool
append_node(DIRNode **nodes, size_t *count, size_t *capacity, DIRNode node)
{
    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!dir_next_capacity(&next_capacity, 16, sizeof(DIRNode)))
            return false;
        DIRNode *grown = realloc(*nodes, next_capacity * sizeof(DIRNode));
        if (grown == NULL)
            return false;
        *nodes = grown;
        *capacity = next_capacity;
    }
    (*nodes)[*count] = node;
    (*count)++;
    return true;
}

static bool
append_edge(DIREdge **edges, size_t *count, size_t *capacity, DIREdge edge)
{
    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!dir_next_capacity(&next_capacity, 32, sizeof(DIREdge)))
            return false;
        DIREdge *grown = realloc(*edges, next_capacity * sizeof(DIREdge));
        if (grown == NULL)
            return false;
        *edges = grown;
        *capacity = next_capacity;
    }
    (*edges)[*count] = edge;
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

bool
dir_track_owned_name(DIRProgram *dir, char *name)
{
    char **grown;

    if (dir == NULL || name == NULL)
        return false;

    if (dir->owned_name_count == dir->owned_name_capacity) {
        size_t next_capacity = dir->owned_name_capacity;
        if (!dir_next_capacity(&next_capacity, 16, sizeof(char *)))
            return false;
        grown = realloc(dir->owned_names, next_capacity * sizeof(char *));
        if (grown == NULL)
            return false;
        dir->owned_names = grown;
        dir->owned_name_capacity = next_capacity;
    }
    dir->owned_names[dir->owned_name_count] = name;
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

ssize_t
dir_find_node_by_name_kind(const DIRProgram *dir, const char *name,
                           DIRNodeKind kind)
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

ssize_t
dir_find_slot_node(const DIRProgram *dir, DIRNodeKind kind,
                   const char *owner_name, const char *slot_name)
{
    char *qualified_name;
    ssize_t found;

    if (dir == NULL || owner_name == NULL || slot_name == NULL)
        return -1;

    qualified_name = dir_strdup_fmt("%s.%s", owner_name, slot_name);
    if (qualified_name == NULL)
        return -1;
    found = dir_find_node_by_name_kind(dir, qualified_name, kind);
    free(qualified_name);
    return found;
}

ssize_t
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

ssize_t
dir_find_type_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_TYPE);
}

ssize_t
dir_find_ability_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_ABILITY);
}

ssize_t
dir_find_role_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_ROLE);
}

ssize_t
dir_find_party_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_PARTY);
}

ssize_t
dir_find_roster_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_SYSTEMIC);
}

ssize_t
dir_find_zone_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_ZONE);
}

ssize_t
dir_find_effect_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_EFFECT);
}

ssize_t
dir_find_relation_node_by_name(const DIRProgram *dir, const char *name)
{
    return dir_find_node_by_name_kind(dir, name, DIR_NODE_RELATION);
}

bool
dir_add_node(DIRProgram *dir, DIRNodeKind kind, const char *name, ASTNode *ast)
{
    DIRNode node;
    node.id = dir->node_count;
    node.kind = kind;
    node.name = name;
    node.ast = ast;
    return append_node(&dir->nodes, &dir->node_count, &dir->node_capacity, node);
}

ssize_t
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

bool
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
    return append_edge(&dir->edges, &dir->edge_count, &dir->edge_capacity, edge);
}
