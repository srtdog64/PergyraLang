#include "dir_internal.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../common/string_compat.h"

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

static char *dir_last_error_message = NULL;

static void
dir_clear_error_message(void)
{
    free(dir_last_error_message);
    dir_last_error_message = NULL;
}

bool
dir_failf(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *message;

    if (dir_last_error_message != NULL)
        return false;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return false;
    }

    message = malloc((size_t)length + 1);
    if (message == NULL) {
        va_end(args);
        return false;
    }

    vsnprintf(message, (size_t)length + 1, fmt, args);
    va_end(args);
    dir_last_error_message = message;
    return false;
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

static bool
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

ssize_t
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

static char *
dir_render_type_name_dup(ASTNode *type_node)
{
    char *result = NULL;

    if (type_node == NULL)
        return NULL;

    switch (type_node->type) {
    case AST_TYPE: {
        const char *base_name = type_node->data.type.name != NULL
            ? type_node->data.type.name
            : "Int";
        result = pergyra_strdup(base_name);
        if (result == NULL)
            return NULL;
        if (type_node->data.type.generic_args != NULL
            && type_node->data.type.generic_args->count > 0) {
            char *next = dir_strdup_fmt("%s<", result);
            free(result);
            result = next;
            if (result == NULL)
                return NULL;
            for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
                GenericParam *param = type_node->data.type.generic_args->params[i];
                char *arg_text = NULL;
                if (param != NULL && param->constraint != NULL) {
                    arg_text = dir_render_type_name_dup(param->constraint);
                } else if (param != NULL && param->name != NULL) {
                    arg_text = pergyra_strdup(param->name);
                } else if (param != NULL && param->default_type != NULL) {
                    arg_text = dir_render_type_name_dup(param->default_type);
                } else {
                    arg_text = pergyra_strdup("Int");
                }
                next = dir_strdup_fmt("%s%s%s",
                                      result,
                                      i > 0 ? ", " : "",
                                      arg_text != NULL ? arg_text : "Int");
                free(arg_text);
                free(result);
                result = next;
                if (result == NULL)
                    return NULL;
            }
            next = dir_strdup_fmt("%s>", result);
            free(result);
            result = next;
        }
        return result;
    }
    case AST_CHANNEL_TYPE: {
        char *inner = dir_render_type_name_dup(type_node->data.channel_type.element_type);
        result = dir_strdup_fmt("Channel<%s>", inner != NULL ? inner : "Int");
        free(inner);
        return result;
    }
    case AST_FUTURE_TYPE: {
        char *inner = dir_render_type_name_dup(type_node->data.future_type.value_type);
        result = dir_strdup_fmt("Future<%s>", inner != NULL ? inner : "Int");
        free(inner);
        return result;
    }
    default:
        break;
    }

    return NULL;
}

const char *
type_name(DIRProgram *dir, ASTNode *type_node)
{
    char *owned;

    if (dir == NULL || type_node == NULL)
        return NULL;

    owned = dir_render_type_name_dup(type_node);
    if (owned == NULL)
        return NULL;
    if (!dir_track_owned_name(dir, owned)) {
        free(owned);
        return NULL;
    }
    return owned;
}

bool
dir_domain_slot_is_projection(ASTNode *slot)
{
    return slot != NULL
        && slot->type == AST_DOMAIN_SLOT
        && !slot->data.domain_slot.is_subject
        && !slot->data.domain_slot.is_vessel;
}

DIRProgram *
dir_lower(ASTNode *annotated_ast, char **error_message)
{
    DIRProgram *dir;

    if (error_message != NULL)
        *error_message = NULL;
    if (annotated_ast == NULL || annotated_ast->type != AST_PROGRAM) {
        dir_clear_error_message();
        if (error_message != NULL)
            *error_message = pergyra_strdup("DIR lowering requires AST_PROGRAM root");
        return NULL;
    }

    dir_clear_error_message();
    dir = calloc(1, sizeof(DIRProgram));
    if (dir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("Out of memory");
        return NULL;
    }

    if (!dir_collect_nodes(dir, annotated_ast) || !dir_collect_edges_and_intents(dir, annotated_ast)) {
        if (error_message != NULL) {
            if (dir_last_error_message != NULL) {
                *error_message = dir_last_error_message;
                dir_last_error_message = NULL;
            } else {
                *error_message = pergyra_strdup("Out of memory");
            }
        }
        dir_clear_error_message();
        dir_destroy(dir);
        return NULL;
    }

    dir_clear_error_message();
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
