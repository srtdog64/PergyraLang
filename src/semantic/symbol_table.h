/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Symbol Table and scope management for semantic analysis
 */

#ifndef PERGYRA_SYMBOL_TABLE_H
#define PERGYRA_SYMBOL_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../semantic/type_system.h"

/* Forward declarations */
typedef struct Symbol  Symbol;
typedef struct Scope   Scope;

/*
 * Symbol kinds
 */
typedef enum
{
    SYMBOL_VARIABLE,    /* let x = ...              */
    SYMBOL_FUNCTION,    /* func Foo()               */
    SYMBOL_CLASS,       /* class Bar                */
    SYMBOL_TYPE_PARAM,  /* T, U (generic parameter) */
    SYMBOL_SLOT,        /* Slot<T> variable         */
    SYMBOL_TOKEN,       /* SecurityToken for SecureSlot */
    SYMBOL_ABILITY,     /* ability Foo              */
    SYMBOL_ROLE,        /* role Bar for Baz         */
    SYMBOL_PARTY,       /* party Baz                */
    SYMBOL_ROSTER,     /* roster Sys             */
    SYMBOL_WORLD,       /* world W                  */
    SYMBOL_INTENT,      /* intent Purchase          */
    SYMBOL_RELATION,    /* relation R               */
    SYMBOL_EFFECT,      /* effect E                 */
    SYMBOL_ZONE         /* zone Z                   */
} SymbolKind;

/*
 * Slot state machine
 *   UNCLAIMED -> CLAIMED -> RELEASED
 */
typedef enum
{
    SLOT_STATE_UNCLAIMED,
    SLOT_STATE_CLAIMED,
    SLOT_STATE_RELEASED
} SlotState;

#define PGY_SLOT_FLOW_ACCESS_READ    0x01u
#define PGY_SLOT_FLOW_ACCESS_WRITE   0x02u
#define PGY_SLOT_FLOW_ACCESS_RELEASE 0x04u

/*
 * Movable-resource compile-time state machine
 * Current concrete user: QubitSlot
 *   NONE          - not a qubit variable (default, backward compat)
 *   SUPERPOSITION - after ClaimQubit() or H()
 *   ENTANGLED     - after Entangle()
 *   COLLAPSED     - after Measure()
 *   CLASSICAL     - after IntoClassical(), qubit consumed
 */
typedef enum
{
    QUBIT_STATE_NONE,
    QUBIT_STATE_SUPERPOSITION,
    QUBIT_STATE_ENTANGLED,
    QUBIT_STATE_COLLAPSED,
    QUBIT_STATE_CLASSICAL
} QubitSemanticState;

/*
 * Symbol - one entry in the symbol table
 */
struct Symbol
{
    char*      name;
    SymbolKind kind;
    Type*      type;
    uint32_t   decl_line;
    uint32_t   decl_col;
    bool       is_used;
    bool       is_consumed;
    bool       embedded_in_world;
    uint8_t    slot_flow_access_mask;

    /* Slot-specific metadata */
    struct
    {
        SlotState state;
        bool      is_secure;
        char*     paired_token_name;  /* SecureSlot: name of its token */
        char*     paired_slot_name;   /* Token: name of its slot       */
        uint32_t  scope_depth;
    } slot_info;

    /* QubitSlot compile-time state tracking */
    struct
    {
        QubitSemanticState semantic_state;
        int32_t           entangle_pool_id;  /* -1 = no pool */
    } qubit_info;
};

/*
 * Scope kind
 */
typedef enum
{
    SCOPE_GLOBAL,
    SCOPE_FUNCTION,
    SCOPE_CLASS,
    SCOPE_BLOCK,
    SCOPE_WITH      /* with slot<T> as s { ... } */
} ScopeKind;

/*
 * Scope - one level in the scope chain
 */
struct Scope
{
    Scope*    parent;
    ScopeKind kind;
    uint32_t  depth;

    Symbol**  symbols;
    size_t    symbol_count;
    size_t    symbol_capacity;
    Symbol**  symbol_index;
    size_t    symbol_index_capacity;

    /*
     * Slots that were opened in this scope and must be released
     * before or at the end of the scope.
     * Used by with-block automatic release.
     */
    Symbol**  owned_slots;
    size_t    owned_slot_count;
    size_t    owned_slot_capacity;
};

/* -----------------------------------------------------------------
 * Scope operations
 * ----------------------------------------------------------------- */

Scope*   scope_create(Scope* parent, ScopeKind kind);
void     scope_destroy(Scope* scope);
void     scope_enter(Scope** current, ScopeKind kind);
void     scope_exit(Scope** current);

/* -----------------------------------------------------------------
 * Symbol registration
 * ----------------------------------------------------------------- */

/*
 * Returns false and sets error if a symbol with the same name
 * already exists in the CURRENT scope (shadowing across scopes
 * is allowed).
 */
bool     scope_declare(Scope* scope, Symbol* symbol);

Symbol*  scope_lookup(Scope* scope, const char* name);
Symbol*  scope_lookup_current(Scope* scope, const char* name);

/* -----------------------------------------------------------------
 * Slot helpers
 * ----------------------------------------------------------------- */

void     scope_register_slot(Scope* scope, Symbol* slot_sym);
void     scope_release_slot(Scope* scope, const char* slot_name);

/*
 * Called when exiting a with-block scope:
 * marks all owned slots as RELEASED.
 */
void     scope_auto_release_slots(Scope* scope);

/* -----------------------------------------------------------------
 * Symbol construction helpers
 * ----------------------------------------------------------------- */

Symbol* symbol_create_variable(const char* name, Type* type,
                                uint32_t line, uint32_t col);

Symbol* symbol_create_function(const char* name, Type* func_type,
                                uint32_t line, uint32_t col);

Symbol* symbol_create_slot(const char* name, Type* slot_type,
                            bool is_secure, const char* paired_token,
                            uint32_t line, uint32_t col);

Symbol* symbol_create_token(const char* name, const char* paired_slot,
                             uint32_t line, uint32_t col);
Symbol* symbol_create_view(const char* name, Type* view_type,
                           const char* source_slot,
                           uint32_t line, uint32_t col);

void    symbol_destroy(Symbol* sym);

#endif /* PERGYRA_SYMBOL_TABLE_H */
