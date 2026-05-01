/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Symbol Table implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "../common/string_compat.h"
#include "symbol_table.h"

#define INITIAL_SYMBOL_CAPACITY 16
#define INITIAL_SYMBOL_INDEX_CAPACITY 32
#define INITIAL_SLOT_CAPACITY   8

static bool
symbol_tracks_slot_state(const Symbol *sym)
{
    if (sym == NULL || sym->type == NULL)
        return false;

    if (sym->kind == SYMBOL_SLOT)
        return true;

    if (sym->type->kind == TYPE_KIND_CONSTRUCTED
        && sym->type->data.constructed.constructor != NULL
        && sym->type->data.constructed.constructor->name != NULL
        && strcmp(sym->type->data.constructed.constructor->name, "DeviceSlot") == 0)
        return true;

    return false;
}

static uint64_t
symbol_hash_name(const char *name)
{
    uint64_t hash = 1469598103934665603ull;

    if (name == NULL)
        return 0;

    while (*name != '\0') {
        hash ^= (unsigned char)*name++;
        hash *= 1099511628211ull;
    }
    return hash;
}

static bool
symbol_index_insert(Symbol **index, size_t capacity, Symbol *symbol)
{
    size_t mask;
    size_t slot;

    if (index == NULL || capacity == 0 || symbol == NULL || symbol->name == NULL)
        return false;

    mask = capacity - 1;
    slot = (size_t)symbol_hash_name(symbol->name) & mask;
    for (size_t probe = 0; probe < capacity; probe++) {
        Symbol *current = index[slot];
        if (current == NULL) {
            index[slot] = symbol;
            return true;
        }
        if (strcmp(current->name, symbol->name) == 0)
            return false;
        slot = (slot + 1) & mask;
    }
    return false;
}

static bool
scope_rebuild_symbol_index(Scope *scope, size_t new_capacity)
{
    Symbol **index;

    if (scope == NULL || new_capacity == 0)
        return false;

    index = calloc(new_capacity, sizeof(Symbol *));
    if (index == NULL)
        return false;

    for (size_t i = 0; i < scope->symbol_count; i++) {
        if (!symbol_index_insert(index, new_capacity, scope->symbols[i])) {
            free(index);
            return false;
        }
    }

    free(scope->symbol_index);
    scope->symbol_index = index;
    scope->symbol_index_capacity = new_capacity;
    return true;
}

static bool
scope_ensure_symbol_index_capacity(Scope *scope, size_t next_count)
{
    size_t new_capacity;

    if (scope == NULL)
        return false;

    if (scope->symbol_index_capacity != 0
        && next_count * 2 < scope->symbol_index_capacity) {
        return true;
    }

    new_capacity = scope->symbol_index_capacity == 0
        ? INITIAL_SYMBOL_INDEX_CAPACITY
        : scope->symbol_index_capacity * 2;
    while (next_count * 2 >= new_capacity)
        new_capacity *= 2;

    return scope_rebuild_symbol_index(scope, new_capacity);
}

static Symbol *
scope_lookup_current_linear(Scope *scope, const char *name)
{
    if (scope == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < scope->symbol_count; i++) {
        if (strcmp(scope->symbols[i]->name, name) == 0)
            return scope->symbols[i];
    }
    return NULL;
}

/* -----------------------------------------------------------------
 * Scope
 * ----------------------------------------------------------------- */

Scope *
scope_create(Scope *parent, ScopeKind kind)
{
    Scope *s = calloc(1, sizeof(Scope));
    if (s == NULL)
        return NULL;

    s->parent           = parent;
    s->kind             = kind;
    s->depth            = (parent != NULL) ? parent->depth + 1 : 0;
    s->symbol_capacity  = INITIAL_SYMBOL_CAPACITY;
    s->symbols          = calloc(INITIAL_SYMBOL_CAPACITY, sizeof(Symbol *));
    s->symbol_index_capacity = INITIAL_SYMBOL_INDEX_CAPACITY;
    s->symbol_index = calloc(INITIAL_SYMBOL_INDEX_CAPACITY, sizeof(Symbol *));
    s->owned_slot_capacity = INITIAL_SLOT_CAPACITY;
    s->owned_slots      = calloc(INITIAL_SLOT_CAPACITY, sizeof(Symbol *));

    if (s->symbols == NULL || s->symbol_index == NULL
        || s->owned_slots == NULL) {
        free(s->symbols);
        free(s->symbol_index);
        free(s->owned_slots);
        free(s);
        return NULL;
    }

    return s;
}

void
scope_destroy(Scope *scope)
{
    if (scope == NULL)
        return;

    for (size_t i = 0; i < scope->symbol_count; i++)
        symbol_destroy(scope->symbols[i]);

    free(scope->symbols);
    free(scope->symbol_index);
    free(scope->owned_slots); /* Symbols already freed above */
    free(scope);
}

void
scope_enter(Scope **current, ScopeKind kind)
{
    Scope *child = scope_create(*current, kind);
    if (child != NULL)
        *current = child;
}

void
scope_exit(Scope **current)
{
    if (*current == NULL || (*current)->parent == NULL)
        return;

    Scope *leaving = *current;
    *current = leaving->parent;
    scope_destroy(leaving);
}

/* -----------------------------------------------------------------
 * Symbol registration
 * ----------------------------------------------------------------- */

bool
scope_declare(Scope *scope, Symbol *symbol)
{
    if (scope == NULL || symbol == NULL || symbol->name == NULL)
        return false;

    if (scope_lookup_current(scope, symbol->name) != NULL)
        return false;

    /* Grow if needed */
    if (scope->symbol_count >= scope->symbol_capacity) {
        size_t new_cap = scope->symbol_capacity * 2;
        Symbol **grown = realloc(scope->symbols, new_cap * sizeof(Symbol *));
        if (grown == NULL)
            return false;
        scope->symbols          = grown;
        scope->symbol_capacity  = new_cap;
    }

    if (!scope_ensure_symbol_index_capacity(scope, scope->symbol_count + 1))
        return false;

    if (!symbol_index_insert(scope->symbol_index,
                             scope->symbol_index_capacity,
                             symbol)) {
        return false;
    }

    scope->symbols[scope->symbol_count++] = symbol;
    return true;
}

Symbol *
scope_lookup(Scope *scope, const char *name)
{
    Scope *cur = scope;
    while (cur != NULL) {
        Symbol *sym = scope_lookup_current(cur, name);
        if (sym != NULL)
            return sym;
        cur = cur->parent;
    }
    return NULL;
}

Symbol *
scope_lookup_current(Scope *scope, const char *name)
{
    size_t mask;
    size_t slot;

    if (scope == NULL || name == NULL)
        return NULL;

    if (scope->symbol_index == NULL || scope->symbol_index_capacity == 0)
        return scope_lookup_current_linear(scope, name);

    mask = scope->symbol_index_capacity - 1;
    slot = (size_t)symbol_hash_name(name) & mask;
    for (size_t probe = 0; probe < scope->symbol_index_capacity; probe++) {
        Symbol *sym = scope->symbol_index[slot];
        if (sym == NULL)
            return NULL;
        if (strcmp(sym->name, name) == 0)
            return sym;
        slot = (slot + 1) & mask;
    }
    return NULL;
}

/* -----------------------------------------------------------------
 * Slot helpers
 * ----------------------------------------------------------------- */

void
scope_register_slot(Scope *scope, Symbol *slot_sym)
{
    if (scope->owned_slot_count >= scope->owned_slot_capacity) {
        size_t new_cap = scope->owned_slot_capacity * 2;
        Symbol **grown = realloc(scope->owned_slots,
                                 new_cap * sizeof(Symbol *));
        if (grown == NULL)
            return;
        scope->owned_slots         = grown;
        scope->owned_slot_capacity = new_cap;
    }
    scope->owned_slots[scope->owned_slot_count++] = slot_sym;
}

void
scope_release_slot(Scope *scope, const char *slot_name)
{
    Scope *cur = scope;
    while (cur != NULL) {
        Symbol *sym = scope_lookup_current(cur, slot_name);
        if (symbol_tracks_slot_state(sym)) {
            sym->slot_info.state = SLOT_STATE_RELEASED;
            return;
        }
        cur = cur->parent;
    }
}

void
scope_auto_release_slots(Scope *scope)
{
    for (size_t i = 0; i < scope->owned_slot_count; i++) {
        Symbol *sym = scope->owned_slots[i];
        if (sym->slot_info.state == SLOT_STATE_CLAIMED)
            sym->slot_info.state = SLOT_STATE_RELEASED;
    }
}

/* -----------------------------------------------------------------
 * Symbol construction helpers
 * ----------------------------------------------------------------- */

Symbol *
symbol_create_variable(const char *name, Type *type,
                        uint32_t line, uint32_t col)
{
    Symbol *sym = calloc(1, sizeof(Symbol));
    if (sym == NULL)
        return NULL;

    sym->name      = pergyra_strdup(name);
    sym->kind      = SYMBOL_VARIABLE;
    sym->type      = type;
    sym->decl_line = line;
    sym->decl_col  = col;
    sym->qubit_info.entangle_pool_id = -1;
    return sym;
}

Symbol *
symbol_create_function(const char *name, Type *func_type,
                        uint32_t line, uint32_t col)
{
    Symbol *sym = calloc(1, sizeof(Symbol));
    if (sym == NULL)
        return NULL;

    sym->name      = pergyra_strdup(name);
    sym->kind      = SYMBOL_FUNCTION;
    sym->type      = func_type;
    sym->decl_line = line;
    sym->decl_col  = col;
    return sym;
}

Symbol *
symbol_create_slot(const char *name, Type *slot_type,
                    bool is_secure, const char *paired_token,
                    uint32_t line, uint32_t col)
{
    Symbol *sym = calloc(1, sizeof(Symbol));
    if (sym == NULL)
        return NULL;

    sym->name      = pergyra_strdup(name);
    sym->kind      = SYMBOL_SLOT;
    sym->type      = slot_type;
    sym->decl_line = line;
    sym->decl_col  = col;

    sym->slot_info.state             = SLOT_STATE_CLAIMED;
    sym->slot_info.is_secure         = is_secure;
    sym->slot_info.paired_token_name = paired_token
                                       ? pergyra_strdup(paired_token)
                                       : NULL;
    return sym;
}

Symbol *
symbol_create_token(const char *name, const char *paired_slot,
                     uint32_t line, uint32_t col)
{
    Symbol *sym = calloc(1, sizeof(Symbol));
    if (sym == NULL)
        return NULL;

    sym->name      = pergyra_strdup(name);
    sym->kind      = SYMBOL_TOKEN;
    sym->decl_line = line;
    sym->decl_col  = col;

    sym->slot_info.paired_slot_name = paired_slot
                                      ? pergyra_strdup(paired_slot)
                                      : NULL;
    return sym;
}

Symbol *
symbol_create_view(const char *name, Type *view_type,
                   const char *source_slot,
                   uint32_t line, uint32_t col)
{
    Symbol *sym = symbol_create_variable(name, view_type, line, col);
    if (sym == NULL)
        return NULL;

    sym->slot_info.paired_slot_name = source_slot != NULL
        ? pergyra_strdup(source_slot)
        : NULL;
    return sym;
}

void
symbol_destroy(Symbol *sym)
{
    if (sym == NULL)
        return;

    free(sym->name);
    free(sym->slot_info.paired_token_name);
    free(sym->slot_info.paired_slot_name);
    /* Type is owned by type_system, not freed here */
    free(sym);
}
