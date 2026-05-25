#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "diag_codes.h"
#include "type_checker_internal.h"

static char *
type_resolution_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static bool
type_name_is_builtin_provider(const char *name)
{
    return semantic_type_resolution_metadata_named_builtin_or_shell_singleton(
        name) != NULL;
}

static Type *
type_resolution_builtin_singleton(const char *name)
{
    return semantic_type_resolution_metadata_builtin_singleton(name);
}

static bool
type_resolution_labels_equal(const char *lhs, const char *rhs)
{
    if (lhs == rhs)
        return true;
    if (lhs == NULL || rhs == NULL)
        return false;
    return strcmp(lhs, rhs) == 0;
}

size_t
type_resolution_intern_node(TypeResolutionGraph *graph,
                            TypeResolutionNodeKind kind,
                            const ASTNode *site,
                            const char *label)
{
    TypeResolutionNode *grown;
    char *label_copy = NULL;

    if (graph == NULL)
        return (size_t)-1;

    for (size_t i = 0; i < graph->node_count; i++) {
        TypeResolutionNode *node = &graph->nodes[i];
        if (node->kind == kind
            && node->site == site
            && type_resolution_labels_equal(node->label, label)) {
            return i;
        }
    }

    if (graph->node_count >= graph->node_capacity) {
        size_t new_cap;
        if (graph->node_capacity > SIZE_MAX / 2)
            return (size_t)-1;
        new_cap = graph->node_capacity == 0 ? 16 : graph->node_capacity * 2;
        if (new_cap > SIZE_MAX / sizeof(TypeResolutionNode))
            return (size_t)-1;
        grown = realloc(graph->nodes, new_cap * sizeof(TypeResolutionNode));
        if (grown == NULL)
            return (size_t)-1;
        graph->nodes = grown;
        graph->node_capacity = new_cap;
    }

    if (label != NULL) {
        label_copy = pergyra_strdup(label);
        if (label_copy == NULL)
            return (size_t)-1;
    }
    graph->nodes[graph->node_count].kind = kind;
    graph->nodes[graph->node_count].site = site;
    graph->nodes[graph->node_count].label = label_copy;
    return graph->node_count++;
}

void
type_resolution_add_edge(TypeResolutionGraph *graph,
                         size_t from,
                         size_t to,
                         const char *reason)
{
    TypeResolutionEdge *grown;
    char *reason_copy = NULL;

    if (graph == NULL || from == (size_t)-1 || to == (size_t)-1)
        return;

    for (size_t i = 0; i < graph->edge_count; i++) {
        TypeResolutionEdge *edge = &graph->edges[i];
        if (edge->from == from
            && edge->to == to
            && type_resolution_labels_equal(edge->reason, reason)) {
            return;
        }
    }

    if (graph->edge_count >= graph->edge_capacity) {
        size_t new_cap;
        if (graph->edge_capacity > SIZE_MAX / 2)
            return;
        new_cap = graph->edge_capacity == 0 ? 32 : graph->edge_capacity * 2;
        if (new_cap > SIZE_MAX / sizeof(TypeResolutionEdge))
            return;
        grown = realloc(graph->edges, new_cap * sizeof(TypeResolutionEdge));
        if (grown == NULL)
            return;
        graph->edges = grown;
        graph->edge_capacity = new_cap;
    }

    if (reason != NULL) {
        reason_copy = pergyra_strdup(reason);
        if (reason_copy == NULL)
            return;
    }
    graph->edges[graph->edge_count].from = from;
    graph->edges[graph->edge_count].to = to;
    graph->edges[graph->edge_count].reason = reason_copy;
    graph->edge_count++;
}

bool
type_resolution_find_path(TypeResolutionGraph *graph,
                          size_t current,
                          size_t goal,
                          bool *visited,
                          size_t *path,
                          size_t *path_len,
                          size_t path_cap)
{
    if (graph == NULL || visited == NULL || path == NULL || path_len == NULL)
        return false;
    if (current >= graph->node_count || goal >= graph->node_count)
        return false;
    if (*path_len >= path_cap)
        return false;
    if (visited[current])
        return false;

    visited[current] = true;
    path[(*path_len)++] = current;
    if (current == goal)
        return true;

    for (size_t i = 0; i < graph->edge_count; i++) {
        TypeResolutionEdge *edge = &graph->edges[i];
        if (edge->from != current)
            continue;
        if (type_resolution_find_path(graph,
                                      edge->to,
                                      goal,
                                      visited,
                                      path,
                                      path_len,
                                      path_cap)) {
            return true;
        }
    }

    if (*path_len > 0)
        (*path_len)--;
    return false;
}

static char *
type_resolution_edge_reason(TypeResolutionGraph *graph,
                            size_t from,
                            size_t to)
{
    if (graph == NULL)
        return NULL;

    for (size_t i = 0; i < graph->edge_count; i++) {
        TypeResolutionEdge *edge = &graph->edges[i];
        if (edge->from == from && edge->to == to)
            return edge->reason;
    }

    return NULL;
}

char *
type_resolution_format_cycle(TypeResolutionGraph *graph,
                             size_t *path,
                             size_t path_len,
                             size_t closing_node)
{
    char *result = NULL;

    if (graph == NULL || path == NULL || path_len == 0)
        return type_resolution_strdup_fmt("<cycle>");

    for (size_t i = 0; i < path_len; i++) {
        const char *label = graph->nodes[path[i]].label != NULL
            ? graph->nodes[path[i]].label
            : "<node>";
        char *next;
        if (result == NULL) {
            next = type_resolution_strdup_fmt("%s", label);
        } else {
            const char *reason = type_resolution_edge_reason(
                graph, path[i - 1], path[i]);
            next = type_resolution_strdup_fmt("%s -[%s]-> %s",
                                             result,
                                             reason != NULL ? reason : "dependency",
                                             label);
        }
        free(result);
        result = next;
        if (result == NULL)
            return type_resolution_strdup_fmt("<cycle>");
    }

    {
        const char *closing = graph->nodes[closing_node].label != NULL
            ? graph->nodes[closing_node].label
            : "<node>";
        const char *reason = path_len > 0
            ? type_resolution_edge_reason(graph, path[path_len - 1], closing_node)
            : NULL;
        char *next = type_resolution_strdup_fmt("%s -[%s]-> %s",
                                               result != NULL ? result : "<cycle>",
                                               reason != NULL ? reason : "dependency",
                                               closing);
        free(result);
        result = next;
    }

    return result != NULL ? result : type_resolution_strdup_fmt("<cycle>");
}

void
semantic_type_resolution_record_named_dependency(SemanticContext *ctx,
                                                 const ASTNode *consumer_site,
                                                 const char *consumer_name,
                                                 TypeResolutionNodeKind provider_kind,
                                                 const ASTNode *provider_site,
                                                 const char *provider_name,
                                                 const char *reason)
{
    TypeResolutionGraph *graph;
    size_t from;
    size_t to;
    bool *visited = NULL;
    size_t *path = NULL;
    size_t path_len = 0;

    if (ctx == NULL)
        return;

    graph = &ctx->type_resolution_graph;
    from = type_resolution_intern_node(graph,
                                       TYPE_RES_NODE_TYPE_REF,
                                       consumer_site,
                                       consumer_name != NULL ? consumer_name : "<type-ref>");
    to = type_resolution_intern_node(graph,
                                     provider_kind,
                                     provider_site,
                                     provider_name != NULL ? provider_name : "<provider>");

    if (from != (size_t)-1 && to != (size_t)-1 && graph->node_count > 0) {
        /* Scratch arrays are populated for the immediate cycle check and
         * discarded with the semantic context scratch arena. */
        if (graph->node_count <= SIZE_MAX / sizeof(bool)
            && graph->node_count <= SIZE_MAX / sizeof(size_t)) {
            visited = pgy_arena_calloc(&ctx->scratch_arena,
                graph->node_count * sizeof(bool));
            path = pgy_arena_calloc(&ctx->scratch_arena,
                graph->node_count * sizeof(size_t));
        }
        if (visited != NULL && path != NULL) {
            bool has_cycle = (from == to)
                || type_resolution_find_path(graph,
                                             to,
                                             from,
                                             visited,
                                             path,
                                             &path_len,
                                             graph->node_count);
            if (has_cycle && consumer_site != NULL) {
                char *cycle_text = type_resolution_format_cycle(
                    graph,
                    path,
                    path_len,
                    from);
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_DEPENDENCY_CYCLE,
                    PGY_CAUSE_TYPE_RESOLUTION_CYCLE,
                    PGY_FIX_BREAK_CYCLE_VIA_INDIRECTION,
                    consumer_site,
                    "Type resolution dependency cycle detected around '%s'.\n"
                    "Reason:\n"
                    "- resolving '%s' would feed back into itself through the current dependency graph\n"
                    "- cycle path: %s\n"
                    "Fix:\n"
                    "- break the alias/default/bound dependency loop so one side becomes concrete first\n"
                    "- or split the contract into acyclic declarations",
                    consumer_name != NULL ? consumer_name : "<type-ref>",
                    consumer_name != NULL ? consumer_name : "<type-ref>",
                    cycle_text != NULL ? cycle_text : "<cycle>");
                free(cycle_text);
            }
        }
    }

    type_resolution_add_edge(graph, from, to, reason);
}

void
semantic_type_resolution_record_type_ref_dependency(SemanticContext *ctx,
                                                    const ASTNode *consumer_site,
                                                    const char *consumer_name,
                                                    const ASTNode *provider_type_ref,
                                                    const char *reason)
{
    const char *provider_name;
    ASTNode *alias_decl;
    ASTNode *decl;

    if (ctx == NULL || provider_type_ref == NULL)
        return;

    if (ast_type_name(provider_type_ref) == NULL) {
        semantic_type_resolution_record_named_dependency(
            ctx,
            consumer_site,
            consumer_name,
            TYPE_RES_NODE_TYPE_REF,
            provider_type_ref,
            "<type>",
            reason);
        return;
    }

    provider_name = ast_type_name(provider_type_ref);
    if (type_name_is_builtin_provider(provider_name)) {
        Type *builtin = type_resolution_builtin_singleton(provider_name);
        if (builtin != NULL
            && ast_type_generic_args(provider_type_ref) == NULL) {
            semantic_type_resolution_record_resolved_type(
                ctx,
                (ASTNode *)provider_type_ref,
                builtin);
        }
        semantic_type_resolution_record_named_dependency(
            ctx,
            consumer_site,
            consumer_name,
            TYPE_RES_NODE_BUILTIN,
            NULL,
            provider_name,
            reason);
        return;
    }

    alias_decl = semantic_find_type_alias_decl_by_name(ctx, provider_name);
    if (alias_decl != NULL) {
        semantic_type_resolution_record_named_dependency(
            ctx,
            consumer_site,
            consumer_name,
            TYPE_RES_NODE_ALIAS,
            alias_decl,
            provider_name,
            reason);
        return;
    }

    decl = semantic_find_class_decl_by_name(ctx, provider_name);
    if (decl != NULL) {
        semantic_type_resolution_record_named_dependency(
            ctx,
            consumer_site,
            consumer_name,
            TYPE_RES_NODE_DECL,
            decl,
            provider_name,
            reason);
        return;
    }

    semantic_type_resolution_record_named_dependency(
        ctx,
        consumer_site,
        consumer_name,
        TYPE_RES_NODE_TYPE_REF,
        provider_type_ref,
        provider_name,
        reason);
}
