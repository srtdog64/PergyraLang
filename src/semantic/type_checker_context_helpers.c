#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "diag_payload.h"
#include "type_checker_internal.h"
#include "type_checker_flow_loop_summary.h"
#include "type_checker_flow_universe.h"

#define INITIAL_DIAG_CAPACITY 16

char *
tc_strdup_fmt(const char *fmt, ...)
{
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) { va_end(ap2); return NULL; }
    char *buf = malloc((size_t)len + 1);
    if (buf != NULL) vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static size_t
semantic_ctx_embedded_world_zone_index(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return (size_t)-1;
    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++) {
        if (ctx->embedded_world_zone_names[i] != NULL
            && strcmp(ctx->embedded_world_zone_names[i], name) == 0) {
            return i;
        }
    }
    return (size_t)-1;
}

__attribute__((unused))
static bool
semantic_ctx_has_embedded_world_zone_name(SemanticContext *ctx, const char *name)
{
    return semantic_ctx_embedded_world_zone_index(ctx, name) != (size_t)-1;
}

__attribute__((unused))
static const char *
semantic_ctx_embedded_world_zone_world_name(SemanticContext *ctx, const char *name)
{
    size_t index = semantic_ctx_embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_world_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_world_names[index];
}

__attribute__((unused))
static const char *
semantic_ctx_embedded_world_zone_slot_name(SemanticContext *ctx, const char *name)
{
    size_t index = semantic_ctx_embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_slot_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_slot_names[index];
}

void
semantic_ctx_mark_embedded_world_zone_name(SemanticContext *ctx,
                                           const char *name,
                                           const char *world_name,
                                           const char *slot_name)
{
    size_t index;
    char *owned_name;
    char *owned_world_name = NULL;
    char *owned_slot_name = NULL;

    if (ctx == NULL || name == NULL || *name == '\0')
        return;
    index = semantic_ctx_embedded_world_zone_index(ctx, name);
    if (index != (size_t)-1) {
        if (world_name != NULL && *world_name != '\0'
            && ctx->embedded_world_zone_world_names[index] == NULL) {
            ctx->embedded_world_zone_world_names[index] = pergyra_strdup(world_name);
        }
        if (slot_name != NULL && *slot_name != '\0'
            && ctx->embedded_world_zone_slot_names[index] == NULL) {
            ctx->embedded_world_zone_slot_names[index] = pergyra_strdup(slot_name);
        }
        return;
    }

    if (ctx->embedded_world_zone_count >= ctx->embedded_world_zone_capacity) {
        size_t new_cap = ctx->embedded_world_zone_capacity == 0
            ? 8
            : ctx->embedded_world_zone_capacity * 2;
        char **grown_names;
        char **grown_world_names;
        char **grown_slot_names;
        if (new_cap <= ctx->embedded_world_zone_capacity
            || new_cap > (size_t)-1 / sizeof(char *))
            return;
        grown_names = calloc(new_cap, sizeof(char *));
        if (grown_names == NULL)
            return;
        grown_world_names = calloc(new_cap, sizeof(char *));
        if (grown_world_names == NULL) {
            free(grown_names);
            return;
        }
        grown_slot_names = calloc(new_cap, sizeof(char *));
        if (grown_slot_names == NULL) {
            free(grown_names);
            free(grown_world_names);
            return;
        }
        if (ctx->embedded_world_zone_count > 0) {
            memcpy(grown_names, ctx->embedded_world_zone_names,
                   ctx->embedded_world_zone_count * sizeof(char *));
            memcpy(grown_world_names, ctx->embedded_world_zone_world_names,
                   ctx->embedded_world_zone_count * sizeof(char *));
            memcpy(grown_slot_names, ctx->embedded_world_zone_slot_names,
                   ctx->embedded_world_zone_count * sizeof(char *));
        }
        free(ctx->embedded_world_zone_names);
        free(ctx->embedded_world_zone_world_names);
        free(ctx->embedded_world_zone_slot_names);
        ctx->embedded_world_zone_names = grown_names;
        ctx->embedded_world_zone_world_names = grown_world_names;
        ctx->embedded_world_zone_slot_names = grown_slot_names;
        ctx->embedded_world_zone_capacity = new_cap;
    }

    owned_name = pergyra_strdup(name);
    if (owned_name == NULL)
        return;
    if (world_name != NULL && *world_name != '\0') {
        owned_world_name = pergyra_strdup(world_name);
        if (owned_world_name == NULL) {
            free(owned_name);
            return;
        }
    }
    if (slot_name != NULL && *slot_name != '\0') {
        owned_slot_name = pergyra_strdup(slot_name);
        if (owned_slot_name == NULL) {
            free(owned_name);
            free(owned_world_name);
            return;
        }
    }
    ctx->embedded_world_zone_names[ctx->embedded_world_zone_count] =
        owned_name;
    ctx->embedded_world_zone_world_names[ctx->embedded_world_zone_count] =
        owned_world_name;
    ctx->embedded_world_zone_slot_names[ctx->embedded_world_zone_count] =
        owned_slot_name;
    ctx->embedded_world_zone_count++;
}

/* -----------------------------------------------------------------
 * Context lifecycle
 * ----------------------------------------------------------------- */

SemanticContext *
semantic_context_create(void)
{
    SemanticContext *ctx = calloc(1, sizeof(SemanticContext));
    if (ctx == NULL)
        return NULL;

    ctx->scope               = scope_create(NULL, SCOPE_GLOBAL);
    ctx->diagnostic_capacity = INITIAL_DIAG_CAPACITY;
    ctx->diagnostics         = calloc(INITIAL_DIAG_CAPACITY,
                                      sizeof(Diagnostic *));
    pgy_arena_init(&ctx->scratch_arena, 0);
    if (ctx->scope == NULL || ctx->diagnostics == NULL) {
        scope_destroy(ctx->scope);
        free(ctx->diagnostics);
        pgy_arena_destroy(&ctx->scratch_arena);
        free(ctx);
        return NULL;
    }

    /* Register built-in types in global scope */
    type_system_init();

    return ctx;
}

void
semantic_context_destroy(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;

    loop_flow_summary_end_function(ctx);
    resource_flow_universe_end(ctx);
    function_param_flow_summary_store_destroy(ctx);
    scope_destroy(ctx->scope);

    for (size_t i = 0; i < ctx->diagnostic_count; i++) {
        free(ctx->diagnostics[i]->message);
        diag_payload_snapshot_destroy(ctx->diagnostics[i]->payload);
        free(ctx->diagnostics[i]);
    }
    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++)
        free(ctx->embedded_world_zone_names[i]);
    free(ctx->embedded_world_zone_names);
    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++)
        free(ctx->embedded_world_zone_world_names[i]);
    free(ctx->embedded_world_zone_world_names);
    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++)
        free(ctx->embedded_world_zone_slot_names[i]);
    free(ctx->embedded_world_zone_slot_names);
    for (size_t i = 0; i < ctx->stdlib_use_module_count; i++)
        free(ctx->stdlib_use_module_names[i]);
    free(ctx->stdlib_use_module_names);
    pgy_resource_flow_facts_destroy(
        ctx->resource_flow_facts,
        ctx->resource_flow_fact_count);
    pgy_function_param_flow_facts_destroy(
        ctx->function_param_flow_facts);
    free(ctx->loop_flow_summary_facts);
    free(ctx->loop_flow_state_facts);
    pgy_iteration_type_facts_destroy(
        ctx->iteration_type_facts,
        ctx->iteration_type_fact_count);
    for (size_t i = 0; i < ctx->type_resolution_stage_alias_diagnostic_name_count; i++)
        free(ctx->type_resolution_stage_alias_diagnostic_names[i]);
    free(ctx->type_resolution_stage_alias_diagnostic_names);
    for (size_t i = 0; i < ctx->type_resolution_graph.node_count; i++)
        free(ctx->type_resolution_graph.nodes[i].label);
    free(ctx->type_resolution_graph.nodes);
    for (size_t i = 0; i < ctx->type_resolution_graph.edge_count; i++)
        free(ctx->type_resolution_graph.edges[i].reason);
    free(ctx->type_resolution_graph.edges);
    semantic_type_resolution_free_metadata(ctx);
    free(ctx->host_decl_index.decls);
    free(ctx->host_decl_index.names);
    free(ctx->host_decl_index.types);
    free(ctx->host_decl_index.hash);
    semantic_parallel_capture_facts_clear(
        ctx->parallel_capture_boundaries,
        ctx->parallel_capture_boundary_count);
    free(ctx->diagnostics);
    pgy_arena_destroy(&ctx->scratch_arena);
    free(ctx);
}

/* -----------------------------------------------------------------
 * Utility — resolve AST type node to Type*
 * ----------------------------------------------------------------- */
