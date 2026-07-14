/*
 * Function-local resource-flow identity owner.
 *
 * Flow re-entry destroys and recreates block scopes, so Symbol * is not a
 * semantic identity.  This universe gives each declaration identity one
 * stable index for the duration of a function analysis and rebinds that index
 * to the currently live Symbol allocation.
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
    Symbol *current_symbol;
} ResourceFlowUniverseEntry;

struct ResourceFlowUniverse
{
    ResourceFlowUniverseEntry *entries;
    size_t count;
    size_t capacity;
};

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
        universe->entries[symbol->flow_universe_index].current_symbol = symbol;
        return symbol->flow_universe_index;
    }

    for (index = 0; index < universe->count; index++) {
        if (resource_flow_entry_matches(&universe->entries[index], symbol)) {
            universe->entries[index].current_symbol = symbol;
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
    entry->current_symbol = symbol;
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
    return universe->entries[index].current_symbol;
}
