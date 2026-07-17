/*
 * Function-local resource-flow identity owner.
 *
 * Flow re-entry destroys and recreates block scopes, so Symbol * is not a
 * semantic identity.  This universe gives each declaration identity one
 * stable index for the duration of a function analysis.  Scope remains the
 * sole owner of live Symbol allocations; this owner resolves a stable index
 * through the current scope chain instead of caching a borrowed pointer.
 */

#include <stdlib.h>
#include <string.h>

#include "type_checker_flow_universe.h"

typedef struct
{
    uint32_t syntax_id;
    uint32_t line;
    uint32_t column;
    SymbolKind kind;
    char *name;
    bool is_parameter;
    size_t parameter_index;
} ResourceFlowUniverseEntry;

struct ResourceFlowUniverse
{
    ResourceFlowUniverseEntry *entries;
    size_t count;
    size_t capacity;
};

static bool
resource_flow_universe_tracks_type(const Type *type)
{
    return type_is_resource_handle(type)
        || type_is_constructed_named(type, "Future")
        || type_is_constructed_named(type, "RemoteFuture");
}

static void
resource_flow_universe_destroy(ResourceFlowUniverse *universe)
{
    if (universe == NULL)
        return;
    for (size_t i = 0; i < universe->count; i++)
        free(universe->entries[i].name);
    free(universe->entries);
    free(universe);
}

void
resource_flow_universe_begin(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;
    resource_flow_universe_destroy(ctx->resource_flow_universe);
    ctx->resource_flow_universe = calloc(1, sizeof(ResourceFlowUniverse));
    ctx->resource_flow_epoch++;
    if (ctx->resource_flow_epoch == 0)
        ctx->resource_flow_epoch = 1;
}

void
resource_flow_universe_end(SemanticContext *ctx)
{
    if (ctx == NULL)
        return;
    resource_flow_universe_destroy(ctx->resource_flow_universe);
    ctx->resource_flow_universe = NULL;
}

static bool
resource_flow_entry_matches(const ResourceFlowUniverseEntry *entry,
                            const Symbol *symbol)
{
    if (entry == NULL || symbol == NULL || entry->kind != symbol->kind)
        return false;
    if (entry->syntax_id != 0 || symbol->decl_syntax_id != 0) {
        if (entry->syntax_id == 0
            || entry->syntax_id != symbol->decl_syntax_id)
            return false;
    } else if (entry->line != symbol->decl_line
               || entry->column != symbol->decl_col) {
        return false;
    }
    if (entry->name == NULL || symbol->name == NULL)
        return entry->name == symbol->name;
    return strcmp(entry->name, symbol->name) == 0;
}

size_t
resource_flow_universe_bind(SemanticContext *ctx, Symbol *symbol)
{
    ResourceFlowUniverse *universe;
    ResourceFlowUniverseEntry *entry;
    size_t index;

    if (ctx == NULL || symbol == NULL)
        return RESOURCE_FLOW_INDEX_NONE;
    universe = ctx->resource_flow_universe;
    if (universe == NULL)
        return RESOURCE_FLOW_INDEX_NONE;

    if (symbol->flow_universe_epoch == ctx->resource_flow_epoch
        && symbol->flow_universe_index < universe->count) {
        return symbol->flow_universe_index;
    }

    for (index = 0; index < universe->count; index++) {
        if (resource_flow_entry_matches(&universe->entries[index], symbol)) {
            symbol->flow_universe_epoch = ctx->resource_flow_epoch;
            symbol->flow_universe_index = index;
            return index;
        }
    }

    if (universe->count == universe->capacity) {
        size_t next = universe->capacity == 0 ? 16 : universe->capacity * 2;
        ResourceFlowUniverseEntry *grown;
        if (next <= universe->capacity
            || next > SIZE_MAX / sizeof(ResourceFlowUniverseEntry))
            return RESOURCE_FLOW_INDEX_NONE;
        grown = realloc(universe->entries,
                        next * sizeof(ResourceFlowUniverseEntry));
        if (grown == NULL)
            return RESOURCE_FLOW_INDEX_NONE;
        memset(grown + universe->capacity, 0,
               (next - universe->capacity) * sizeof(ResourceFlowUniverseEntry));
        universe->entries = grown;
        universe->capacity = next;
    }

    index = universe->count++;
    entry = &universe->entries[index];
    entry->syntax_id = symbol->decl_syntax_id;
    entry->line = symbol->decl_line;
    entry->column = symbol->decl_col;
    entry->kind = symbol->kind;
    if (symbol->name != NULL) {
        size_t length = strlen(symbol->name) + 1;
        entry->name = malloc(length);
        if (entry->name == NULL) {
            universe->count--;
            memset(entry, 0, sizeof(*entry));
            return RESOURCE_FLOW_INDEX_NONE;
        }
        memcpy(entry->name, symbol->name, length);
    }
    entry->is_parameter = symbol->is_parameter;
    if (entry->is_parameter) {
        for (size_t prior = 0; prior < index; prior++) {
            if (universe->entries[prior].is_parameter)
                entry->parameter_index++;
        }
    }
    symbol->flow_universe_epoch = ctx->resource_flow_epoch;
    symbol->flow_universe_index = index;
    return index;
}

Symbol *
resource_flow_universe_symbol(SemanticContext *ctx, size_t index)
{
    ResourceFlowUniverse *universe =
        ctx != NULL ? ctx->resource_flow_universe : NULL;
    if (universe == NULL || index >= universe->count)
        return NULL;
    ResourceFlowUniverseEntry *entry = &universe->entries[index];
    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent) {
        Symbol *symbol = entry->name != NULL
            ? scope_lookup_current(scope, entry->name)
            : NULL;
        if (resource_flow_entry_matches(entry, symbol))
            return symbol;
    }
    return NULL;
}

void
pgy_resource_flow_facts_destroy(PgyResourceFlowFact *facts, size_t count)
{
    if (facts == NULL)
        return;
    for (size_t i = 0; i < count; i++)
        free(facts[i].name);
    free(facts);
}

static bool
resource_flow_facts_reserve(SemanticContext *ctx, size_t minimum)
{
    PgyResourceFlowFact *grown;
    size_t capacity;

    if (ctx == NULL)
        return false;
    if (minimum <= ctx->resource_flow_fact_capacity)
        return true;
    capacity = ctx->resource_flow_fact_capacity == 0
        ? 16 : ctx->resource_flow_fact_capacity;
    while (capacity < minimum) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*grown))
        return false;
    grown = realloc(ctx->resource_flow_facts,
                    capacity * sizeof(*grown));
    if (grown == NULL)
        return false;
    memset(grown + ctx->resource_flow_fact_capacity, 0,
           (capacity - ctx->resource_flow_fact_capacity) * sizeof(*grown));
    ctx->resource_flow_facts = grown;
    ctx->resource_flow_fact_capacity = capacity;
    return true;
}

bool
resource_flow_universe_capture_function_facts(
    SemanticContext *ctx,
    uint32_t function_syntax_id)
{
    ResourceFlowUniverse *universe;

    if (ctx == NULL)
        return false;
    /* Unit-level semantic callers may build ASTs without merged-program
     * SyntaxNodeIds. Preserve their diagnostic contract; the driver-owned HIR
     * path only consumes snapshots for source nodes with a stable ID. */
    if (function_syntax_id == 0)
        return true;
    universe = ctx->resource_flow_universe;
    if (universe == NULL)
        return false;
    if (ctx->scope != NULL) {
        for (Scope *scope = ctx->scope;
             scope != NULL;
             scope = scope->parent) {
            for (size_t j = 0; j < scope->symbol_count; j++) {
                Symbol *symbol = scope->symbols[j];
                if (symbol == NULL || symbol->type == NULL
                    || !resource_flow_universe_tracks_type(symbol->type))
                    continue;
                if (resource_flow_universe_bind(ctx, symbol)
                    == RESOURCE_FLOW_INDEX_NONE)
                    return false;
            }
        }
    }

    for (size_t i = 0; i < universe->count; i++) {
        ResourceFlowUniverseEntry *entry = &universe->entries[i];
        PgyResourceFlowFact *fact;

        if (!resource_flow_facts_reserve(
                ctx, ctx->resource_flow_fact_count + 1))
            return false;
        fact = &ctx->resource_flow_facts[ctx->resource_flow_fact_count];
        memset(fact, 0, sizeof(*fact));
        fact->function_syntax_id = function_syntax_id;
        fact->stable_index = i;
        fact->declaration_syntax_id = entry->syntax_id;
        fact->line = entry->line;
        fact->column = entry->column;
        fact->symbol_kind = (uint32_t)entry->kind;
        fact->is_parameter = entry->is_parameter;
        fact->parameter_index = entry->parameter_index;
        if (entry->name != NULL) {
            size_t length = strlen(entry->name) + 1;
            fact->name = malloc(length);
            if (fact->name == NULL)
                return false;
            memcpy(fact->name, entry->name, length);
        }
        ctx->resource_flow_fact_count++;
    }
    return true;
}
