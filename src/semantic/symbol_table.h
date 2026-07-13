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
#include "type_system.h"

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
    uint32_t   decl_syntax_id;
    bool       is_forward_placeholder;
    bool       is_used;
    bool       is_consumed;
    bool       is_parameter;
    ParamMode  param_mode;
    bool       embedded_in_world;
    uint8_t    slot_flow_access_mask;
    char*      reflect_target_name;  /* reflect(): type name for projection lets */

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

    /* `let mut` vs `let` for locals (parameters use param_mode instead). */
    bool is_mut_binding;

    /*
     * Disjoint slice-split provenance (docs/178 WO-DOP-1 rung 0).
     * Recorded only on immutable Slice bindings whose initializer is the
     * canonical split shape over one Array local:
     *   let lo = base.Slice(0, B);      -> lower half, [0, B)
     *   let hi = base.Slice(B, LEN);    -> upper half, [B, B+LEN)
     * [0,B) and [B,B+LEN) are disjoint for every B and LEN, so the fact
     * only needs base identity and the shared boundary (an immutable Int
     * local or an Int literal). Immutable-binding-only means a recorded
     * fact can never go stale through rebinding. Consumed as Disjointness
     * evidence by the parallel boundary check.
     */
    struct
    {
        bool      has_fact;
        bool      is_upper;
        Symbol*   base_sym;       /* Array<T> local the view was taken from */
        Symbol*   boundary_sym;   /* NULL when the boundary is a literal */
        long long boundary_lit;   /* valid when boundary_sym == NULL */
    } slice_split_info;

    /*
     * Memo for flow-snapshot tracking (docs/183 round 2). Branch/loop flow
     * snapshots classify every in-scope symbol per snapshot; the ownership
     * classification of a (symbol, type) pair is stable while the type
     * pointer is unchanged, so it is computed once per pointer. A type
     * re-resolution swaps the pointer and invalidates the memo naturally.
     * 0 = unset, 1 = not tracked, 2 = tracked.
     */
    const Type* flow_tracks_memo_type;
    uint8_t     flow_tracks_memo;
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

void symbol_mark_declaration(Symbol* symbol, uint32_t syntax_id,
                             bool is_forward_placeholder);
bool symbol_is_forward_declaration_for(const Symbol* symbol,
                                       SymbolKind expected_kind,
                                       uint32_t syntax_id);
void symbol_complete_forward_declaration(Symbol* symbol);

void    symbol_destroy(Symbol* sym);

#endif /* PERGYRA_SYMBOL_TABLE_H */
