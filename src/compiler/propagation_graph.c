/*
 * Copyright (c) 2026 Pergyra Language Project
 * Transitive frontier propagation dependency graph and schedule.
 */
#include "propagation_graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PropagationGraph *
propagation_graph_create(void)
{
    return calloc(1, sizeof(PropagationGraph));
}

void
propagation_graph_destroy(PropagationGraph *g)
{
    if (g == NULL)
        return;
    for (size_t i = 0; i < g->node_count; i++)
        free(g->node_names[i]);
    free(g->node_names);
    free(g->edges);
    free(g->order);
    free(g->scc_of);
    free(g);
}

size_t
propagation_graph_intern_node(PropagationGraph *g, const char *name)
{
    char *copy;
    char **grown;

    if (g == NULL || name == NULL)
        return (size_t)-1;
    for (size_t i = 0; i < g->node_count; i++) {
        if (strcmp(g->node_names[i], name) == 0)
            return i;
    }
    grown = realloc(g->node_names, (g->node_count + 1) * sizeof(char *));
    if (grown == NULL)
        return (size_t)-1;
    g->node_names = grown;
    copy = malloc(strlen(name) + 1);
    if (copy == NULL)
        return (size_t)-1;
    strcpy(copy, name);
    g->node_names[g->node_count] = copy;
    return g->node_count++;
}

bool
propagation_graph_add_edge(PropagationGraph *g, size_t from, size_t to)
{
    PropEdge *grown;

    if (g == NULL || from >= g->node_count || to >= g->node_count)
        return false;
    if (from == to)
        return true; /* self-refresh is not a transitive dependency */
    for (size_t i = 0; i < g->edge_count; i++) {
        if (g->edges[i].from == from && g->edges[i].to == to)
            return true; /* dedup */
    }
    if (g->edge_count == g->edge_capacity) {
        size_t cap = g->edge_capacity == 0 ? 8 : g->edge_capacity * 2;
        grown = realloc(g->edges, cap * sizeof(PropEdge));
        if (grown == NULL)
            return false;
        g->edges = grown;
        g->edge_capacity = cap;
    }
    g->edges[g->edge_count].from = from;
    g->edges[g->edge_count].to = to;
    g->edge_count++;
    return true;
}

/* Iterative DFS post-order over the forward graph. */
static bool
prop_forward_finish_order(const PropagationGraph *g, size_t *finish,
                          size_t *finish_count)
{
    size_t n = g->node_count;
    bool *seen = calloc(n, sizeof(bool));
    size_t *stack = malloc(n * sizeof(size_t));
    size_t *iter = calloc(n, sizeof(size_t));
    size_t **adj = NULL;
    size_t *adj_len = calloc(n, sizeof(size_t));
    bool ok = false;

    if (seen == NULL || stack == NULL || iter == NULL || adj_len == NULL)
        goto done;
    adj = calloc(n, sizeof(size_t *));
    if (adj == NULL)
        goto done;
    for (size_t e = 0; e < g->edge_count; e++)
        adj_len[g->edges[e].from]++;
    for (size_t i = 0; i < n; i++) {
        adj[i] = adj_len[i] ? malloc(adj_len[i] * sizeof(size_t)) : NULL;
        adj_len[i] = 0;
    }
    for (size_t e = 0; e < g->edge_count; e++) {
        size_t u = g->edges[e].from;
        adj[u][adj_len[u]++] = g->edges[e].to;
    }
    *finish_count = 0;
    for (size_t s = 0; s < n; s++) {
        if (seen[s])
            continue;
        size_t top = 0;
        stack[top] = s;
        seen[s] = true;
        iter[s] = 0;
        while (top != (size_t)-1) {
            size_t u = stack[top];
            if (iter[u] < adj_len[u]) {
                size_t v = adj[u][iter[u]++];
                if (!seen[v]) {
                    seen[v] = true;
                    iter[v] = 0;
                    stack[++top] = v;
                }
            } else {
                finish[(*finish_count)++] = u;
                top = top == 0 ? (size_t)-1 : top - 1;
            }
        }
    }
    ok = true;
done:
    if (adj != NULL)
        for (size_t i = 0; i < n; i++)
            free(adj[i]);
    free(adj);
    free(adj_len);
    free(iter);
    free(stack);
    free(seen);
    return ok;
}

/* Assign SCC ids via DFS on the transpose in reverse finish order. */
static bool
prop_assign_sccs(PropagationGraph *g, const size_t *finish)
{
    size_t n = g->node_count;
    size_t *radj_len = calloc(n, sizeof(size_t));
    size_t **radj = calloc(n, sizeof(size_t *));
    size_t *stack = malloc((n ? n : 1) * sizeof(size_t));
    bool ok = false;

    if (radj_len == NULL || radj == NULL || stack == NULL)
        goto done;
    for (size_t e = 0; e < g->edge_count; e++)
        radj_len[g->edges[e].to]++;
    for (size_t i = 0; i < n; i++) {
        radj[i] = radj_len[i] ? malloc(radj_len[i] * sizeof(size_t)) : NULL;
        radj_len[i] = 0;
    }
    for (size_t e = 0; e < g->edge_count; e++) {
        size_t v = g->edges[e].to;
        radj[v][radj_len[v]++] = g->edges[e].from;
    }
    for (size_t i = 0; i < n; i++)
        g->scc_of[i] = (size_t)-1;
    g->scc_count = 0;
    for (size_t idx = n; idx-- > 0;) {
        size_t root = finish[idx];
        if (g->scc_of[root] != (size_t)-1)
            continue;
        size_t top = 0, size = 0;
        stack[top] = root;
        g->scc_of[root] = g->scc_count;
        while (top != (size_t)-1) {
            size_t u = stack[top];
            top = top == 0 ? (size_t)-1 : top - 1;
            size++;
            for (size_t k = 0; k < radj_len[u]; k++) {
                size_t w = radj[u][k];
                if (g->scc_of[w] == (size_t)-1) {
                    g->scc_of[w] = g->scc_count;
                    stack[++top] = w;
                }
            }
        }
        if (size > g->max_scc_size)
            g->max_scc_size = size;
        g->scc_count++;
    }
    ok = true;
done:
    if (radj != NULL)
        for (size_t i = 0; i < n; i++)
            free(radj[i]);
    free(radj);
    free(radj_len);
    free(stack);
    return ok;
}

/* Condensation topo order + SCC-size-weighted longest path -> pass limit. */
static bool
prop_condense_and_bound(PropagationGraph *g)
{
    size_t c = g->scc_count;
    size_t *indeg = calloc(c, sizeof(size_t));
    size_t *scc_size = calloc(c, sizeof(size_t));
    size_t *longest = calloc(c, sizeof(size_t)); /* weighted longest path end */
    size_t *depth = calloc(c, sizeof(size_t));   /* #SCCs in longest path */
    size_t *queue = malloc((c ? c : 1) * sizeof(size_t));
    size_t *topo = malloc((c ? c : 1) * sizeof(size_t));
    bool ok = false;

    if (indeg == NULL || scc_size == NULL || longest == NULL || depth == NULL
        || queue == NULL || topo == NULL)
        goto done;
    for (size_t i = 0; i < g->node_count; i++)
        scc_size[g->scc_of[i]]++;
    for (size_t e = 0; e < g->edge_count; e++) {
        size_t a = g->scc_of[g->edges[e].from];
        size_t b = g->scc_of[g->edges[e].to];
        if (a != b)
            indeg[b]++;
    }
    size_t qh = 0, qt = 0, topo_n = 0;
    for (size_t s = 0; s < c; s++) {
        longest[s] = scc_size[s];
        depth[s] = 1;
        if (indeg[s] == 0)
            queue[qt++] = s;
    }
    while (qh < qt) {
        size_t u = queue[qh++];
        topo[topo_n++] = u;
        for (size_t e = 0; e < g->edge_count; e++) {
            size_t a = g->scc_of[g->edges[e].from];
            size_t b = g->scc_of[g->edges[e].to];
            if (a != u || b == u)
                continue;
            if (longest[u] + scc_size[b] > longest[b]) {
                longest[b] = longest[u] + scc_size[b];
                depth[b] = depth[u] + 1;
            }
            if (--indeg[b] == 0)
                queue[qt++] = b;
        }
    }
    g->pass_limit = 0;
    g->chain_depth = 0;
    for (size_t s = 0; s < c; s++) {
        if (longest[s] > g->pass_limit)
            g->pass_limit = longest[s];
        if (depth[s] > g->chain_depth)
            g->chain_depth = depth[s];
    }
    /* Expand condensation topo order to a node propagation order. */
    g->order_count = 0;
    for (size_t t = 0; t < topo_n; t++) {
        size_t scc = topo[t];
        for (size_t i = 0; i < g->node_count; i++)
            if (g->scc_of[i] == scc)
                g->order[g->order_count++] = i;
    }
    ok = (topo_n == c); /* sanity: condensation is a DAG */
done:
    free(indeg); free(scc_size); free(longest); free(depth);
    free(queue); free(topo);
    return ok;
}

bool
propagation_graph_schedule(PropagationGraph *g)
{
    size_t *finish = NULL;
    size_t finish_count = 0;
    bool ok = false;

    if (g == NULL)
        return false;
    if (g->node_count == 0) {
        g->pass_limit = 0;
        g->chain_depth = 0;
        g->has_cycle = false;
        return true;
    }
    free(g->order);
    free(g->scc_of);
    g->order = malloc(g->node_count * sizeof(size_t));
    g->scc_of = malloc(g->node_count * sizeof(size_t));
    finish = malloc(g->node_count * sizeof(size_t));
    if (g->order == NULL || g->scc_of == NULL || finish == NULL)
        goto done;
    g->max_scc_size = 0;
    if (!prop_forward_finish_order(g, finish, &finish_count))
        goto done;
    if (!prop_assign_sccs(g, finish))
        goto done;
    if (!prop_condense_and_bound(g))
        goto done;
    g->has_cycle = g->max_scc_size > 1;
    ok = true;
done:
    free(finish);
    return ok;
}

void
propagation_graph_dump(const PropagationGraph *g, void *out_file,
                       const char *label)
{
    FILE *out = (FILE *)out_file;

    if (g == NULL || out == NULL)
        return;
    fprintf(out,
        "[propagation-graph] %s: nodes=%zu edges=%zu %s depth=%zu pass_limit=%zu\n",
        label != NULL ? label : "(anon)", g->node_count, g->edge_count,
        g->has_cycle ? "cyclic" : "acyclic", g->chain_depth, g->pass_limit);
    if (g->order_count > 0) {
        fprintf(out, "  propagation order:");
        for (size_t i = 0; i < g->order_count; i++)
            fprintf(out, " %s", g->node_names[g->order[i]]);
        fprintf(out, "\n");
    }
    for (size_t e = 0; e < g->edge_count; e++) {
        fprintf(out, "  dep: %s <- %s\n",
            g->node_names[g->edges[e].to], g->node_names[g->edges[e].from]);
    }
    if (g->has_cycle) {
        for (size_t scc = 0; scc < g->scc_count; scc++) {
            size_t members = 0;
            for (size_t i = 0; i < g->node_count; i++)
                if (g->scc_of[i] == scc)
                    members++;
            if (members <= 1)
                continue;
            fprintf(out, "  cycle (fixpoint cluster):");
            for (size_t i = 0; i < g->node_count; i++)
                if (g->scc_of[i] == scc)
                    fprintf(out, " %s", g->node_names[i]);
            fprintf(out, "\n");
        }
    }
}
