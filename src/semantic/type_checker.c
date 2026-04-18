/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "slot_analyzer.h"

#define INITIAL_DIAG_CAPACITY 16

static bool
explicit_type_reference_allowed(ASTNode *decl, const ASTNode *site, SemanticContext *ctx);
static bool
semantic_is_known_stdlib_use_module(const char *module_name);
static bool
callable_contract_is_externally_visible(ASTNode *node, SemanticContext *ctx);
static size_t
generic_params_required_count(GenericParams *params);
static char *
format_type_constraint_bounds(TypeConstraint *tc);
static char *
format_generic_subject_signature(const char *name, GenericParams *params);
static char *
format_effective_generic_type_list(const char *name, Type **types, size_t count);
static void
semantic_type_resolution_record_named_dependency(SemanticContext *ctx,
                                                 const ASTNode *consumer_site,
                                                 const char *consumer_name,
                                                 TypeResolutionNodeKind provider_kind,
                                                 const ASTNode *provider_site,
                                                 const char *provider_name,
                                                 const char *reason);
static void
semantic_type_resolution_record_type_ref_dependency(SemanticContext *ctx,
                                                    const ASTNode *consumer_site,
                                                    const char *consumer_name,
                                                    const ASTNode *provider_type_ref,
                                                    const char *reason);
static bool
type_resolution_find_path(TypeResolutionGraph *graph,
                          size_t current,
                          size_t goal,
                          bool *visited,
                          size_t *path,
                          size_t *path_len,
                          size_t path_cap);
static bool
type_resolution_find_cycle_visit(TypeResolutionGraph *graph,
                                 size_t current,
                                 unsigned char *color,
                                 size_t *stack,
                                 size_t *stack_len,
                                 size_t *cycle_path,
                                 size_t *cycle_len,
                                 size_t cycle_cap,
                                 size_t *closing_node);
static char *
type_resolution_format_cycle(TypeResolutionGraph *graph,
                             size_t *path,
                             size_t path_len,
                             size_t closing_node);
static bool
type_resolution_validate_graph(SemanticContext *ctx);
static bool
type_resolution_build_topo_order(TypeResolutionGraph *graph,
                                 size_t **out_order,
                                 size_t *out_count);
static void
semantic_type_resolution_precollect_action_contract(ASTNode *method,
                                                    SemanticContext *ctx,
                                                    const char *fallback_name);
static void
semantic_type_resolution_precollect_event_inventory(ASTNode *event_decl,
                                                    SemanticContext *ctx);
static void
semantic_type_resolution_precollect_enum_inventory(ASTNode *enum_decl,
                                                   SemanticContext *ctx);
static void
semantic_type_resolution_precollect_role_inventory(ASTNode *role_decl,
                                                   SemanticContext *ctx);
static ASTNode **
collect_effective_generic_arg_nodes(GenericParams *decl_params,
                                    GenericParams *provided_args,
                                    const ASTNode *site,
                                    SemanticContext *ctx,
                                    const char *owner_kind,
                                    const char *owner_name,
                                    size_t *out_count);
static void
semantic_stage_method_array(ASTNode **methods,
                            size_t method_count,
                            SemanticContext *ctx,
                            const char *fallback_name);
static void
semantic_stage_event_signature(ASTNode *event_decl,
                               SemanticContext *ctx);
static void
semantic_type_resolution_register_local_contract_node(SemanticContext *ctx,
                                                      const ASTNode *site,
                                                      const char *label);
static void
semantic_type_resolution_record_local_contract_dependency(SemanticContext *ctx,
                                                          const ASTNode *consumer_site,
                                                          const char *consumer_label,
                                                          const ASTNode *provider_site,
                                                          const char *provider_label,
                                                          const char *reason);
static char *
semantic_type_resolution_world_zone_slot_label(ASTNode *world_decl,
                                               const char *slot_name);
static char *
semantic_type_resolution_world_state_label(ASTNode *world_decl,
                                           const char *state_name);
static char *
semantic_type_resolution_zone_slot_label(ASTNode *zone_decl,
                                         const char *slot_name);
static char *
semantic_type_resolution_zone_layer_label(ASTNode *zone_decl,
                                          const char *slot_name);
static char *
semantic_type_resolution_zone_state_label(ASTNode *zone_decl,
                                          const char *state_name);
static char *
semantic_type_resolution_projection_path_label(ASTNode *zone_decl,
                                               const char *target_slot_name,
                                               const char *source_slot_name,
                                               const char *target_field_name,
                                               const char *source_field_name);
static char *
semantic_type_resolution_projection_slot_field_label(ASTNode *zone_decl,
                                                     const char *slot_name,
                                                     const char *field_path);
static ASTNode *
semantic_type_resolution_projection_source_decl(ASTNode *zone_decl,
                                                const char *slot_name,
                                                SemanticContext *ctx);
static ASTNode *
semantic_world_find_zone_slot_local(ASTNode *world, const char *slot_name);
static ASTNode *
semantic_find_top_level_decl_by_label(ASTNode *program,
                                      const char *label,
                                      TypeResolutionNodeKind kind);
static ASTNode *
semantic_find_graph_host_decl(ASTNode *program,
                              const char *label);
static void
semantic_stage_world_local_contract_from_label(ASTNode *world_decl,
                                               const char *label,
                                               SemanticContext *ctx);
static void
semantic_stage_zone_local_contract_from_label(ASTNode *zone_decl,
                                              const char *label,
                                              SemanticContext *ctx);
static int
find_generic_param_index(GenericParams *gp, const char *param_name);
static bool
concrete_type_satisfies_bound(Type *concrete_type, ASTNode *bound_node,
                              SemanticContext *ctx);

/* Local printf-to-heap helper (same as transpiler's strdup_fmt) */
#include "type_checker_helpers.inc"
#include "type_checker_visibility.inc"
#include "type_checker_module_contracts.inc"

const char *
qubit_state_name(QubitSemanticState state)
{
    switch (state) {
    case QUBIT_STATE_NONE:           return "NONE";
    case QUBIT_STATE_SUPERPOSITION:  return "SUPERPOSITION";
    case QUBIT_STATE_ENTANGLED:      return "ENTANGLED";
    case QUBIT_STATE_COLLAPSED:      return "COLLAPSED";
    case QUBIT_STATE_CLASSICAL:      return "CLASSICAL";
    default:                         return "UNKNOWN";
    }
}

QubitSemanticState
get_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return QUBIT_STATE_NONE;
    return sym->qubit_info.semantic_state;
}

bool
set_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx,
                         QubitSemanticState new_state)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return false;
    sym->qubit_info.semantic_state = new_state;
    return true;
}

/* -----------------------------------------------------------------
 * Compile-time entanglement pool tracking
 * ----------------------------------------------------------------- */

int32_t
alloc_entangle_pool(SemanticContext *ctx)
{
    return ctx->next_entangle_pool++;
}

int32_t
get_qubit_entangle_pool(ASTNode *expr, SemanticContext *ctx)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return -1;
    return sym->qubit_info.entangle_pool_id;
}

void
set_qubit_entangle_pool(ASTNode *expr, SemanticContext *ctx,
                        int32_t pool_id)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL || !type_is_qubit(sym->type))
        return;
    sym->qubit_info.entangle_pool_id = pool_id;
}

void
merge_entangle_pools(SemanticContext *ctx,
                     int32_t dst_pool, int32_t src_pool)
{
    if (dst_pool == src_pool || dst_pool < 0 || src_pool < 0)
        return;
    /* Walk the entire scope chain and re-assign src → dst */
    for (Scope *s = ctx->scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->symbol_count; i++) {
            Symbol *sym = s->symbols[i];
            if (sym != NULL && type_is_qubit(sym->type)
                && sym->qubit_info.entangle_pool_id == src_pool) {
                sym->qubit_info.entangle_pool_id = dst_pool;
            }
        }
    }
}

static void
validate_where_clause_bounds(WhereClause *wc, SemanticContext *ctx, ASTNode *owner)
{
    if (wc == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < wc->count; i++) {
        TypeConstraint *tc = wc->constraints[i];
        if (tc == NULL)
            continue;
        for (size_t b = 0; b < tc->bound_count; b++) {
            if (tc->bounds[b] != NULL) {
                Symbol *bound_sym = NULL;
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    owner != NULL ? owner : tc->bounds[b],
                    tc->type_param != NULL ? tc->type_param : "<type-param>",
                    tc->bounds[b],
                    "where-bound lookup");
                size_t saved_diag = ctx->diagnostic_count;
                bool saved_err = ctx->has_error;
                Type *bound_type = resolve_type_node(tc->bounds[b], ctx);
                if (tc->bounds[b]->type == AST_TYPE
                    && tc->bounds[b]->data.type.name != NULL) {
                    bound_sym = scope_lookup(ctx->scope,
                                             tc->bounds[b]->data.type.name);
                }
                if (ctx->diagnostic_count > saved_diag
                    || bound_type == NULL
                    || bound_type == TYPE_UNKNOWN) {
                    ctx->diagnostic_count = saved_diag;
                    ctx->has_error = saved_err;
                    if ((bound_sym != NULL && bound_sym->kind == SYMBOL_ABILITY)
                        || (tc->bounds[b]->type == AST_TYPE
                            && tc->bounds[b]->data.type.name != NULL
                            && ctx->program_root != NULL
                            && find_ability_decl_by_name(
                                   ctx->program_root,
                                   tc->bounds[b]->data.type.name) != NULL)) {
                        continue;
                    }
                    semantic_error(ctx, owner != NULL ? owner : tc->bounds[b],
                        "Unknown constraint type '%s' in where clause.\n"
                        "Reason:\n"
                        "- generic where-clause validation could not resolve this bound\n"
                        "- every bound in a multi-bound contract must resolve before specialization can be trusted\n"
                        "Fix:\n"
                        "- declare or import '%s'\n"
                        "- or remove the unresolved bound from the where-clause",
                        tc->bounds[b]->data.type.name,
                        tc->bounds[b]->data.type.name);
                }
            }
        }
    }
}

static void
validate_generic_param_defaults(GenericParams *gp, SemanticContext *ctx,
                                ASTNode *owner, const char *kind_name)
{
    bool saw_default = false;

    if (gp == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < gp->count; i++) {
        GenericParam *param = gp->params[i];
        if (param == NULL)
            continue;
        if (param->default_type != NULL) {
            saw_default = true;
        } else if (saw_default) {
            semantic_error(ctx, owner != NULL ? owner : (ASTNode *)gp,
                "Non-trailing default generic parameter '%s' in %s declaration.\n"
                "Reason:\n"
                "- a required generic parameter appears after a parameter with a default\n"
                "- generic defaults are only closed for trailing parameters\n"
                "Fix:\n"
                "- move '%s' before all defaulted generic parameters\n"
                "- or give '%s' a default type argument too",
                param->name != NULL ? param->name : "<type-param>",
                kind_name != NULL ? kind_name : "generic",
                param->name != NULL ? param->name : "<type-param>",
                param->name != NULL ? param->name : "<type-param>");
        }
        if (param == NULL || param->default_type == NULL)
            continue;
        {
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                owner != NULL ? owner : param->default_type,
                param->name != NULL ? param->name : "<type-param>",
                param->default_type,
                "default-type lookup");
            size_t saved_diag = ctx->diagnostic_count;
            bool saved_err = ctx->has_error;
            Type *resolved = resolve_type_node(param->default_type, ctx);
            if (resolved == NULL || resolved == TYPE_UNKNOWN
                || ctx->diagnostic_count > saved_diag) {
                ctx->diagnostic_count = saved_diag;
                ctx->has_error = saved_err;
                semantic_error(ctx, owner != NULL ? owner : param->default_type,
                    "Invalid default generic type argument '%s' in %s declaration (parameter '%s').\n"
                    "Reason:\n"
                    "- the declared default type could not be resolved as a valid concrete type\n"
                    "- generic defaults must be fully valid before they can participate in effective-argument derivation\n"
                    "Fix:\n"
                    "- replace '%s' with a resolvable concrete type\n"
                    "- or remove the default and require the caller to supply it",
                    param->default_type->type == AST_TYPE
                        && param->default_type->data.type.name != NULL
                            ? param->default_type->data.type.name
                            : "<type>",
                    kind_name != NULL ? kind_name : "generic",
                    param->name != NULL ? param->name : "<type-param>",
                    param->default_type->type == AST_TYPE
                        && param->default_type->data.type.name != NULL
                            ? param->default_type->data.type.name
                            : "<type>");
            }
        }
    }
}

static char *
format_type_constraint_bounds(TypeConstraint *tc)
{
    char *result = NULL;

    if (tc == NULL || tc->bound_count == 0)
        return tc_strdup_fmt("<constraint>");

    for (size_t i = 0; i < tc->bound_count; i++) {
        ASTNode *bound = tc->bounds[i];
        const char *bound_name =
            (bound != NULL
             && bound->type == AST_TYPE
             && bound->data.type.name != NULL)
                ? bound->data.type.name
                : "<constraint>";
        char *next;

        if (result == NULL) {
            result = tc_strdup_fmt("%s", bound_name);
        } else {
            next = tc_strdup_fmt("%s + %s", result, bound_name);
            free(result);
            result = next;
        }

        if (result == NULL)
            return tc_strdup_fmt("<constraint>");
    }

    return result;
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

static size_t
type_resolution_intern_node(TypeResolutionGraph *graph,
                            TypeResolutionNodeKind kind,
                            const ASTNode *site,
                            const char *label)
{
    TypeResolutionNode *grown;

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
        size_t new_cap = graph->node_capacity == 0 ? 16 : graph->node_capacity * 2;
        grown = realloc(graph->nodes, new_cap * sizeof(TypeResolutionNode));
        if (grown == NULL)
            return (size_t)-1;
        graph->nodes = grown;
        graph->node_capacity = new_cap;
    }

    graph->nodes[graph->node_count].kind = kind;
    graph->nodes[graph->node_count].site = site;
    graph->nodes[graph->node_count].label =
        label != NULL ? pergyra_strdup(label) : NULL;
    return graph->node_count++;
}

static void
type_resolution_add_edge(TypeResolutionGraph *graph,
                         size_t from,
                         size_t to,
                         const char *reason)
{
    TypeResolutionEdge *grown;

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
        size_t new_cap = graph->edge_capacity == 0 ? 32 : graph->edge_capacity * 2;
        grown = realloc(graph->edges, new_cap * sizeof(TypeResolutionEdge));
        if (grown == NULL)
            return;
        graph->edges = grown;
        graph->edge_capacity = new_cap;
    }

    graph->edges[graph->edge_count].from = from;
    graph->edges[graph->edge_count].to = to;
    graph->edges[graph->edge_count].reason =
        reason != NULL ? pergyra_strdup(reason) : NULL;
    graph->edge_count++;
}

static bool
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

static char *
type_resolution_format_cycle(TypeResolutionGraph *graph,
                             size_t *path,
                             size_t path_len,
                             size_t closing_node)
{
    char *result = NULL;

    if (graph == NULL || path == NULL || path_len == 0)
        return tc_strdup_fmt("<cycle>");

    for (size_t i = 0; i < path_len; i++) {
        const char *label = graph->nodes[path[i]].label != NULL
            ? graph->nodes[path[i]].label
            : "<node>";
        char *next;
        if (result == NULL) {
            next = tc_strdup_fmt("%s", label);
        } else {
            const char *reason = type_resolution_edge_reason(
                graph, path[i - 1], path[i]);
            next = tc_strdup_fmt("%s -[%s]-> %s",
                                 result,
                                 reason != NULL ? reason : "dependency",
                                 label);
        }
        free(result);
        result = next;
        if (result == NULL)
            return tc_strdup_fmt("<cycle>");
    }

    {
        const char *closing = graph->nodes[closing_node].label != NULL
            ? graph->nodes[closing_node].label
            : "<node>";
        const char *reason = path_len > 0
            ? type_resolution_edge_reason(graph, path[path_len - 1], closing_node)
            : NULL;
        char *next = tc_strdup_fmt("%s -[%s]-> %s",
                                   result != NULL ? result : "<cycle>",
                                   reason != NULL ? reason : "dependency",
                                   closing);
        free(result);
        result = next;
    }

    return result != NULL ? result : tc_strdup_fmt("<cycle>");
}

static bool
type_resolution_find_cycle_visit(TypeResolutionGraph *graph,
                                 size_t current,
                                 unsigned char *color,
                                 size_t *stack,
                                 size_t *stack_len,
                                 size_t *cycle_path,
                                 size_t *cycle_len,
                                 size_t cycle_cap,
                                 size_t *closing_node)
{
    if (graph == NULL || color == NULL || stack == NULL || stack_len == NULL
        || cycle_path == NULL || cycle_len == NULL || closing_node == NULL) {
        return false;
    }

    color[current] = 1;
    stack[(*stack_len)++] = current;

    for (size_t i = 0; i < graph->edge_count; i++) {
        TypeResolutionEdge *edge = &graph->edges[i];
        if (edge->from != current)
            continue;

        if (color[edge->to] == 0) {
            if (type_resolution_find_cycle_visit(graph,
                                                 edge->to,
                                                 color,
                                                 stack,
                                                 stack_len,
                                                 cycle_path,
                                                 cycle_len,
                                                 cycle_cap,
                                                 closing_node)) {
                return true;
            }
        } else if (color[edge->to] == 1) {
            size_t start = 0;
            while (start < *stack_len && stack[start] != edge->to)
                start++;
            *cycle_len = 0;
            for (size_t j = start; j < *stack_len && *cycle_len < cycle_cap; j++)
                cycle_path[(*cycle_len)++] = stack[j];
            *closing_node = edge->to;
            return true;
        }
    }

    if (*stack_len > 0)
        (*stack_len)--;
    color[current] = 2;
    return false;
}

static bool
type_resolution_validate_graph(SemanticContext *ctx)
{
    TypeResolutionGraph *graph;
    unsigned char *color = NULL;
    size_t *stack = NULL;
    size_t *cycle_path = NULL;
    size_t stack_len = 0;
    size_t cycle_len = 0;
    size_t closing_node = (size_t)-1;
    bool ok = true;

    if (ctx == NULL)
        return false;

    graph = &ctx->type_resolution_graph;
    if (graph->node_count == 0)
        return true;

    color = calloc(graph->node_count, sizeof(unsigned char));
    stack = calloc(graph->node_count, sizeof(size_t));
    cycle_path = calloc(graph->node_count, sizeof(size_t));
    if (color == NULL || stack == NULL || cycle_path == NULL) {
        free(color);
        free(stack);
        free(cycle_path);
        return false;
    }

    for (size_t i = 0; i < graph->node_count; i++) {
        if (color[i] != 0)
            continue;
        stack_len = 0;
        cycle_len = 0;
        closing_node = (size_t)-1;
        if (type_resolution_find_cycle_visit(graph,
                                             i,
                                             color,
                                             stack,
                                             &stack_len,
                                             cycle_path,
                                             &cycle_len,
                                             graph->node_count,
                                             &closing_node)) {
            ASTNode *site = (ASTNode *)graph->nodes[closing_node].site;
            char *cycle_text = type_resolution_format_cycle(
                graph, cycle_path, cycle_len, closing_node);
            semantic_error(ctx, site,
                "Type resolution dependency cycle detected in the semantic graph around '%s'.\n"
                "Reason:\n"
                "- provider/consumer resolution formed a closed dependency loop\n"
                "- cycle path: %s\n"
                "Fix:\n"
                "- break the generic/alias/ability dependency loop so one edge resolves first\n"
                "- or split the contract into acyclic declarations",
                graph->nodes[closing_node].label != NULL
                    ? graph->nodes[closing_node].label : "<type-ref>",
                cycle_text != NULL ? cycle_text : "<cycle>");
            free(cycle_text);
            ok = false;
            break;
        }
    }

    free(color);
    free(stack);
    free(cycle_path);
    return ok;
}

static bool
type_resolution_build_topo_order(TypeResolutionGraph *graph,
                                 size_t **out_order,
                                 size_t *out_count)
{
    size_t *indegree = NULL;
    size_t *queue = NULL;
    size_t *order = NULL;
    size_t head = 0;
    size_t tail = 0;
    size_t produced = 0;

    if (out_order != NULL)
        *out_order = NULL;
    if (out_count != NULL)
        *out_count = 0;
    if (graph == NULL)
        return false;
    if (graph->node_count == 0)
        return true;

    indegree = calloc(graph->node_count, sizeof(size_t));
    queue = calloc(graph->node_count, sizeof(size_t));
    order = calloc(graph->node_count, sizeof(size_t));
    if (indegree == NULL || queue == NULL || order == NULL) {
        free(indegree);
        free(queue);
        free(order);
        return false;
    }

    for (size_t i = 0; i < graph->edge_count; i++) {
        if (graph->edges[i].to < graph->node_count)
            indegree[graph->edges[i].to]++;
    }

    for (size_t i = 0; i < graph->node_count; i++) {
        if (indegree[i] == 0)
            queue[tail++] = i;
    }

    while (head < tail) {
        size_t node = queue[head++];
        order[produced++] = node;
        for (size_t i = 0; i < graph->edge_count; i++) {
            TypeResolutionEdge *edge = &graph->edges[i];
            if (edge->from != node || edge->to >= graph->node_count)
                continue;
            if (indegree[edge->to] > 0)
                indegree[edge->to]--;
            if (indegree[edge->to] == 0)
                queue[tail++] = edge->to;
        }
    }

    free(indegree);
    free(queue);

    if (produced != graph->node_count) {
        free(order);
        return false;
    }

    if (out_order != NULL)
        *out_order = order;
    else
        free(order);
    if (out_count != NULL)
        *out_count = produced;
    return true;
}

static void
semantic_type_resolution_collect_type_refs(ASTNode *type_node,
                                           SemanticContext *ctx,
                                           const ASTNode *consumer_site,
                                           const char *consumer_name,
                                           const char *reason)
{
    if (type_node == NULL || ctx == NULL || consumer_name == NULL)
        return;

    switch (type_node->type) {
    case AST_CHANNEL_TYPE:
        semantic_type_resolution_collect_type_refs(
            type_node->data.channel_type.element_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        return;

    case AST_FUTURE_TYPE:
        semantic_type_resolution_collect_type_refs(
            type_node->data.future_type.value_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        return;

    case AST_EVENT_HANDLER_TYPE:
        for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
            semantic_type_resolution_collect_type_refs(
                type_node->data.event_handler_type.param_types[i],
                ctx,
                consumer_site,
                consumer_name,
                reason);
        }
        semantic_type_resolution_collect_type_refs(
            type_node->data.event_handler_type.return_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        return;

    case AST_TYPE:
        if (type_node->data.type.name != NULL) {
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                consumer_site != NULL ? consumer_site : type_node,
                consumer_name,
                type_node,
                reason != NULL ? reason : "type dependency");
        }
        if (type_node->data.type.generic_args != NULL) {
            for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
                GenericParam *gp = type_node->data.type.generic_args->params[i];
                if (gp != NULL && gp->constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        gp->constraint,
                        ctx,
                        consumer_site,
                        consumer_name,
                        reason);
                }
            }
        }
        return;

    default:
        return;
    }
}

static void
semantic_type_resolution_collect_generic_contract_inventory(GenericParams *gp,
                                                            WhereClause *wc,
                                                            SemanticContext *ctx,
                                                            const ASTNode *owner,
                                                            const char *owner_kind,
                                                            const char *owner_name)
{
    if (ctx == NULL)
        return;

    if (gp != NULL) {
        for (size_t i = 0; i < gp->count; i++) {
            GenericParam *param = gp->params[i];
            char *consumer_name;

            if (param == NULL)
                continue;

            consumer_name = tc_strdup_fmt("%s %s.%s",
                                          owner_kind != NULL ? owner_kind : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          param->name != NULL ? param->name : "<type-param>");
            if (consumer_name == NULL)
                continue;

            semantic_type_resolution_collect_type_refs(
                param->default_type,
                ctx,
                owner,
                consumer_name,
                "default-type lookup");
            free(consumer_name);
        }
    }

    if (wc != NULL) {
        for (size_t i = 0; i < wc->count; i++) {
            TypeConstraint *tc = wc->constraints[i];
            char *consumer_name;

            if (tc == NULL)
                continue;

            consumer_name = tc_strdup_fmt("%s %s.%s",
                                          owner_kind != NULL ? owner_kind : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          tc->type_param != NULL ? tc->type_param : "<type-param>");
            if (consumer_name == NULL)
                continue;

            for (size_t b = 0; b < tc->bound_count; b++) {
                semantic_type_resolution_collect_type_refs(
                    tc->bounds[b],
                    ctx,
                    owner != NULL ? owner : tc->bounds[b],
                    consumer_name,
                    "where-bound lookup");
            }
            free(consumer_name);
        }
    }
}

static void
semantic_type_resolution_record_string_dependency(SemanticContext *ctx,
                                                  const ASTNode *consumer_site,
                                                  const char *consumer_name,
                                                  const char *provider_name,
                                                  const char *reason)
{
    if (provider_name == NULL || provider_name[0] == '\0')
        return;

    semantic_type_resolution_record_named_dependency(
        ctx,
        consumer_site,
        consumer_name,
        TYPE_RES_NODE_DECL,
        NULL,
        provider_name,
        reason);
}

static void
semantic_type_resolution_precollect_required_abilities(ASTNode **ability_refs,
                                                       size_t ability_count,
                                                       SemanticContext *ctx,
                                                       const ASTNode *owner,
                                                       const char *consumer_name,
                                                       const char *reason)
{
    if (ability_refs == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        semantic_type_resolution_collect_type_refs(
            ability_refs[i],
            ctx,
            owner,
            consumer_name,
            reason);
    }
}

static void
semantic_type_resolution_precollect_ability_inventory(ASTNode *ability_decl,
                                                      SemanticContext *ctx)
{
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        ability_decl->data.ability_decl.generic_params,
        ability_decl->data.ability_decl.where_clause,
        ctx,
        ability_decl,
        "ability",
        ability_decl->data.ability_decl.name);

    for (size_t i = 0; i < ability_decl->data.ability_decl.require_count; i++) {
        ASTNode *req = ability_decl->data.ability_decl.require_fields[i];
        char *consumer_name;

        if (req == NULL || req->type != AST_REQUIRE_FIELD)
            continue;

        consumer_name = tc_strdup_fmt("ability %s.%s",
                                      ability_decl->data.ability_decl.name != NULL
                                          ? ability_decl->data.ability_decl.name : "<ability>",
                                      req->data.require_field.name != NULL
                                          ? req->data.require_field.name : "<require-field>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                req->data.require_field.type,
                ctx,
                req,
                consumer_name,
                "ability require-field type lookup");
            free(consumer_name);
        }
    }

    for (size_t i = 0; i < ability_decl->data.ability_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            ability_decl->data.ability_decl.methods[i],
            ctx,
            ability_decl->data.ability_decl.name);
    }
}

static void
semantic_type_resolution_precollect_event_inventory(ASTNode *event_decl,
                                                    SemanticContext *ctx)
{
    if (event_decl == NULL || event_decl->type != AST_EVENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < event_decl->data.event_decl.param_count; i++) {
        ASTNode *param = event_decl->data.event_decl.params[i];
        char *consumer_name;

        if (param == NULL || param->type != AST_LET_DECL)
            continue;

        consumer_name = tc_strdup_fmt("event %s.%s",
                                      event_decl->data.event_decl.name != NULL
                                          ? event_decl->data.event_decl.name : "<event>",
                                      param->data.let_decl.name != NULL
                                          ? param->data.let_decl.name : "<param>");
        if (consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            param->data.let_decl.type,
            ctx,
            event_decl,
            consumer_name,
            "event parameter type lookup");
        free(consumer_name);
    }

    semantic_type_resolution_collect_type_refs(
        event_decl->data.event_decl.return_type,
        ctx,
        event_decl,
        event_decl->data.event_decl.name != NULL
            ? event_decl->data.event_decl.name : "<event>",
        "event return type lookup");
}

static void
semantic_type_resolution_precollect_enum_inventory(ASTNode *enum_decl,
                                                   SemanticContext *ctx)
{
    if (enum_decl == NULL || enum_decl->type != AST_ENUM_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < enum_decl->data.enum_decl.variant_count; i++) {
        ASTNode **params = enum_decl->data.enum_decl.variant_params != NULL
            ? enum_decl->data.enum_decl.variant_params[i] : NULL;
        size_t param_count = enum_decl->data.enum_decl.variant_param_counts != NULL
            ? enum_decl->data.enum_decl.variant_param_counts[i] : 0;
        const char *variant_name = enum_decl->data.enum_decl.variants != NULL
            ? enum_decl->data.enum_decl.variants[i] : NULL;
        char *consumer_name;

        if (params == NULL || param_count == 0)
            continue;

        consumer_name = tc_strdup_fmt("enum %s.%s",
                                      enum_decl->data.enum_decl.name != NULL
                                          ? enum_decl->data.enum_decl.name : "<enum>",
                                      variant_name != NULL ? variant_name : "<variant>");
        if (consumer_name == NULL)
            continue;

        for (size_t j = 0; j < param_count; j++) {
            semantic_type_resolution_collect_type_refs(
                params[j],
                ctx,
                enum_decl,
                consumer_name,
                "enum variant payload type lookup");
        }
        free(consumer_name);
    }

    semantic_stage_method_array(
        enum_decl->data.enum_decl.methods,
        enum_decl->data.enum_decl.method_count,
        ctx,
        enum_decl->data.enum_decl.name);
}

static void
semantic_type_resolution_precollect_role_inventory(ASTNode *role_decl,
                                                   SemanticContext *ctx)
{
    if (role_decl == NULL || role_decl->type != AST_ROLE_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        role_decl->data.role_decl.generic_params,
        role_decl->data.role_decl.where_clause,
        ctx,
        role_decl,
        "role",
        role_decl->data.role_decl.name);

    semantic_type_resolution_collect_type_refs(
        role_decl->data.role_decl.for_type,
        ctx,
        role_decl,
        role_decl->data.role_decl.name != NULL
            ? role_decl->data.role_decl.name : "<role>",
        "role host-type lookup");

    for (size_t i = 0; i < role_decl->data.role_decl.include_count; i++) {
        ASTNode *inc = role_decl->data.role_decl.includes[i];
        char *consumer_name;

        if (inc == NULL || inc->type != AST_INCLUDE_STMT)
            continue;

        consumer_name = tc_strdup_fmt("role %s.include",
                                      role_decl->data.role_decl.name != NULL
                                          ? role_decl->data.role_decl.name : "<role>");
        if (consumer_name == NULL)
            continue;

        semantic_type_resolution_record_string_dependency(
            ctx,
            inc,
            consumer_name,
            inc->data.include_stmt.role_name,
            "role include lookup");

        if (inc->data.include_stmt.type_args != NULL) {
            for (size_t j = 0; j < inc->data.include_stmt.type_args->count; j++) {
                GenericParam *arg = inc->data.include_stmt.type_args->params[j];
                if (arg != NULL && arg->constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        arg->constraint,
                        ctx,
                        inc,
                        consumer_name,
                        "role include type-argument lookup");
                }
            }
        }
        free(consumer_name);
    }

    for (size_t i = 0; i < role_decl->data.role_decl.impl_count; i++) {
        ASTNode *impl = role_decl->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        semantic_type_resolution_collect_type_refs(
            impl->data.impl_ability.ability_ref,
            ctx,
            impl,
            role_decl->data.role_decl.name != NULL
                ? role_decl->data.role_decl.name : "<role>",
            "role impl ability lookup");
    }
}

static void
semantic_type_resolution_precollect_action_contract(ASTNode *method,
                                                    SemanticContext *ctx,
                                                    const char *fallback_name)
{
    const char *consumer_name;

    if (method == NULL || method->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = method->data.func_decl.name != NULL
        ? method->data.func_decl.name
        : (fallback_name != NULL ? fallback_name : "<action>");

    semantic_type_resolution_collect_generic_contract_inventory(
        method->data.func_decl.generic_params,
        method->data.func_decl.where_clause,
        ctx,
        method,
        "func",
        consumer_name);

    for (size_t i = 0; i < method->data.func_decl.param_count; i++) {
        FuncParam *param = method->data.func_decl.params[i];
        char *param_consumer_name;

        if (param == NULL)
            continue;

        param_consumer_name = tc_strdup_fmt("func %s.%s",
                                            consumer_name,
                                            param->name != NULL ? param->name : "<param>");
        if (param_consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                param->type,
                ctx,
                method,
                param_consumer_name,
                "function parameter type lookup");
            free(param_consumer_name);
        }
    }

    semantic_type_resolution_collect_type_refs(
        method->data.func_decl.return_type,
        ctx,
        method,
        consumer_name,
        "function return type lookup");

    semantic_type_resolution_precollect_required_abilities(
        method->data.func_decl.required_abilities,
        method->data.func_decl.required_ability_count,
        ctx,
        method,
        consumer_name,
        "action ability consumer lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        method->data.func_decl.within_zone,
        "action within-zone lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        method->data.func_decl.causes_effect,
        "action causes-effect lookup");
}

static void
semantic_type_resolution_precollect_class_inventory(ASTNode *class_decl,
                                                    SemanticContext *ctx)
{
    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        class_decl->data.class_decl.generic_params,
        class_decl->data.class_decl.where_clause,
        ctx,
        class_decl,
        "class",
        class_decl->data.class_decl.name);

    for (size_t i = 0; i < class_decl->data.class_decl.field_count; i++) {
        ClassField *field = class_decl->data.class_decl.fields[i];
        char *consumer_name;

        if (field == NULL)
            continue;

        consumer_name = tc_strdup_fmt("class %s.%s",
                                      class_decl->data.class_decl.name != NULL
                                          ? class_decl->data.class_decl.name : "<class>",
                                      field->name != NULL ? field->name : "<field>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                field->type,
                ctx,
                class_decl,
                consumer_name,
                "class field type lookup");
            free(consumer_name);
        }
    }

    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        semantic_type_resolution_precollect_action_contract(
            method,
            ctx,
            class_decl->data.class_decl.name);
    }
}

static void
semantic_type_resolution_precollect_party_inventory(ASTNode *party_decl,
                                                    SemanticContext *ctx)
{
    if (party_decl == NULL || party_decl->type != AST_PARTY_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        party_decl->data.party_decl.generic_params,
        NULL,
        ctx,
        party_decl,
        "party",
        party_decl->data.party_decl.name);

    for (size_t i = 0; i < party_decl->data.party_decl.shared_count; i++) {
        ASTNode *field = party_decl->data.party_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<party-shared>",
            "party shared field type lookup");
    }

    for (size_t i = 0; i < party_decl->data.party_decl.role_count; i++) {
        ASTNode *role_slot = party_decl->data.party_decl.role_slots[i];
        char *consumer_name;

        if (role_slot == NULL || role_slot->type != AST_ROLE_SLOT)
            continue;

        consumer_name = tc_strdup_fmt("party %s.%s",
                                      party_decl->data.party_decl.name != NULL
                                          ? party_decl->data.party_decl.name : "<party>",
                                      role_slot->data.role_slot.slot_name != NULL
                                          ? role_slot->data.role_slot.slot_name : "<role-slot>");
        if (consumer_name == NULL)
            continue;
        semantic_type_resolution_precollect_required_abilities(
            role_slot->data.role_slot.required_abilities,
            role_slot->data.role_slot.ability_count,
            ctx,
            role_slot,
            consumer_name,
            "party role slot ability consumer lookup");
        free(consumer_name);
    }

    for (size_t i = 0; i < party_decl->data.party_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            party_decl->data.party_decl.methods[i],
            ctx,
            party_decl->data.party_decl.name);
    }
}

static void
semantic_type_resolution_precollect_roster_inventory(ASTNode *roster_decl,
                                                     SemanticContext *ctx)
{
    if (roster_decl == NULL || roster_decl->type != AST_ROSTER_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        roster_decl->data.roster_decl.generic_params,
        NULL,
        ctx,
        roster_decl,
        "roster",
        roster_decl->data.roster_decl.name);

    for (size_t i = 0; i < roster_decl->data.roster_decl.shared_count; i++) {
        ASTNode *field = roster_decl->data.roster_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<roster-shared>",
            "roster shared field type lookup");
    }

    for (size_t i = 0; i < roster_decl->data.roster_decl.party_count; i++) {
        ASTNode *slot = roster_decl->data.roster_decl.party_slots[i];
        if (slot == NULL || slot->type != AST_SYSTEMIC_SLOT)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            slot,
            slot->data.roster_slot.slot_name != NULL
                ? slot->data.roster_slot.slot_name : "<roster-slot>",
            slot->data.roster_slot.party_type,
            "roster party lookup");
    }

    for (size_t i = 0; i < roster_decl->data.roster_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            roster_decl->data.roster_decl.methods[i],
            ctx,
            roster_decl->data.roster_decl.name);
    }
}

static void
semantic_type_resolution_precollect_world_inventory(ASTNode *world_decl,
                                                    SemanticContext *ctx)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        char *zone_slot_label;

        if (zone == NULL || zone->type != AST_WORLD_ZONE)
            continue;

        zone_slot_label = semantic_type_resolution_world_zone_slot_label(
            world_decl,
            zone->data.world_zone.slot_name);
        if (zone_slot_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, zone, zone_slot_label);
            free(zone_slot_label);
        }
    }

    for (size_t i = 0; i < world_decl->data.world_decl.shared_count; i++) {
        ASTNode *field = world_decl->data.world_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<world-shared>",
            "world shared field type lookup");
    }

    for (size_t i = 0; i < world_decl->data.world_decl.roster_count; i++) {
        ASTNode *roster = world_decl->data.world_decl.rosters[i];
        if (roster == NULL || roster->type != AST_WORLD_SYSTEMIC)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            roster,
            roster->data.world_roster.slot_name != NULL
                ? roster->data.world_roster.slot_name : "<world-roster>",
            roster->data.world_roster.roster_type,
            "world roster lookup");
    }

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone == NULL || zone->type != AST_WORLD_ZONE)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            zone,
            zone->data.world_zone.slot_name != NULL
                ? zone->data.world_zone.slot_name : "<world-zone>",
            zone->data.world_zone.zone_type,
            "world zone lookup");
    }

    for (size_t i = 0; i < world_decl->data.world_decl.state_count; i++) {
        ASTNode *state = world_decl->data.world_decl.states[i];
        ASTNode *zone_slot_decl = NULL;
        char *state_label;

        if (state == NULL || state->type != AST_WORLD_STATE)
            continue;

        state_label = semantic_type_resolution_world_state_label(
            world_decl,
            state->data.world_state.state_name);
        if (state_label == NULL)
            continue;

        semantic_type_resolution_register_local_contract_node(
            ctx, state, state_label);

        zone_slot_decl = semantic_world_find_zone_slot_local(
            world_decl,
            state->data.world_state.zone_slot_name);

        if (state->data.world_state.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                state->data.world_state.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    zone_slot_label,
                    "world state zone-slot lookup");
                free(zone_slot_label);
            }
        }

        if (zone_slot_decl != NULL
            && zone_slot_decl->type == AST_WORLD_ZONE
            && zone_slot_decl->data.world_zone.zone_type != NULL
            && state->data.world_state.detail_name != NULL) {
            ASTNode *zone_type_decl = find_domain_decl_by_name(
                ctx->program_root,
                AST_ZONE_DECL,
                zone_slot_decl->data.world_zone.zone_type);
            if (zone_type_decl != NULL) {
                if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_PROJECTION) {
                    char *projection_label = semantic_type_resolution_zone_slot_label(
                        zone_type_decl,
                        state->data.world_state.detail_name);
                    if (projection_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            zone_type_decl,
                            projection_label,
                            "world state projection lookup");
                        free(projection_label);
                    }
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_LAYER) {
                    char *layer_label = semantic_type_resolution_zone_layer_label(
                        zone_type_decl,
                        state->data.world_state.detail_name);
                    if (layer_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            zone_type_decl,
                            layer_label,
                            "world state layer lookup");
                        free(layer_label);
                    }
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_STATE) {
                    char *nested_state_label = semantic_type_resolution_zone_state_label(
                        zone_type_decl,
                        state->data.world_state.detail_name);
                    if (nested_state_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            zone_type_decl,
                            nested_state_label,
                            "world state nested-state lookup");
                        free(nested_state_label);
                    }
                }
            }
        }

        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                char *input_state_label = semantic_type_resolution_world_state_label(
                    world_decl,
                    input_name);
                if (input_state_label != NULL) {
                    semantic_type_resolution_record_local_contract_dependency(
                        ctx,
                        state,
                        state_label,
                        NULL,
                        input_state_label,
                        "world state composition input lookup");
                    free(input_state_label);
                }

                if (input_name != NULL) {
                    char *input_zone_label = semantic_type_resolution_world_zone_slot_label(
                        world_decl,
                        input_name);
                    if (input_zone_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            NULL,
                            input_zone_label,
                            "world state composition zone-input lookup");
                        free(input_zone_label);
                    }
                }
            }
        }

        free(state_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.activate_count; i++) {
        ASTNode *activate = world_decl->data.world_decl.activations[i];
        char *consumer_label;

        if (activate == NULL || activate->type != AST_WORLD_ACTIVATE)
            continue;

        consumer_label = tc_strdup_fmt("world %s.activate.%s",
                                       world_decl->data.world_decl.name != NULL
                                           ? world_decl->data.world_decl.name : "<world>",
                                       activate->data.world_activate.state_name != NULL
                                           ? activate->data.world_activate.state_name
                                           : (activate->data.world_activate.zone_slot_name != NULL
                                               ? activate->data.world_activate.zone_slot_name
                                               : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (activate->data.world_activate.state_name != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                activate->data.world_activate.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    activate,
                    consumer_label,
                    NULL,
                    state_label,
                    "world activate state lookup");
                free(state_label);
            }
        } else if (activate->data.world_activate.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                activate->data.world_activate.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    activate,
                    consumer_label,
                    NULL,
                    zone_slot_label,
                    "world activate zone lookup");
                free(zone_slot_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.deactivate_count; i++) {
        ASTNode *deactivate = world_decl->data.world_decl.deactivations[i];
        char *consumer_label;

        if (deactivate == NULL || deactivate->type != AST_WORLD_DEACTIVATE)
            continue;

        consumer_label = tc_strdup_fmt("world %s.deactivate.%s",
                                       world_decl->data.world_decl.name != NULL
                                           ? world_decl->data.world_decl.name : "<world>",
                                       deactivate->data.world_deactivate.state_name != NULL
                                           ? deactivate->data.world_deactivate.state_name
                                           : (deactivate->data.world_deactivate.zone_slot_name != NULL
                                               ? deactivate->data.world_deactivate.zone_slot_name
                                               : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (deactivate->data.world_deactivate.state_name != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                deactivate->data.world_deactivate.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    deactivate,
                    consumer_label,
                    NULL,
                    state_label,
                    "world deactivate state lookup");
                free(state_label);
            }
        } else if (deactivate->data.world_deactivate.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                deactivate->data.world_deactivate.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    deactivate,
                    consumer_label,
                    NULL,
                    zone_slot_label,
                    "world deactivate zone lookup");
                free(zone_slot_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.maintained_zone_count; i++) {
        ASTNode *maintain = world_decl->data.world_decl.maintained_zones[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_WORLD_MAINTAIN)
            continue;

        consumer_label = tc_strdup_fmt("world %s.maintain.%s",
                                       world_decl->data.world_decl.name != NULL
                                           ? world_decl->data.world_decl.name : "<world>",
                                       maintain->data.world_maintain.state_name != NULL
                                           ? maintain->data.world_maintain.state_name
                                           : (maintain->data.world_maintain.zone_slot_name != NULL
                                               ? maintain->data.world_maintain.zone_slot_name
                                               : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (maintain->data.world_maintain.state_name != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                maintain->data.world_maintain.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    state_label,
                    "world maintain state lookup");
                free(state_label);
            }
        } else if (maintain->data.world_maintain.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                maintain->data.world_maintain.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    zone_slot_label,
                    "world maintain zone lookup");
                free(zone_slot_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            world_decl->data.world_decl.methods[i],
            ctx,
            world_decl->data.world_decl.name);
    }
}

static void
semantic_type_resolution_precollect_domain_inventory(ASTNode *domain_decl,
                                                     ASTNode **slots,
                                                     size_t slot_count,
                                                     ASTNode **methods,
                                                     size_t method_count,
                                                     SemanticContext *ctx,
                                                     const char *kind_name,
                                                     const char *decl_name)
{
    (void)domain_decl;

    if (ctx == NULL)
        return;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        semantic_type_resolution_collect_type_refs(
            slot->data.domain_slot.type,
            ctx,
            slot,
            slot->data.domain_slot.slot_name != NULL
                ? slot->data.domain_slot.slot_name : "<domain-slot>",
            "domain slot type lookup");
    }

    for (size_t i = 0; i < method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            methods[i],
            ctx,
            decl_name != NULL ? decl_name : kind_name);
    }
}

static void
semantic_type_resolution_precollect_intent_inventory(ASTNode *intent_decl,
                                                     SemanticContext *ctx)
{
    if (intent_decl == NULL || intent_decl->type != AST_INTENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent_decl->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        semantic_type_resolution_collect_type_refs(
            involves->data.intent_involves.subject_type,
            ctx,
            involves,
            involves->data.intent_involves.alias != NULL
                ? involves->data.intent_involves.alias : "<intent-binding>",
            "intent binding type lookup");
    }

    for (size_t i = 0; i < intent_decl->data.intent_decl.value_count; i++) {
        ASTNode *value = intent_decl->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        semantic_type_resolution_collect_type_refs(
            value->data.intent_value.value_type,
            ctx,
            value,
            value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias : "<intent-value>",
            "intent value type lookup");
    }

    semantic_type_resolution_collect_type_refs(
        intent_decl->data.intent_decl.default_where_type,
        ctx,
        intent_decl,
        intent_decl->data.intent_decl.name != NULL
            ? intent_decl->data.intent_decl.name : "<intent>",
        "intent default zone lookup");

    for (size_t i = 0; i < intent_decl->data.intent_decl.step_count; i++) {
        ASTNode *step = intent_decl->data.intent_decl.steps[i];
        char *step_consumer_name;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        step_consumer_name = tc_strdup_fmt(
            "intent %s.%s",
            intent_decl->data.intent_decl.name != NULL
                ? intent_decl->data.intent_decl.name : "<intent>",
            step->data.intent_step.name != NULL
                ? step->data.intent_step.name : "<step>");
        if (step_consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            step->data.intent_step.where_type,
            ctx,
            step,
            step_consumer_name,
            "intent step zone lookup");
        semantic_type_resolution_precollect_required_abilities(
            step->data.intent_step.required_abilities,
            step->data.intent_step.required_ability_count,
            ctx,
            step,
            step_consumer_name,
            "intent step ability consumer lookup");
        semantic_type_resolution_record_string_dependency(
            ctx,
            step,
            step_consumer_name,
            step->data.intent_step.causes_effect,
            "intent step causes-effect lookup");
        free(step_consumer_name);
    }
}

static void
semantic_type_resolution_precollect_zone_inventory(ASTNode *zone_decl,
                                                   SemanticContext *ctx)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        char *slot_label;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;

        slot_label = semantic_type_resolution_zone_slot_label(
            zone_decl,
            slot->data.domain_slot.slot_name);
        if (slot_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, slot, slot_label);
            free(slot_label);
        }

        semantic_type_resolution_collect_type_refs(
            slot->data.domain_slot.type,
            ctx,
            slot,
            slot->data.domain_slot.slot_name != NULL
                ? slot->data.domain_slot.slot_name : "<zone-slot>",
            "zone slot type lookup");
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
        char *layer_label;
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT)
            continue;
        layer_label = semantic_type_resolution_zone_layer_label(
            zone_decl,
            slot->data.zone_layer_slot.slot_name);
        if (layer_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, slot, layer_label);
            free(layer_label);
        }
        semantic_type_resolution_record_string_dependency(
            ctx,
            slot,
            slot->data.zone_layer_slot.slot_name != NULL
                ? slot->data.zone_layer_slot.slot_name : "<zone-layer>",
            slot->data.zone_layer_slot.layer_type,
            "zone layer lookup");
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = zone_decl->data.zone_decl.refreshes[i];
        char *consumer_label;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.refresh.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       refresh->data.zone_refresh.object_slot_name != NULL
                                           ? refresh->data.zone_refresh.object_slot_name : "<refresh>");
        if (consumer_label == NULL)
            continue;

        if (refresh->data.zone_refresh.object_slot_name != NULL) {
            char *object_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                refresh->data.zone_refresh.object_slot_name);
            if (object_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    refresh,
                    consumer_label,
                    NULL,
                    object_label,
                    "zone refresh target-slot lookup");
                free(object_label);
            }
        }

        if (refresh->data.zone_refresh.source_slot_name != NULL) {
            char *source_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                refresh->data.zone_refresh.source_slot_name);
            if (source_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    refresh,
                    consumer_label,
                    NULL,
                    source_label,
                    "zone refresh source-slot lookup");
                free(source_label);
            }
        }

        for (size_t map_i = 0; map_i < refresh->data.zone_refresh.field_map_count; map_i++) {
            const char *target_field = refresh->data.zone_refresh.mapped_target_fields != NULL
                ? refresh->data.zone_refresh.mapped_target_fields[map_i] : NULL;
            const char *source_field = refresh->data.zone_refresh.mapped_source_fields != NULL
                ? refresh->data.zone_refresh.mapped_source_fields[map_i] : NULL;
            char *projection_label = semantic_type_resolution_projection_path_label(
                zone_decl,
                refresh->data.zone_refresh.object_slot_name,
                refresh->data.zone_refresh.source_slot_name,
                target_field,
                source_field);
            if (projection_label != NULL) {
                (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                  TYPE_RES_NODE_PROJECTION_PATH,
                                                  refresh,
                                                  projection_label);
                semantic_type_resolution_record_named_dependency(
                    ctx,
                    refresh,
                    consumer_label,
                    TYPE_RES_NODE_PROJECTION_PATH,
                    refresh,
                    projection_label,
                    "zone refresh projection-path lookup");

                if (refresh->data.zone_refresh.object_slot_name != NULL) {
                    char *target_slot_label = semantic_type_resolution_zone_slot_label(
                        zone_decl,
                        refresh->data.zone_refresh.object_slot_name);
                    if (target_slot_label != NULL) {
                        semantic_type_resolution_record_named_dependency(
                            ctx,
                            refresh,
                            projection_label,
                            TYPE_RES_NODE_LOCAL_CONTRACT,
                            refresh,
                            target_slot_label,
                            "projection target-slot carrier");
                        free(target_slot_label);
                    }
                }

                if (refresh->data.zone_refresh.source_slot_name != NULL) {
                    char *source_slot_label = semantic_type_resolution_zone_slot_label(
                        zone_decl,
                        refresh->data.zone_refresh.source_slot_name);
                    if (source_slot_label != NULL) {
                        semantic_type_resolution_record_named_dependency(
                            ctx,
                            refresh,
                            projection_label,
                            TYPE_RES_NODE_LOCAL_CONTRACT,
                            refresh,
                            source_slot_label,
                            "projection source-slot carrier");
                        free(source_slot_label);
                    }
                }

                if (target_field != NULL) {
                    char *target_field_label = semantic_type_resolution_projection_slot_field_label(
                        zone_decl,
                        refresh->data.zone_refresh.object_slot_name,
                        target_field);
                    if (target_field_label != NULL) {
                        (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                          TYPE_RES_NODE_PROJECTION_PATH,
                                                          refresh,
                                                          target_field_label);
                        semantic_type_resolution_record_named_dependency(
                            ctx,
                            refresh,
                            projection_label,
                            TYPE_RES_NODE_PROJECTION_PATH,
                            refresh,
                            target_field_label,
                            "projection target field-path lookup");
                        free(target_field_label);
                    }
                }

                if (source_field != NULL) {
                    ASTNode *source_decl = semantic_type_resolution_projection_source_decl(
                        zone_decl,
                        refresh->data.zone_refresh.source_slot_name,
                        ctx);
                    char *resolved_source_path = NULL;
                    const char *source_path_text = source_field;

                    if (source_decl != NULL) {
                        size_t saved_diag = ctx->diagnostic_count;
                        bool saved_error = ctx->has_error;
                        Type *field_type = NULL;
                        int path_status = resolve_projection_source_field_path(
                            ctx->program_root,
                            source_decl,
                            source_field,
                            ctx,
                            &resolved_source_path,
                            &field_type);
                        (void)field_type;
                        if (ctx->diagnostic_count > saved_diag) {
                            ctx->diagnostic_count = saved_diag;
                            ctx->has_error = saved_error;
                        }
                        if (path_status == 1 && resolved_source_path != NULL)
                            source_path_text = resolved_source_path;
                    }

                    if (source_path_text != NULL) {
                        char *source_field_label = semantic_type_resolution_projection_slot_field_label(
                            zone_decl,
                            refresh->data.zone_refresh.source_slot_name,
                            source_path_text);
                        if (source_field_label != NULL) {
                            (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                              TYPE_RES_NODE_PROJECTION_PATH,
                                                              refresh,
                                                              source_field_label);
                            semantic_type_resolution_record_named_dependency(
                                ctx,
                                refresh,
                                projection_label,
                                TYPE_RES_NODE_PROJECTION_PATH,
                                refresh,
                                source_field_label,
                                "projection source field-path lookup");
                            free(source_field_label);
                        }
                    }

                    if (resolved_source_path != NULL)
                        free(resolved_source_path);
                }

                free(projection_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.apply_count; i++) {
        ASTNode *apply = zone_decl->data.zone_decl.applies[i];
        char *consumer_label;

        if (apply == NULL || apply->type != AST_ZONE_APPLY)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.apply.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       apply->data.zone_apply.effect_slot_name != NULL
                                           ? apply->data.zone_apply.effect_slot_name : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        if (apply->data.zone_apply.effect_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                apply->data.zone_apply.effect_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    apply,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone apply effect-slot lookup");
                free(layer_label);
            }
        }
        if (apply->data.zone_apply.target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                apply->data.zone_apply.target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    apply,
                    consumer_label,
                    NULL,
                    target_label,
                    "zone apply target-slot lookup");
                free(target_label);
            }
        }
        if (apply->data.zone_apply.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                apply->data.zone_apply.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    apply,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone apply state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.link_count; i++) {
        ASTNode *link = zone_decl->data.zone_decl.links[i];
        char *consumer_label;

        if (link == NULL || link->type != AST_ZONE_LINK)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.link.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       link->data.zone_link.relation_slot_name != NULL
                                           ? link->data.zone_link.relation_slot_name : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        if (link->data.zone_link.relation_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                link->data.zone_link.relation_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone link relation-slot lookup");
                free(layer_label);
            }
        }
        if (link->data.zone_link.left_slot_name != NULL) {
            char *left_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                link->data.zone_link.left_slot_name);
            if (left_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    left_label,
                    "zone link left-slot lookup");
                free(left_label);
            }
        }
        if (link->data.zone_link.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                link->data.zone_link.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    right_label,
                    "zone link right-slot lookup");
                free(right_label);
            }
        }
        if (link->data.zone_link.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                link->data.zone_link.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone link state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.detach_count; i++) {
        ASTNode *detach = zone_decl->data.zone_decl.detaches[i];
        char *consumer_label;

        if (detach == NULL || detach->type != AST_ZONE_DETACH)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.detach.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       detach->data.zone_detach.effect_slot_name != NULL
                                           ? detach->data.zone_detach.effect_slot_name : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        if (detach->data.zone_detach.effect_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                detach->data.zone_detach.effect_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    detach,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone detach effect-slot lookup");
                free(layer_label);
            }
        }
        if (detach->data.zone_detach.target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                detach->data.zone_detach.target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    detach,
                    consumer_label,
                    NULL,
                    target_label,
                    "zone detach target-slot lookup");
                free(target_label);
            }
        }
        if (detach->data.zone_detach.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                detach->data.zone_detach.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    detach,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone detach state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = zone_decl->data.zone_decl.unlinks[i];
        char *consumer_label;

        if (unlink == NULL || unlink->type != AST_ZONE_UNLINK)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.unlink.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       unlink->data.zone_unlink.relation_slot_name != NULL
                                           ? unlink->data.zone_unlink.relation_slot_name : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        if (unlink->data.zone_unlink.relation_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                unlink->data.zone_unlink.relation_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone unlink relation-slot lookup");
                free(layer_label);
            }
        }
        if (unlink->data.zone_unlink.left_slot_name != NULL) {
            char *left_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                unlink->data.zone_unlink.left_slot_name);
            if (left_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    left_label,
                    "zone unlink left-slot lookup");
                free(left_label);
            }
        }
        if (unlink->data.zone_unlink.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                unlink->data.zone_unlink.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    right_label,
                    "zone unlink right-slot lookup");
                free(right_label);
            }
        }
        if (unlink->data.zone_unlink.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                unlink->data.zone_unlink.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone unlink state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_effects[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_EFFECT)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.maintain-effect.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       maintain->data.zone_maintain_effect.effect_slot_name != NULL
                                           ? maintain->data.zone_maintain_effect.effect_slot_name : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        if (maintain->data.zone_maintain_effect.effect_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                maintain->data.zone_maintain_effect.effect_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone maintain-effect slot lookup");
                free(layer_label);
            }
        }
        if (maintain->data.zone_maintain_effect.target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                maintain->data.zone_maintain_effect.target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    target_label,
                    "zone maintain-effect target-slot lookup");
                free(target_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_relations[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_RELATION)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.maintain-relation.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       maintain->data.zone_maintain_relation.relation_slot_name != NULL
                                           ? maintain->data.zone_maintain_relation.relation_slot_name : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        if (maintain->data.zone_maintain_relation.relation_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                maintain->data.zone_maintain_relation.relation_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone maintain-relation slot lookup");
                free(layer_label);
            }
        }
        if (maintain->data.zone_maintain_relation.left_slot_name != NULL) {
            char *left_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                maintain->data.zone_maintain_relation.left_slot_name);
            if (left_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    left_label,
                    "zone maintain-relation left-slot lookup");
                free(left_label);
            }
        }
        if (maintain->data.zone_maintain_relation.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                maintain->data.zone_maintain_relation.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    right_label,
                    "zone maintain-relation right-slot lookup");
                free(right_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.state_count; i++) {
        ASTNode *state = zone_decl->data.zone_decl.states[i];
        char *state_label;

        if (state == NULL || state->type != AST_ZONE_STATE)
            continue;

        state_label = semantic_type_resolution_zone_state_label(
            zone_decl,
            state->data.zone_state.state_name);
        if (state_label == NULL)
            continue;

        semantic_type_resolution_register_local_contract_node(
            ctx, state, state_label);

        if (state->data.zone_state.layer_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                state->data.zone_state.layer_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    layer_label,
                    "zone state layer lookup");
                free(layer_label);
            }
        }

        if (state->data.zone_state.left_or_target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                state->data.zone_state.left_or_target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    target_label,
                    "zone state target-slot lookup");
                free(target_label);
            }
        }

        if (state->data.zone_state.is_relation
            && state->data.zone_state.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                state->data.zone_state.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    right_label,
                    "zone state right-slot lookup");
                free(right_label);
            }
        }
        free(state_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_states[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_STATE)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.maintain-state.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       maintain->data.zone_maintain_state.state_name != NULL
                                           ? maintain->data.zone_maintain_state.state_name
                                           : "<state>");
        if (consumer_label == NULL)
            continue;

        if (maintain->data.zone_maintain_state.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                maintain->data.zone_maintain_state.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone maintain-state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone_decl->data.zone_decl.authorities[i];
        char *consumer_name;

        if (authority == NULL || authority->type != AST_ZONE_AUTHORITY)
            continue;

        consumer_name = tc_strdup_fmt("zone %s.%s",
                                      zone_decl->data.zone_decl.name != NULL
                                          ? zone_decl->data.zone_decl.name : "<zone>",
                                      authority->data.zone_authority.subject_slot_name != NULL
                                          ? authority->data.zone_authority.subject_slot_name
                                          : "<authority>");
        if (consumer_name == NULL)
            continue;
        semantic_type_resolution_precollect_required_abilities(
            authority->data.zone_authority.required_abilities,
            authority->data.zone_authority.ability_count,
            ctx,
            authority,
            consumer_name,
            "zone authority ability consumer lookup");
        free(consumer_name);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            zone_decl->data.zone_decl.methods[i],
            ctx,
            zone_decl->data.zone_decl.name);
    }
}

static void
semantic_type_resolution_register_local_contract_node(SemanticContext *ctx,
                                                      const ASTNode *site,
                                                      const char *label)
{
    if (ctx == NULL || label == NULL || label[0] == '\0')
        return;

    (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                      TYPE_RES_NODE_LOCAL_CONTRACT,
                                      site,
                                      label);
}

static void
semantic_type_resolution_record_local_contract_dependency(SemanticContext *ctx,
                                                          const ASTNode *consumer_site,
                                                          const char *consumer_label,
                                                          const ASTNode *provider_site,
                                                          const char *provider_label,
                                                          const char *reason)
{
    if (ctx == NULL || provider_label == NULL || provider_label[0] == '\0')
        return;

    semantic_type_resolution_record_named_dependency(
        ctx,
        consumer_site,
        consumer_label,
        TYPE_RES_NODE_LOCAL_CONTRACT,
        provider_site,
        provider_label,
        reason);
}

static char *
semantic_type_resolution_world_zone_slot_label(ASTNode *world_decl,
                                               const char *slot_name)
{
    return tc_strdup_fmt("world %s.zone.%s",
                         world_decl != NULL
                             && world_decl->type == AST_WORLD_DECL
                             && world_decl->data.world_decl.name != NULL
                                 ? world_decl->data.world_decl.name : "<world>",
                         slot_name != NULL ? slot_name : "<zone-slot>");
}

static char *
semantic_type_resolution_world_state_label(ASTNode *world_decl,
                                           const char *state_name)
{
    return tc_strdup_fmt("world %s.state.%s",
                         world_decl != NULL
                             && world_decl->type == AST_WORLD_DECL
                             && world_decl->data.world_decl.name != NULL
                                 ? world_decl->data.world_decl.name : "<world>",
                         state_name != NULL ? state_name : "<state>");
}

static char *
semantic_type_resolution_zone_slot_label(ASTNode *zone_decl,
                                         const char *slot_name)
{
    return tc_strdup_fmt("zone %s.slot.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         slot_name != NULL ? slot_name : "<slot>");
}

static char *
semantic_type_resolution_zone_layer_label(ASTNode *zone_decl,
                                          const char *slot_name)
{
    return tc_strdup_fmt("zone %s.layer.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         slot_name != NULL ? slot_name : "<layer>");
}

static char *
semantic_type_resolution_zone_state_label(ASTNode *zone_decl,
                                          const char *state_name)
{
    return tc_strdup_fmt("zone %s.state.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         state_name != NULL ? state_name : "<state>");
}

static char *
semantic_type_resolution_projection_path_label(ASTNode *zone_decl,
                                               const char *target_slot_name,
                                               const char *source_slot_name,
                                               const char *target_field_name,
                                               const char *source_field_name)
{
    return tc_strdup_fmt("zone %s.projection.%s.%s<-%s.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         target_slot_name != NULL ? target_slot_name : "<target-slot>",
                         target_field_name != NULL ? target_field_name : "<target-field>",
                         source_slot_name != NULL ? source_slot_name : "<source-slot>",
                         source_field_name != NULL ? source_field_name : "<source-field>");
}

static char *
semantic_type_resolution_projection_slot_field_label(ASTNode *zone_decl,
                                                     const char *slot_name,
                                                     const char *field_path)
{
    return tc_strdup_fmt("zone %s.slot.%s.field.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         slot_name != NULL ? slot_name : "<slot>",
                         field_path != NULL ? field_path : "<field-path>");
}

static ASTNode *
semantic_type_resolution_projection_source_decl(ASTNode *zone_decl,
                                                const char *slot_name,
                                                SemanticContext *ctx)
{
    ASTNode *slot;
    ASTNode *type_node;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL || ctx == NULL) {
        return NULL;
    }

    slot = find_zone_domain_slot(zone_decl, slot_name);
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return NULL;

    type_node = slot->data.domain_slot.type;
    if (type_node == NULL || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        return NULL;
    }

    return find_type_decl_by_name(ctx->program_root, type_node->data.type.name);
}

static void
semantic_type_resolution_register_top_level_decl(ASTNode *stmt,
                                                 SemanticContext *ctx)
{
    const char *label = NULL;
    TypeResolutionNodeKind kind = TYPE_RES_NODE_DECL;

    if (stmt == NULL || ctx == NULL)
        return;

    switch (stmt->type) {
    case AST_TYPE_ALIAS:
        label = stmt->data.type_alias.name;
        kind = TYPE_RES_NODE_ALIAS;
        break;
    case AST_CLASS_DECL:
        label = stmt->data.class_decl.name;
        break;
    case AST_FUNC_DECL:
        label = stmt->data.func_decl.name;
        break;
    case AST_EVENT_DECL:
        label = stmt->data.event_decl.name;
        break;
    case AST_ENUM_DECL:
        label = stmt->data.enum_decl.name;
        break;
    case AST_ABILITY_DECL:
        label = stmt->data.ability_decl.name;
        break;
    case AST_ROLE_DECL:
        label = stmt->data.role_decl.name;
        break;
    case AST_PARTY_DECL:
        label = stmt->data.party_decl.name;
        break;
    case AST_ROSTER_DECL:
        label = stmt->data.roster_decl.name;
        break;
    case AST_WORLD_DECL:
        label = stmt->data.world_decl.name;
        break;
    case AST_INTENT_DECL:
        label = stmt->data.intent_decl.name;
        break;
    case AST_RELATION_DECL:
        label = stmt->data.relation_decl.name;
        break;
    case AST_EFFECT_DECL:
        label = stmt->data.effect_decl.name;
        break;
    case AST_ZONE_DECL:
        label = stmt->data.zone_decl.name;
        break;
    default:
        return;
    }

    if (label == NULL || label[0] == '\0')
        return;

    (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                      kind,
                                      stmt,
                                      label);
}

static void
semantic_type_resolution_precollect_program(ASTNode *program,
                                            SemanticContext *ctx)
{
    if (program == NULL || ctx == NULL || program->type != AST_PROGRAM)
        return;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL)
            continue;

        semantic_type_resolution_register_top_level_decl(stmt, ctx);

        switch (stmt->type) {
        case AST_TYPE_ALIAS:
            if (stmt->data.type_alias.name != NULL) {
                semantic_type_resolution_collect_type_refs(
                    stmt->data.type_alias.target_type,
                    ctx,
                    stmt,
                    stmt->data.type_alias.name,
                    "type-alias target lookup");
            }
            break;

        case AST_CLASS_DECL:
            semantic_type_resolution_precollect_class_inventory(stmt, ctx);
            break;

        case AST_FUNC_DECL:
            semantic_type_resolution_precollect_action_contract(
                stmt,
                ctx,
                stmt->data.func_decl.name);
            break;

        case AST_EVENT_DECL:
            semantic_type_resolution_precollect_event_inventory(stmt, ctx);
            break;

        case AST_ENUM_DECL:
            semantic_type_resolution_precollect_enum_inventory(stmt, ctx);
            break;

        case AST_ABILITY_DECL:
            semantic_type_resolution_precollect_ability_inventory(stmt, ctx);
            break;

        case AST_ROLE_DECL:
            semantic_type_resolution_precollect_role_inventory(stmt, ctx);
            break;

        case AST_PARTY_DECL:
            semantic_type_resolution_precollect_party_inventory(stmt, ctx);
            break;

        case AST_ROSTER_DECL:
            semantic_type_resolution_precollect_roster_inventory(stmt, ctx);
            break;

        case AST_WORLD_DECL:
            semantic_type_resolution_precollect_world_inventory(stmt, ctx);
            break;

        case AST_INTENT_DECL:
            semantic_type_resolution_precollect_intent_inventory(stmt, ctx);
            break;

        case AST_ZONE_DECL:
            semantic_type_resolution_precollect_zone_inventory(stmt, ctx);
            break;

        case AST_RELATION_DECL:
            semantic_type_resolution_precollect_domain_inventory(
                stmt,
                stmt->data.relation_decl.slots,
                stmt->data.relation_decl.slot_count,
                stmt->data.relation_decl.methods,
                stmt->data.relation_decl.method_count,
                ctx,
                "relation",
                stmt->data.relation_decl.name);
            break;

        case AST_EFFECT_DECL:
            semantic_type_resolution_precollect_domain_inventory(
                stmt,
                stmt->data.effect_decl.slots,
                stmt->data.effect_decl.slot_count,
                stmt->data.effect_decl.methods,
                stmt->data.effect_decl.method_count,
                ctx,
                "effect",
                stmt->data.effect_decl.name);
            break;

        default:
            break;
        }
    }
}

static const char *
semantic_type_resolution_decl_label(ASTNode *stmt)
{
    if (stmt == NULL)
        return NULL;

    switch (stmt->type) {
    case AST_TYPE_ALIAS:
        return stmt->data.type_alias.name;
    case AST_CLASS_DECL:
        return stmt->data.class_decl.name;
    case AST_FUNC_DECL:
        return stmt->data.func_decl.name;
    case AST_EVENT_DECL:
        return stmt->data.event_decl.name;
    case AST_ENUM_DECL:
        return stmt->data.enum_decl.name;
    case AST_ABILITY_DECL:
        return stmt->data.ability_decl.name;
    case AST_ROLE_DECL:
        return stmt->data.role_decl.name;
    case AST_PARTY_DECL:
        return stmt->data.party_decl.name;
    case AST_ROSTER_DECL:
        return stmt->data.roster_decl.name;
    case AST_WORLD_DECL:
        return stmt->data.world_decl.name;
    case AST_INTENT_DECL:
        return stmt->data.intent_decl.name;
    case AST_RELATION_DECL:
        return stmt->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return stmt->data.effect_decl.name;
    case AST_ZONE_DECL:
        return stmt->data.zone_decl.name;
    default:
        return NULL;
    }
}

static TypeResolutionNodeKind
semantic_type_resolution_decl_kind(ASTNode *stmt)
{
    if (stmt != NULL && stmt->type == AST_TYPE_ALIAS)
        return TYPE_RES_NODE_ALIAS;
    return TYPE_RES_NODE_DECL;
}

static Type *
semantic_stage_resolve_type_quiet(ASTNode *type_node,
                                  SemanticContext *ctx,
                                  const ASTNode *consumer_site,
                                  const char *consumer_name,
                                  const char *reason)
{
    size_t saved_diag;
    bool saved_error;
    Type *resolved;

    if (type_node == NULL || ctx == NULL)
        return TYPE_UNKNOWN;

    (void)consumer_site;
    (void)consumer_name;
    (void)reason;

    saved_diag = ctx->diagnostic_count;
    saved_error = ctx->has_error;
    resolved = resolve_type_node(type_node, ctx);
    if (ctx->diagnostic_count > saved_diag) {
        ctx->diagnostic_count = saved_diag;
        ctx->has_error = saved_error;
        return TYPE_UNKNOWN;
    }

    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static ASTNode *
semantic_stage_named_decl_quiet(SemanticContext *ctx,
                                ASTNodeType decl_type,
                                const char *provider_name)
{
    if (ctx == NULL || ctx->program_root == NULL
        || provider_name == NULL || provider_name[0] == '\0') {
        return NULL;
    }

    switch (decl_type) {
    case AST_CLASS_DECL:
        return find_type_decl_by_name(ctx->program_root, provider_name);
    case AST_ABILITY_DECL:
        return find_ability_decl_by_name(ctx->program_root, provider_name);
    case AST_ROLE_DECL:
        return semantic_find_top_level_decl_by_label(ctx->program_root,
                                                     provider_name,
                                                     TYPE_RES_NODE_DECL);
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_ZONE_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
        return find_domain_decl_by_name(ctx->program_root, decl_type, provider_name);
    default:
        return semantic_find_top_level_decl_by_label(ctx->program_root,
                                                     provider_name,
                                                     TYPE_RES_NODE_DECL);
    }
}

static ASTNode *
semantic_world_find_zone_slot_local(ASTNode *world, const char *slot_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.zone_count; i++) {
        ASTNode *zone = world->data.world_decl.zones[i];
        if (zone != NULL
            && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

static ASTNode *
semantic_world_find_state_local(ASTNode *world, const char *state_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.state_count; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL
            && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
semantic_zone_find_layer_slot_local(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

static ASTNode *
semantic_zone_find_state_local(ASTNode *zone, const char *state_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        ASTNode *state = zone->data.zone_decl.states[i];
        if (state != NULL
            && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static void
semantic_stage_world_local_contracts(ASTNode *world_decl,
                                     SemanticContext *ctx)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < world_decl->data.world_decl.state_count; i++) {
        ASTNode *state = world_decl->data.world_decl.states[i];
        ASTNode *zone_slot_decl = NULL;
        ASTNode *zone_decl = NULL;

        if (state == NULL || state->type != AST_WORLD_STATE)
            continue;

        zone_slot_decl = semantic_world_find_zone_slot_local(
            world_decl,
            state->data.world_state.zone_slot_name);
        if (zone_slot_decl != NULL) {
            zone_decl = semantic_stage_named_decl_quiet(
                ctx,
                AST_ZONE_DECL,
                zone_slot_decl->data.world_zone.zone_type);
        }

        switch (state->data.world_state.source_kind) {
        case WORLD_STATE_SOURCE_ZONE:
            (void)zone_slot_decl;
            break;

        case WORLD_STATE_SOURCE_PROJECTION:
            if (zone_decl != NULL && state->data.world_state.detail_name != NULL)
                (void)find_zone_domain_slot(zone_decl, state->data.world_state.detail_name);
            break;

        case WORLD_STATE_SOURCE_LAYER:
            if (zone_decl != NULL && state->data.world_state.detail_name != NULL)
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    state->data.world_state.detail_name);
            break;

        case WORLD_STATE_SOURCE_STATE:
            if (zone_decl != NULL && state->data.world_state.detail_name != NULL)
                (void)semantic_zone_find_state_local(zone_decl,
                    state->data.world_state.detail_name);
            break;

        case WORLD_STATE_SOURCE_ALL:
        case WORLD_STATE_SOURCE_ANY:
            for (size_t j = 0; j < state->data.world_state.input_count; j++) {
                const char *input_name = state->data.world_state.input_names[j];
                if (semantic_world_find_state_local(world_decl, input_name) == NULL)
                    (void)semantic_world_find_zone_slot_local(world_decl, input_name);
            }
            break;
        }
    }

    for (size_t i = 0; i < world_decl->data.world_decl.activate_count; i++) {
        ASTNode *activate = world_decl->data.world_decl.activations[i];
        if (activate == NULL || activate->type != AST_WORLD_ACTIVATE)
            continue;
        if (activate->data.world_activate.state_name != NULL)
            (void)semantic_world_find_state_local(world_decl,
                activate->data.world_activate.state_name);
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                activate->data.world_activate.zone_slot_name);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.deactivate_count; i++) {
        ASTNode *deactivate = world_decl->data.world_decl.deactivations[i];
        if (deactivate == NULL || deactivate->type != AST_WORLD_DEACTIVATE)
            continue;
        if (deactivate->data.world_deactivate.state_name != NULL)
            (void)semantic_world_find_state_local(world_decl,
                deactivate->data.world_deactivate.state_name);
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                deactivate->data.world_deactivate.zone_slot_name);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.maintained_zone_count; i++) {
        ASTNode *maintain = world_decl->data.world_decl.maintained_zones[i];
        if (maintain == NULL || maintain->type != AST_WORLD_MAINTAIN)
            continue;
        if (maintain->data.world_maintain.state_name != NULL)
            (void)semantic_world_find_state_local(world_decl,
                maintain->data.world_maintain.state_name);
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                maintain->data.world_maintain.zone_slot_name);
    }
}

static void
semantic_stage_zone_local_contracts(ASTNode *zone_decl)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return;

    for (size_t i = 0; i < zone_decl->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = zone_decl->data.zone_decl.refreshes[i];
        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        (void)find_zone_domain_slot(zone_decl, refresh->data.zone_refresh.object_slot_name);
        (void)find_zone_domain_slot(zone_decl, refresh->data.zone_refresh.source_slot_name);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.state_count; i++) {
        ASTNode *state = zone_decl->data.zone_decl.states[i];
        if (state == NULL || state->type != AST_ZONE_STATE)
            continue;
        (void)semantic_zone_find_layer_slot_local(zone_decl,
            state->data.zone_state.layer_slot_name);
        (void)find_zone_domain_slot(zone_decl,
            state->data.zone_state.left_or_target_slot_name);
        if (state->data.zone_state.is_relation)
            (void)find_zone_domain_slot(zone_decl,
                state->data.zone_state.right_slot_name);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_states[i];
        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_STATE)
            continue;
        (void)semantic_zone_find_state_local(zone_decl,
            maintain->data.zone_maintain_state.state_name);
    }
}

static void
semantic_stage_required_abilities(ASTNode **ability_refs,
                                  size_t ability_count,
                                  SemanticContext *ctx,
                                  const ASTNode *owner,
                                  const char *consumer_name,
                                  const char *reason)
{
    if (ability_refs == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *ability_ref = ability_refs[i];

        if (ability_ref != NULL
            && ability_ref->type == AST_TYPE
            && ability_ref->data.type.name != NULL) {
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ABILITY_DECL,
                ability_ref->data.type.name);
        }

        (void)semantic_stage_resolve_type_quiet(
            ability_ref,
            ctx,
            owner,
            consumer_name,
            reason);
    }
}

static void
semantic_stage_generic_contract_nodes(GenericParams *gp,
                                      WhereClause *wc,
                                      SemanticContext *ctx,
                                      ASTNode *owner,
                                      const char *kind_name,
                                      const char *owner_name)
{
    if (ctx == NULL)
        return;

    if (gp != NULL) {
        for (size_t i = 0; i < gp->count; i++) {
            GenericParam *param = gp->params[i];
            char *consumer_name;

            if (param == NULL)
                continue;

            consumer_name = tc_strdup_fmt("%s %s.%s",
                                          kind_name != NULL ? kind_name : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          param->name != NULL ? param->name : "<type-param>");
            if (consumer_name == NULL)
                continue;

            if (param->default_type != NULL) {
                (void)semantic_stage_resolve_type_quiet(
                    param->default_type,
                    ctx,
                    owner,
                    consumer_name,
                    "default-type lookup");
            }

            if (param->constraint != NULL) {
                (void)semantic_stage_resolve_type_quiet(
                    param->constraint,
                    ctx,
                    owner,
                    consumer_name,
                    "generic constraint lookup");
            }
            free(consumer_name);
        }
    }

    if (wc != NULL) {
        for (size_t i = 0; i < wc->count; i++) {
            TypeConstraint *tc = wc->constraints[i];
            char *consumer_name;

            if (tc == NULL)
                continue;

            consumer_name = tc_strdup_fmt("%s %s.%s",
                                          kind_name != NULL ? kind_name : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          tc->type_param != NULL ? tc->type_param : "<type-param>");
            if (consumer_name == NULL)
                continue;

            for (size_t b = 0; b < tc->bound_count; b++) {
                (void)semantic_stage_resolve_type_quiet(
                    tc->bounds[b],
                    ctx,
                    owner,
                    consumer_name,
                    "where-bound lookup");
            }
            free(consumer_name);
        }
    }

    validate_generic_param_defaults(gp, ctx, owner, kind_name);
    validate_where_clause_bounds(wc, ctx, owner);
}

static void
semantic_stage_function_signature(ASTNode *func_decl,
                                  SemanticContext *ctx,
                                  const char *fallback_name)
{
    const char *consumer_name;

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = func_decl->data.func_decl.name != NULL
        ? func_decl->data.func_decl.name
        : (fallback_name != NULL ? fallback_name : "<func>");

    semantic_stage_generic_contract_nodes(
        func_decl->data.func_decl.generic_params,
        func_decl->data.func_decl.where_clause,
        ctx,
        func_decl,
        "func",
        consumer_name);

    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        char *param_consumer_name;

        if (param == NULL)
            continue;

        param_consumer_name = tc_strdup_fmt("func %s.%s",
                                            consumer_name,
                                            param->name != NULL ? param->name : "<param>");
        if (param_consumer_name == NULL)
            continue;

        (void)semantic_stage_resolve_type_quiet(
            param->type,
            ctx,
            func_decl,
            param_consumer_name,
            "function parameter type lookup");
        free(param_consumer_name);
    }

    (void)semantic_stage_resolve_type_quiet(
        func_decl->data.func_decl.return_type,
        ctx,
        func_decl,
        consumer_name,
        "function return type lookup");

    semantic_stage_required_abilities(
        func_decl->data.func_decl.required_abilities,
        func_decl->data.func_decl.required_ability_count,
        ctx,
        func_decl,
        consumer_name,
        "action ability consumer lookup");
    (void)semantic_stage_named_decl_quiet(
        ctx,
        AST_ZONE_DECL,
        func_decl->data.func_decl.within_zone);
    (void)semantic_stage_named_decl_quiet(
        ctx,
        AST_EFFECT_DECL,
        func_decl->data.func_decl.causes_effect);
}

static void
semantic_stage_method_array(ASTNode **methods,
                            size_t method_count,
                            SemanticContext *ctx,
                            const char *fallback_name)
{
    if (methods == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < method_count; i++)
        semantic_stage_function_signature(methods[i], ctx, fallback_name);
}

static void
semantic_stage_event_signature(ASTNode *event_decl,
                               SemanticContext *ctx)
{
    if (event_decl == NULL || event_decl->type != AST_EVENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < event_decl->data.event_decl.param_count; i++) {
        ASTNode *param = event_decl->data.event_decl.params[i];
        char *consumer_name;

        if (param == NULL || param->type != AST_LET_DECL)
            continue;

        consumer_name = tc_strdup_fmt("event %s.%s",
                                      event_decl->data.event_decl.name != NULL
                                          ? event_decl->data.event_decl.name : "<event>",
                                      param->data.let_decl.name != NULL
                                          ? param->data.let_decl.name : "<param>");
        if (consumer_name == NULL)
            continue;

        (void)semantic_stage_resolve_type_quiet(
            param->data.let_decl.type,
            ctx,
            event_decl,
            consumer_name,
            "event parameter type lookup");
        free(consumer_name);
    }

    (void)semantic_stage_resolve_type_quiet(
        event_decl->data.event_decl.return_type,
        ctx,
        event_decl,
        event_decl->data.event_decl.name,
        "event return type lookup");
}

static ASTNode *
semantic_find_top_level_decl_by_label(ASTNode *program,
                                      const char *label,
                                      TypeResolutionNodeKind kind)
{
    if (program == NULL || program->type != AST_PROGRAM || label == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        const char *stmt_label;

        if (stmt == NULL)
            continue;
        if (semantic_type_resolution_decl_kind(stmt) != kind)
            continue;

        stmt_label = semantic_type_resolution_decl_label(stmt);
        if (stmt_label != NULL && strcmp(stmt_label, label) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
semantic_find_graph_host_decl(ASTNode *program,
                              const char *label)
{
    const char *space;
    const char *dot;
    size_t name_len;
    char *host_name;
    ASTNode *decl = NULL;

    if (program == NULL || label == NULL)
        return NULL;

    if (strncmp(label, "world ", 6) == 0) {
        space = label + 6;
        dot = strchr(space, '.');
        if (dot == NULL || dot == space)
            return NULL;
        name_len = (size_t)(dot - space);
        host_name = calloc(name_len + 1, 1);
        if (host_name == NULL)
            return NULL;
        memcpy(host_name, space, name_len);
        decl = find_domain_decl_by_name(program, AST_WORLD_DECL, host_name);
        free(host_name);
        return decl;
    }

    if (strncmp(label, "zone ", 5) == 0) {
        space = label + 5;
        dot = strchr(space, '.');
        if (dot == NULL || dot == space)
            return NULL;
        name_len = (size_t)(dot - space);
        host_name = calloc(name_len + 1, 1);
        if (host_name == NULL)
            return NULL;
        memcpy(host_name, space, name_len);
        decl = find_domain_decl_by_name(program, AST_ZONE_DECL, host_name);
        free(host_name);
        return decl;
    }

    return NULL;
}

static void
semantic_stage_world_local_contract_from_label(ASTNode *world_decl,
                                               const char *label,
                                               SemanticContext *ctx)
{
    const char *suffix;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || label == NULL || ctx == NULL) {
        return;
    }

    suffix = strstr(label, ".zone.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 6;
        (void)semantic_world_find_zone_slot_local(world_decl, slot_name);
        return;
    }

    suffix = strstr(label, ".state.");
    if (suffix != NULL) {
        ASTNode *state = semantic_world_find_state_local(world_decl, suffix + 7);
        ASTNode *zone_slot_decl = NULL;

        if (state == NULL || state->type != AST_WORLD_STATE)
            return;

        zone_slot_decl = semantic_world_find_zone_slot_local(
            world_decl,
            state->data.world_state.zone_slot_name);
        if (zone_slot_decl != NULL && zone_slot_decl->type == AST_WORLD_ZONE) {
            ASTNode *zone_decl = semantic_stage_named_decl_quiet(
                ctx,
                AST_ZONE_DECL,
                zone_slot_decl->data.world_zone.zone_type);
            if (zone_decl != NULL && zone_decl->type == AST_ZONE_DECL
                && state->data.world_state.detail_name != NULL) {
                if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_PROJECTION) {
                    (void)find_zone_domain_slot(zone_decl, state->data.world_state.detail_name);
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_LAYER) {
                    (void)semantic_zone_find_layer_slot_local(zone_decl,
                        state->data.world_state.detail_name);
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_STATE) {
                    (void)semantic_zone_find_state_local(zone_decl,
                        state->data.world_state.detail_name);
                }
            }
        }

        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            for (size_t i = 0; i < state->data.world_state.input_count; i++) {
                const char *input_name = state->data.world_state.input_names[i];
                if (semantic_world_find_state_local(world_decl, input_name) == NULL)
                    (void)semantic_world_find_zone_slot_local(world_decl, input_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".activate.");
    if (suffix != NULL) {
        const char *target = suffix + 10;
        if (semantic_world_find_state_local(world_decl, target) == NULL)
            (void)semantic_world_find_zone_slot_local(world_decl, target);
        return;
    }

    suffix = strstr(label, ".deactivate.");
    if (suffix != NULL) {
        const char *target = suffix + 12;
        if (semantic_world_find_state_local(world_decl, target) == NULL)
            (void)semantic_world_find_zone_slot_local(world_decl, target);
        return;
    }

    suffix = strstr(label, ".maintain.");
    if (suffix != NULL) {
        const char *target = suffix + 10;
        if (semantic_world_find_state_local(world_decl, target) == NULL)
            (void)semantic_world_find_zone_slot_local(world_decl, target);
    }
}

static void
semantic_stage_zone_local_contract_from_label(ASTNode *zone_decl,
                                              const char *label,
                                              SemanticContext *ctx)
{
    const char *suffix;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || label == NULL || ctx == NULL) {
        return;
    }

    suffix = strstr(label, ".slot.");
    if (suffix != NULL && strstr(label, ".field.") == NULL) {
        (void)find_zone_domain_slot(zone_decl, suffix + 6);
        return;
    }

    suffix = strstr(label, ".layer.");
    if (suffix != NULL) {
        (void)semantic_zone_find_layer_slot_local(zone_decl, suffix + 7);
        return;
    }

    suffix = strstr(label, ".state.");
    if (suffix != NULL) {
        ASTNode *state = semantic_zone_find_state_local(zone_decl, suffix + 7);
        if (state == NULL || state->type != AST_ZONE_STATE)
            return;
        (void)semantic_zone_find_layer_slot_local(zone_decl,
            state->data.zone_state.layer_slot_name);
        (void)find_zone_domain_slot(zone_decl,
            state->data.zone_state.left_or_target_slot_name);
        if (state->data.zone_state.is_relation)
            (void)find_zone_domain_slot(zone_decl,
                state->data.zone_state.right_slot_name);
        return;
    }

    suffix = strstr(label, ".refresh.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 9;
        for (size_t i = 0; i < zone_decl->data.zone_decl.refresh_count; i++) {
            ASTNode *refresh = zone_decl->data.zone_decl.refreshes[i];
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            if (refresh->data.zone_refresh.object_slot_name != NULL
                && strcmp(refresh->data.zone_refresh.object_slot_name, slot_name) == 0) {
                (void)find_zone_domain_slot(zone_decl,
                    refresh->data.zone_refresh.object_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    refresh->data.zone_refresh.source_slot_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".apply.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 7;
        for (size_t i = 0; i < zone_decl->data.zone_decl.apply_count; i++) {
            ASTNode *apply = zone_decl->data.zone_decl.applies[i];
            if (apply == NULL || apply->type != AST_ZONE_APPLY)
                continue;
            if (apply->data.zone_apply.effect_slot_name != NULL
                && strcmp(apply->data.zone_apply.effect_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    apply->data.zone_apply.effect_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    apply->data.zone_apply.target_slot_name);
                if (apply->data.zone_apply.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        apply->data.zone_apply.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".link.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 6;
        for (size_t i = 0; i < zone_decl->data.zone_decl.link_count; i++) {
            ASTNode *link = zone_decl->data.zone_decl.links[i];
            if (link == NULL || link->type != AST_ZONE_LINK)
                continue;
            if (link->data.zone_link.relation_slot_name != NULL
                && strcmp(link->data.zone_link.relation_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    link->data.zone_link.relation_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    link->data.zone_link.left_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    link->data.zone_link.right_slot_name);
                if (link->data.zone_link.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        link->data.zone_link.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".detach.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 8;
        for (size_t i = 0; i < zone_decl->data.zone_decl.detach_count; i++) {
            ASTNode *detach = zone_decl->data.zone_decl.detaches[i];
            if (detach == NULL || detach->type != AST_ZONE_DETACH)
                continue;
            if (detach->data.zone_detach.effect_slot_name != NULL
                && strcmp(detach->data.zone_detach.effect_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    detach->data.zone_detach.effect_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    detach->data.zone_detach.target_slot_name);
                if (detach->data.zone_detach.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        detach->data.zone_detach.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".unlink.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 8;
        for (size_t i = 0; i < zone_decl->data.zone_decl.unlink_count; i++) {
            ASTNode *unlink = zone_decl->data.zone_decl.unlinks[i];
            if (unlink == NULL || unlink->type != AST_ZONE_UNLINK)
                continue;
            if (unlink->data.zone_unlink.relation_slot_name != NULL
                && strcmp(unlink->data.zone_unlink.relation_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    unlink->data.zone_unlink.relation_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    unlink->data.zone_unlink.left_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    unlink->data.zone_unlink.right_slot_name);
                if (unlink->data.zone_unlink.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        unlink->data.zone_unlink.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".maintain-effect.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 17;
        for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_effect_count; i++) {
            ASTNode *maintain = zone_decl->data.zone_decl.maintained_effects[i];
            if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_EFFECT)
                continue;
            if (maintain->data.zone_maintain_effect.effect_slot_name != NULL
                && strcmp(maintain->data.zone_maintain_effect.effect_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    maintain->data.zone_maintain_effect.effect_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    maintain->data.zone_maintain_effect.target_slot_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".maintain-relation.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 19;
        for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_relation_count; i++) {
            ASTNode *maintain = zone_decl->data.zone_decl.maintained_relations[i];
            if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_RELATION)
                continue;
            if (maintain->data.zone_maintain_relation.relation_slot_name != NULL
                && strcmp(maintain->data.zone_maintain_relation.relation_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    maintain->data.zone_maintain_relation.relation_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    maintain->data.zone_maintain_relation.left_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    maintain->data.zone_maintain_relation.right_slot_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".maintain-state.");
    if (suffix != NULL) {
        (void)semantic_zone_find_state_local(zone_decl, suffix + 15);
        return;
    }

    if (strstr(label, ".projection.") != NULL) {
        for (size_t i = 0; i < zone_decl->data.zone_decl.refresh_count; i++) {
            ASTNode *refresh = zone_decl->data.zone_decl.refreshes[i];
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            (void)find_zone_domain_slot(zone_decl,
                refresh->data.zone_refresh.object_slot_name);
            (void)find_zone_domain_slot(zone_decl,
                refresh->data.zone_refresh.source_slot_name);
        }
        return;
    }

    if (strstr(label, ".field.") != NULL) {
        const char *slot_part = strstr(label, ".slot.");
        const char *field_part = strstr(label, ".field.");
        if (slot_part != NULL && field_part != NULL && field_part > slot_part) {
            size_t slot_len = (size_t)(field_part - (slot_part + 6));
            char *slot_name = calloc(slot_len + 1, 1);
            if (slot_name == NULL)
                return;
            memcpy(slot_name, slot_part + 6, slot_len);
            if (semantic_type_resolution_projection_source_decl(zone_decl, slot_name, ctx) == NULL)
                (void)find_zone_domain_slot(zone_decl, slot_name);
            free(slot_name);
        }
    }
}

static void
semantic_stage_top_level_decl(ASTNode *decl, SemanticContext *ctx)
{
    ASTNode *saved_nominal;
    ASTNode *saved_relation;
    ASTNode *saved_effect;
    ASTNode *saved_zone;
    ASTNode *saved_world;

    if (decl == NULL || ctx == NULL)
        return;

    saved_nominal = ctx->current_nominal_decl;
    saved_relation = ctx->current_relation;
    saved_effect = ctx->current_effect;
    saved_zone = ctx->current_zone;
    saved_world = ctx->current_world;

    switch (decl->type) {
    case AST_TYPE_ALIAS: {
        Symbol *sym;
        if (decl->data.type_alias.name == NULL)
            break;
        sym = scope_lookup_current(ctx->scope, decl->data.type_alias.name);
        if (sym != NULL)
            sym->type = resolve_type_node(decl->data.type_alias.target_type, ctx);
        break;
    }

    case AST_CLASS_DECL:
        ctx->current_nominal_decl = decl;
        semantic_stage_generic_contract_nodes(
            decl->data.class_decl.generic_params,
            decl->data.class_decl.where_clause,
            ctx,
            decl,
            "class",
            decl->data.class_decl.name);
        for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
            ClassField *field = decl->data.class_decl.fields[i];
            char *consumer_name;
            if (field == NULL)
                continue;
            consumer_name = tc_strdup_fmt("class %s.%s",
                                          decl->data.class_decl.name != NULL
                                              ? decl->data.class_decl.name : "<class>",
                                          field->name != NULL ? field->name : "<field>");
            if (consumer_name == NULL)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->type,
                ctx,
                decl,
                consumer_name,
                "class field type lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.class_decl.methods,
            decl->data.class_decl.method_count,
            ctx,
            decl->data.class_decl.name);
        break;

    case AST_FUNC_DECL:
        semantic_stage_function_signature(decl, ctx, decl->data.func_decl.name);
        break;

    case AST_EVENT_DECL:
        semantic_stage_event_signature(decl, ctx);
        break;

    case AST_ENUM_DECL:
        for (size_t i = 0; i < decl->data.enum_decl.variant_count; i++) {
            ASTNode **params = decl->data.enum_decl.variant_params != NULL
                ? decl->data.enum_decl.variant_params[i] : NULL;
            size_t param_count = decl->data.enum_decl.variant_param_counts != NULL
                ? decl->data.enum_decl.variant_param_counts[i] : 0;
            const char *variant_name = decl->data.enum_decl.variants != NULL
                ? decl->data.enum_decl.variants[i] : NULL;
            char *consumer_name;

            if (params == NULL || param_count == 0)
                continue;

            consumer_name = tc_strdup_fmt("enum %s.%s",
                                          decl->data.enum_decl.name != NULL
                                              ? decl->data.enum_decl.name : "<enum>",
                                          variant_name != NULL ? variant_name : "<variant>");
            if (consumer_name == NULL)
                continue;

            for (size_t j = 0; j < param_count; j++) {
                (void)semantic_stage_resolve_type_quiet(
                    params[j],
                    ctx,
                    decl,
                    consumer_name,
                    "enum variant payload type lookup");
            }
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.enum_decl.methods,
            decl->data.enum_decl.method_count,
            ctx,
            decl->data.enum_decl.name);
        break;

    case AST_ABILITY_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.ability_decl.generic_params,
            decl->data.ability_decl.where_clause,
            ctx,
            decl,
            "ability",
            decl->data.ability_decl.name);
        for (size_t i = 0; i < decl->data.ability_decl.require_count; i++) {
            ASTNode *req = decl->data.ability_decl.require_fields[i];
            char *consumer_name;
            if (req == NULL || req->type != AST_REQUIRE_FIELD)
                continue;
            consumer_name = tc_strdup_fmt("ability %s.%s",
                                          decl->data.ability_decl.name != NULL
                                              ? decl->data.ability_decl.name : "<ability>",
                                          req->data.require_field.name != NULL
                                              ? req->data.require_field.name : "<require-field>");
            if (consumer_name == NULL)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                req->data.require_field.type,
                ctx,
                req,
                consumer_name,
                "ability require-field type lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.ability_decl.methods,
            decl->data.ability_decl.method_count,
            ctx,
            decl->data.ability_decl.name);
        break;

    case AST_ROLE_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.role_decl.generic_params,
            decl->data.role_decl.where_clause,
            ctx,
            decl,
            "role",
            decl->data.role_decl.name);
        (void)semantic_stage_resolve_type_quiet(
            decl->data.role_decl.for_type,
            ctx,
            decl,
            decl->data.role_decl.name,
            "role host-type lookup");
        for (size_t i = 0; i < decl->data.role_decl.include_count; i++) {
            ASTNode *inc = decl->data.role_decl.includes[i];
            ASTNode *included_role_decl;
            ASTNode **effective = NULL;
            size_t effective_count = 0;

            if (inc == NULL || inc->type != AST_INCLUDE_STMT)
                continue;

            included_role_decl = semantic_stage_named_decl_quiet(
                ctx,
                AST_ROLE_DECL,
                inc->data.include_stmt.role_name);
            effective = collect_effective_generic_arg_nodes(
                (included_role_decl != NULL && included_role_decl->type == AST_ROLE_DECL)
                    ? included_role_decl->data.role_decl.generic_params
                    : NULL,
                inc->data.include_stmt.type_args,
                inc,
                ctx,
                "role include",
                inc->data.include_stmt.role_name,
                &effective_count);
            free(effective);
            (void)effective_count;
        }
        for (size_t i = 0; i < decl->data.role_decl.impl_count; i++) {
            ASTNode *impl = decl->data.role_decl.impl_abilities[i];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;
            if (impl->data.impl_ability.ability_ref != NULL
                && impl->data.impl_ability.ability_ref->type == AST_TYPE
                && impl->data.impl_ability.ability_ref->data.type.name != NULL) {
                (void)semantic_stage_named_decl_quiet(
                    ctx,
                    AST_ABILITY_DECL,
                    impl->data.impl_ability.ability_ref->data.type.name);
            }
            (void)semantic_stage_resolve_type_quiet(
                impl->data.impl_ability.ability_ref,
                ctx,
                impl,
                decl->data.role_decl.name,
                "role impl ability lookup");
        }
        break;

    case AST_PARTY_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.party_decl.generic_params,
            NULL,
            ctx,
            decl,
            "party",
            decl->data.party_decl.name);
        (void)semantic_stage_resolve_type_quiet(
            decl->data.party_decl.extends,
            ctx,
            decl,
            decl->data.party_decl.name,
            "party extends lookup");
        for (size_t i = 0; i < decl->data.party_decl.shared_count; i++) {
            ASTNode *field = decl->data.party_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "party shared field type lookup");
        }
        for (size_t i = 0; i < decl->data.party_decl.role_count; i++) {
            ASTNode *role_slot = decl->data.party_decl.role_slots[i];
            char *consumer_name;
            if (role_slot == NULL || role_slot->type != AST_ROLE_SLOT)
                continue;
            consumer_name = tc_strdup_fmt("party %s.%s",
                                          decl->data.party_decl.name != NULL
                                              ? decl->data.party_decl.name : "<party>",
                                          role_slot->data.role_slot.slot_name != NULL
                                              ? role_slot->data.role_slot.slot_name : "<role-slot>");
            if (consumer_name == NULL)
                continue;
            semantic_stage_required_abilities(
                role_slot->data.role_slot.required_abilities,
                role_slot->data.role_slot.ability_count,
                ctx,
                role_slot,
                consumer_name,
                "party role slot ability consumer lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.party_decl.methods,
            decl->data.party_decl.method_count,
            ctx,
            decl->data.party_decl.name);
        break;

    case AST_ROSTER_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.roster_decl.generic_params,
            NULL,
            ctx,
            decl,
            "roster",
            decl->data.roster_decl.name);
        for (size_t i = 0; i < decl->data.roster_decl.party_count; i++) {
            ASTNode *slot = decl->data.roster_decl.party_slots[i];
            if (slot == NULL || slot->type != AST_SYSTEMIC_SLOT)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_PARTY_DECL,
                slot->data.roster_slot.party_type);
        }
        for (size_t i = 0; i < decl->data.roster_decl.shared_count; i++) {
            ASTNode *field = decl->data.roster_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "roster shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.roster_decl.methods,
            decl->data.roster_decl.method_count,
            ctx,
            decl->data.roster_decl.name);
        break;

    case AST_WORLD_DECL:
        ctx->current_world = decl;
        for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
            ASTNode *roster = decl->data.world_decl.rosters[i];
            if (roster == NULL || roster->type != AST_WORLD_SYSTEMIC)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ROSTER_DECL,
                roster->data.world_roster.roster_type);
        }
        for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
            ASTNode *zone = decl->data.world_decl.zones[i];
            if (zone == NULL || zone->type != AST_WORLD_ZONE)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ZONE_DECL,
                zone->data.world_zone.zone_type);
        }
        semantic_stage_world_local_contracts(decl, ctx);
        for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
            ASTNode *field = decl->data.world_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "world shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.world_decl.methods,
            decl->data.world_decl.method_count,
            ctx,
            decl->data.world_decl.name);
        break;

    case AST_INTENT_DECL:
        for (size_t i = 0; i < decl->data.intent_decl.involve_count; i++) {
            ASTNode *binding = decl->data.intent_decl.involves[i];
            if (binding == NULL || binding->type != AST_INTENT_INVOLVES)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                binding->data.intent_involves.subject_type,
                ctx,
                binding,
                binding->data.intent_involves.alias,
                "intent involves type lookup");
        }
        for (size_t i = 0; i < decl->data.intent_decl.value_count; i++) {
            ASTNode *binding = decl->data.intent_decl.values[i];
            if (binding == NULL || binding->type != AST_INTENT_VALUE)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                binding->data.intent_value.value_type,
                ctx,
                binding,
                binding->data.intent_value.alias,
                "intent value type lookup");
        }
        (void)semantic_stage_resolve_type_quiet(
            decl->data.intent_decl.default_where_type,
            ctx,
            decl,
            decl->data.intent_decl.name,
            "intent default where-type lookup");
        for (size_t i = 0; i < decl->data.intent_decl.step_count; i++) {
            ASTNode *step = decl->data.intent_decl.steps[i];
            char *step_consumer_name;
            if (step == NULL || step->type != AST_INTENT_STEP)
                continue;
            step_consumer_name = tc_strdup_fmt(
                "intent %s.%s",
                decl->data.intent_decl.name != NULL
                    ? decl->data.intent_decl.name : "<intent>",
                step->data.intent_step.name != NULL
                    ? step->data.intent_step.name : "<step>");
            if (step_consumer_name == NULL)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                step->data.intent_step.where_type,
                ctx,
                step,
                step_consumer_name,
                "intent step where-type lookup");
            semantic_stage_required_abilities(
                step->data.intent_step.required_abilities,
                step->data.intent_step.required_ability_count,
                ctx,
                step,
                step_consumer_name,
                "intent step ability consumer lookup");
            free(step_consumer_name);
        }
        break;

    case AST_RELATION_DECL:
        ctx->current_relation = decl;
        (void)semantic_stage_resolve_type_quiet(
            decl->data.relation_decl.between_left_type,
            ctx,
            decl,
            decl->data.relation_decl.name,
            "relation between-left type lookup");
        (void)semantic_stage_resolve_type_quiet(
            decl->data.relation_decl.between_right_type,
            ctx,
            decl,
            decl->data.relation_decl.name,
            "relation between-right type lookup");
        for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
            ASTNode *slot = decl->data.relation_decl.slots[i];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                slot->data.domain_slot.type,
                ctx,
                slot,
                slot->data.domain_slot.slot_name,
                "relation slot type lookup");
        }
        for (size_t i = 0; i < decl->data.relation_decl.shared_count; i++) {
            ASTNode *field = decl->data.relation_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "relation shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.relation_decl.methods,
            decl->data.relation_decl.method_count,
            ctx,
            decl->data.relation_decl.name);
        break;

    case AST_EFFECT_DECL:
        ctx->current_effect = decl;
        for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
            ASTNode *slot = decl->data.effect_decl.slots[i];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                slot->data.domain_slot.type,
                ctx,
                slot,
                slot->data.domain_slot.slot_name,
                "effect slot type lookup");
        }
        for (size_t i = 0; i < decl->data.effect_decl.shared_count; i++) {
            ASTNode *field = decl->data.effect_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "effect shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.effect_decl.methods,
            decl->data.effect_decl.method_count,
            ctx,
            decl->data.effect_decl.name);
        break;

    case AST_ZONE_DECL:
        ctx->current_zone = decl;
        for (size_t i = 0; i < decl->data.zone_decl.slot_count; i++) {
            ASTNode *slot = decl->data.zone_decl.slots[i];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                slot->data.domain_slot.type,
                ctx,
                slot,
                slot->data.domain_slot.slot_name,
                "zone slot type lookup");
        }
        for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
            ASTNode *layer = decl->data.zone_decl.layer_slots[i];
            if (layer == NULL || layer->type != AST_ZONE_LAYER_SLOT)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                layer->data.zone_layer_slot.is_relation
                    ? AST_RELATION_DECL
                    : AST_EFFECT_DECL,
                layer->data.zone_layer_slot.layer_type);
        }
        semantic_stage_zone_local_contracts(decl);
        for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
            ASTNode *field = decl->data.zone_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "zone shared field type lookup");
        }
        for (size_t i = 0; i < decl->data.zone_decl.authority_count; i++) {
            ASTNode *authority = decl->data.zone_decl.authorities[i];
            char *consumer_name;
            if (authority == NULL || authority->type != AST_ZONE_AUTHORITY)
                continue;
            consumer_name = tc_strdup_fmt("zone %s.%s",
                                          decl->data.zone_decl.name != NULL
                                              ? decl->data.zone_decl.name : "<zone>",
                                          authority->data.zone_authority.subject_slot_name != NULL
                                              ? authority->data.zone_authority.subject_slot_name
                                              : "<authority>");
            if (consumer_name == NULL)
                continue;
            semantic_stage_required_abilities(
                authority->data.zone_authority.required_abilities,
                authority->data.zone_authority.ability_count,
                ctx,
                authority,
                consumer_name,
                "zone authority ability consumer lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.zone_decl.methods,
            decl->data.zone_decl.method_count,
            ctx,
            decl->data.zone_decl.name);
        break;

    default:
        break;
    }

    ctx->current_nominal_decl = saved_nominal;
    ctx->current_relation = saved_relation;
    ctx->current_effect = saved_effect;
    ctx->current_zone = saved_zone;
    ctx->current_world = saved_world;
}

static void
semantic_run_type_resolution_worklist(ASTNode *program,
                                      SemanticContext *ctx,
                                      size_t *topo_order,
                                      size_t topo_count)
{
    TypeResolutionGraph *graph;

    if (program == NULL || ctx == NULL || topo_order == NULL)
        return;

    graph = &ctx->type_resolution_graph;
    for (size_t i = topo_count; i > 0; i--) {
        size_t node_index = topo_order[i - 1];
        TypeResolutionNode *node;
        ASTNode *decl;
        ASTNode *host_decl;

        if (node_index >= graph->node_count)
            continue;
        node = &graph->nodes[node_index];
        if (node->kind == TYPE_RES_NODE_DECL || node->kind == TYPE_RES_NODE_ALIAS) {
            decl = semantic_find_top_level_decl_by_label(program,
                                                         node->label,
                                                         node->kind);
            if (decl == NULL)
                continue;

            semantic_stage_top_level_decl(decl, ctx);
            continue;
        }

        if (node->kind == TYPE_RES_NODE_LOCAL_CONTRACT
            || node->kind == TYPE_RES_NODE_PROJECTION_PATH) {
            host_decl = semantic_find_graph_host_decl(program, node->label);
            if (host_decl == NULL)
                continue;
            if (host_decl->type == AST_WORLD_DECL)
                semantic_stage_world_local_contract_from_label(host_decl,
                                                               node->label,
                                                               ctx);
            else if (host_decl->type == AST_ZONE_DECL)
                semantic_stage_zone_local_contract_from_label(host_decl,
                                                              node->label,
                                                              ctx);
        }
    }
}

static void
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
        visited = calloc(graph->node_count, sizeof(bool));
        path = calloc(graph->node_count > 0 ? graph->node_count : 1, sizeof(size_t));
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
                semantic_error(ctx, consumer_site,
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
        free(visited);
        free(path);
    }

    type_resolution_add_edge(graph, from, to, reason);
}

static char *
format_generic_subject_signature(const char *name, GenericParams *params)
{
    char *result;

    if (name == NULL)
        return tc_strdup_fmt("<generic>");
    if (params == NULL || params->count == 0)
        return tc_strdup_fmt("%s", name);

    result = tc_strdup_fmt("%s<", name);
    if (result == NULL)
        return tc_strdup_fmt("%s", name);

    for (size_t i = 0; i < params->count; i++) {
        GenericParam *gp = params->params[i];
        const char *param_name =
            (gp != NULL && gp->name != NULL) ? gp->name : "<type>";
        char *next = (i + 1 < params->count)
            ? tc_strdup_fmt("%s%s, ", result, param_name)
            : tc_strdup_fmt("%s%s>", result, param_name);
        free(result);
        result = next;
        if (result == NULL)
            return tc_strdup_fmt("%s", name);
    }

    return result;
}

static char *
format_effective_generic_type_list(const char *name, Type **types, size_t count)
{
    char *result;

    if (name == NULL)
        return tc_strdup_fmt("<generic>");
    if (types == NULL || count == 0)
        return tc_strdup_fmt("%s", name);

    result = tc_strdup_fmt("%s<", name);
    if (result == NULL)
        return tc_strdup_fmt("%s", name);

    for (size_t i = 0; i < count; i++) {
        const char *type_name =
            (types[i] != NULL && types[i]->name != NULL)
                ? types[i]->name : "<type>";
        char *next = (i + 1 < count)
            ? tc_strdup_fmt("%s%s, ", result, type_name)
            : tc_strdup_fmt("%s%s>", result, type_name);
        free(result);
        result = next;
        if (result == NULL)
            return tc_strdup_fmt("%s", name);
    }

    return result;
}

bool
identifier_is_borrowed_boundary_param(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *func_decl;
    const char *ident_name;

    if (expr == NULL || ctx == NULL
        || expr->type != AST_IDENTIFIER
        || expr->data.identifier.name == NULL) {
        return false;
    }

    func_decl = ctx->current_function_decl;
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;

    ident_name = expr->data.identifier.name;
    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        Type *param_type;

        if (param == NULL || param->name == NULL || param->type == NULL)
            continue;
        if (param->mode != PARAM_MODE_REF)
            continue;
        if (strcmp(param->name, ident_name) != 0)
            continue;

        param_type = resolve_type_node(param->type, ctx);
        return type_is_general_boundary_type(param_type, ctx);
    }

    return false;
}

static size_t
generic_params_required_count(GenericParams *params)
{
    size_t required = 0;
    if (params == NULL)
        return 0;
    for (size_t i = 0; i < params->count; i++) {
        GenericParam *param = params->params[i];
        if (param != NULL && param->default_type == NULL)
            required++;
    }
    return required;
}

static ASTNode **
collect_effective_generic_arg_nodes(GenericParams *decl_params,
                                    GenericParams *provided_args,
                                    const ASTNode *site,
                                    SemanticContext *ctx,
                                    const char *owner_kind,
                                    const char *owner_name,
                                    size_t *out_count)
{
    size_t decl_count;
    size_t provided_count;
    size_t required_count;
    ASTNode **effective = NULL;

    if (out_count != NULL)
        *out_count = 0;
    if (decl_params == NULL)
        return NULL;

    decl_count = decl_params->count;
    provided_count = provided_args != NULL ? provided_args->count : 0;
    required_count = generic_params_required_count(decl_params);

    if (decl_count == 0) {
        if (provided_count == 0)
            return NULL;
        if (ctx != NULL) {
            semantic_error(ctx, site,
                "%s '%s' does not accept generic type arguments.\n"
                "Reason:\n"
                "- this declaration has no generic parameters\n"
                "- supplied type arguments therefore have nowhere to bind\n"
                "Fix:\n"
                "- remove the generic arguments at the use site\n"
                "- or declare generic parameters on %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
        }
        return NULL;
    }

    if (provided_count > decl_count) {
        if (ctx != NULL) {
            semantic_error(ctx, site,
                "%s '%s' accepts at most %zu generic argument(s), got %zu.\n"
                "Reason:\n"
                "- more type arguments were supplied than there are generic parameters\n"
                "- effective generic argument derivation cannot match extras safely\n"
                "Fix:\n"
                "- remove the extra generic argument(s)\n"
                "- or add matching generic parameters to %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                decl_count, provided_count,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
        }
        return NULL;
    }

    if (provided_count < required_count) {
        if (ctx != NULL) {
            semantic_error(ctx, site,
                "%s '%s' requires at least %zu generic argument(s), got %zu.\n"
                "Reason:\n"
                "- some generic parameters have no default type argument\n"
                "- effective generic argument derivation therefore cannot close the contract\n"
                "Fix:\n"
                "- provide the missing generic argument(s)\n"
                "- or declare trailing default type arguments on %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                required_count, provided_count,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
        }
        return NULL;
    }

    effective = calloc(decl_count > 0 ? decl_count : 1, sizeof(ASTNode *));
    if (effective == NULL)
        return NULL;

    for (size_t i = 0; i < decl_count; i++) {
        ASTNode *arg = NULL;
        if (provided_args != NULL && i < provided_args->count) {
            GenericParam *provided = provided_args->params[i];
            arg = provided != NULL ? provided->constraint : NULL;
            if (arg != NULL)
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    owner_name != NULL ? owner_name : "<anonymous>",
                    arg,
                    "provided generic argument lookup");
        } else if (decl_params->params[i] != NULL) {
            arg = decl_params->params[i]->default_type;
            if (arg != NULL)
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    owner_name != NULL ? owner_name : "<anonymous>",
                    arg,
                    "omitted default generic argument lookup");
        }

        if (arg == NULL) {
            if (ctx != NULL) {
                const char *param_name =
                    decl_params->params[i] != NULL && decl_params->params[i]->name != NULL
                        ? decl_params->params[i]->name
                        : "<type-param>";
                semantic_error(ctx, site,
                    "%s '%s' is missing generic argument for parameter '%s'.\n"
                    "Reason:\n"
                    "- this parameter has no provided argument and no usable default\n"
                    "- effective generic argument derivation stopped at '%s'\n"
                    "Fix:\n"
                    "- provide a type argument for '%s'\n"
                    "- or declare a default type argument for '%s'",
                    owner_kind != NULL ? owner_kind : "declaration",
                    owner_name != NULL ? owner_name : "<anonymous>",
                    param_name,
                    param_name,
                    param_name,
                    param_name);
            }
            free(effective);
            return NULL;
        }

        effective[i] = arg;
    }

    if (out_count != NULL)
        *out_count = decl_count;
    return effective;
}

static bool
subject_type_has_ability(ASTNode *program, const char *type_name,
                         ASTNode *ability_ref);
static ASTNode *
subject_type_find_base_ability_impl(ASTNode *program, const char *type_name,
                                    const char *ability_name);

static int
semantic_find_labeled_loop_depth(SemanticContext *ctx, const char *label)
{
    if (ctx == NULL || label == NULL)
        return -1;

    for (int i = ctx->loop_depth - 1; i >= 0; i--) {
        if (ctx->loop_labels[i] != NULL
            && strcmp(ctx->loop_labels[i], label) == 0) {
            return i;
        }
    }

    return -1;
}

static int
find_generic_param_index(GenericParams *gp, const char *param_name)
{
    if (gp == NULL || param_name == NULL)
        return -1;

    for (size_t i = 0; i < gp->count; i++) {
        if (gp->params[i] != NULL
            && gp->params[i]->name != NULL
            && strcmp(gp->params[i]->name, param_name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static bool
concrete_type_satisfies_bound(Type *concrete_type, ASTNode *bound_node,
                              SemanticContext *ctx)
{
    Type *bound_type;
    const char *bound_name;
    Symbol *bound_sym;

    if (concrete_type == NULL || bound_node == NULL || ctx == NULL)
        return false;

    bound_type = resolve_type_node(bound_node, ctx);
    if (bound_type != NULL
        && bound_type != TYPE_UNKNOWN
        && type_satisfies_constraint(concrete_type, bound_type)) {
        return true;
    }

    if (ctx->program_root == NULL
        || bound_node->type != AST_TYPE
        || bound_node->data.type.name == NULL
        || concrete_type->name == NULL) {
        return false;
    }

    bound_name = bound_node->data.type.name;
    bound_sym = scope_lookup(ctx->scope, bound_name);
    if ((bound_sym != NULL && bound_sym->kind == SYMBOL_ABILITY)
        || (ctx->program_root != NULL
            && find_ability_decl_by_name(ctx->program_root, bound_name) != NULL)) {
        return subject_type_has_ability(ctx->program_root,
                                        concrete_type->name,
                                        bound_node);
    }

    return false;
}

static void
validate_generic_param_default_bounds(GenericParams *gp,
                                      WhereClause *wc,
                                      SemanticContext *ctx,
                                      ASTNode *owner,
                                      const char *owner_kind,
                                      const char *owner_name)
{
    if (gp == NULL || wc == NULL || ctx == NULL)
        return;

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        GenericParam *param;
        Type *default_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = find_generic_param_index(gp, tc->type_param);
        if (param_index < 0 || (size_t)param_index >= gp->count) {
            semantic_error(ctx, owner != NULL ? owner : (ASTNode *)wc,
                "%s '%s' could not validate default generic bound for unknown parameter '%s'.\n"
                "Reason:\n"
                "- where clause references '%s'\n"
                "- that parameter does not exist in the generic parameter list\n"
                "Fix:\n"
                "- change the where-clause to reference an existing generic parameter\n"
                "- or add generic parameter '%s' to %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                tc->type_param,
                tc->type_param,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
            continue;
        }

        param = gp->params[param_index];
        if (param == NULL || param->default_type == NULL)
            continue;

        semantic_type_resolution_record_type_ref_dependency(
            ctx,
            owner != NULL ? owner : param->default_type,
            tc->type_param != NULL ? tc->type_param : "<type-param>",
            param->default_type,
            "default-bound subject lookup");

        default_type = resolve_type_node(param->default_type, ctx);
        if (default_type == NULL || default_type == TYPE_UNKNOWN) {
            semantic_error(ctx, owner != NULL ? owner : param->default_type,
                "Default generic type argument for parameter '%s' in %s '%s' could not be resolved.\n"
                "Reason:\n"
                "- default type argument participates in effective generic argument derivation\n"
                "- unresolved defaults make where-clause validation partial\n"
                "Fix:\n"
                "- provide a resolvable default type argument for '%s'\n"
                "- or remove the default and require the caller to pass the type argument explicitly",
                tc->type_param,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param);
            continue;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                (bound_node != NULL
                 && bound_node->type == AST_TYPE
                 && bound_node->data.type.name != NULL)
                    ? bound_node->data.type.name
                    : "<constraint>";

            if (bound_node != NULL) {
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    owner != NULL ? owner : bound_node,
                    tc->type_param != NULL ? tc->type_param : "<type-param>",
                    bound_node,
                    "default-bound constraint lookup");
            }

            if (concrete_type_satisfies_bound(default_type, bound_node, ctx)) {
                free(bounds_text);
                continue;
            }

            semantic_error(ctx, owner != NULL ? owner : param->default_type,
                "Default generic type argument '%s' does not satisfy constraint '%s' for parameter '%s' in %s '%s'.\n"
                "Reason:\n"
                "- %s '%s' declares '%s = %s'\n"
                "- where clause requires '%s: %s'\n"
                "- full bound set is '%s: %s'\n"
                "Fix:\n"
                "- change the default type argument to satisfy '%s'\n"
                "- or relax the where-clause on %s '%s'",
                default_type->name != NULL ? default_type->name : "<type>",
                bound_name,
                tc->type_param,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                default_type->name != NULL ? default_type->name : "<type>",
                tc->type_param,
                bound_name,
                tc->type_param,
                bounds_text != NULL ? bounds_text : "<constraint>",
                bound_name,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
            free(bounds_text);
        }
    }
}

static void
validate_class_where_clause_instantiation(ASTNode *class_decl,
                                          Type *constructed_type,
                                          ASTNode *site,
                                          SemanticContext *ctx)
{
    WhereClause *wc;
    GenericParams *gp;
    char *expected_text = NULL;

    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL
        || constructed_type == NULL
        || constructed_type->kind != TYPE_KIND_CONSTRUCTED
        || site == NULL || ctx == NULL) {
        return;
    }

    gp = class_decl->data.class_decl.generic_params;
    wc = class_decl->data.class_decl.where_clause;
    if (gp == NULL || gp->count == 0 || wc == NULL || wc->count == 0)
        return;
    expected_text = format_generic_subject_signature(
        class_decl->data.class_decl.name != NULL
            ? class_decl->data.class_decl.name : "<class>",
        gp);

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        Type *concrete_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = find_generic_param_index(gp, tc->type_param);
        if (param_index < 0
            || (size_t)param_index >= constructed_type->data.constructed.arg_count) {
            semantic_error(ctx, site,
                "Class '%s' could not validate where-clause parameter '%s' during instantiation.\n"
                "Reason:\n"
                "- instantiated type '%s' does not provide an effective type argument for '%s'\n"
                "- class where-clause validation cannot be trusted when a generic parameter is unresolved\n"
                "Fix:\n"
                "- pass/supply a type argument for '%s'\n"
                "- or fix the class generic parameter list/default arguments so '%s' is materialized",
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name : "<class>",
                tc->type_param,
                constructed_type->name != NULL ? constructed_type->name : "<constructed>",
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        concrete_type = constructed_type->data.constructed.args[param_index];
        if (concrete_type == NULL) {
            semantic_error(ctx, site,
                "Class '%s' could not resolve instantiated type argument for '%s'.\n"
                "Reason:\n"
                "- where-clause validation reached instantiation with no concrete type for '%s'\n"
                "- unresolved generic arguments make class specialization partial\n"
                "Fix:\n"
                "- pass a concrete type argument for '%s'\n"
                "- or fix the default type argument / imported type so it resolves",
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name : "<class>",
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                (bound_node != NULL
                 && bound_node->type == AST_TYPE
                 && bound_node->data.type.name != NULL)
                    ? bound_node->data.type.name
                    : "<constraint>";

            if (bound_node != NULL) {
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    bound_node,
                    "class instantiation where-bound lookup");
            }

            if (!concrete_type_satisfies_bound(concrete_type, bound_node, ctx)) {
                semantic_error(ctx, site,
                    "Type '%s' does not satisfy constraint '%s' for generic parameter '%s' in class '%s'.\n"
                    "Reason:\n"
                    "- class '%s' requires '%s: %s'\n"
                    "- full bound set is '%s: %s'\n"
                    "- expected type args are '%s'\n"
                    "- actual type args are '%s'\n"
                    "- instantiated type argument is '%s'\n"
                    "Fix:\n"
                    "- pass a type argument that satisfies '%s'\n"
                    "- or relax the class where-clause",
                    concrete_type->name != NULL ? concrete_type->name : "<type>",
                    bound_name,
                    tc->type_param,
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    tc->type_param,
                    bound_name,
                    tc->type_param,
                    bounds_text != NULL ? bounds_text : "<constraint>",
                    expected_text != NULL ? expected_text : "<class>",
                    constructed_type->name != NULL ? constructed_type->name : "<constructed>",
                    concrete_type->name != NULL ? concrete_type->name : "<type>",
                    bound_name,
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>");
            }
            free(bounds_text);
        }
    }
    free(expected_text);
}

static void
validate_class_where_clause_specialization_ast(ASTNode *class_decl,
                                               ASTNode *specialized_type,
                                               ASTNode *site,
                                               SemanticContext *ctx)
{
    GenericParams *decl_params;
    WhereClause *wc;
    ASTNode **effective_args = NULL;
    size_t effective_count = 0;
    char *expected_text = NULL;

    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL
        || specialized_type == NULL || specialized_type->type != AST_TYPE
        || site == NULL || ctx == NULL) {
        return;
    }

    decl_params = class_decl->data.class_decl.generic_params;
    wc = class_decl->data.class_decl.where_clause;
    if (decl_params == NULL || decl_params->count == 0
        || wc == NULL || wc->count == 0) {
        return;
    }
    expected_text = format_generic_subject_signature(
        class_decl->data.class_decl.name != NULL
            ? class_decl->data.class_decl.name : "<class>",
        decl_params);

    effective_args = collect_effective_generic_arg_nodes(
        decl_params,
        specialized_type->data.type.generic_args,
        specialized_type,
        ctx,
        "class",
        class_decl->data.class_decl.name != NULL
            ? class_decl->data.class_decl.name : "<class>",
        &effective_count);
    if (effective_args == NULL)
        return;

    for (size_t i = 0; i < effective_count; i++) {
        if (effective_args[i] != NULL) {
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                specialized_type,
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name : "<class>",
                effective_args[i],
                "class specialization effective argument lookup");
        }
    }

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        Type *concrete_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = find_generic_param_index(decl_params, tc->type_param);
        if (param_index < 0 || (size_t)param_index >= effective_count) {
            semantic_error(ctx, site,
                "Class '%s' could not validate where-clause parameter '%s' during specialization.\n"
                "Reason:\n"
                "- specialized type syntax did not materialize an effective type argument for '%s'\n"
                "- class where-clause validation cannot be trusted when a generic parameter is unresolved\n"
                "Fix:\n"
                "- provide/supply a type argument for '%s'\n"
                "- or fix the class generic parameter list/default arguments so '%s' is materialized",
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name : "<class>",
                tc->type_param,
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        concrete_type = resolve_type_node(effective_args[param_index], ctx);
        if (concrete_type == NULL) {
            semantic_error(ctx, site,
                "Class '%s' could not resolve specialized type argument for '%s'.\n"
                "Reason:\n"
                "- where-clause validation reached specialization with no concrete type for '%s'\n"
                "- unresolved generic arguments make specialization partial\n"
                "Fix:\n"
                "- pass a concrete type argument for '%s'\n"
                "- or fix the default type argument / imported type so it resolves",
                class_decl->data.class_decl.name != NULL
                    ? class_decl->data.class_decl.name : "<class>",
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                (bound_node != NULL
                 && bound_node->type == AST_TYPE
                 && bound_node->data.type.name != NULL)
                    ? bound_node->data.type.name
                    : "<constraint>";

            if (bound_node != NULL) {
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    bound_node,
                    "class specialization where-bound lookup");
            }

            if (!concrete_type_satisfies_bound(concrete_type, bound_node, ctx)) {
                semantic_error(ctx, site,
                    "Type '%s' does not satisfy constraint '%s' for generic parameter '%s' in class '%s'.\n"
                    "Reason:\n"
                    "- class '%s' requires '%s: %s'\n"
                    "- full bound set is '%s: %s'\n"
                    "- expected type args are '%s'\n"
                    "- actual type args are '%s'\n"
                    "- specialized type argument is '%s'\n"
                    "Fix:\n"
                    "- specialize '%s' with a type that satisfies '%s'\n"
                    "- or relax the class where-clause",
                    concrete_type->name != NULL ? concrete_type->name : "<type>",
                    bound_name,
                    tc->type_param,
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    tc->type_param,
                    bound_name,
                    tc->type_param,
                    bounds_text != NULL ? bounds_text : "<constraint>",
                    expected_text != NULL ? expected_text : "<class>",
                    specialized_type->data.type.name != NULL
                        ? specialized_type->data.type.name : "<specialized>",
                    concrete_type->name != NULL ? concrete_type->name : "<type>",
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>",
                    bound_name,
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name : "<class>");
            }
            free(bounds_text);
        }
    }

    free(expected_text);
    free(effective_args);
}

void
propagate_collapse_to_pool(SemanticContext *ctx, int32_t pool_id)
{
    if (pool_id < 0)
        return;
    /* Walk the entire scope chain and collapse all members of this pool */
    for (Scope *s = ctx->scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->symbol_count; i++) {
            Symbol *sym = s->symbols[i];
            if (sym != NULL && type_is_qubit(sym->type)
                && sym->qubit_info.entangle_pool_id == pool_id
                && sym->qubit_info.semantic_state != QUBIT_STATE_COLLAPSED
                && sym->qubit_info.semantic_state != QUBIT_STATE_CLASSICAL) {
                sym->qubit_info.semantic_state = QUBIT_STATE_COLLAPSED;
            }
        }
    }
}

Type *
type_check_qubit_use(ASTNode *expr, SemanticContext *ctx)
{
    if (expr != NULL && expr->type == AST_IDENTIFIER) {
        Symbol *sym = lookup_identifier_symbol(expr, ctx);
        if (sym == NULL) {
            if (name_looks_qualified(expr->data.identifier.name)) {
                semantic_error(ctx, expr,
                    "Undefined symbol '%s' (check namespace spelling or export visibility)",
                    expr->data.identifier.name);
            } else {
                semantic_error(ctx, expr,
                    "Undefined symbol '%s'",
                    expr->data.identifier.name);
            }
            return TYPE_UNKNOWN;
        }
        if (!type_is_qubit(sym->type)) {
            semantic_error(ctx, expr,
                "Expected a movable resource handle (currently QubitSlot), got '%s'.\n"
                "Reason:\n"
                "- this consumer path expects a move-only resource value\n"
                "- value '%s' has type '%s', which is not part of the current movable-resource subset\n"
                "Fix:\n"
                "- pass a QubitSlot value instead\n"
                "- or keep this value on the non-movable path",
                sym->type->name,
                expr->data.identifier.name != NULL ? expr->data.identifier.name : "<value>",
                sym->type->name);
            return TYPE_UNKNOWN;
        }
        if (sym->is_consumed) {
            semantic_error(ctx, expr,
                "%s '%s' was moved or released and cannot be used again.\n"
                "Reason:\n"
                "- value '%s' was already consumed by an ownership transfer or release path\n"
                "- move-only values cannot be reused after consumption\n"
                "Fix:\n"
                "- create/acquire a fresh %s value\n"
                "- or keep ownership in one binding and avoid the earlier move",
                resource_handle_display_name(sym->type),
                expr->data.identifier.name,
                expr->data.identifier.name != NULL ? expr->data.identifier.name : "<value>",
                resource_handle_display_name(sym->type));
            return TYPE_UNKNOWN;
        }
        sym->is_used = true;
        return sym->type;
    }

    if (expr_is_movable_resource_boundary(expr)) {
        semantic_error(ctx, expr,
            "Movable resources from recv/await must first be bound to a named variable before use.\n"
            "Reason:\n"
            "- transfer boundaries create a fresh move-only resource value\n"
            "- the ownership checker needs a stable binding to track later moves and releases\n"
            "Fix:\n"
            "- assign the recv/await result to a local variable first\n"
            "- then pass or consume that named binding");
        return TYPE_UNKNOWN;
    }

    return type_check_expression(expr, ctx);
}

Type *
type_get_constructed_arg(const Type *type, size_t index)
{
    if (type == NULL || type->kind != TYPE_KIND_CONSTRUCTED)
        return TYPE_UNKNOWN;
    if (index >= type->data.constructed.arg_count)
        return TYPE_UNKNOWN;
    return type->data.constructed.args[index];
}

/* Instrumentation counters for type-resolution audit (단계 1.0). */
static size_t g_resolve_type_node_calls = 0;
static size_t g_resolve_type_node_unique_nodes = 0;
static void **g_resolve_type_node_seen = NULL;
static size_t g_resolve_type_node_seen_cap = 0;

static void
resolve_type_node_stats_record(ASTNode *node)
{
    const char *env = getenv("PGY_TYPE_RES_STATS");
    if (env == NULL || env[0] == '\0' || env[0] == '0') return;
    g_resolve_type_node_calls++;
    for (size_t i = 0; i < g_resolve_type_node_unique_nodes; i++) {
        if (g_resolve_type_node_seen[i] == node) return;
    }
    if (g_resolve_type_node_unique_nodes == g_resolve_type_node_seen_cap) {
        size_t new_cap = g_resolve_type_node_seen_cap == 0 ? 64 : g_resolve_type_node_seen_cap * 2;
        void **grown = realloc(g_resolve_type_node_seen, new_cap * sizeof(void*));
        if (grown == NULL) return;
        g_resolve_type_node_seen = grown;
        g_resolve_type_node_seen_cap = new_cap;
    }
    g_resolve_type_node_seen[g_resolve_type_node_unique_nodes++] = node;
}

Type *
resolve_type_node(ASTNode *node, SemanticContext *ctx)
{
    resolve_type_node_stats_record(node);
    if (node == NULL)
        return TYPE_VOID;

    if (node->type == AST_CHANNEL_TYPE) {
        Type *inner = resolve_type_node(node->data.channel_type.element_type, ctx);
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_CHANNEL, args, 1);
    }

    if (node->type == AST_FUTURE_TYPE) {
        Type *inner = resolve_type_node(node->data.future_type.value_type, ctx);
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_FUTURE, args, 1);
    }

    if (node->type == AST_EVENT_HANDLER_TYPE) {
        size_t param_count = node->data.event_handler_type.param_count;
        Type **param_types = calloc(param_count > 0 ? param_count : 1, sizeof(Type *));
        Type *return_type = TYPE_VOID;
        Type *result;

        if (param_types == NULL)
            return TYPE_UNKNOWN;

        for (size_t i = 0; i < param_count; i++) {
            param_types[i] = resolve_type_node(
                node->data.event_handler_type.param_types[i], ctx);
        }

        if (node->data.event_handler_type.return_type != NULL)
            return_type = resolve_type_node(
                node->data.event_handler_type.return_type, ctx);

        result = type_create_function(param_types, param_count, return_type);
        free(param_types);
        return result != NULL ? result : TYPE_UNKNOWN;
    }

    if (node->type != AST_TYPE)
        return TYPE_UNKNOWN;

    const char *name = node->data.type.name;

    if (strcmp(name, "Slot") == 0 || strcmp(name, "SecureSlot") == 0
        || strcmp(name, "ReadView") == 0 || strcmp(name, "WriteView") == 0
        || strcmp(name, "MoveToken") == 0) {
        if (node->data.type.generic_args == NULL
            || node->data.type.generic_args->count != 1) {
            semantic_error(ctx, node,
                "%s requires exactly one type argument", name);
            return TYPE_UNKNOWN;
        }
        Type *inner = resolve_generic_type_arg(
            node->data.type.generic_args->params[0], ctx, node);
        if (strcmp(name, "SecureSlot") == 0)
            return type_create_slot(inner, true);
        if (strcmp(name, "ReadView") == 0)
            return type_create_read_view(inner);
        if (strcmp(name, "WriteView") == 0)
            return type_create_write_view(inner);
        if (strcmp(name, "MoveToken") == 0)
            return type_create_slot_access(inner, false, SLOT_ACCESS_MOVE_TOKEN);
        return type_create_slot(inner, false);
    }

    if (strcmp(name, "HashMap") == 0) {
        if (node->data.type.generic_args == NULL
            || node->data.type.generic_args->count != 2) {
            semantic_error(ctx, node,
                "HashMap requires exactly two type arguments");
            return TYPE_UNKNOWN;
        }

        Type *key = resolve_generic_type_arg(
            node->data.type.generic_args->params[0], ctx, node);
        Type *value = resolve_generic_type_arg(
            node->data.type.generic_args->params[1], ctx, node);
        if (!type_equals(key, TYPE_STRING)
            && !type_equals(key, TYPE_INT)
            && !type_equals(key, TYPE_LONG)
            && !type_equals(key, TYPE_BOOL)) {
            semantic_error(ctx, node,
                "HashMap currently supports only String, Int, Long, or Bool keys");
            return TYPE_UNKNOWN;
        }
        Type *args[2] = { key, value };
        return type_create_constructed(TYPE_HASHMAP, args, 2);
    }

    if (strcmp(name, "Set") == 0
        && node->data.type.generic_args != NULL
        && node->data.type.generic_args->count == 1) {
        Type *inner = resolve_generic_type_arg(
            node->data.type.generic_args->params[0], ctx, node);
        Type *args[1] = { inner };
        return type_create_constructed(TYPE_SET, args, 1);
    }

    Symbol *builtin_shadow = scope_lookup(ctx->scope, name);
    bool allow_builtin_constructed =
        builtin_shadow == NULL || builtin_shadow->kind != SYMBOL_CLASS;

    if (allow_builtin_constructed
        && (strcmp(name, "Array") == 0 || strcmp(name, "Slice") == 0
            || strcmp(name, "List") == 0 || strcmp(name, "Queue") == 0
            || strcmp(name, "Box") == 0 || strcmp(name, "Rc") == 0
            || strcmp(name, "Weak") == 0 || strcmp(name, "Channel") == 0
            || strcmp(name, "Future") == 0 || strcmp(name, "RemoteFuture") == 0
            || strcmp(name, "Token") == 0
            || strcmp(name, "DeviceSlot") == 0 || strcmp(name, "Result") == 0
            || strcmp(name, "Option") == 0)) {
        size_t expected_min = strcmp(name, "Result") == 0 ? 1 : 1;
        size_t expected_max = strcmp(name, "Result") == 0 ? 2 : 1;
        size_t provided = node->data.type.generic_args != NULL
            ? node->data.type.generic_args->count : 0;
        if (node->data.type.generic_args == NULL
            || provided < expected_min
            || provided > expected_max) {
            semantic_error(ctx, node,
                strcmp(name, "Result") == 0
                    ? "Result requires one or two type arguments"
                    : "%s requires exactly one type argument",
                name);
            return TYPE_UNKNOWN;
        }

        Type *inner = resolve_generic_type_arg(
            node->data.type.generic_args->params[0], ctx, node);
        Type *constructor = TYPE_UNKNOWN;
        if (strcmp(name, "Array") == 0) constructor = TYPE_ARRAY;
        else if (strcmp(name, "Slice") == 0) constructor = TYPE_SLICE;
        else if (strcmp(name, "List") == 0) constructor = TYPE_LIST;
        else if (strcmp(name, "Queue") == 0) constructor = TYPE_QUEUE;
        else if (strcmp(name, "Box") == 0) constructor = TYPE_BOX;
        else if (strcmp(name, "Rc") == 0) constructor = TYPE_RC;
        else if (strcmp(name, "Weak") == 0) constructor = TYPE_WEAK;
        else if (strcmp(name, "Channel") == 0) constructor = TYPE_CHANNEL;
        else if (strcmp(name, "Future") == 0) constructor = TYPE_FUTURE;
        else if (strcmp(name, "RemoteFuture") == 0) constructor = TYPE_REMOTE_FUTURE;
        else if (strcmp(name, "Token") == 0) constructor = TYPE_TOKEN;
        else if (strcmp(name, "DeviceSlot") == 0) constructor = TYPE_DEVICE_SLOT;
        else if (strcmp(name, "Result") == 0) constructor = TYPE_RESULT;
        else if (strcmp(name, "Option") == 0) constructor = TYPE_OPTION;
        if (strcmp(name, "Result") == 0 && provided == 2) {
            Type *err = resolve_generic_type_arg(
                node->data.type.generic_args->params[1], ctx, node);
            Type *args[2] = { inner, err };
            return type_create_constructed(constructor, args, 2);
        }
        Type *args[1] = { inner };
        return type_create_constructed(constructor, args, 1);
    }

    /* User-defined generic class: Node<Int>, Pair<String> etc.
     * If the name resolves to a SYMBOL_CLASS and generic_args are present,
     * build a TYPE_KIND_CONSTRUCTED type so the class specialization can
     * be tracked through the type system. */
    {
        Symbol *sym = scope_lookup(ctx->scope, name);
        if (sym != NULL && sym->kind == SYMBOL_CLASS && sym->type != NULL
            && ctx != NULL && ctx->program_root != NULL) {
            ASTNode *class_decl = find_type_decl_by_name(ctx->program_root, name);
            if (class_decl != NULL
                && class_decl->type == AST_CLASS_DECL
                && class_decl->data.class_decl.generic_params != NULL
                && class_decl->data.class_decl.generic_params->count > 0) {
                size_t argc = 0;
                ASTNode **effective_args = collect_effective_generic_arg_nodes(
                    class_decl->data.class_decl.generic_params,
                    node->data.type.generic_args,
                    node, ctx, "class", name, &argc);
                if (effective_args == NULL)
                    return TYPE_UNKNOWN;

                Type **args = calloc(argc > 0 ? argc : 1, sizeof(Type *));
                if (args != NULL) {
                    for (size_t i = 0; i < argc; i++)
                        args[i] = resolve_type_node(effective_args[i], ctx);
                    free(effective_args);
                    Type *result = type_create_constructed(sym->type, args, argc);
                    validate_class_where_clause_instantiation(class_decl,
                                                              result,
                                                              node,
                                                              ctx);
                    free(args);
                    return result;
                }
                free(effective_args);
            }
        }
    }

    return resolve_named_type(name, ctx, node);
}

bool
require_assignable(Type *from, Type *to,
                    const ASTNode *site, SemanticContext *ctx)
{
    if (type_is_assignable(from, to))
        return true;

    /* Slot sugar: allow assigning T to Slot<T> (auto Claim+Write) */
    if (to->kind == TYPE_KIND_SLOT && to->data.slot.inner_type != NULL
        && type_is_assignable(from, to->data.slot.inner_type))
        return true;

    semantic_error(ctx, site,
        "Type mismatch: cannot assign '%s' to '%s'",
        from->name, to->name);
    return false;
}

Type *
wrap_constructed(Type *constructor, Type *inner)
{
    Type *args[1] = { inner };
    return type_create_constructed(constructor, args, 1);
}

#include "type_checker_operator_expr.inc"

Type *
type_check_expression(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL)
        return TYPE_VOID;

    switch (expr->type) {
    case AST_NUMBER:
        return TYPE_INT;

    case AST_STRING:
        return TYPE_STRING;

    case AST_BOOLEAN:
        return TYPE_BOOL;

    case AST_LAMBDA_EXPR: {
        size_t param_count = expr->data.lambda_expr.param_count;
        Type **param_types = calloc(param_count > 0 ? param_count : 1, sizeof(Type *));
        Type *return_type = TYPE_VOID;
        Type *result;

        if (param_types == NULL)
            return TYPE_UNKNOWN;

        scope_enter(&ctx->scope, SCOPE_FUNCTION);
        for (size_t i = 0; i < param_count; i++) {
            ASTNode *param = expr->data.lambda_expr.params[i];
            const char *param_name = NULL;
            Type *param_type = TYPE_UNKNOWN;

            if (param != NULL && param->type == AST_LET_DECL) {
                param_name = param->data.let_decl.name;
                if (param->data.let_decl.type != NULL)
                    param_type = resolve_type_node(param->data.let_decl.type, ctx);
            } else if (param != NULL && param->type == AST_IDENTIFIER) {
                param_name = param->data.identifier.name;
            }

            if (param_name != NULL) {
                Symbol *param_sym = symbol_create_variable(
                    param_name, param_type, expr->line, expr->column);
                if (param_sym != NULL)
                    scope_declare(ctx->scope, param_sym);
            }
            param_types[i] = param_type;
        }

        if (expr->data.lambda_expr.return_type != NULL) {
            return_type = resolve_type_node(expr->data.lambda_expr.return_type, ctx);
        } else if (expr->data.lambda_expr.body != NULL
                   && expr->data.lambda_expr.body->type != AST_BLOCK) {
            return_type = type_check_expression(expr->data.lambda_expr.body, ctx);
        } else {
            return_type = TYPE_VOID;
        }

        if (expr->data.lambda_expr.body != NULL
            && expr->data.lambda_expr.body->type == AST_BLOCK) {
            bool saved_in_async = ctx->in_async_func;
            Type *saved_return = ctx->current_return;
            ctx->in_async_func = expr->data.lambda_expr.is_async;
            ctx->current_return = return_type;
            type_check_block(expr->data.lambda_expr.body, ctx);
            ctx->current_return = saved_return;
            ctx->in_async_func = saved_in_async;
        }

        scope_exit(&ctx->scope);
        result = type_create_function(param_types, param_count, return_type);
        free(param_types);
        return result != NULL ? result : TYPE_UNKNOWN;
    }

    case AST_IDENTIFIER: {
        /* Special handling for Option value constructors used without parens */
        if (strcmp(expr->data.identifier.name, "None") == 0) {
            return wrap_constructed(TYPE_OPTION, TYPE_UNKNOWN);
        }
        Symbol *sym = scope_lookup(ctx->scope,
                                    expr->data.identifier.name);
        if (sym == NULL) {
            Type *field_type = current_host_field_type(ctx, expr->data.identifier.name);
            if (field_type != NULL)
                return field_type;
            if (name_looks_qualified(expr->data.identifier.name)) {
                semantic_error(ctx, expr,
                    "Undefined symbol '%s' (check namespace spelling or export visibility)",
                    expr->data.identifier.name);
            } else {
                semantic_error(ctx, expr,
                    "Undefined symbol '%s'",
                    expr->data.identifier.name);
            }
            return TYPE_UNKNOWN;
        }
        if ((type_is_general_boundary_type(sym->type, ctx)
             || type_is_move_token(sym->type))
            && sym->is_consumed) {
            semantic_error(ctx, expr,
                "%s '%s' was moved or released and cannot be used again.\n"
                "Reason:\n"
                "- value '%s' was already consumed by an ownership transfer or release path\n"
                "- ownership-bearing boundary values cannot be reused after consumption\n"
                "Fix:\n"
                "- create/acquire a fresh %s value\n"
                "- or keep ownership in one binding and avoid the earlier move",
                type_is_subject_type(sym->type, ctx)
                    ? type_name_or_unknown(sym->type)
                    : resource_handle_display_name(sym->type),
                expr->data.identifier.name,
                expr->data.identifier.name != NULL ? expr->data.identifier.name : "<value>",
                type_is_subject_type(sym->type, ctx)
                    ? type_name_or_unknown(sym->type)
                    : resource_handle_display_name(sym->type));
            return TYPE_UNKNOWN;
        }
        sym->is_used = true;
        return sym->type;
    }

    case AST_BINARY:
        return type_check_binary(expr, ctx);

    case AST_UNARY:
        return type_check_unary(expr, ctx);

    case AST_CALL:
        return type_check_call(expr, ctx);

    case AST_MEMBER_ACCESS:
        return type_check_member_access(expr, ctx);

    case AST_ARRAY_ACCESS:
        return type_check_array_access(expr, ctx);

    case AST_ARRAY_LITERAL:
        return type_check_array_literal(expr, ctx);

    case AST_ASSIGNMENT:
        return type_check_assignment(expr, ctx);

    case AST_AWAIT_EXPR:
        if (!ctx->in_async_func) {
            semantic_error(ctx, expr,
                "'await' used outside of async function");
        }
        semantic_record_effect(ctx, EFFECT_REMOTE);
        {
            Type *future_type = type_check_expression(expr->data.await_expr.expression, ctx);
            if (future_type != NULL
                && future_type->kind == TYPE_KIND_CONSTRUCTED
                && (type_equals(future_type->data.constructed.constructor, TYPE_FUTURE)
                    || type_equals(future_type->data.constructed.constructor, TYPE_REMOTE_FUTURE))
                && future_type->data.constructed.arg_count == 1) {
                Type *inner = future_type->data.constructed.args[0];
                if (type_is_anchored_resource_handle(inner)) {
                    semantic_error(ctx, expr->data.await_expr.expression,
                        "'await' cannot yield anchored resource handles (Slot/SecureSlot/DeviceSlot) yet; await a plain value or movable transfer result instead");
                    return TYPE_UNKNOWN;
                }
                /* RemoteFuture<T> → Result<T>: remote operations can fail
                 * (network partition, timeout, etc.) so the result must be
                 * explicitly handled.  Local Future<T> → T as before. */
                if (type_equals(future_type->data.constructed.constructor, TYPE_REMOTE_FUTURE)) {
                    Type *result_args[1] = { inner };
                    return type_create_constructed(TYPE_RESULT, result_args, 1);
                }
                return inner;
            }
            semantic_error(ctx, expr->data.await_expr.expression,
                "'await' requires Future<T> or RemoteFuture<T>");
            return TYPE_UNKNOWN;
        }

    case AST_SPAWN_EXPR:
        return type_check_spawn_expr(expr, ctx);

    case AST_CHANNEL_SEND:
        return type_check_channel_send(expr, ctx);

    case AST_CHANNEL_RECV:
        return type_check_channel_recv(expr, ctx);

    default:
        return TYPE_UNKNOWN;
    }
}

Type *
type_check_binary(ASTNode *expr, SemanticContext *ctx)
{
    Type *left  = type_check_expression(expr->data.binary.left,  ctx);
    Type *right = type_check_expression(expr->data.binary.right, ctx);

    if (type_is_slot_handle(left) && left->data.slot.inner_type != NULL)
        left = left->data.slot.inner_type;
    if (type_is_slot_handle(right) && right->data.slot.inner_type != NULL)
        right = right->data.slot.inner_type;

    Type *overloaded = type_check_operator_overload(expr, ctx, left, right);
    if (overloaded == NULL)
        overloaded = type_check_role_operator_overload(expr, ctx, left, right);
    if (overloaded != NULL)
        return overloaded;

    /* If either operand is unknown, skip checks and propagate */
    if (left == TYPE_UNKNOWN || right == TYPE_UNKNOWN) {
        PgyTokenType op = expr->data.binary.op.type;
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL)
            return TYPE_BOOL;
        return (left != TYPE_UNKNOWN) ? left : right;
    }

    /* Comparison operators → Bool */
    PgyTokenType op = expr->data.binary.op.type;
    if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
        || op == TOKEN_LESS     || op == TOKEN_LESS_EQUAL
        || op == TOKEN_GREATER  || op == TOKEN_GREATER_EQUAL) {
        if (!type_equals(left, right)) {
            semantic_error(ctx, expr,
                "Cannot compare '%s' and '%s'",
                left->name, right->name);
        }
        return TYPE_BOOL;
    }

    /* Arithmetic: both operands must match */
    if (!type_equals(left, right)) {
        semantic_error(ctx, expr,
            "Type mismatch in binary operation: '%s' and '%s'",
            left->name, right->name);
        return TYPE_UNKNOWN;
    }

    return left;
}

Type *
type_check_unary(ASTNode *expr, SemanticContext *ctx)
{
    Type *operand = type_check_expression(expr->data.unary.operand, ctx);

    PgyTokenType op = expr->data.unary.op.type;
    if (op == TOKEN_NOT) {
        if (!type_equals(operand, TYPE_BOOL)) {
            semantic_error(ctx, expr,
                "'!' operator requires Bool, got '%s'", operand->name);
        }
        return TYPE_BOOL;
    }

    if (op == TOKEN_MINUS) {
        if (!type_equals(operand, TYPE_INT)
            && !type_equals(operand, TYPE_FLOAT)) {
            semantic_error(ctx, expr,
                "Unary '-' requires numeric type, got '%s'",
                operand->name);
        }
        return operand;
    }

    /* Postfix ?: try/propagate — unwrap Result<T> to T, propagate error */
    if (op == TOKEN_QUESTION) {
        if (!type_is_constructed_named(operand, "Result")) {
            semantic_error(ctx, expr,
                "'?' operator requires Result<T> or Result<T, E>, got '%s'",
                operand->name);
            return TYPE_UNKNOWN;
        }
        /* Return the inner T of Result<T> / Result<T, E> */
        return type_get_constructed_arg(operand, 0);
    }

    return operand;
}

Type *
type_check_call(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee = expr->data.call.callee;

    if (callee->type == AST_IDENTIFIER) {
        const char *name = callee->data.identifier.name;
        BuiltinKind bk   = builtin_resolve(name);
        if (bk != BUILTIN_NOT_BUILTIN)
            return type_check_builtin_call(expr, bk, ctx);

        {
            ASTNode *host_method = current_host_method_decl(ctx, name);
            if (host_method != NULL)
                return type_check_host_method_call(expr, host_method, ctx);
        }

        /* Channel(capacity) is a built-in constructor that the transpiler
         * handles; let it pass without a symbol table entry. The actual
         * type is resolved from the annotation in emit_let_decl. */
        if (strcmp(name, "Channel") == 0)
            return TYPE_UNKNOWN;  /* type inferred from let annotation */

        /* Result built-in functions — Ok/Err still return TYPE_UNKNOWN
         * (type from let annotation); others go to stdlib_call */
        if (strcmp(name, "Ok") == 0 || strcmp(name, "Err") == 0)
            return TYPE_UNKNOWN;

        /* Standard library built-in functions */
        {
            Type *stdlib_type = type_check_stdlib_call(expr, name, ctx);
            if (stdlib_type != NULL)
                return stdlib_type;
        }

        Symbol *sym = scope_lookup(ctx->scope, name);
        return type_check_function_symbol_call(expr, sym, name, ctx);
    }

    /* Callee is a member access (method call) */
    if (callee->type == AST_MEMBER_ACCESS) {
        ASTNode *object = callee->data.member.object;
        const char *method_name = callee->data.member.name;

        if (object != NULL && method_name != NULL
            && (strcmp(method_name, "Write") == 0
                || strcmp(method_name, "Read") == 0
                || strcmp(method_name, "Release") == 0)) {
            Type *slot_type = type_check_expression(object, ctx);
            if (slot_type != NULL && slot_type->kind == TYPE_KIND_SLOT) {
                size_t orig_argc = expr->data.call.arg_count;
                bool inject_token = false;
                char token_name_buf[256];
                ASTNode token_arg;
                ASTNode *synthetic_args[4];
                ASTNode fake_call;
                size_t new_argc = 1 + orig_argc;

                memset(&token_arg, 0, sizeof(token_arg));
                memset(&fake_call, 0, sizeof(fake_call));

                if (slot_type->data.slot.is_secure
                    && object->type == AST_IDENTIFIER
                    && object->data.identifier.name != NULL) {
                    snprintf(token_name_buf, sizeof(token_name_buf), "%s_token",
                        object->data.identifier.name);
                    if ((strcmp(method_name, "Write") == 0 && orig_argc < 2)
                        || ((strcmp(method_name, "Read") == 0
                             || strcmp(method_name, "Release") == 0)
                            && orig_argc < 1)) {
                        inject_token = true;
                        new_argc++;
                        token_arg.type = AST_IDENTIFIER;
                        token_arg.data.identifier.name = token_name_buf;
                    }
                }

                if (new_argc <= sizeof(synthetic_args) / sizeof(synthetic_args[0])) {
                    synthetic_args[0] = object;
                    for (size_t i = 0; i < orig_argc; i++)
                        synthetic_args[i + 1] = expr->data.call.arguments[i];
                    if (inject_token)
                        synthetic_args[new_argc - 1] = &token_arg;

                    fake_call.type = AST_CALL;
                    fake_call.line = expr->line;
                    fake_call.column = expr->column;
                    fake_call.data.call.callee = callee;
                    fake_call.data.call.arguments = synthetic_args;
                    fake_call.data.call.arg_count = new_argc;

                    if (strcmp(method_name, "Write") == 0) {
                        (void)type_check_write_slot(&fake_call, ctx);
                        return TYPE_VOID;
                    }
                    if (strcmp(method_name, "Read") == 0)
                        return type_check_read_slot(&fake_call, ctx);

                    (void)type_check_release_slot(&fake_call, ctx);
                    return TYPE_VOID;
                }
            }
        }

        if (expr_is_static_member_access(callee)) {
            char *flat_name = flatten_static_member_access(callee, '_');
            char *display_name = flatten_static_member_access(callee, '.');
            Symbol *sym = flat_name != NULL
                ? scope_lookup(ctx->scope, flat_name)
                : NULL;
            if (sym == NULL && callee->data.member.name != NULL) {
                sym = scope_lookup(ctx->scope, callee->data.member.name);
            }
            Type *result = type_check_function_symbol_call(
                expr, sym, display_name != NULL ? display_name : "<member>", ctx);
            free(flat_name);
            free(display_name);
            return result;
        }

        if (!(object != NULL
              && object->type == AST_IDENTIFIER
              && object->data.identifier.name != NULL
              && object->data.identifier.name[0] >= 'A'
              && object->data.identifier.name[0] <= 'Z')) {
            reject_if_embedded_world_zone_mutation(ctx, expr, object, "hosted func/action call");
            /* Resolve object type for normal method calls.
             * Namespace/static-style calls like Math.Add are lowered later. */
            Type *object_type = type_check_expression(object, ctx);
            if (object_type != NULL
                && object_type->kind == TYPE_KIND_SLOT
                && method_name != NULL) {
                Symbol *sym = NULL;
                Symbol *owner = NULL;
                if (object->type == AST_IDENTIFIER)
                    sym = scope_lookup(ctx->scope, object->data.identifier.name);

                if (strcmp(method_name, "Write") == 0) {
                    if (expr->data.call.arg_count < 1) {
                        semantic_error(ctx, expr,
                            "slot.Write(value) requires a value argument");
                        return TYPE_UNKNOWN;
                    }
                    if (type_is_read_view(object_type)) {
                        semantic_error(ctx, object,
                            "Cannot write through ReadView<T>; create a WriteView(slot) or keep the owning Slot<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (type_is_move_token(object_type)) {
                        semantic_error(ctx, object,
                            "Cannot write through MoveToken<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (object_type->data.slot.is_secure)
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    if (sym != NULL && sym->kind == SYMBOL_SLOT) {
                        if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error(ctx, object,
                                "Cannot write to released slot '%s'",
                                sym->name);
                            return TYPE_UNKNOWN;
                        }
                    } else if (sym != NULL && type_is_write_view(sym->type)
                               && sym->slot_info.paired_slot_name != NULL) {
                        owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
                        if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error(ctx, object,
                                "Cannot write through WriteView '%s' because source slot '%s' was released",
                                sym->name, owner->name);
                            return TYPE_UNKNOWN;
                        }
                        if (owner != NULL && owner->slot_info.is_secure)
                            semantic_record_effect(ctx, EFFECT_SECURE);
                    }
                    require_assignable(
                        type_check_expression(expr->data.call.arguments[0], ctx),
                        object_type->data.slot.inner_type,
                        expr->data.call.arguments[0], ctx);
                    return TYPE_VOID;
                }

                if (strcmp(method_name, "Read") == 0) {
                    if (type_is_write_view(object_type)) {
                        semantic_error(ctx, object,
                            "Cannot read through WriteView<T>; create a ReadView(slot) or keep the owning Slot<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (type_is_move_token(object_type)) {
                        semantic_error(ctx, object,
                            "Cannot read through MoveToken<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (object_type->data.slot.is_secure)
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    if (sym != NULL && sym->kind == SYMBOL_SLOT) {
                        if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error(ctx, object,
                                "Cannot read from released slot '%s'",
                                sym->name);
                            return TYPE_UNKNOWN;
                        }
                    } else if (sym != NULL && type_is_read_view(sym->type)
                               && sym->slot_info.paired_slot_name != NULL) {
                        owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
                        if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error(ctx, object,
                                "Cannot read through ReadView '%s' because source slot '%s' was released",
                                sym->name, owner->name);
                            return TYPE_UNKNOWN;
                        }
                        if (owner != NULL && owner->slot_info.is_secure)
                            semantic_record_effect(ctx, EFFECT_SECURE);
                    }
                    return object_type->data.slot.inner_type;
                }

                if (strcmp(method_name, "Release") == 0) {
                    if (object->type != AST_IDENTIFIER || sym == NULL || sym->kind != SYMBOL_SLOT) {
                        semantic_error(ctx, object,
                            "slot.Release() requires an owning slot identifier");
                        return TYPE_UNKNOWN;
                    }
                    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                        semantic_error(ctx, object,
                            "Slot '%s' has already been released", sym->name);
                        return TYPE_UNKNOWN;
                    }
                    if (sym->slot_info.is_secure)
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    scope_release_slot(ctx->scope, sym->name);
                    return TYPE_VOID;
                }
            }
            ASTNode *class_decl;

            if (type_is_nominal_host_type(object_type, ctx)
                && object_type->name != NULL
                && method_name != NULL) {
                class_decl = find_type_decl_by_name(ctx->program_root,
                    object_type->name);
                if (class_decl != NULL) {
                    ASTNode **methods = NULL;
                    size_t method_count = 0;
                    if (class_decl->type == AST_CLASS_DECL) {
                        methods = class_decl->data.class_decl.methods;
                        method_count = class_decl->data.class_decl.method_count;
                    } else if (class_decl->type == AST_ENUM_DECL) {
                        methods = class_decl->data.enum_decl.methods;
                        method_count = class_decl->data.enum_decl.method_count;
                    }
                    for (size_t i = 0; i < method_count; i++) {
                        ASTNode *method = methods[i];
                        if (method == NULL || method->type != AST_FUNC_DECL
                            || method->data.func_decl.name == NULL)
                            continue;
                        if (strcmp(method->data.func_decl.name, method_name) == 0) {
                            uint32_t method_effects =
                                declared_effects_from_function_node(method, ctx, NULL);
                            if (!explicit_member_access_allowed(class_decl,
                                    object_type,
                                    method->data.func_decl.access,
                                    method->data.func_decl.has_explicit_access,
                                    ctx)) {
                                semantic_error(ctx, expr,
                                    "Member '%s.%s' is not accessible across the current visibility boundary",
                                    object_type->name,
                                    method_name);
                                return TYPE_UNKNOWN;
                            }
                            semantic_record_effect(ctx, method_effects);
                            if (ctx->in_parallel
                                && type_effect_mask_has(method_effects, EFFECT_SECURE)) {
                                semantic_error(ctx, expr,
                                    "Parallel context does not permit calling secure-effect method '%s.%s'; serialize capability-bearing operations outside the parallel block",
                                    object_type->name,
                                    method_name);
                                return TYPE_UNKNOWN;
                            }
                            if (method->data.func_decl.return_type != NULL)
                                return resolve_type_node(
                                    method->data.func_decl.return_type, ctx);
                            return TYPE_VOID;
                        }
                    }
                }
            }
        }
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}

Type *
type_check_member_access(ASTNode *expr, SemanticContext *ctx)
{
    if (expr_is_static_member_access(expr)) {
        char *flat_name = flatten_static_member_access(expr, '_');
        char *display_name = flatten_static_member_access(expr, '.');
        Symbol *sym = flat_name != NULL ? scope_lookup(ctx->scope, flat_name) : NULL;
        if (sym == NULL && expr->data.member.name != NULL)
            sym = scope_lookup(ctx->scope, expr->data.member.name);
        if (sym != NULL) {
            sym->is_used = true;
            free(flat_name);
            free(display_name);
            return sym->type;
        }
        semantic_error(ctx, expr,
            "Undefined symbol '%s' (check namespace spelling or export visibility)",
            display_name != NULL ? display_name : "<member>");
        free(flat_name);
        free(display_name);
        return TYPE_UNKNOWN;
    }

    Type *object_type = type_check_expression(expr->data.member.object, ctx);

    if ((type_is_constructed_named(object_type, "Array")
         || type_is_constructed_named(object_type, "Slice"))
        && strcmp(expr->data.member.name, "Length") == 0) {
        return TYPE_INT;
    }

    /* Resolve nominal/domain field types by looking up the declaration AST. */
    if (object_type != NULL && object_type->kind == TYPE_KIND_CLASS
        && object_type->name != NULL && ctx->program_root != NULL) {
        const char *field_name = expr->data.member.name;
        ASTNode *decl = find_type_decl_by_name(ctx->program_root, object_type->name);

        if (decl == NULL)
            decl = find_domain_decl_by_name(ctx->program_root, AST_ROSTER_DECL,
                object_type->name);
        if (decl == NULL)
            decl = find_domain_decl_by_name(ctx->program_root, AST_WORLD_DECL,
                object_type->name);
        if (decl == NULL)
            decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                object_type->name);
        if (decl == NULL)
            decl = find_domain_decl_by_name(ctx->program_root, AST_RELATION_DECL,
                object_type->name);
        if (decl == NULL)
            decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                object_type->name);

        if (decl != NULL && decl->type == AST_CLASS_DECL) {
            size_t field_count = projection_source_field_count(decl);
            for (size_t fi = 0; fi < field_count; fi++) {
                ClassField *cf = projection_source_field_at(decl, fi);
                if (cf == NULL || cf->name == NULL)
                    continue;
                if (strcmp(cf->name, field_name) == 0) {
                    if (!explicit_member_access_allowed(decl,
                            object_type,
                            cf->access,
                            cf->has_explicit_access,
                            ctx)) {
                        semantic_error(ctx, expr,
                            "Member '%s.%s' is not accessible across the current visibility boundary",
                            object_type->name,
                            field_name);
                        return TYPE_UNKNOWN;
                    }
                    return resolve_type_node(cf->type, ctx);
                }
            }
        } else if (decl != NULL) {
            if (decl->type == AST_WORLD_DECL) {
                for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
                    ASTNode *slot = decl->data.world_decl.rosters[i];
                    if (slot != NULL && slot->data.world_roster.slot_name != NULL
                        && strcmp(slot->data.world_roster.slot_name, field_name) == 0) {
                        return resolve_named_type(slot->data.world_roster.roster_type,
                            ctx, slot);
                    }
                }
                for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
                    ASTNode *slot = decl->data.world_decl.zones[i];
                    if (slot != NULL && slot->data.world_zone.slot_name != NULL
                        && strcmp(slot->data.world_zone.slot_name, field_name) == 0) {
                        return resolve_named_type(slot->data.world_zone.zone_type,
                            ctx, slot);
                    }
                }
            }

            for (size_t fi = 0; fi < overlay_field_count(decl); fi++) {
                const char *overlay_field_name = NULL;
                ASTNode *field_type_node = overlay_field_decl_at(decl, fi,
                    &overlay_field_name);
                if (overlay_field_name != NULL
                    && strcmp(overlay_field_name, field_name) == 0) {
                    return resolve_type_node(field_type_node, ctx);
                }
            }
        }

        /* Accept any remaining field access on nominal/domain types. */
        return TYPE_UNKNOWN;
    }

    /* Unknown member access — allow without error for class/enum types */
    if (object_type != NULL
        && (object_type->kind == TYPE_KIND_CLASS
            || object_type->kind == TYPE_KIND_ENUM))
        return TYPE_UNKNOWN;

    return TYPE_UNKNOWN;
}

Type *
type_check_array_access(ASTNode *expr, SemanticContext *ctx)
{
    Type *object_type = type_check_expression(expr->data.array_access.array, ctx);
    Type *index_type  = type_check_expression(expr->data.array_access.index, ctx);

    if (!type_equals(index_type, TYPE_INT)) {
        semantic_error(ctx, expr->data.array_access.index,
            "Array index must be Int, got '%s'", index_type->name);
        return TYPE_UNKNOWN;
    }

    if (type_is_constructed_named(object_type, "Array")
        || type_is_constructed_named(object_type, "Slice")) {
        return type_get_constructed_arg(object_type, 0);
    }

    semantic_error(ctx, expr->data.array_access.array,
        "Index access requires Array<T> or Slice<T>, got '%s'",
        object_type->name);
    return TYPE_UNKNOWN;
}

Type *
type_check_assignment(ASTNode *expr, SemanticContext *ctx)
{
    reject_if_embedded_world_zone_mutation(ctx, expr,
        expr->data.assignment.target, "assignment");
    Type *value_type  = type_check_expression(expr->data.assignment.value,  ctx);
    Type *target_type = type_check_expression(expr->data.assignment.target, ctx);

    if (type_is_slot_handle(target_type)
        && target_type->data.slot.inner_type != NULL
        && !type_is_resource_handle(value_type)
        && type_is_assignable(value_type, target_type->data.slot.inner_type)) {
        return target_type;
    }

    if (type_is_resource_handle(target_type) || type_is_resource_handle(value_type)) {
        if (expr->data.assignment.value != NULL
            && expr->data.assignment.value->type == AST_IDENTIFIER
            && identifier_is_borrowed_boundary_param(expr->data.assignment.value, ctx)) {
            const char *borrowed_name =
                expr->data.assignment.value->data.identifier.name != NULL
                    ? expr->data.assignment.value->data.identifier.name : "<value>";
            const char *target_name =
                expr->data.assignment.target != NULL
                && expr->data.assignment.target->type == AST_IDENTIFIER
                && expr->data.assignment.target->data.identifier.name != NULL
                    ? expr->data.assignment.target->data.identifier.name
                    : "<target>";
            semantic_error(ctx, expr,
                "Borrowed ref movable resource '%s' cannot escape through assignment rebind into '%s'.\n"
                "Reason:\n"
                "- consumer path is function '%s'\n"
                "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                "- assignment would rebind that borrow into target '%s'\n"
                "- the compiler treats this as an escaping ownership rewrite at the current call boundary\n"
                "Fix:\n"
                "- keep using '%s' directly without rebinding it\n"
                "- or change the parameter to 'own' if transfer is intended",
                borrowed_name,
                target_name,
                ctx->current_function_decl != NULL
                    && ctx->current_function_decl->data.func_decl.name != NULL
                        ? ctx->current_function_decl->data.func_decl.name
                        : "<anonymous>",
                borrowed_name,
                target_name,
                borrowed_name);
            return target_type;
        }
        semantic_error(ctx, expr,
            "Resource handle assignment is not allowed.\n"
            "Reason:\n"
            "- anchored handles (Slot/SecureSlot/DeviceSlot) cannot be copied or rebound with '='\n"
            "- movable handles must transfer ownership through a new binding\n"
            "Fix:\n"
            "- use Read/Write for Slot<T>\n"
            "- keep DeviceSlot local\n"
            "- move a QubitSlot into a new binding\n"
            "- or Claim... to create a fresh handle");
        return target_type;
    }

    if ((type_is_class_object_type(target_type, ctx)
         || type_is_class_object_type(value_type, ctx))
        && expr->data.assignment.value != NULL
        && expr->data.assignment.value->type == AST_IDENTIFIER
        && identifier_is_borrowed_boundary_param(expr->data.assignment.value, ctx)) {
        const char *borrowed_name =
            expr->data.assignment.value->data.identifier.name != NULL
                ? expr->data.assignment.value->data.identifier.name : "<subject>";
        const char *target_name =
            expr->data.assignment.target != NULL
            && expr->data.assignment.target->type == AST_IDENTIFIER
            && expr->data.assignment.target->data.identifier.name != NULL
                ? expr->data.assignment.target->data.identifier.name
                : "<target>";
        semantic_error(ctx, expr,
            "Borrowed ref subject '%s' cannot escape through assignment rebind into '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' subject\n"
            "- assigning it into '%s' would create a second boundary-visible binding for the same borrowed identity\n"
            "Fix:\n"
            "- keep mutating '%s' through its original binding\n"
            "- or change the parameter to 'own' if transfer is intended",
            borrowed_name,
            target_name,
            ctx->current_function_decl != NULL
                && ctx->current_function_decl->data.func_decl.name != NULL
                    ? ctx->current_function_decl->data.func_decl.name
                    : "<anonymous>",
            borrowed_name,
            target_name,
            borrowed_name);
        return target_type;
    }

    if (type_requires_boundary_borrow_tracking(target_type, ctx)
        && expr->data.assignment.value != NULL
        && expr->data.assignment.value->type == AST_IDENTIFIER
        && identifier_is_borrowed_boundary_param(expr->data.assignment.value, ctx)) {
        const char *borrowed_name =
            expr->data.assignment.value->data.identifier.name != NULL
                ? expr->data.assignment.value->data.identifier.name : "<value>";
        const char *target_name =
            expr->data.assignment.target != NULL
            && expr->data.assignment.target->type == AST_IDENTIFIER
            && expr->data.assignment.target->data.identifier.name != NULL
                ? expr->data.assignment.target->data.identifier.name
                : "<target>";
        semantic_error(ctx, expr,
            "Borrowed ref boundary value '%s' cannot escape through assignment rebind into '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' boundary value\n"
            "- assigning it into '%s' would create a second boundary-visible binding for the same borrowed provenance\n"
            "Fix:\n"
            "- keep using '%s' directly without rebinding it\n"
            "- or change the parameter to 'own' if transfer is intended",
            borrowed_name,
            target_name,
            ctx->current_function_decl != NULL
                && ctx->current_function_decl->data.func_decl.name != NULL
                    ? ctx->current_function_decl->data.func_decl.name
                    : "<anonymous>",
            borrowed_name,
            target_name,
            borrowed_name);
        return target_type;
    }

    if (type_is_class_object_type(target_type, ctx)
        || type_is_class_object_type(value_type, ctx)) {
        semantic_error(ctx, expr,
            "Subject assignment is not allowed; subjects are identity-bearing active hosts. Mutate fields or methods on the existing subject instead of rebinding it with '='");
        return target_type;
    }

    /* Reject field assignment on object/tobject — they are read-only
     * after construction.  Zone-internal projection sync (refresh/publish)
     * uses a separate code path and is not affected by this check. */
    if (expr->data.assignment.target != NULL
        && expr->data.assignment.target->type == AST_MEMBER_ACCESS) {
        ASTNode *obj_node = expr->data.assignment.target->data.member.object;
        if (obj_node != NULL && obj_node->type == AST_IDENTIFIER) {
            const char *var_name = obj_node->data.identifier.name;
            Symbol *sym = scope_lookup(ctx->scope, var_name);
            if (sym != NULL && sym->type != NULL
                && sym->type->kind == TYPE_KIND_CLASS
                && sym->type->name != NULL) {
                ASTNode *decl = find_type_decl_by_name(ctx->program_root,
                                                        sym->type->name);
                if (decl != NULL && decl->type == AST_CLASS_DECL) {
                    NominalDeclKind nk = decl->data.class_decl.nominal_kind;
                    if (nk == NOMINAL_DECL_OBJECT) {
                        semantic_error(ctx, expr,
                            "object '%s' fields are read-only after construction.\n"
                            "Reason:\n"
                            "- object is an internal projection contract\n"
                            "- projection state must be refreshed from its source, not mutated directly\n"
                            "Fix:\n"
                            "- update the source subject/value and refresh the object slot\n"
                            "- or construct a new object projection",
                            var_name);
                    } else if (nk == NOMINAL_DECL_TOBJECT) {
                        semantic_error(ctx, expr,
                            "tobject '%s' fields are immutable.\n"
                            "Reason:\n"
                            "- tobject is a boundary transfer contract\n"
                            "- transfer snapshots must be republished from their source, not mutated in place\n"
                            "Fix:\n"
                            "- update the source subject/value and publish a new tobject\n"
                            "- or construct a new transfer snapshot",
                            var_name);
                    }
                }
            }
        }
    }

    require_assignable(value_type, target_type, expr, ctx);
    return target_type;
}

/* -----------------------------------------------------------------
 * Statement checkers
 * ----------------------------------------------------------------- */

bool
type_check_let_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode    *init = node->data.let_decl.initializer;
    ASTNode    *ann  = node->data.let_decl.type;

    /* Check for duplicate in current scope */
    if (scope_lookup_current(ctx->scope, name) != NULL) {
        semantic_error(ctx, node,
            "Redeclaration of '%s' in the same scope", name);
        return false;
    }

    /*
     * Detect ClaimSlot / ClaimSecureSlot calls so we can register
     * a SYMBOL_SLOT with proper metadata.
     */
    bool is_slot_decl = false;
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name =
            init->data.call.callee->data.identifier.name;
        BuiltinKind bk = builtin_resolve(callee_name);

        if (bk == BUILTIN_CLAIM_SLOT || bk == BUILTIN_CLAIM_SECURE_SLOT) {
            is_slot_decl = true;
            bool is_secure = (bk == BUILTIN_CLAIM_SECURE_SLOT);
            if (is_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);

            /* Resolve inner type from annotation if present.
             * If the annotation is already a Slot type (e.g. Slot<String>),
             * use it directly instead of double-wrapping. */
            Type *slot_type = NULL;
            if (ann != NULL) {
                Type *ann_type = resolve_type_node(ann, ctx);
                if (ann_type->kind == TYPE_KIND_SLOT) {
                    slot_type = ann_type;
                    is_secure = ann_type->data.slot.is_secure;
                } else {
                    slot_type = type_create_slot(ann_type, is_secure);
                }
            } else {
                slot_type = type_create_slot(TYPE_INT, is_secure);
            }
            char token_name_buf[256];
            const char *paired_token = NULL;
            if (is_secure) {
                snprintf(token_name_buf, sizeof(token_name_buf), "%s_token", name);
                paired_token = token_name_buf;
            }
            Symbol *sym       = symbol_create_slot(name, slot_type,
                                                    is_secure, paired_token,
                                                    node->line, node->column);
            scope_declare(ctx->scope, sym);
            if (is_secure) {
                Symbol *tok = symbol_create_token(paired_token, name,
                                                  node->line, node->column);
                if (tok != NULL && slot_type != NULL && slot_type->kind == TYPE_KIND_SLOT) {
                    Type *token_args[1] = { slot_type->data.slot.inner_type };
                    tok->type = type_create_constructed(TYPE_TOKEN, token_args, 1);
                }
                if (!scope_declare(ctx->scope, tok))
                    symbol_destroy(tok);
            }
            scope_register_slot(ctx->scope, sym);
            return true;
        }
    }

    /* Normal variable declaration with type inference */
    Type *init_type = (init != NULL)
                      ? type_check_expression(init, ctx)
                      : TYPE_VOID;
    if (init != NULL)
        mark_world_embedded_zone_arguments(init, ctx);

    Type *decl_type;
    
    /* Type inference: if no annotation, infer from initializer */
    if (ann != NULL) {
        /* Explicit type annotation */
        decl_type = resolve_type_node(ann, ctx);
        if (ctx->program_root != NULL
            && ann->type == AST_TYPE
            && ann->data.type.name != NULL) {
            ASTNode *class_decl = find_type_decl_by_name(ctx->program_root,
                                                         ann->data.type.name);
            if (class_decl != NULL && class_decl->type == AST_CLASS_DECL) {
                validate_class_where_clause_specialization_ast(class_decl,
                                                               ann,
                                                               ann,
                                                               ctx);
            }
        }
        if (init != NULL) {
            if (init->type == AST_CALL
                && init->data.call.callee->type == AST_IDENTIFIER
                && strcmp(init->data.call.callee->data.identifier.name,
                          "BoxArray") == 0) {
                init_type = decl_type;
            } else if (init->type == AST_CALL
                       && init->data.call.callee != NULL
                       && init->data.call.callee->type == AST_IDENTIFIER
                       && ann->type == AST_TYPE
                       && ann->data.type.name != NULL
                       && strcmp(init->data.call.callee->data.identifier.name,
                                 ann->data.type.name) == 0
                       && decl_type != NULL
                       && decl_type->kind == TYPE_KIND_CONSTRUCTED) {
                init_type = decl_type;
            }
            if (!slot_transfer_compatible(init_type, decl_type))
                require_assignable(init_type, decl_type, init, ctx);
        }
    } else if (init != NULL) {
        /* Infer type from initializer */
        decl_type = init_type;

        if (init->type == AST_CALL
            && init->data.call.callee != NULL
            && init->data.call.callee->type == AST_IDENTIFIER
            && init_type == TYPE_UNKNOWN) {
            const char *callee_name = init->data.call.callee->data.identifier.name;
            if (callee_name != NULL
                && (strcmp(callee_name, "ListNew") == 0
                    || strcmp(callee_name, "SetNew") == 0
                    || strcmp(callee_name, "MapNew") == 0
                    || strcmp(callee_name, "QueueNew") == 0)) {
                semantic_error(ctx, init,
                    "Cannot infer collection type from '%s()' without an explicit annotation; write 'let value: %s<...> = %s()'",
                    callee_name,
                    strcmp(callee_name, "MapNew") == 0 ? "HashMap" :
                    (strcmp(callee_name, "QueueNew") == 0 ? "Queue" :
                    (strcmp(callee_name, "SetNew") == 0 ? "Set" : "List")),
                    callee_name);
                decl_type = TYPE_UNKNOWN;
            }
        }
        
        /* For generic types like Box<T>, Array<T>, Result<T,E>, 
           ensure the inferred type is concrete */
        if (init_type->kind == TYPE_KIND_GENERIC) {
            semantic_error(ctx, init, 
                "Cannot infer type: generic parameter '%s' is ambiguous. "
                "Please provide a type annotation.", init_type->name);
            decl_type = TYPE_UNKNOWN;
        }
    } else {
        /* No annotation and no initializer */
        semantic_error(ctx, node,
            "Cannot infer type: provide a type annotation or initializer");
        decl_type = TYPE_UNKNOWN;
    }

    if (type_is_class_object_type(decl_type, ctx)
        && init != NULL
        && type_is_class_object_type(init_type, ctx)
        && !expr_is_class_constructor_call(init, ctx)
        && (init->type == AST_IDENTIFIER || init->type == AST_MEMBER_ACCESS)) {
        if (init->type == AST_IDENTIFIER
            && identifier_is_borrowed_boundary_param(init, ctx)) {
            semantic_error(ctx, node,
                "Borrowed ref subject '%s' cannot escape into a new binding '%s'.\n"
                "Reason:\n"
                "- consumer path is function '%s'\n"
                "- '%s' entered this function as a borrowed 'ref' subject\n"
                "- binding it as '%s' would extend that borrow beyond its original boundary provenance\n"
                "Fix:\n"
                "- keep using '%s' directly within this function\n"
                "- or change the parameter to 'own' if transfer is intended",
                init->data.identifier.name != NULL ? init->data.identifier.name : "<value>",
                name != NULL ? name : "<binding>",
                ctx->current_function_decl != NULL
                    && ctx->current_function_decl->data.func_decl.name != NULL
                        ? ctx->current_function_decl->data.func_decl.name
                        : "<anonymous>",
                init->data.identifier.name != NULL ? init->data.identifier.name : "<value>",
                name != NULL ? name : "<binding>",
                init->data.identifier.name != NULL ? init->data.identifier.name : "<value>");
        }
        semantic_error(ctx, node,
            "Subjects cannot be copied into a new binding.\n"
            "Reason:\n"
            "- subject values carry identity and anchored state semantics\n"
            "- rebinding an existing subject value as a plain copy would duplicate that identity contract\n"
            "Fix:\n"
            "- construct a fresh subject value instead\n"
            "- or mutate the existing subject through its current binding");
    }

    if (type_is_qubit(decl_type)) {
        bool borrowed_movable_new_binding =
            init != NULL
            && init->type == AST_IDENTIFIER
            && identifier_is_borrowed_boundary_param(init, ctx);
        bool valid_qubit_init = false;
        if (borrowed_movable_new_binding) {
            semantic_error(ctx, node,
                "Borrowed ref movable resource '%s' cannot escape into a new binding '%s'.\n"
                "Reason:\n"
                "- consumer path is function '%s'\n"
                "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                "- binding it as '%s' would extend that borrow beyond its original boundary provenance\n"
                "Fix:\n"
                "- keep using '%s' directly within this function\n"
                "- or change the parameter to 'own' if transfer is intended",
                init->data.identifier.name != NULL ? init->data.identifier.name : "<value>",
                name != NULL ? name : "<binding>",
                ctx->current_function_decl != NULL
                    && ctx->current_function_decl->data.func_decl.name != NULL
                        ? ctx->current_function_decl->data.func_decl.name
                        : "<anonymous>",
                init->data.identifier.name != NULL ? init->data.identifier.name : "<value>",
                name != NULL ? name : "<binding>",
                init->data.identifier.name != NULL ? init->data.identifier.name : "<value>");
            valid_qubit_init = true;
        }
        if (init_type == TYPE_UNKNOWN) {
            valid_qubit_init = true;
        } else if (expr_is_qubit_claim(init)) {
            valid_qubit_init = true;
        } else if (init != NULL && init_type != NULL && type_is_qubit(init_type)) {
            if (init->type == AST_IDENTIFIER) {
                valid_qubit_init = true;
                consume_qubit_value(init, ctx, "moved");
            } else if (expr_is_movable_resource_transfer_source(init)) {
                valid_qubit_init = true;
            }
        }
        if (!valid_qubit_init) {
            semantic_error(ctx, node,
                "QubitSlot is a movable resource handle and cannot be plain-copied into a new binding.\n"
                "Reason:\n"
                "- movable resource values must come from a fresh claim, an explicit move, or a transfer boundary\n"
                "- current initializer does not preserve ownership provenance for QubitSlot\n"
                "Fix:\n"
                "- initialize from ClaimQubit()\n"
                "- or move an existing QubitSlot identifier\n"
                "- or use a transfer boundary such as recv/await");
        }
    }

    if (type_requires_boundary_borrow_tracking(decl_type, ctx)
        && init != NULL
        && init->type == AST_IDENTIFIER
        && identifier_is_borrowed_boundary_param(init, ctx)) {
        semantic_error(ctx, node,
            "Borrowed ref boundary value '%s' cannot escape into a new binding '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' boundary value\n"
            "- binding it as '%s' would extend that borrow beyond its original boundary provenance\n"
            "Fix:\n"
            "- keep using '%s' directly within this function\n"
            "- or change the parameter to 'own' if transfer is intended",
            init->data.identifier.name != NULL ? init->data.identifier.name : "<value>",
            name != NULL ? name : "<binding>",
            ctx->current_function_decl != NULL
                && ctx->current_function_decl->data.func_decl.name != NULL
                    ? ctx->current_function_decl->data.func_decl.name
                    : "<anonymous>",
            init->data.identifier.name != NULL ? init->data.identifier.name : "<value>",
            name != NULL ? name : "<binding>",
            init->data.identifier.name != NULL ? init->data.identifier.name : "<value>");
    }

    /* Slot<T> move semantics: when assigning from another Slot variable,
     * consume (invalidate) the source.  ClaimSlot() creates fresh. */
    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT
        && decl_type->data.slot.access_mode == SLOT_ACCESS_OWNED
        && init != NULL && init->type == AST_IDENTIFIER
        && init_type != NULL && init_type->kind == TYPE_KIND_SLOT
        && init_type->data.slot.access_mode == SLOT_ACCESS_OWNED) {
        /* Move: source Slot becomes invalid after this point */
        const char *src_name = init->data.identifier.name;
        Symbol *src_sym = scope_lookup(ctx->scope, src_name);
        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT) {
            if (src_sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error(ctx, init,
                    "Cannot move from released slot '%s'.\n"
                    "Reason:\n"
                    "- slot '%s' was already released or invalidated earlier in this scope\n"
                    "- ownership transfer requires a live source handle\n"
                    "Fix:\n"
                    "- reacquire the slot before moving it\n"
                    "- or remove the earlier release/invalidating transfer",
                    src_name,
                    src_name);
            } else {
                src_sym->slot_info.state = SLOT_STATE_RELEASED;
            }
        }
    }

    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT
        && decl_type->data.slot.access_mode != SLOT_ACCESS_OWNED) {
        const char *source_slot = NULL;
        if (init != NULL && init->type == AST_CALL
            && init->data.call.callee != NULL
            && init->data.call.callee->type == AST_IDENTIFIER
            && init->data.call.arg_count >= 1
            && init->data.call.arguments[0] != NULL
            && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
            const char *callee_name = init->data.call.callee->data.identifier.name;
            if (callee_name != NULL
                && (strcmp(callee_name, "ViewRead") == 0
                    || strcmp(callee_name, "ViewWrite") == 0
                    || strcmp(callee_name, "Move") == 0)) {
                source_slot = init->data.call.arguments[0]->data.identifier.name;
            }
        }

        Symbol *sym = symbol_create_view(name, decl_type, source_slot,
            node->line, node->column);
        scope_declare(ctx->scope, sym);
        return !ctx->has_error;
    }

    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT) {
        if (decl_type->data.slot.is_secure)
            semantic_record_effect(ctx, EFFECT_SECURE);
        if (slot_transfer_compatible(init_type, decl_type)) {
            if (init != NULL && init->type == AST_IDENTIFIER) {
                Symbol *move_sym = scope_lookup(ctx->scope, init->data.identifier.name);
                if (move_sym != NULL)
                    move_sym->is_consumed = true;
            }
        }
        if (init_type != NULL && type_is_anchored_resource_handle(init_type)) {
            semantic_error(ctx, node,
                "Anchored resource handles (Slot/SecureSlot/DeviceSlot) cannot be copied into a new binding.\n"
                "Reason:\n"
                "- anchored handles are tied to one local ownership/runtime anchor\n"
                "- rebinding them as plain values would duplicate that anchor contract\n"
                "Fix:\n"
                "- use a fresh claim\n"
                "- or initialize from an inner/plain value instead");
        }
        char token_name_buf[256];
        const char *paired_token = NULL;
        if (decl_type->data.slot.is_secure) {
            snprintf(token_name_buf, sizeof(token_name_buf), "%s_token", name);
            paired_token = token_name_buf;
        }
        Symbol *sym = symbol_create_slot(name, decl_type,
            decl_type->data.slot.is_secure, paired_token, node->line, node->column);
        scope_declare(ctx->scope, sym);
        if (decl_type->data.slot.is_secure) {
            Symbol *tok = symbol_create_token(paired_token, name,
                                              node->line, node->column);
            if (tok != NULL) {
                Type *token_args[1] = { decl_type->data.slot.inner_type };
                tok->type = type_create_constructed(TYPE_TOKEN, token_args, 1);
            }
            if (!scope_declare(ctx->scope, tok))
                symbol_destroy(tok);
        }
        scope_register_slot(ctx->scope, sym);
        return !ctx->has_error;
    }

    if (type_is_anchored_resource_handle(decl_type)) {
        bool is_fresh_claim = expr_is_device_slot_claim(init);
        if (!is_fresh_claim && init_type != NULL && type_is_anchored_resource_handle(init_type)) {
            semantic_error(ctx, node,
                "Anchored resource handles (Slot/SecureSlot/DeviceSlot) cannot be copied into a new binding.\n"
                "Reason:\n"
                "- anchored handles remain tied to one owning runtime anchor\n"
                "- copying them into another binding would duplicate that anchor identity\n"
                "Fix:\n"
                "- keep the handle in one binding\n"
                "- or reacquire it from the owning system");
        }
        semantic_record_effect(ctx, EFFECT_REMOTE);
    }

    Symbol *sym = symbol_create_variable(name, decl_type,
                                          node->line, node->column);

    if (type_is_constructed_named(decl_type, "DeviceSlot"))
        sym->slot_info.state = SLOT_STATE_CLAIMED;

    /* Set qubit semantic state for QubitSlot variables */
    if (type_is_qubit(decl_type)) {
        if (expr_is_qubit_claim(init)) {
            sym->qubit_info.semantic_state = QUBIT_STATE_SUPERPOSITION;
        } else if (init != NULL && init->type == AST_IDENTIFIER) {
            /* Move: copy source qubit's semantic state */
            Symbol *src = lookup_identifier_symbol(init, ctx);
            if (src != NULL)
                sym->qubit_info.semantic_state = src->qubit_info.semantic_state;
            else
                sym->qubit_info.semantic_state = QUBIT_STATE_NONE;
        } else if (init != NULL && init->type == AST_CALL) {
            /* Function returning QubitSlot */
            sym->qubit_info.semantic_state = QUBIT_STATE_SUPERPOSITION;
        } else if (init != NULL
                   && (init->type == AST_CHANNEL_RECV
                       || init->type == AST_AWAIT_EXPR)) {
            /* Transfer from orchestration boundary; precise state is unknown. */
            sym->qubit_info.semantic_state = QUBIT_STATE_NONE;
        }
    }

    if (!is_slot_decl)
        scope_declare(ctx->scope, sym);

    return !ctx->has_error;
}

bool
type_check_return_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *ret_type = TYPE_VOID;
    if (node->data.return_stmt.value != NULL)
        ret_type = type_check_expression(node->data.return_stmt.value, ctx);

    if (ctx->current_return != NULL)
        require_assignable(ret_type, ctx->current_return, node, ctx);

    if (type_is_anchored_resource_handle(ret_type)) {
        semantic_error(ctx, node,
            "Returning anchored resource handles (Slot/SecureSlot/DeviceSlot) is not supported yet.\n"
            "Reason:\n"
            "- return boundary would let an anchored handle escape its local ownership/runtime anchor\n"
            "- anchored handles are still local-only at return boundaries in the closed subset\n"
            "Fix:\n"
            "- return the inner value instead\n"
            "- or return a future/result token or other boundary-safe projection");
    }

    if (node->data.return_stmt.value != NULL
        && identifier_is_borrowed_boundary_param(node->data.return_stmt.value, ctx)
        && type_is_subject_type(ret_type, ctx)) {
        semantic_error(ctx, node,
            "Borrowed ref subject '%s' cannot escape via return.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' subject\n"
            "- returning it would let the borrow outlive the current call boundary\n"
            "Fix:\n"
            "- return a projection/object/tobject/value result instead\n"
            "- or change the parameter to 'own' if transfer is intended",
            node->data.return_stmt.value->data.identifier.name,
            ctx->current_function_decl != NULL
                && ctx->current_function_decl->data.func_decl.name != NULL
                    ? ctx->current_function_decl->data.func_decl.name
                    : "<anonymous>",
            node->data.return_stmt.value->data.identifier.name);
    }

    if (node->data.return_stmt.value != NULL
        && identifier_is_borrowed_boundary_param(node->data.return_stmt.value, ctx)
        && type_is_movable_resource_handle(ret_type)) {
        semantic_error(ctx, node,
            "Borrowed ref movable resource '%s' cannot escape via return.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' movable resource\n"
            "- returning it would let the borrow outlive the current call boundary\n"
            "Fix:\n"
            "- return a copied/projection/value result instead\n"
            "- or change the parameter to 'own' if transfer is intended",
            node->data.return_stmt.value->data.identifier.name,
            ctx->current_function_decl != NULL
                && ctx->current_function_decl->data.func_decl.name != NULL
                    ? ctx->current_function_decl->data.func_decl.name
                    : "<anonymous>",
            node->data.return_stmt.value->data.identifier.name);
    }

    if (node->data.return_stmt.value != NULL
        && identifier_is_borrowed_boundary_param(node->data.return_stmt.value, ctx)
        && type_requires_boundary_borrow_tracking(ret_type, ctx)) {
        semantic_error(ctx, node,
            "Borrowed ref boundary value '%s' cannot escape via return.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' boundary value\n"
            "- returning it would let the borrow outlive the current call boundary\n"
            "Fix:\n"
            "- return a copied/value/projection result instead\n"
            "- or change the parameter to 'own' if transfer is intended",
            node->data.return_stmt.value->data.identifier.name,
            ctx->current_function_decl != NULL
                && ctx->current_function_decl->data.func_decl.name != NULL
                    ? ctx->current_function_decl->data.func_decl.name
                    : "<anonymous>",
            node->data.return_stmt.value->data.identifier.name);
    }

    if (node->data.return_stmt.value != NULL
        && type_is_qubit(ret_type)
        && node->data.return_stmt.value->type == AST_IDENTIFIER) {
        consume_qubit_value(node->data.return_stmt.value, ctx, "returned");
    }

    return !ctx->has_error;
}

bool
type_check_parallel_block(ASTNode *node, SemanticContext *ctx)
{
    bool prev_parallel = ctx->in_parallel;
    ctx->in_parallel   = true;

    for (size_t i = 0; i < node->data.parallel.task_count; i++) {
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        type_check_statement(node->data.parallel.tasks[i], ctx);
        scope_exit(&ctx->scope);
    }

    ctx->in_parallel = prev_parallel;
    return !ctx->has_error;
}

static bool
semantic_is_known_stdlib_use_module(const char *module_name)
{
    static const char *const modules[] = {
        "datetime",
        "device_adapter",
        "http",
        "ledger",
        "money",
        "obligation",
        "page",
        "spray",
        "storage",
        "timer",
        "versioning"
    };

    if (module_name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        if (strcmp(module_name, modules[i]) == 0)
            return true;
    }

    return false;
}

static bool
callable_contract_is_externally_visible(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *host = current_host_decl(ctx);

    if (node == NULL || ctx == NULL || node->type != AST_FUNC_DECL)
        return false;
    if (node->is_exported)
        return true;
    if (host == NULL || !host->is_exported)
        return false;
    if (!node->data.func_decl.has_explicit_access)
        return true;
    return node->data.func_decl.access == ACCESS_PUBLIC
        || node->data.func_decl.access == ACCESS_PROTECTED;
}

bool
type_check_ability_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.ability_decl.name;
    bool has_generics = (node->data.ability_decl.generic_params != NULL
                         && node->data.ability_decl.generic_params->count > 0);

    /* Register ability as a symbol so roles can reference it */
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ABILITY;
    sym->type = TYPE_VOID; /* Abilities don't have a concrete type */
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of ability '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    if (has_generics) {
        validate_generic_param_defaults(node->data.ability_decl.generic_params,
            ctx, node, "ability");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.ability_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    validate_where_clause_bounds(node->data.ability_decl.where_clause, ctx, node);
    validate_generic_param_default_bounds(
        node->data.ability_decl.generic_params,
        node->data.ability_decl.where_clause,
        ctx,
        node,
        "ability",
        name);
    validate_ability_require_fields(node, ctx);

    /* Check method signatures */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
        ASTNode *method = node->data.ability_decl.methods[i];
        /* Only type-check methods that have a body */
        if (method->data.func_decl.body != NULL) {
            type_check_func_decl(method, ctx);
        } else {
            /* Abstract method — just validate the signature types */
            if (method->data.func_decl.return_type != NULL)
                resolve_type_node(method->data.func_decl.return_type, ctx);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                if (method->data.func_decl.params[j]->type != NULL)
                    resolve_type_node(method->data.func_decl.params[j]->type, ctx);
            }
        }
    }
    scope_exit(&ctx->scope);
    if (has_generics)
        scope_exit(&ctx->scope);

    return !ctx->has_error;
}

#include "type_checker_decls.inc"

Type *
type_check_spawn_expr(ASTNode *expr, SemanticContext *ctx)
{
    semantic_record_effect(ctx, EFFECT_REMOTE);
    /* Type-check the spawned function/expression */
    Type *inner = type_check_expression(expr->data.spawn_expr.function, ctx);
    Type *args[1] = { inner != NULL ? inner : TYPE_UNKNOWN };
    return type_create_constructed(TYPE_FUTURE, args, 1);
}

Type *
type_check_channel_send(ASTNode *expr, SemanticContext *ctx)
{
    semantic_record_effect(ctx, EFFECT_REMOTE);
    /* Check channel and value types */
    Type *channel_type = type_check_expression(expr->data.channel_send.channel, ctx);
    Type *value_type = type_check_expression(expr->data.channel_send.value, ctx);
    if (channel_type == NULL
        || channel_type->kind != TYPE_KIND_CONSTRUCTED
        || !type_equals(channel_type->data.constructed.constructor, TYPE_CHANNEL)
        || channel_type->data.constructed.arg_count != 1) {
        semantic_error(ctx, expr->data.channel_send.channel,
            "Channel send requires Channel<T>, got '%s'",
            channel_type != NULL ? channel_type->name : "<null>");
        return TYPE_VOID;
    }

    Type *element_type = channel_type->data.constructed.args[0];

    if (type_is_anchored_resource_handle(element_type)
        || type_is_anchored_resource_handle(value_type)) {
        semantic_error(ctx, expr->data.channel_send.value,
            "Channels cannot transport anchored resource handles (Slot/SecureSlot/DeviceSlot) yet.\n"
            "Reason:\n"
            "- channel send would move an anchored handle outside its local ownership/runtime anchor\n"
            "- anchored handles are still local-only at channel boundaries in the closed subset\n"
            "Fix:\n"
            "- send the inner value instead\n"
            "- or keep the anchored handle local");
        return TYPE_VOID;
    }

    if (type_is_capability_bearing(element_type)
        || type_is_capability_bearing(value_type)) {
        semantic_error(ctx, expr->data.channel_send.value,
            "Channels cannot transport capability-bearing values (SecureSlot/Token) yet.\n"
            "Reason:\n"
            "- capability-bearing values would cross the channel boundary without a closed authority contract\n"
            "- authorized flow must keep capability-bearing state local for now\n"
            "Fix:\n"
            "- keep capability-bearing state local to the authorized flow\n"
            "- or send a plain projection/value instead");
        return TYPE_VOID;
    }

    if (type_is_subject_type(element_type, ctx)
        || type_is_subject_type(value_type, ctx)) {
        if (!type_is_subject_type(element_type, ctx)
            || !type_is_subject_type(value_type, ctx)) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Channel send subject mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the subject boundary contract\n"
                "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type));
            return TYPE_VOID;
        }
        if (expr->data.channel_send.value->type != AST_IDENTIFIER) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Subject channel sends must transfer from a named variable.\n"
                "Reason:\n"
                "- ownership transfer at a channel boundary must point to one concrete source binding\n"
                "- unnamed expressions make moved-here provenance ambiguous\n"
                "Fix:\n"
                "- bind the subject first in a local variable\n"
                "- then send that named variable");
            return TYPE_VOID;
        }
        if (identifier_is_borrowed_boundary_param(expr->data.channel_send.value, ctx)) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Borrowed ref subject '%s' cannot escape through channel send.\n"
                "Reason:\n"
                "- consumer path is function '%s'\n"
                "- '%s' entered this function as a borrowed 'ref' subject\n"
                "- channel send would transfer that borrow beyond the current call boundary\n"
                "Fix:\n"
                "- send a projection/object/tobject/value snapshot instead\n"
                "- or change the parameter to 'own' before transfer",
                expr->data.channel_send.value->data.identifier.name,
                ctx->current_function_decl != NULL
                    && ctx->current_function_decl->data.func_decl.name != NULL
                        ? ctx->current_function_decl->data.func_decl.name
                        : "<anonymous>",
                expr->data.channel_send.value->data.identifier.name);
            return TYPE_VOID;
        }
        consume_qubit_value(expr->data.channel_send.value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    if (type_is_movable_resource_handle(element_type)
        || type_is_movable_resource_handle(value_type)) {
        if (!type_is_movable_resource_handle(element_type)
            || !type_is_movable_resource_handle(value_type)) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Channel send movable-resource mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the movable resource contract\n"
                "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type),
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type),
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type));
            return TYPE_VOID;
        }
        if (expr->data.channel_send.value->type != AST_IDENTIFIER) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Movable resource channel sends must transfer from a named variable.\n"
                "Reason:\n"
                "- ownership transfer at a channel boundary must point to one concrete source binding\n"
                "- unnamed expressions make moved-here provenance ambiguous\n"
                "Fix:\n"
                "- bind the value first by storing the movable resource in a local variable\n"
                "- then send that named variable");
            return TYPE_VOID;
        }
        if (identifier_is_borrowed_boundary_param(expr->data.channel_send.value, ctx)) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Borrowed ref movable resource '%s' cannot escape through channel send.\n"
                "Reason:\n"
                "- consumer path is function '%s'\n"
                "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                "- channel send would transfer that borrow beyond the current call boundary\n"
                "Fix:\n"
                "- send a copied/value/projection form instead\n"
                "- or change the parameter to 'own' before transfer",
                expr->data.channel_send.value->data.identifier.name,
                ctx->current_function_decl != NULL
                    && ctx->current_function_decl->data.func_decl.name != NULL
                        ? ctx->current_function_decl->data.func_decl.name
                        : "<anonymous>",
                expr->data.channel_send.value->data.identifier.name);
            return TYPE_VOID;
        }
        consume_qubit_value(expr->data.channel_send.value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    if (type_requires_boundary_borrow_tracking(element_type, ctx)
        || type_requires_boundary_borrow_tracking(value_type, ctx)) {
        if (!type_requires_boundary_borrow_tracking(element_type, ctx)
            || !type_requires_boundary_borrow_tracking(value_type, ctx)
            || type_is_subject_type(element_type, ctx)
            || type_is_subject_type(value_type, ctx)
            || type_is_movable_resource_handle(element_type)
            || type_is_movable_resource_handle(value_type)) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Channel send boundary value mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the same boundary value contract\n"
                "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type));
            return TYPE_VOID;
        }
        if (expr->data.channel_send.value->type != AST_IDENTIFIER) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Boundary value channel sends must transfer from a named variable.\n"
                "Reason:\n"
                "- ownership transfer at a channel boundary must point to one concrete source binding\n"
                "- unnamed expressions make moved-here provenance ambiguous\n"
                "Fix:\n"
                "- bind the value first in a local variable\n"
                "- then send that named variable");
            return TYPE_VOID;
        }
        if (identifier_is_borrowed_boundary_param(expr->data.channel_send.value, ctx)) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Borrowed ref boundary value '%s' cannot escape through channel send.\n"
                "Reason:\n"
                "- consumer path is function '%s'\n"
                "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                "- channel send would transfer that borrow beyond the current call boundary\n"
                "Fix:\n"
                "- send a copied/value/projection snapshot instead\n"
                "- or change the parameter to 'own' before transfer",
                expr->data.channel_send.value->data.identifier.name,
                ctx->current_function_decl != NULL
                    && ctx->current_function_decl->data.func_decl.name != NULL
                        ? ctx->current_function_decl->data.func_decl.name
                        : "<anonymous>",
                expr->data.channel_send.value->data.identifier.name);
            return TYPE_VOID;
        }
        consume_qubit_value(expr->data.channel_send.value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    require_assignable(value_type, element_type, expr->data.channel_send.value, ctx);
    return TYPE_VOID;
}

Type *
type_check_channel_recv(ASTNode *expr, SemanticContext *ctx)
{
    semantic_record_effect(ctx, EFFECT_REMOTE);
    Type *channel_type = type_check_expression(expr->data.channel_recv.channel, ctx);
    if (channel_type == NULL
        || channel_type->kind != TYPE_KIND_CONSTRUCTED
        || !type_equals(channel_type->data.constructed.constructor, TYPE_CHANNEL)
        || channel_type->data.constructed.arg_count != 1) {
        semantic_error(ctx, expr->data.channel_recv.channel,
            "Channel recv requires Channel<T>, got '%s'",
            channel_type != NULL ? channel_type->name : "<null>");
        return TYPE_UNKNOWN;
    }

    if (type_is_anchored_resource_handle(channel_type->data.constructed.args[0])) {
        semantic_error(ctx, expr->data.channel_recv.channel,
            "Channels cannot yield anchored resource handles (Slot/SecureSlot/DeviceSlot) yet.\n"
            "Reason:\n"
            "- receive would materialize an anchored handle beyond its local ownership/runtime anchor\n"
            "- anchored handles are still local-only at channel boundaries in the closed subset\n"
            "Fix:\n"
            "- receive a plain value instead\n"
            "- or keep the anchored handle local");
        return TYPE_UNKNOWN;
    }

    if (type_is_capability_bearing(channel_type->data.constructed.args[0])) {
        semantic_error(ctx, expr->data.channel_recv.channel,
            "Channels cannot yield capability-bearing values (SecureSlot/Token) yet.\n"
            "Reason:\n"
            "- receive would materialize capability-bearing state outside the closed authority flow\n"
            "- capability-bearing values are still local-only at channel boundaries in the closed subset\n"
            "Fix:\n"
            "- receive a plain value instead\n"
            "- or keep capability-bearing state local");
        return TYPE_UNKNOWN;
    }

    return channel_type->data.constructed.args[0];
}

bool
type_check_event_decl(ASTNode *node, SemanticContext *ctx)
{
    bool ok = true;

    if (node == NULL || ctx == NULL || node->type != AST_EVENT_DECL)
        return false;

    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        if (param == NULL)
            continue;

        if (param->type != AST_LET_DECL) {
            semantic_error(ctx, param,
                "Event '%s' parameter %zu must be a typed binding",
                node->data.event_decl.name != NULL
                    ? node->data.event_decl.name : "<event>",
                i + 1);
            ok = false;
            continue;
        }

        if (param->data.let_decl.type == NULL) {
            semantic_error(ctx, param,
                "Event '%s' parameter '%s' requires an explicit type",
                node->data.event_decl.name != NULL
                    ? node->data.event_decl.name : "<event>",
                param->data.let_decl.name != NULL
                    ? param->data.let_decl.name : "<param>");
            ok = false;
            continue;
        }

        if (resolve_type_node(param->data.let_decl.type, ctx) == NULL)
            ok = false;
    }

    if (node->data.event_decl.return_type != NULL) {
        Type *return_type = resolve_type_node(node->data.event_decl.return_type, ctx);
        if (return_type != NULL && !type_equals(return_type, TYPE_VOID)) {
            semantic_error(ctx, node->data.event_decl.return_type,
                "Event '%s' must return Void, got '%s'",
                node->data.event_decl.name != NULL
                    ? node->data.event_decl.name : "<event>",
                return_type->name != NULL ? return_type->name : "<type>");
            ok = false;
        }
    }

    return ok && !ctx->has_error;
}

static const char *
semantic_event_expr_name(ASTNode *expr)
{
    if (expr == NULL)
        return "<event>";
    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL)
        return expr->data.identifier.name;
    if (expr->type == AST_MEMBER_ACCESS && expr->data.member.name != NULL)
        return expr->data.member.name;
    return "<event>";
}

static Type *
semantic_event_handler_signature(ASTNode *handler, SemanticContext *ctx)
{
    if (handler == NULL || ctx == NULL)
        return NULL;

    if (handler->type == AST_LAMBDA_EXPR) {
        size_t param_count = handler->data.lambda_expr.param_count;
        Type **param_types = calloc(param_count > 0 ? param_count : 1, sizeof(Type *));
        Type *return_type = TYPE_VOID;
        Type *lambda_type;

        if (param_types == NULL)
            return TYPE_UNKNOWN;

        for (size_t i = 0; i < param_count; i++) {
            ASTNode *param = handler->data.lambda_expr.params[i];
            if (param != NULL
                && param->type == AST_LET_DECL
                && param->data.let_decl.type != NULL) {
                param_types[i] = resolve_type_node(param->data.let_decl.type, ctx);
            } else {
                param_types[i] = TYPE_UNKNOWN;
            }
        }

        if (handler->data.lambda_expr.return_type != NULL) {
            Type *resolved = resolve_type_node(handler->data.lambda_expr.return_type, ctx);
            if (resolved != NULL)
                return_type = resolved;
        }

        lambda_type = type_create_function(param_types, param_count, return_type);
        free(param_types);
        return lambda_type != NULL ? lambda_type : TYPE_UNKNOWN;
    }

    return type_check_expression(handler, ctx);
}

static bool
type_check_event_subscription(ASTNode *node, SemanticContext *ctx,
                              const char *op_name)
{
    Type *event_type;
    Type *handler_type;
    const char *event_name;
    bool ok = true;

    if (node == NULL || ctx == NULL)
        return false;

    event_type = type_check_expression(node->data.event_op.event, ctx);
    handler_type = semantic_event_handler_signature(node->data.event_op.handler, ctx);
    event_name = semantic_event_expr_name(node->data.event_op.event);

    if (event_type == NULL || event_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error(ctx, node->data.event_op.event,
            "Event %s target '%s' must be an event-compatible callable",
            op_name != NULL ? op_name : "operation",
            event_name);
        return false;
    }

    if (event_type->data.function.return_type != NULL
        && !type_equals(event_type->data.function.return_type, TYPE_VOID)) {
        semantic_error(ctx, node->data.event_op.event,
            "Event '%s' must return Void to support %s",
            event_name, op_name != NULL ? op_name : "subscription");
        ok = false;
    }

    if (handler_type == NULL || handler_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error(ctx, node->data.event_op.handler,
            "Event %s handler for '%s' must be a function or typed lambda",
            op_name != NULL ? op_name : "operation",
            event_name);
        return false;
    }

    if (handler_type->data.function.return_type != NULL
        && !type_equals(handler_type->data.function.return_type, TYPE_VOID)) {
        semantic_error(ctx, node->data.event_op.handler,
            "Event %s handler for '%s' must return Void, got '%s'",
            op_name != NULL ? op_name : "operation",
            event_name,
            handler_type->data.function.return_type->name != NULL
                ? handler_type->data.function.return_type->name : "<type>");
        ok = false;
    }

    if (event_type->data.function.param_count != handler_type->data.function.param_count) {
        semantic_error(ctx, node->data.event_op.handler,
            "Event %s handler for '%s' has parameter count mismatch: expected %zu, got %zu",
            op_name != NULL ? op_name : "operation",
            event_name,
            event_type->data.function.param_count,
            handler_type->data.function.param_count);
        return false;
    }

    for (size_t i = 0; i < event_type->data.function.param_count; i++) {
        Type *expected = event_type->data.function.param_types[i];
        Type *actual = handler_type->data.function.param_types[i];

        if (expected == NULL || actual == NULL
            || expected == TYPE_UNKNOWN || actual == TYPE_UNKNOWN) {
            continue;
        }

        if (!type_equals(expected, actual)) {
            semantic_error(ctx, node->data.event_op.handler,
                "Event %s handler for '%s' parameter %zu mismatch: expected '%s', got '%s'",
                op_name != NULL ? op_name : "operation",
                event_name,
                i + 1,
                expected->name != NULL ? expected->name : "<type>",
                actual->name != NULL ? actual->name : "<type>");
            ok = false;
        }
    }

    return ok && !ctx->has_error;
}

static bool
type_check_event_invoke_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *event_type;
    const char *event_name;
    bool ok = true;

    if (node == NULL || ctx == NULL || node->type != AST_EVENT_INVOKE)
        return false;

    event_type = type_check_expression(node->data.event_invoke.event, ctx);
    event_name = semantic_event_expr_name(node->data.event_invoke.event);

    if (event_type == NULL || event_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error(ctx, node->data.event_invoke.event,
            "Event invoke target '%s' must be an event-compatible callable",
            event_name);
        return false;
    }

    if (event_type->data.function.param_count != node->data.event_invoke.arg_count) {
        semantic_error(ctx, node,
            "Event '%s' invoke argument count mismatch: expected %zu, got %zu",
            event_name,
            event_type->data.function.param_count,
            node->data.event_invoke.arg_count);
        return false;
    }

    for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
        Type *expected = event_type->data.function.param_types[i];
        Type *actual = type_check_expression(node->data.event_invoke.arguments[i], ctx);

        if (expected == NULL || actual == NULL
            || expected == TYPE_UNKNOWN || actual == TYPE_UNKNOWN) {
            continue;
        }

        if (!type_is_assignable(actual, expected)) {
            semantic_error(ctx, node->data.event_invoke.arguments[i],
                "Event '%s' invoke argument %zu mismatch: expected '%s', got '%s'",
                event_name,
                i + 1,
                expected->name != NULL ? expected->name : "<type>",
                actual->name != NULL ? actual->name : "<type>");
            ok = false;
        }
    }

    return ok && !ctx->has_error;
}

bool
type_check_statement(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return true;

    switch (node->type) {
    case AST_LET_DECL:
        return type_check_let_decl(node, ctx);
    case AST_LET_DESTRUCTURE:
    {
        /* let (a, b, c) = expr; — destructure struct/array fields by position */
        ASTNode *init = node->data.let_destructure.initializer;
        Type *init_type = init != NULL ? type_check_expression(init, ctx) : TYPE_UNKNOWN;
        for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
            Type *elem_type = TYPE_UNKNOWN;
            /* Array destructuring: element type from Array<T> */
            if (type_is_constructed_named(init_type, "Array")
                || type_is_constructed_named(init_type, "Slice"))
                elem_type = type_get_constructed_arg(init_type, 0);
            /* CLASS/struct: field type by position (future — for now use UNKNOWN) */
            Symbol *s = symbol_create_variable(
                node->data.let_destructure.names[i], elem_type,
                node->line, node->column);
            scope_declare(ctx->scope, s);
        }
        return true;
    }
    case AST_FUNC_DECL:
        return type_check_func_decl(node, ctx);
    case AST_EVENT_DECL:
        return type_check_event_decl(node, ctx);
    case AST_TYPE_ALIAS:
        if (node->data.type_alias.target_type != NULL)
            (void)resolve_type_node(node->data.type_alias.target_type, ctx);
        return !ctx->has_error;
    case AST_CLASS_DECL:
        return type_check_class_decl(node, ctx);
    case AST_EXTERN_BLOCK:
        return type_check_extern_block(node, ctx);
    case AST_IF_STMT:
        return type_check_if_stmt(node, ctx);
    case AST_FOR_LOOP:
        return type_check_for_loop(node, ctx);
    case AST_WHILE_LOOP:
        return type_check_while_loop(node, ctx);
    case AST_MATCH_STMT:
        return type_check_match_stmt(node, ctx);
    case AST_RETURN:
        return type_check_return_stmt(node, ctx);
    case AST_BREAK:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'break' used outside of loop");
            return false;
        }
        if (node->data.break_stmt.label != NULL
            && semantic_find_labeled_loop_depth(ctx,
                node->data.break_stmt.label) < 0) {
            semantic_error(ctx, node,
                "Unknown loop label '%s' in break",
                node->data.break_stmt.label);
            return false;
        }
        return true;
    case AST_CONTINUE:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'continue' used outside of loop");
            return false;
        }
        if (node->data.continue_stmt.label != NULL
            && semantic_find_labeled_loop_depth(ctx,
                node->data.continue_stmt.label) < 0) {
            semantic_error(ctx, node,
                "Unknown loop label '%s' in continue",
                node->data.continue_stmt.label);
            return false;
        }
        return true;
    case AST_ENUM_DECL:
        {
            const char *name = node->data.enum_decl.name;
            ASTNode *saved_nominal = ctx->current_nominal_decl;

            scope_enter(&ctx->scope, SCOPE_CLASS);
            ctx->current_nominal_decl = node;

            for (size_t i = 0; i < node->data.enum_decl.method_count; i++)
                type_check_func_decl(node->data.enum_decl.methods[i], ctx);

            for (size_t i = 0; i < node->data.enum_decl.method_count; i++) {
                ASTNode *method = node->data.enum_decl.methods[i];
                if (method == NULL || method->type != AST_FUNC_DECL
                    || method->data.func_decl.name == NULL || name == NULL)
                    continue;
                Symbol *msym = scope_lookup_current(ctx->scope, method->data.func_decl.name);
                if (msym == NULL || msym->kind != SYMBOL_FUNCTION)
                    continue;
                size_t len = strlen(name) + 1 + strlen(method->data.func_decl.name) + 1;
                char *mangled = malloc(len);
                if (mangled == NULL)
                    continue;
                snprintf(mangled, len, "%s_%s", name, method->data.func_decl.name);
                Symbol *mangled_sym = symbol_create_function(
                    mangled, msym->type, method->line, method->column);
                Scope *enum_scope = ctx->scope;
                ctx->scope = enum_scope->parent;
                if (!scope_declare(ctx->scope, mangled_sym))
                    symbol_destroy(mangled_sym);
                ctx->scope = enum_scope;
                free(mangled);
            }

            scope_exit(&ctx->scope);
            ctx->current_nominal_decl = saved_nominal;
            return !ctx->has_error;
        }
    case AST_WITH_STMT:
        return type_check_with_stmt(node, ctx);
    case AST_PARALLEL_BLOCK:
        return type_check_parallel_block(node, ctx);
    case AST_ABILITY_DECL:
        return type_check_ability_decl(node, ctx);
    case AST_ROLE_DECL:
        return type_check_role_decl(node, ctx);
    case AST_PARTY_DECL:
        return type_check_party_decl(node, ctx);
    case AST_ROSTER_DECL:
        return type_check_roster_decl(node, ctx);
    case AST_WORLD_DECL:
        return type_check_world_decl(node, ctx);
    case AST_INTENT_DECL:
        return type_check_intent_decl(node, ctx);
    case AST_RELATION_DECL:
        return type_check_relation_decl(node, ctx);
    case AST_EFFECT_DECL:
        return type_check_effect_decl(node, ctx);
    case AST_ZONE_DECL:
        return type_check_zone_decl(node, ctx);
    case AST_ASYNC_BLOCK:
        return type_check_async_block(node, ctx);
    case AST_SELECT_STMT:
        return type_check_select_stmt(node, ctx);
    case AST_EVENT_SUBSCRIBE:
        return type_check_event_subscription(node, ctx, "subscription");
    case AST_EVENT_UNSUBSCRIBE:
        return type_check_event_subscription(node, ctx, "unsubscription");
    case AST_EVENT_INVOKE:
        return type_check_event_invoke_stmt(node, ctx);
    case AST_BLOCK:
        return type_check_block(node, ctx);
    case AST_IMPORT_DECL:
        /* Already resolved by driver — skip */
        return true;
    case AST_USE_DECL:
        validate_stdlib_use_decl(node, ctx);
        return !ctx->has_error;
    case AST_UNSAFE_BLOCK:
        /* Type-check body normally; safety constraints relaxed at codegen */
        if (node->data.unsafe_block.body != NULL)
            type_check_block(node->data.unsafe_block.body, ctx);
        return !ctx->has_error;
    case AST_DEFER_STMT:
        /* Type-check deferred body — actual slot state save/restore
         * is handled in type_check_statement_flow (type_checker_flow.c). */
        if (node->data.defer_stmt.body != NULL)
            type_check_block(node->data.defer_stmt.body, ctx);
        return !ctx->has_error;
    case AST_BIND_STMT:
        /* bind party.slot = Role; — validated at codegen level */
        return true;
    default:
        /* Expression statement */
        type_check_expression(node, ctx);
        return !ctx->has_error;
    }
}

bool
type_check_func_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.func_decl.name;
    bool is_action = (!node->is_async_decl && node->data.func_decl.is_action);
    ASTNode *enclosing_nominal = ctx->current_nominal_decl;
    ASTNode *prev_function_decl = ctx->current_function_decl;
    uint32_t prev_effects = ctx->current_function_effects;
    bool prev_tracking = ctx->tracking_function_effects;
    bool prev_async = ctx->in_async_func;
    const char *prev_module_path = ctx->current_module_path;
    bool has_effect_contract = false;
    uint32_t declared_effects =
        declared_effects_from_function_node(node, ctx, &has_effect_contract);

    if (is_action) {
        const char *subject_name = NULL;

        if (enclosing_nominal != NULL
            && enclosing_nominal->type == AST_CLASS_DECL
            && enclosing_nominal->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT) {
            subject_name = enclosing_nominal->data.class_decl.name;
        }
        validate_action_required_abilities(node, enclosing_nominal, ctx);

        /* Derive 'within' from the surrounding lexical zone */
        if (node->data.func_decl.within_zone == NULL
            && ctx->current_zone != NULL
            && ctx->current_zone->type == AST_ZONE_DECL) {
            node->data.func_decl.within_zone =
                pergyra_strdup(ctx->current_zone->data.zone_decl.name);
        }

        if (node->data.func_decl.within_zone != NULL
            && find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                node->data.func_decl.within_zone) == NULL) {
            semantic_error(ctx, node,
                "action '%s' references unknown zone '%s'",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.within_zone);
        }
        if (node->data.func_decl.within_zone != NULL) {
            ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                node->data.func_decl.within_zone);
            if (zone_decl != NULL
                && !explicit_type_reference_allowed(zone_decl, node, ctx)) {
                semantic_error(ctx, node,
                    "action '%s' cannot reference non-exported zone '%s' from another module",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone);
            }
            if (zone_decl != NULL
                && !zone_decl->is_exported
                && callable_contract_is_externally_visible(node, ctx)) {
                semantic_error(ctx, node,
                    "action '%s' cannot reference non-exported zone '%s' in an externally visible contract",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone);
            }
        }

        if (node->data.func_decl.causes_effect != NULL
            && find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect) == NULL) {
            semantic_error(ctx, node,
                "action '%s' references unknown effect '%s'",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.causes_effect);
        }
        if (node->data.func_decl.causes_effect != NULL) {
            ASTNode *effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect);
            if (effect_decl != NULL
                && !explicit_type_reference_allowed(effect_decl, node, ctx)) {
                semantic_error(ctx, node,
                    "action '%s' cannot reference non-exported effect '%s' from another module",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.causes_effect);
            }
            if (effect_decl != NULL
                && !effect_decl->is_exported
                && callable_contract_is_externally_visible(node, ctx)) {
                semantic_error(ctx, node,
                    "action '%s' cannot reference non-exported effect '%s' in an externally visible contract",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.causes_effect);
            }
        }

        for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++) {
            const char *auth_name = node->data.func_decl.authorized_by[i];
            bool found = auth_name != NULL && strcmp(auth_name, "self") == 0;
            const char *auth_type_name = NULL;

            for (size_t j = 0; !found && j < node->data.func_decl.param_count; j++) {
                FuncParam *param = node->data.func_decl.params[j];
                if (param != NULL && param->name != NULL
                    && strcmp(param->name, auth_name) == 0) {
                    found = true;
                }
            }

            if (!found) {
                semantic_error(ctx, node,
                    "action '%s' authorized subject '%s' must be 'self' or one of the action parameters",
                    name != NULL ? name : "<anonymous>",
                    auth_name != NULL ? auth_name : "<subject>");
                continue;
            }

            auth_type_name = find_action_binding_type_name(
                node, enclosing_nominal, ctx, auth_name);
            if (auth_type_name == NULL) {
                semantic_error(ctx, node,
                    "action '%s' authorized subject '%s' must be a subject host",
                    name != NULL ? name : "<anonymous>",
                    auth_name != NULL ? auth_name : "<subject>");
            }
        }

        if (node->data.func_decl.within_zone != NULL) {
            ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                node->data.func_decl.within_zone);
            if (zone_decl != NULL && subject_name != NULL
                && !domain_has_subject_slot_type(zone_decl->data.zone_decl.slots,
                    zone_decl->data.zone_decl.slot_count, ctx, subject_name)) {
                semantic_error(ctx, node,
                    "action '%s' references zone '%s', but that zone has no subject slot for '%s'",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone,
                    subject_name);
            }

            if (zone_decl != NULL) {
                for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++) {
                    const char *auth_name = node->data.func_decl.authorized_by[i];
                    const char *auth_type_name = find_action_binding_type_name(
                        node, enclosing_nominal, ctx, auth_name);
                    if (auth_type_name == NULL) {
                        continue;
                    }
                    if (!domain_has_subject_slot_type(zone_decl->data.zone_decl.slots,
                            zone_decl->data.zone_decl.slot_count, ctx, auth_type_name)) {
                        semantic_error(ctx, node,
                            "action '%s' authorized subject '%s' has type '%s', but zone '%s' has no matching subject slot.\n"
                            "Reason:\n"
                            "- action contract derives authority provenance from binding '%s'\n"
                            "- binding '%s' has subject type '%s'\n"
                            "- zone '%s' exposes no subject slot for that type\n"
                            "Fix:\n"
                            "- add a subject slot for '%s' to zone '%s'\n"
                            "- or authorize this action by a subject already declared in zone '%s'",
                            name != NULL ? name : "<anonymous>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            node->data.func_decl.within_zone);
                    } else if (!zone_has_authority_for_subject_type(zone_decl, ctx, auth_type_name)) {
                        semantic_error(ctx, node,
                            "action '%s' authorized subject '%s' has type '%s', but zone '%s' declares no matching authority.\n"
                            "Reason:\n"
                            "- within-zone contract comes from action clause 'within %s'\n"
                            "- action contract derives authority provenance from binding '%s'\n"
                            "- binding '%s' has subject type '%s'\n"
                            "- authority check edge is action '%s' -> zone '%s' -> binding '%s'\n"
                            "- zone '%s' has a subject slot for that type but no authority contract\n"
                            "Fix:\n"
                            "- declare authority for '%s' in zone '%s'\n"
                            "- or change/remove 'authorized by %s' on action '%s'",
                            name != NULL ? name : "<anonymous>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            name != NULL ? name : "<anonymous>",
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            node->data.func_decl.within_zone,
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            name != NULL ? name : "<anonymous>");
                    }
                }
            }
        }

        if (node->data.func_decl.causes_effect != NULL) {
            ASTNode *effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect);
            if (effect_decl != NULL) {
                for (size_t i = 0; i < effect_decl->data.effect_decl.slot_count; i++) {
                    ASTNode *slot = effect_decl->data.effect_decl.slots[i];
                    Type *slot_type;
                    bool matched = false;

                    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                        || !slot->data.domain_slot.is_binding
                        || slot->data.domain_slot.type == NULL) {
                        continue;
                    }
                    slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
                    if (slot_type == NULL || slot_type->name == NULL)
                        continue;

                    if (subject_name != NULL && strcmp(subject_name, slot_type->name) == 0) {
                        matched = true;
                    } else {
                        for (size_t j = 0; j < node->data.func_decl.param_count; j++) {
                            FuncParam *param = node->data.func_decl.params[j];
                            Type *param_type;
                            if (param == NULL || param->type == NULL)
                                continue;
                            param_type = resolve_type_node(param->type, ctx);
                            if (param_type != NULL
                                && param_type->name != NULL
                                && strcmp(param_type->name, slot_type->name) == 0) {
                                matched = true;
                                break;
                            }
                        }
                    }

                    if (!matched) {
                        semantic_error(ctx, node,
                            "action '%s' causes effect '%s', but no self/parameter matches effect target type '%s'",
                            name != NULL ? name : "<anonymous>",
                            node->data.func_decl.causes_effect,
                            slot_type->name);
                    }
                }
            }

            if (node->data.func_decl.within_zone != NULL) {
                ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                    node->data.func_decl.within_zone);
                if (zone_decl != NULL
                    && !zone_has_effect_layer_type(zone_decl, node->data.func_decl.causes_effect)) {
                    semantic_error(ctx, node,
                        "action '%s' causes effect '%s', but zone '%s' has no matching effect slot.\n"
                        "Reason:\n"
                        "- action contract declares causes '%s'\n"
                        "- zone '%s' does not materialize any matching effect slot for that contract\n"
                        "Fix:\n"
                        "- add an effect slot of type '%s' to zone '%s'\n"
                        "- or remove/change the causes clause on action '%s'",
                        name != NULL ? name : "<anonymous>",
                        node->data.func_decl.causes_effect,
                        node->data.func_decl.within_zone,
                        node->data.func_decl.causes_effect,
                        node->data.func_decl.within_zone,
                        node->data.func_decl.causes_effect,
                        node->data.func_decl.within_zone,
                        name != NULL ? name : "<anonymous>");
                }
            }
        }
    }
    /* subject now allows both func (private internal computation)
     * and action (public plot behavior with zone/effect/authority). */

    /* If the function has generic parameters (<T, U, ...>),
     * register them as opaque types in a temporary scope so that
     * resolve_type_node("T") succeeds for params and return type. */
    bool has_generics = (node->data.func_decl.generic_params != NULL
                         && node->data.func_decl.generic_params->count > 0);
    if (has_generics) {
        validate_generic_param_defaults(node->data.func_decl.generic_params,
            ctx, node, "function");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.func_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    /* Build parameter types for the function type */
    size_t   param_count = node->data.func_decl.param_count;
    Type   **param_types = NULL;

    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(Type *));
        if (param_types == NULL) {
            if (has_generics) scope_exit(&ctx->scope);
            return false;
        }
    }

    Type *return_type = TYPE_VOID;
    if (node->data.func_decl.return_type != NULL)
        return_type = resolve_type_node(node->data.func_decl.return_type, ctx);
    if (type_is_anchored_resource_handle(return_type)) {
        semantic_error(ctx, node->data.func_decl.return_type,
            "Anchored resource handle return types (Slot/SecureSlot/DeviceSlot) are not supported yet.\n"
            "Reason:\n"
            "- function return boundary would let an anchored handle escape its local ownership/runtime anchor\n"
            "- anchored handles are still local-only at return boundaries in the closed subset\n"
            "Fix:\n"
            "- return the inner value instead\n"
            "- or keep the anchored handle local and return a boundary-safe projection/result");
    }
    if (type_is_class_object_type(return_type, ctx)) {
        semantic_error(ctx, node->data.func_decl.return_type,
            "Returning subjects by value is not supported yet.\n"
            "Reason:\n"
            "- return type '%s' is a subject, and subject values are zone/world anchored handles\n"
            "Fix:\n"
            "- return a struct/class/object/tobject projection instead\n"
            "- keep the subject local to its owning zone/world\n"
            "- or use Box<T>/another explicit handle layer",
            type_name_or_unknown(return_type));
    }

    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = node->data.func_decl.params[i];
        /* Implicit 'self' type: if a parameter named "self" has no
         * type annotation and we're inside a class scope, infer the
         * enclosing class type. */
        if (param->type == NULL && param->name != NULL
            && strcmp(param->name, "self") == 0
            && ctx->scope != NULL && ctx->scope->kind == SCOPE_CLASS) {
            Scope *parent = ctx->scope->parent;
            const char *nominal_name = NULL;

            if (ctx->current_nominal_decl != NULL
                && ctx->current_nominal_decl->type == AST_CLASS_DECL) {
                nominal_name = ctx->current_nominal_decl->data.class_decl.name;
            }

            if (parent != NULL && nominal_name != NULL) {
                Symbol *self_sym = scope_lookup(parent, nominal_name);
                if (self_sym != NULL && self_sym->kind == SYMBOL_CLASS)
                    param_types[i] = self_sym->type;
            }

            if (param_types[i] == NULL && parent != NULL) {
                /* Fallback for older paths that do not set current_nominal_decl. */
                for (size_t s = parent->symbol_count; s > 0; s--) {
                    Symbol *cs = parent->symbols[s - 1];
                    if (cs != NULL && cs->kind == SYMBOL_CLASS) {
                        param_types[i] = cs->type;
                        break;
                    }
                }
            }

            if (param_types[i] == NULL)
                param_types[i] = TYPE_UNKNOWN;
        } else {
            param_types[i] = resolve_type_node(param->type, ctx);
        }
        if (param->mode != PARAM_MODE_DEFAULT
            && !type_is_anchored_resource_handle(param_types[i])) {
            if (!type_is_general_boundary_type(param_types[i], ctx)) {
                semantic_error(ctx, node,
                    "'%s' parameter mode is currently a closed subset: only ref/own subject values, ref/own movable resources, "
                    "ref/own boundary values, ref/own Slot<subject-host>, and own SecureSlot<subject-host> are supported at function boundaries.\n"
                    "Reason:\n"
                    "- value is parameter '%s'\n"
                    "- ownership mode is '%s'\n"
                    "- consumer path is function '%s'\n"
                    "- this parameter type is not part of the current move/borrow boundary subset\n"
                    "- the compiler does not yet enforce general own/ref rules for arbitrary value types at call boundaries\n"
                    "Fix:\n"
                    "- keep this value local to the function boundary\n"
                    "- or pass a subject value, movable resource, boundary value, Slot<subject-host>, or own SecureSlot<subject-host> instead",
                    param->mode == PARAM_MODE_OWN ? "own" : "ref",
                    param->name != NULL ? param->name : "<param>",
                    param->mode == PARAM_MODE_OWN ? "own" : "ref",
                    node->data.func_decl.name != NULL ? node->data.func_decl.name : "<anonymous>");
            }
        }
        if (type_is_anchored_resource_handle(param_types[i])) {
            bool allowed_subject_slot = type_is_subject_host_slot_handle(param_types[i], ctx);
            if (!allowed_subject_slot) {
                semantic_error(ctx, node,
                    "Anchored handle parameters are currently closed to ref/own Slot<subject-host> / "
                    "own SecureSlot<subject-host>; other anchored handles remain local-only.\n"
                    "Reason:\n"
                    "- this anchored handle kind is not part of the stable boundary subset\n"
                    "- passing it across the function boundary would require a stronger ownership contract than the compiler currently guarantees\n"
                    "Fix:\n"
                    "- keep this anchored handle local to the current scope\n"
                    "- or convert it to a projection/value/boundary-safe result before the call");
            } else if (param->mode == PARAM_MODE_DEFAULT) {
                semantic_error(ctx, node,
                    "Slot<subject-host>/SecureSlot<subject-host> parameters are part of the anchored "
                    "subject-slot boundary and require explicit 'own' or 'ref'.\n"
                    "Reason:\n"
                    "- anchored subject-slot handles must declare whether the boundary borrows or transfers ownership\n"
                    "- implicit parameter passing would hide that boundary contract\n"
                    "Fix:\n"
                    "- mark the parameter as 'ref' for borrowing\n"
                    "- or mark it as 'own' for transfer");
            } else if (param_types[i]->data.slot.is_secure
                       && param->mode != PARAM_MODE_OWN) {
                semantic_error(ctx, node,
                    "SecureSlot<subject-host> parameters currently support only 'own' at function boundaries.\n"
                    "Reason:\n"
                    "- secure anchored handles carry capability-bearing ownership across the boundary\n"
                    "- borrowed secure forwarding is not part of the closed subset yet\n"
                    "Fix:\n"
                    "- change the parameter to 'own SecureSlot<subject-host>'\n"
                    "- or keep the SecureSlot local and pass a projection/value instead");
            }
        }
        /* Subject parameters are passed by reference (pointer) internally.
         * The language hides pointer semantics from the user — subjects
         * are identity-bearing types, so reference passing is automatic. */
    }

    Type *func_type = type_create_function(param_types, param_count,
                                            return_type);

    Symbol *func_sym = symbol_create_function(name, func_type,
                                               node->line, node->column);
    /* If a forward-declaration placeholder exists from Pass 1,
       update its type instead of re-declaring. */
    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_FUNCTION) {
        existing->type = func_type;
        symbol_destroy(func_sym);
    } else if (!scope_declare(ctx->scope, func_sym)) {
        semantic_error(ctx, node, "Redeclaration of function '%s'", name);
        symbol_destroy(func_sym);
        return false;
    }

    /* Close the temporary generic-params scope (if opened) before
     * entering the real function scope — the function scope will
     * re-register the generic params so they're visible in the body. */
    if (has_generics)
        scope_exit(&ctx->scope);

    validate_where_clause_bounds(node->data.func_decl.where_clause, ctx, node);
    validate_generic_param_default_bounds(
        node->data.func_decl.generic_params,
        node->data.func_decl.where_clause,
        ctx,
        node,
        "function",
        name);

    /* Check body in new function scope */
    scope_enter(&ctx->scope, SCOPE_FUNCTION);
    if (node->origin_path != NULL)
        ctx->current_module_path = node->origin_path;

    /* Re-register generic type params inside the function scope */
    if (has_generics) {
        GenericParams *gp = node->data.func_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    Type *prev_return  = ctx->current_return;
    ctx->current_function_decl = node;
    ctx->current_return = return_type;
    ctx->current_function_effects = EFFECT_NONE;
    ctx->tracking_function_effects = true;
    ctx->in_async_func = prev_async || node->is_async_decl;

    /* Register parameters */
    for (size_t i = 0; i < param_count; i++) {
        Type *pt = func_type->data.function.param_types[i];
        const char *param_name = node->data.func_decl.params[i]->name;
        if (pt != NULL && pt->kind == TYPE_KIND_SLOT && pt->data.slot.is_secure)
            semantic_record_effect(ctx, EFFECT_SECURE);
        if (type_is_constructed_named(pt, "Token"))
            semantic_record_effect(ctx, EFFECT_SECURE);
        if (type_is_subject_host_slot_handle(pt, ctx) && param_name != NULL) {
            char token_name[256];
            const char *paired_token = NULL;
            Symbol *slot_sym;

            if (pt->data.slot.is_secure) {
                snprintf(token_name, sizeof(token_name), "%s_token", param_name);
                paired_token = token_name;
            }

            slot_sym = symbol_create_slot(param_name, pt,
                pt->data.slot.is_secure, paired_token,
                node->line, node->column);
            scope_declare(ctx->scope, slot_sym);
            scope_register_slot(ctx->scope, slot_sym);

            if (pt->data.slot.is_secure) {
                Symbol *tok = symbol_create_token(token_name, param_name,
                    node->line, node->column);
                if (tok != NULL) {
                    Type *token_args[1] = { pt->data.slot.inner_type };
                    tok->type = type_create_constructed(TYPE_TOKEN, token_args, 1);
                }
                scope_declare(ctx->scope, tok);
            }
            continue;
        }

        Symbol *p = symbol_create_variable(
            param_name, pt,
            node->line, node->column);
        scope_declare(ctx->scope, p);
    }

    if (node->data.func_decl.body != NULL)
        type_check_block(node->data.func_decl.body, ctx);

    if (node->data.func_decl.body != NULL) {
        for (size_t i = 0; i < param_count; i++) {
            FuncParam *param = node->data.func_decl.params[i];
            unsigned summary_mask;

            if (param == NULL || param->name == NULL || param->type == NULL)
                continue;
            if (param->mode != PARAM_MODE_REF)
                continue;
            if (!type_is_anchored_resource_handle(param_types[i])
                && !type_is_movable_resource_handle(param_types[i])
                && !type_is_subject_type(param_types[i], ctx)
                && !type_requires_boundary_borrow_tracking(param_types[i], ctx))
                continue;

            summary_mask = slot_analyze_param_summary_in_program(
                node->data.func_decl.body, param->name, ctx->program_root);
            if ((summary_mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) {
                if (type_is_movable_resource_handle(param_types[i])) {
                    semantic_error(ctx, node,
                        "Borrowed ref movable resource '%s' cannot escape via return.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                        "- slot/resource summary found a return-escape path for that borrowed symbol\n"
                        "- returning it would let the borrow outlive the current call boundary\n"
                        "Fix:\n"
                        "- return a copied/projection/value result instead\n"
                        "- or change the parameter to 'own' if transfer is intended",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                } else if (type_is_subject_type(param_types[i], ctx)) {
                    semantic_error(ctx, node,
                        "Borrowed ref subject '%s' cannot escape via return.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' subject\n"
                        "- slot/resource summary found a return-escape path for that borrowed symbol\n"
                        "- returning it would let the borrow outlive the current call boundary\n"
                        "Fix:\n"
                        "- return a projection/object/tobject/value result instead\n"
                        "- or change the parameter to 'own' if transfer is intended",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                } else if (type_requires_boundary_borrow_tracking(param_types[i], ctx)) {
                    semantic_error(ctx, node,
                        "Borrowed ref boundary value '%s' cannot escape via return.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                        "- slot/resource summary found a return-escape path for that borrowed symbol\n"
                        "- returning it would let the borrow outlive the current call boundary\n"
                        "Fix:\n"
                        "- return a copied/value/projection result instead\n"
                        "- or change the parameter to 'own' if transfer is intended",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                } else {
                    semantic_error(ctx, node,
                        "Borrowed ref slot '%s' cannot escape via return.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' handle\n"
                        "- 'ref' is a non-owning borrow tied to the caller scope\n"
                        "- returning it would let the borrow outlive the call boundary\n"
                        "Fix:\n"
                        "- return a projection/object/tobject/value instead\n"
                        "- or change the parameter to 'own' if transfer is intended",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                }
            }
            if ((summary_mask & SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) != 0) {
                if (type_is_movable_resource_handle(param_types[i])) {
                    semantic_error(ctx, node,
                        "Borrowed ref movable resource '%s' cannot escape through channel send.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                        "- slot/resource summary found a channel-escape path for that borrowed symbol\n"
                        "- channel send would transfer the borrow beyond the current call boundary\n"
                        "Fix:\n"
                        "- send a copied/projection/value snapshot instead\n"
                        "- or take ownership with 'own' before transfer",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                } else if (type_is_subject_type(param_types[i], ctx)) {
                    semantic_error(ctx, node,
                        "Borrowed ref subject '%s' cannot escape through channel send.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' subject\n"
                        "- slot/resource summary found a channel-escape path for that borrowed symbol\n"
                        "- channel send would transfer the borrow beyond the current call boundary\n"
                        "Fix:\n"
                        "- send a projection/object/tobject/value snapshot instead\n"
                        "- or take ownership with 'own' before transfer",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                } else if (type_requires_boundary_borrow_tracking(param_types[i], ctx)) {
                    semantic_error(ctx, node,
                        "Borrowed ref boundary value '%s' cannot escape through channel send.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                        "- slot/resource summary found a channel-escape path for that borrowed symbol\n"
                        "- channel send would transfer the borrow beyond the current call boundary\n"
                        "Fix:\n"
                        "- send a copied/value/projection snapshot instead\n"
                        "- or take ownership with 'own' before transfer",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                } else {
                    semantic_error(ctx, node,
                        "Borrowed ref slot '%s' cannot escape through channel send.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' handle\n"
                        "- 'ref' is a non-owning borrow tied to the current call\n"
                        "- channel send would transfer the borrow beyond that boundary\n"
                        "Fix:\n"
                        "- send a projection/object/tobject/value snapshot instead\n"
                        "- or take ownership with 'own' before transfer",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                }
            }
            if ((summary_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) {
                if (type_is_movable_resource_handle(param_types[i])) {
                    semantic_error(ctx, node,
                        "Borrowed ref movable resource '%s' cannot escape through helper/function call.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                        "- slot/resource summary found a transitive call-escape path for that borrowed symbol\n"
                        "- forwarding it to another call would create a transitive borrow the compiler cannot keep boundary-safe\n"
                        "Fix:\n"
                        "- perform the movable-resource operation locally in this function\n"
                        "- or change the parameter to 'own' if transfer/forwarding is intended",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                } else if (type_is_subject_type(param_types[i], ctx)) {
                    semantic_error(ctx, node,
                        "Borrowed ref subject '%s' cannot escape through helper/function call.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' subject\n"
                        "- slot/resource summary found a transitive call-escape path for that borrowed symbol\n"
                        "- forwarding it to another call would create a transitive borrow the compiler cannot keep boundary-safe\n"
                        "Fix:\n"
                        "- perform the subject operation locally in this function\n"
                        "- or change the parameter to 'own' if transfer/forwarding is intended",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                } else if (type_requires_boundary_borrow_tracking(param_types[i], ctx)) {
                    semantic_error(ctx, node,
                        "Borrowed ref boundary value '%s' cannot escape through helper/function call.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                        "- slot/resource summary found a transitive call-escape path for that borrowed symbol\n"
                        "- forwarding it to another call would create a transitive borrow the compiler cannot keep boundary-safe\n"
                        "Fix:\n"
                        "- perform the value operation locally in this function\n"
                        "- or change the parameter to 'own' if transfer/forwarding is intended",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                } else {
                    semantic_error(ctx, node,
                        "Borrowed ref slot '%s' cannot escape through helper/function call.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' handle\n"
                        "- 'ref' is a non-owning borrow tied to the current call boundary\n"
                        "- forwarding it to another call would create a transitive borrow the compiler does not yet track precisely\n"
                        "Fix:\n"
                        "- perform the slot operation locally in this function\n"
                        "- or change the parameter to 'own' if transfer/forwarding is intended",
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name);
                }
            }
        }
    }

    {
        uint32_t derived_effects = type_effect_mask_closure(ctx->current_function_effects);
        uint32_t missing_effects =
            type_effect_mask_closure(derived_effects) & ~type_effect_mask_closure(declared_effects);
        char derived_buf[128];
        char missing_buf[128];
        char declared_buf[128];

        if (has_effect_contract && missing_effects != EFFECT_NONE) {
            effect_mask_to_string(derived_effects, derived_buf, sizeof(derived_buf));
            effect_mask_to_string(missing_effects, missing_buf, sizeof(missing_buf));
            effect_mask_to_string(declared_effects, declared_buf, sizeof(declared_buf));
            semantic_error(ctx, node,
                "Function '%s' is missing declared effects: %s (declared: %s, derived from body: %s)",
                name != NULL ? name : "<anonymous>",
                missing_buf, declared_buf, derived_buf);
        }

        func_type->data.function.effect_mask =
            type_effect_mask_join(declared_effects, derived_effects);

        if (type_effect_mask_conflicts(func_type->data.function.effect_mask,
                                       func_type->data.function.effect_mask)) {
            effect_mask_to_string(func_type->data.function.effect_mask,
                                  derived_buf, sizeof(derived_buf));
            semantic_warning(ctx, node,
                "Function '%s' combines effect classes that are currently treated as conflicting (%s).\n"
                "Reason:\n"
                "- derived body effects joined into '%s'\n"
                "- current partial order still treats part of that join as conflicting in one routine\n"
                "- this usually means authority-sensitive work and boundary/resource work were merged in one flow\n"
                "Fix:\n"
                "- split the routine into smaller helpers so each helper owns one effect family\n"
                "- or isolate the conflicting branch/handoff path behind an explicit boundary helper",
                name != NULL ? name : "<anonymous>",
                derived_buf,
                derived_buf);
        }

        if (is_action
            && node->data.func_decl.within_zone != NULL
            && type_effect_mask_requires_authority(func_type->data.function.effect_mask)
            && node->data.func_decl.authorized_by_count == 0) {
            semantic_error(ctx, node,
                "secure action '%s' within zone '%s' must declare 'authorized by'.\n"
                "Reason:\n"
                "- action body derives authority-sensitive effects from capability-bearing work\n"
                "- zone '%s' makes this action part of an explicit authority boundary\n"
                "- without 'authorized by', the approval provenance for that boundary is missing\n"
                "Fix:\n"
                "- add 'authorized by <subject-slot>' to the action contract\n"
                "- or move the authority-sensitive work behind a helper that is called from an already-authorized action",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.within_zone,
                node->data.func_decl.within_zone);
        }
        if (is_action
            && node->data.func_decl.within_zone != NULL
            && node->data.func_decl.causes_effect != NULL
            && node->data.func_decl.authorized_by_count == 0) {
            semantic_error(ctx, node,
                "action '%s' causing effect '%s' within zone '%s' must declare 'authorized by'.\n"
                "Reason:\n"
                "- action contract declares causes '%s'\n"
                "- causing an effect inside zone '%s' is an authority-sensitive state change\n"
                "- without 'authorized by', the approval provenance for that state change is missing\n"
                "Fix:\n"
                "- add 'authorized by <subject-slot>' to the action contract\n"
                "- or remove/change the causes clause if this action should stay authority-free",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.causes_effect,
                node->data.func_decl.within_zone,
                node->data.func_decl.causes_effect,
                node->data.func_decl.within_zone);
        }
    }

    ctx->current_return = prev_return;
    ctx->current_function_decl = prev_function_decl;
    ctx->current_function_effects = prev_effects;
    ctx->tracking_function_effects = prev_tracking;
    ctx->in_async_func = prev_async;
    ctx->current_module_path = prev_module_path;
    free(param_types);
    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

bool
type_check_class_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.class_decl.name;
    ASTNode *saved_nominal = ctx->current_nominal_decl;

    /* If the class has generic parameters (<T, U, ...>), register them
     * as opaque types in a temporary scope so that resolve_type_node("T")
     * succeeds for field types and method signatures. */
    bool has_generics = (node->data.class_decl.generic_params != NULL
                         && node->data.class_decl.generic_params->count > 0);
    if (has_generics) {
        validate_generic_param_defaults(node->data.class_decl.generic_params,
            ctx, node, "class");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.class_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    Type *class_type = calloc(1, sizeof(Type));
    if (class_type == NULL) {
        if (has_generics) scope_exit(&ctx->scope);
        return false;
    }
    class_type->kind = TYPE_KIND_CLASS;
    class_type->nominal_flavor = nominal_flavor_from_decl(node);
    class_type->name = pergyra_strdup(name);

    Symbol *class_sym = symbol_create_function(name, class_type,
                                                node->line, node->column);
    class_sym->kind = SYMBOL_CLASS;

    /* Declare in the outer scope (step out of temporary generic scope).
     * Pass 1 may already have registered a nominal placeholder so that
     * forward references in earlier declarations can resolve. */
    {
        Scope *target = has_generics ? ctx->scope->parent : ctx->scope;
        Scope *saved = ctx->scope;
        ctx->scope = target;
        Symbol *existing = scope_lookup_current(ctx->scope, name);
        if (existing != NULL
            && existing->kind == SYMBOL_CLASS
            && existing->decl_line == (uint32_t)node->line
            && existing->decl_col == (uint32_t)node->column) {
            existing->type = class_type;
            symbol_destroy(class_sym);
        } else if (!scope_declare(ctx->scope, class_sym)) {
            semantic_error(ctx, node, "Redeclaration of class '%s'", name);
            symbol_destroy(class_sym);
            ctx->scope = saved;
            if (has_generics) scope_exit(&ctx->scope);
            return false;
        }
        ctx->scope = saved;
    }

    /* Close the temporary generic-params scope before entering the real
     * class scope — the class scope will re-register generic params so
     * they're visible in the body. */
    if (has_generics)
        scope_exit(&ctx->scope);

    validate_where_clause_bounds(node->data.class_decl.where_clause, ctx, node);
    validate_generic_param_default_bounds(
        node->data.class_decl.generic_params,
        node->data.class_decl.where_clause,
        ctx,
        node,
        "class",
        name);

    /* Check methods — type-check each in a temporary class scope,
     * then register the mangled name (ClassName_MethodName) in the
     * parent scope so that callers can find it. */
    scope_enter(&ctx->scope, SCOPE_CLASS);
    ctx->current_nominal_decl = node;

    /* Re-register generic type params inside the class scope */
    if (has_generics) {
        GenericParams *gp = node->data.class_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }
    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *field = node->data.class_decl.fields[i];
        Type *field_type;
        Symbol *field_sym;

        if (field == NULL || field->name == NULL || field->type == NULL)
            continue;

        field_type = resolve_type_node(field->type, ctx);
        if (field->is_vessel_field) {
            ASTNode *field_decl = NULL;
            if (field_type != NULL && field_type->kind == TYPE_KIND_CLASS
                && field_type->name != NULL) {
                field_decl = find_type_decl_by_name(ctx->program_root, field_type->name);
            }
            if (field_decl == NULL
                || field_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
                semantic_error(ctx, field->type,
                    "subject vessel field '%s' must reference a vessel type",
                    field->name != NULL ? field->name : "<field>");
            }
        }
        field_sym = symbol_create_variable(field->name, field_type,
            node->line, node->column);
        if (field_sym != NULL)
            scope_declare(ctx->scope, field_sym);
    }
    /* struct declarations cannot have methods — use class or object */
    if (node->data.class_decl.nominal_kind == NOMINAL_DECL_STRUCT
        && node->data.class_decl.method_count > 0) {
        semantic_error(ctx, node,
            "struct '%s' cannot have methods; use 'class', 'subject', or 'object' instead",
            name != NULL ? name : "<struct>");
    }

    for (size_t i = 0; i < node->data.class_decl.method_count; i++)
        type_check_func_decl(node->data.class_decl.methods[i], ctx);

    /* Collect method signatures before the class scope is destroyed */
    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        const char *mname = method->data.func_decl.name;
        if (mname == NULL)
            continue;
        Symbol *msym = scope_lookup_current(ctx->scope, mname);
        if (msym == NULL || msym->kind != SYMBOL_FUNCTION)
            continue;

        /* Build mangled name: ClassName_MethodName */
        size_t len = strlen(name) + 1 + strlen(mname) + 1;
        char *mangled = malloc(len);
        if (mangled == NULL)
            continue;
        snprintf(mangled, len, "%s_%s", name, mname);

        /* The method's func_type already includes 'self' as the first
         * parameter (registered by type_check_func_decl), so reuse
         * the original signature directly. */
        Type *mangled_ft = msym->type;

        /* Register in parent scope (outside class) */
        Symbol *mangled_sym = symbol_create_function(
            mangled, mangled_ft, method->line, method->column);
        /* Temporarily step out to declare in parent */
        Scope *class_scope = ctx->scope;
        ctx->scope = class_scope->parent;
        if (!scope_declare(ctx->scope, mangled_sym))
            symbol_destroy(mangled_sym);
        ctx->scope = class_scope;
        free(mangled);
    }

    scope_exit(&ctx->scope);
    ctx->current_nominal_decl = saved_nominal;

    return !ctx->has_error;
}

bool
type_check_extern_block(ASTNode *node, SemanticContext *ctx)
{
    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl != NULL && decl->type == AST_FUNC_DECL)
            type_check_func_decl(decl, ctx);
    }
    return !ctx->has_error;
}

/* -----------------------------------------------------------------
 * Program entry point
 * ----------------------------------------------------------------- */

bool
type_check_program(ASTNode *program, SemanticContext *ctx)
{
    size_t *topo_order = NULL;
    size_t topo_count = 0;

    if (program == NULL || program->type != AST_PROGRAM)
        return false;

    ctx->program_root = program;

    /*
     * Pass 1: collect all top-level function and class names
     * so that forward references within the same file work.
     */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt->type == AST_TYPE_ALIAS) {
            const char *tname = stmt->data.type_alias.name;
            if (tname != NULL && scope_lookup_current(ctx->scope, tname) == NULL) {
                Symbol *s = symbol_create_function(tname, TYPE_UNKNOWN,
                    stmt->line, stmt->column);
                if (s != NULL)
                    s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_CLASS_DECL) {
            const char *cname = stmt->data.class_decl.name;
            if (cname != NULL && scope_lookup_current(ctx->scope, cname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_CLASS;
                    t->nominal_flavor = nominal_flavor_from_decl(stmt);
                    t->name = pergyra_strdup(cname);
                }
                Symbol *s = symbol_create_function(cname,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                if (s != NULL)
                    s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_FUNC_DECL) {
            const char *fname = stmt->data.func_decl.name;
            if (scope_lookup_current(ctx->scope, fname) == NULL) {
                /* Forward-declare with correct param count so that
                 * call-site arity checks pass before Pass 2. */
                size_t fpc = stmt->data.func_decl.param_count;
                /* Exclude implicit 'self' param from count */
                size_t real_pc = 0;
                for (size_t j = 0; j < fpc; j++) {
                    FuncParam *p = stmt->data.func_decl.params[j];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    real_pc++;
                }
                Type **ptypes = calloc(real_pc > 0 ? real_pc : 1,
                                         sizeof(Type *));
                for (size_t j = 0; j < real_pc; j++)
                    ptypes[j] = TYPE_UNKNOWN;
                Type *ret = TYPE_VOID;
                if (stmt->data.func_decl.return_type != NULL) {
                    /* Try to resolve return type; use TYPE_UNKNOWN on failure
                     * (e.g., generic return type 'T' not in scope yet). */
                    size_t saved_diag = ctx->diagnostic_count;
                    bool saved_err = ctx->has_error;
                    ret = resolve_type_node(stmt->data.func_decl.return_type, ctx);
                    if (ctx->diagnostic_count > saved_diag) {
                        /* Roll back the diagnostic — Pass 2 will re-check */
                        ctx->diagnostic_count = saved_diag;
                        ctx->has_error = saved_err;
                        ret = TYPE_UNKNOWN;
                    }
                }
                Type *placeholder = type_create_function(ptypes, real_pc, ret);
                if (placeholder != NULL)
                    placeholder->data.function.effect_mask =
                        declared_effects_from_function_node(stmt, ctx, NULL);
                free(ptypes);
                Symbol *s = symbol_create_function(fname, placeholder,
                                                    stmt->line, stmt->column);
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_EVENT_DECL) {
            const char *ename = stmt->data.event_decl.name;
            if (scope_lookup_current(ctx->scope, ename) == NULL) {
                size_t epc = stmt->data.event_decl.param_count;
                Type **eptypes = calloc(epc > 0 ? epc : 1, sizeof(Type *));
                for (size_t j = 0; j < epc; j++) {
                    ASTNode *p = stmt->data.event_decl.params[j];
                    if (p != NULL
                        && p->type == AST_LET_DECL
                        && p->data.let_decl.type != NULL) {
                        size_t saved_diag = ctx->diagnostic_count;
                        bool saved_err = ctx->has_error;
                        eptypes[j] = resolve_type_node(p->data.let_decl.type, ctx);
                        if (ctx->diagnostic_count > saved_diag) {
                            /* Roll back the diagnostic — Pass 2 will re-check. */
                            ctx->diagnostic_count = saved_diag;
                            ctx->has_error = saved_err;
                            eptypes[j] = TYPE_UNKNOWN;
                        }
                    } else {
                        eptypes[j] = TYPE_UNKNOWN;
                    }
                }
                Type *evt_ft = type_create_function(eptypes, epc, TYPE_VOID);
                free(eptypes);
                Symbol *s = symbol_create_function(ename, evt_ft,
                                                    stmt->line, stmt->column);
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_ENUM_DECL) {
            const char *ename = stmt->data.enum_decl.name;
            if (ename != NULL && scope_lookup_current(ctx->scope, ename) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_ENUM;
                    t->name = pergyra_strdup(ename);
                }
                Symbol *s = symbol_create_function(ename,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
            Symbol *enum_sym = scope_lookup_current(ctx->scope, ename);
            Type *etype = enum_sym != NULL ? enum_sym->type : TYPE_UNKNOWN;
            for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
                const char *vname = stmt->data.enum_decl.variants[j];
                if (vname == NULL || scope_lookup_current(ctx->scope, vname) != NULL)
                    continue;
                size_t vpc = (stmt->data.enum_decl.variant_param_counts != NULL)
                    ? stmt->data.enum_decl.variant_param_counts[j] : 0;
                if (vpc > 0) {
                    /* Tagged union variant constructor: register as function
                     * Circle(Int) → func Circle(Int) -> Shape */
                    Type **ptypes = calloc(vpc, sizeof(Type *));
                    for (size_t p = 0; p < vpc && ptypes != NULL; p++) {
                        ASTNode *pt = stmt->data.enum_decl.variant_params[j][p];
                        ptypes[p] = resolve_type_node(pt, ctx);
                    }
                    Type *ft = type_create_function(ptypes, vpc, etype);
                    free(ptypes);
                    Symbol *vs = symbol_create_function(vname, ft,
                        stmt->line, stmt->column);
                    scope_declare(ctx->scope, vs);
                } else {
                    /* Simple variant: register as variable */
                    Symbol *vs = symbol_create_variable(vname, etype,
                        stmt->line, stmt->column);
                    scope_declare(ctx->scope, vs);
                }
            }
        } else if (stmt->type == AST_EXTERN_BLOCK) {
            for (size_t j = 0; j < stmt->data.extern_block.count; j++) {
                ASTNode *decl = stmt->data.extern_block.declarations[j];
                if (decl == NULL || decl->type != AST_FUNC_DECL)
                    continue;
                const char *fname = decl->data.func_decl.name;
                if (scope_lookup_current(ctx->scope, fname) == NULL) {
                    Type *placeholder = type_create_function(NULL, 0, TYPE_VOID);
                    if (placeholder != NULL)
                        placeholder->data.function.effect_mask =
                            declared_effects_from_function_node(decl, ctx, NULL);
                    Symbol *s = symbol_create_function(fname, placeholder,
                                                        decl->line, decl->column);
                    scope_declare(ctx->scope, s);
                }
            }
        } else if (stmt->type == AST_INTENT_DECL) {
            const char *iname = stmt->data.intent_decl.name;
            if (iname != NULL && scope_lookup_current(ctx->scope, iname) == NULL) {
                size_t ipc = stmt->data.intent_decl.binding_count > 0
                    ? stmt->data.intent_decl.binding_count
                    : (stmt->data.intent_decl.involve_count
                        + stmt->data.intent_decl.value_count);
                Type **ptypes = calloc(ipc > 0 ? ipc : 1, sizeof(Type *));
                for (size_t j = 0; j < ipc; j++) {
                    ASTNode *binding = stmt->data.intent_decl.binding_count > 0
                        ? stmt->data.intent_decl.bindings[j]
                        : (j < stmt->data.intent_decl.involve_count
                            ? stmt->data.intent_decl.involves[j]
                            : stmt->data.intent_decl.values[j - stmt->data.intent_decl.involve_count]);
                    if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                        && binding->data.intent_involves.subject_type != NULL) {
                        size_t saved_diag = ctx->diagnostic_count;
                        bool saved_err = ctx->has_error;
                        ptypes[j] = resolve_type_node(
                            binding->data.intent_involves.subject_type, ctx);
                        if (ctx->diagnostic_count > saved_diag) {
                            ctx->diagnostic_count = saved_diag;
                            ctx->has_error = saved_err;
                            ptypes[j] = TYPE_UNKNOWN;
                        }
                    } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                        && binding->data.intent_value.value_type != NULL) {
                        size_t saved_diag = ctx->diagnostic_count;
                        bool saved_err = ctx->has_error;
                        ptypes[j] = resolve_type_node(
                            binding->data.intent_value.value_type, ctx);
                        if (ctx->diagnostic_count > saved_diag) {
                            ctx->diagnostic_count = saved_diag;
                            ctx->has_error = saved_err;
                            ptypes[j] = TYPE_UNKNOWN;
                        }
                    } else {
                        ptypes[j] = TYPE_UNKNOWN;
                    }
                }
                Type *ft = type_create_function(ptypes, ipc, TYPE_BOOL);
                free(ptypes);
                Symbol *s = symbol_create_function(iname,
                    ft != NULL ? ft : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_INTENT;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_PARTY_DECL
                   || stmt->type == AST_ROSTER_DECL
                   || stmt->type == AST_WORLD_DECL
                   || stmt->type == AST_RELATION_DECL
                   || stmt->type == AST_EFFECT_DECL
                   || stmt->type == AST_ZONE_DECL) {
            /* Register domain declarations as class-like symbols
             * so that constructor-like syntax can be introduced consistently */
            const char *dname = NULL;
            if (stmt->type == AST_PARTY_DECL)
                dname = stmt->data.party_decl.name;
            else if (stmt->type == AST_ROSTER_DECL)
                dname = stmt->data.roster_decl.name;
            else if (stmt->type == AST_WORLD_DECL)
                dname = stmt->data.world_decl.name;
            else if (stmt->type == AST_RELATION_DECL)
                dname = stmt->data.relation_decl.name;
            else if (stmt->type == AST_EFFECT_DECL)
                dname = stmt->data.effect_decl.name;
            else
                dname = stmt->data.zone_decl.name;
            if (dname != NULL && scope_lookup_current(ctx->scope, dname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_CLASS;
                    t->nominal_flavor = TYPE_NOMINAL_CLASS;
                    t->name = pergyra_strdup(dname);
                }
                Symbol *s = symbol_create_function(dname,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        }
    }

    semantic_type_resolution_precollect_program(program, ctx);
    if (!type_resolution_validate_graph(ctx))
        return false;
    if (!type_resolution_build_topo_order(&ctx->type_resolution_graph,
                                          &topo_order,
                                          &topo_count)) {
        semantic_error(ctx, program,
            "Type resolution topological ordering could not be constructed.\n"
            "Reason:\n"
            "- the semantic type dependency graph is not acyclic or not fully materialized\n"
            "- graph-backed staged resolution cannot trust the current declaration order\n"
            "Fix:\n"
            "- resolve the earlier type dependency cycle\n"
            "- or close the missing generic/alias/ability dependency edge");
        free(topo_order);
        return false;
    }

    semantic_run_type_resolution_worklist(program, ctx, topo_order, topo_count);

    /*
     * Pass 2: full type-check
     */
    for (size_t i = 0; i < program->data.program.count; i++)
        type_check_statement(program->data.program.statements[i], ctx);

    (void)type_resolution_validate_graph(ctx);

    /* Optional instrumentation for type-resolution audit (단계 1.0).
     * Enabled when PGY_TYPE_RES_STATS is set. */
    {
        const char *stats_env = getenv("PGY_TYPE_RES_STATS");
        if (stats_env != NULL && stats_env[0] != '\0' && stats_env[0] != '0') {
            TypeResolutionGraph *g = &ctx->type_resolution_graph;
            size_t kind_counts[7] = {0};
            size_t *indeg = NULL;
            size_t *outdeg = NULL;
            size_t name_dup = 0;
            size_t topo_count = 0;
            size_t *topo = NULL;
            bool topo_ok;

            for (size_t i = 0; i < g->node_count; i++) {
                int k = (int)g->nodes[i].kind;
                if (k >= 0 && k < 7) kind_counts[k]++;
            }
            indeg = calloc(g->node_count > 0 ? g->node_count : 1, sizeof(size_t));
            outdeg = calloc(g->node_count > 0 ? g->node_count : 1, sizeof(size_t));
            if (indeg != NULL && outdeg != NULL) {
                for (size_t e = 0; e < g->edge_count; e++) {
                    if (g->edges[e].from < g->node_count) outdeg[g->edges[e].from]++;
                    if (g->edges[e].to < g->node_count)   indeg[g->edges[e].to]++;
                }
            }
            /* Detect duplicate labels (re-visits of same named type) */
            for (size_t i = 0; i < g->node_count; i++) {
                const char *li = g->nodes[i].label;
                if (li == NULL) continue;
                for (size_t j = i + 1; j < g->node_count; j++) {
                    const char *lj = g->nodes[j].label;
                    if (lj != NULL && strcmp(li, lj) == 0) { name_dup++; break; }
                }
            }
            if (topo_order != NULL) {
                topo_ok = true;
                topo = NULL;
                topo_count = g->node_count;
            } else {
                topo_ok = type_resolution_build_topo_order(g, &topo, &topo_count);
            }

            fprintf(stderr, "[type-res-stats] nodes=%zu edges=%zu duplicate_labels=%zu topo_ok=%d topo_produced=%zu/%zu\n",
                    g->node_count, g->edge_count, name_dup,
                    topo_ok ? 1 : 0, topo_count, g->node_count);
            fprintf(stderr, "[type-res-stats] resolve_type_node: calls=%zu unique_nodes=%zu revisit_rate=%.1f%%\n",
                    g_resolve_type_node_calls, g_resolve_type_node_unique_nodes,
                    g_resolve_type_node_calls > 0
                        ? 100.0 * (double)(g_resolve_type_node_calls - g_resolve_type_node_unique_nodes)
                          / (double)g_resolve_type_node_calls
                        : 0.0);
            fprintf(stderr, "[type-res-stats] kind: TYPE_REF=%zu BUILTIN=%zu DECL=%zu ALIAS=%zu GENERIC_PARAM=%zu LOCAL_CONTRACT=%zu PROJECTION_PATH=%zu\n",
                    kind_counts[0], kind_counts[1], kind_counts[2],
                    kind_counts[3], kind_counts[4], kind_counts[5],
                    kind_counts[6]);

            /* Top 5 in-degree nodes */
            if (indeg != NULL && g->node_count > 0) {
                for (int rank = 0; rank < 5; rank++) {
                    size_t best = 0;
                    size_t best_val = 0;
                    bool found = false;
                    for (size_t i = 0; i < g->node_count; i++) {
                        if (indeg[i] > best_val) {
                            best = i; best_val = indeg[i]; found = true;
                        }
                    }
                    if (!found || best_val == 0) break;
                    fprintf(stderr, "[type-res-stats] top-indeg[%d] %s (in=%zu)\n",
                            rank,
                            g->nodes[best].label != NULL ? g->nodes[best].label : "<?>",
                            best_val);
                    indeg[best] = 0;
                }
            }
            free(indeg);
            free(outdeg);
            free(topo);
        }
    }

    free(topo_order);
    return !ctx->has_error;
}
