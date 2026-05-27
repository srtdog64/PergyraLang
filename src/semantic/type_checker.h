/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker — expression type inference and rule validation
 */

#ifndef PERGYRA_TYPE_CHECKER_H
#define PERGYRA_TYPE_CHECKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../parser/ast.h"
#include "../common/arena.h"
#include "diagnostic_types.h"
#include "type_system.h"
#include "symbol_table.h"
#include "builtin_kind.h"

#if defined(__GNUC__) || defined(__clang__)
#define PGY_PRINTF_LIKE(fmt_index, first_arg) \
    __attribute__((format(printf, fmt_index, first_arg)))
#else
#define PGY_PRINTF_LIKE(fmt_index, first_arg)
#endif

/* Forward declarations */
typedef struct SemanticContext SemanticContext;
typedef struct TypeResolutionNode TypeResolutionNode;
typedef struct TypeResolutionEdge TypeResolutionEdge;
typedef struct TypeResolutionGraph TypeResolutionGraph;

#define SEMANTIC_MAX_LOOP_DEPTH 64


typedef enum
{
    TYPE_RES_NODE_TYPE_REF,
    TYPE_RES_NODE_BUILTIN,
    TYPE_RES_NODE_DECL,
    TYPE_RES_NODE_ALIAS,
    TYPE_RES_NODE_GENERIC_PARAM,
    TYPE_RES_NODE_LOCAL_CONTRACT,
    TYPE_RES_NODE_PROJECTION_PATH
} TypeResolutionNodeKind;

struct TypeResolutionNode
{
    TypeResolutionNodeKind kind;
    const ASTNode         *site;
    char                  *label;
};

struct TypeResolutionEdge
{
    size_t from;
    size_t to;
    char  *reason;
};

struct TypeResolutionGraph
{
    TypeResolutionNode *nodes;
    size_t              node_count;
    size_t              node_capacity;

    TypeResolutionEdge *edges;
    size_t              edge_count;
    size_t              edge_capacity;
};

/*
 * Semantic analysis context — passed through all check functions
 */
struct SemanticContext
{
    Scope*       scope;          /* Current scope                  */
    ASTNode*     program_root;   /* Root AST for cross-decl lookup */
    const char*  current_module_path; /* Origin module under analysis */
    ASTNode*     current_relation; /* Enclosing relation decl       */
    ASTNode*     current_effect;   /* Enclosing effect decl         */
    ASTNode*     current_party;    /* Enclosing party decl when any */
    ASTNode*     current_roster;   /* Enclosing roster decl when any */
    ASTNode*     current_nominal_decl; /* Enclosing nominal decl      */
    ASTNode*     current_zone;   /* Enclosing zone decl when any   */
    ASTNode*     current_world;  /* Enclosing world decl when any  */
    ASTNode*     current_function_decl; /* Enclosing function/action decl */
    Type*        current_return; /* Expected return type of func   */
    uint32_t     current_function_effects; /* Inferred effect mask    */
    uint32_t     current_function_body_summary; /* Interprocedural body facts */
    bool         tracking_function_effects; /* Only inside function body */
    bool         in_async_func;  /* Inside async func              */
    bool         in_parallel;    /* Inside parallel block          */
    int          loop_depth;     /* Inside loop nesting            */
    const char  *loop_labels[SEMANTIC_MAX_LOOP_DEPTH];
    int32_t      next_entangle_pool; /* Compile-time entangle pool counter */

    Diagnostic** diagnostics;
    size_t       diagnostic_count;
    size_t       diagnostic_capacity;
    PgyArena     scratch_arena;

    char**       embedded_world_zone_names;
    char**       embedded_world_zone_world_names;
    char**       embedded_world_zone_slot_names;
    size_t       embedded_world_zone_count;
    size_t       embedded_world_zone_capacity;

    char**       stdlib_use_module_names;
    size_t       stdlib_use_module_count;
    size_t       stdlib_use_module_capacity;

    TypeResolutionGraph type_resolution_graph;
    size_t type_resolution_stage_graph_backed_skip_count;
    size_t type_resolution_dag_generic_contract_evidence_count;
    size_t type_resolution_dag_ability_consumer_evidence_count;
    size_t type_resolution_stage_alias_materialized_count;
    size_t type_resolution_stage_alias_diagnostic_unresolved_count;
    size_t type_resolution_stage_alias_diagnostic_cycle_count;
    char** type_resolution_stage_alias_diagnostic_names;
    size_t type_resolution_stage_alias_diagnostic_name_count;
    size_t type_resolution_stage_alias_diagnostic_name_capacity;

    /* Graph-backed staged type metadata (AST type node -> Type*).
     * Populated by the DAG stage after a successful materialization, then
     * reused by Pass 2 owner seams without re-entering recursive materialization. */
    struct {
        void **keys;     /* ASTNode * pointers */
        void **values;   /* Type * pointers */
        bool *owned;     /* true when the metadata cache owns the Type shell */
        void **index_keys;      /* ASTNode * -> entry index + 1 */
        size_t *index_entries;
        size_t count;
        size_t capacity;
        size_t index_capacity;
    } type_resolution_metadata;
    size_t type_resolution_metadata_hits;
    size_t type_resolution_metadata_misses;
    size_t type_resolution_metadata_dead_ends;
    /* Dead-end family counters. These classify explicit DAG metadata dead ends,
     * not live fallback use. */
    size_t type_resolution_metadata_unresolved_named;
    size_t type_resolution_metadata_unresolved_generic_named;
    size_t type_resolution_metadata_unresolved_compound;
    size_t type_resolution_metadata_unresolved_other;
    size_t type_resolution_metadata_unresolved_named_builtin_shell;
    size_t type_resolution_metadata_unresolved_named_generic_class;
    size_t type_resolution_metadata_unresolved_named_alias;
    size_t type_resolution_metadata_unresolved_named_non_class_symbol;
    size_t type_resolution_metadata_unresolved_named_missing_symbol;

    bool         has_error;
};

/* -----------------------------------------------------------------
 * Context lifecycle
 * ----------------------------------------------------------------- */

SemanticContext* semantic_context_create(void);
void             semantic_context_destroy(SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Error / warning emission
 * ----------------------------------------------------------------- */

void semantic_error(SemanticContext* ctx, const ASTNode* node,
                    const char* fmt, ...) PGY_PRINTF_LIKE(3, 4);

/* Emit an error with a stable diagnostic code for downstream consumers.
 * `code` must be a string literal (e.g. "PGY_SEM_TYPE_MISMATCH") — its
 * lifetime is assumed to be static. Meaning is frozen once shipped. */
void semantic_error_code(SemanticContext* ctx, const char* code,
                         const ASTNode* node, const char* fmt, ...)
    PGY_PRINTF_LIKE(4, 5);

/* Emit an error with the full hint payload: stable code plus the
 * optional `cause_ir` and `fix_source` tags defined on the Diagnostic
 * struct. All three string fields must be static literals (non-owning,
 * lifetime-of-process). Pass NULL to omit a field. */
void semantic_error_with_hints(SemanticContext* ctx,
                               const char* code,
                               const char* cause_ir,
                               const char* fix_source,
                               const ASTNode* node,
                               const char* fmt, ...)
    PGY_PRINTF_LIKE(6, 7);

void semantic_warning(SemanticContext* ctx, const ASTNode* node,
                      const char* fmt, ...) PGY_PRINTF_LIKE(3, 4);

void semantic_warning_code(SemanticContext* ctx, const char* code,
                           const ASTNode* node, const char* fmt, ...)
    PGY_PRINTF_LIKE(4, 5);

/* Warning variant of semantic_error_with_hints. */
void semantic_warning_with_hints(SemanticContext* ctx,
                                 const char* code,
                                 const char* cause_ir,
                                 const char* fix_source,
                                 const ASTNode* node,
                                 const char* fmt, ...)
    PGY_PRINTF_LIKE(6, 7);

void semantic_print_diagnostics(SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Top-level check entry points
 * ----------------------------------------------------------------- */

bool type_check_program(ASTNode* program, SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Declaration checkers
 * ----------------------------------------------------------------- */

bool type_check_func_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_type_alias_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_class_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_enum_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_extern_block(ASTNode* node, SemanticContext* ctx);
bool type_check_let_decl(ASTNode* node, SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Statement checkers
 * ----------------------------------------------------------------- */

bool type_check_statement(ASTNode* node, SemanticContext* ctx);
bool type_check_block(ASTNode* node, SemanticContext* ctx);
bool type_check_use_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_let_destructure_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_namespace_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_unsafe_block(ASTNode* node, SemanticContext* ctx);
bool type_check_if_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_for_loop(ASTNode* node, SemanticContext* ctx);
bool type_check_while_loop(ASTNode* node, SemanticContext* ctx);
bool type_check_match_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_return_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_break_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_continue_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_defer_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_ability_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_role_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_party_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_roster_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_world_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_relation_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_effect_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_zone_decl(ASTNode* node, SemanticContext* ctx);

/* Async system checkers */
bool type_check_async_block(ASTNode* node, SemanticContext* ctx);
bool type_check_select_stmt(ASTNode* node, SemanticContext* ctx);
Type* type_check_spawn_expr(ASTNode* expr, SemanticContext* ctx);
Type* type_check_channel_send(ASTNode* expr, SemanticContext* ctx);
Type* type_check_channel_recv(ASTNode* expr, SemanticContext* ctx);

/*
 * with slot<T> as s { ... }
 *
 * Validates that:
 *   - T is a valid type
 *   - s is registered as a Slot<T> in the inner scope
 *   - s is automatically released when the block exits
 */
bool type_check_with_stmt(ASTNode* node, SemanticContext* ctx);

/*
 * parallel { A()  B()  C() }
 *
 * Each task must be an expression statement.
 * Tasks must not write to the same Slot<T> simultaneously.
 * (basic check: no two tasks reference the same slot in write position)
 */
bool type_check_parallel_block(ASTNode* node, SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Expression checkers — return inferred Type* (NULL on error)
 * ----------------------------------------------------------------- */

Type* type_check_expression(ASTNode* expr, SemanticContext* ctx);
Type* type_check_binary(ASTNode* expr, SemanticContext* ctx);
Type* type_check_unary(ASTNode* expr, SemanticContext* ctx);
Type* type_check_call(ASTNode* expr, SemanticContext* ctx);
Type* type_check_member_access(ASTNode* expr, SemanticContext* ctx);
Type* type_check_array_access(ASTNode* expr, SemanticContext* ctx);
Type* type_check_array_literal(ASTNode* expr, SemanticContext* ctx);
Type* type_check_assignment(ASTNode* expr, SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Resource-handle checkers (Pergyra core rules)
 *
 * Today the first concrete anchored resource family is Slot<T>/SecureSlot<T>.
 * Slot is the source-level modular resource boundary: callers observe the
 * Slot contract, not backend pointer/address ownership. Additional resource
 * families can grow on top of the same boundary model.
 * ----------------------------------------------------------------- */

/*
 * ClaimSlot<T>() → validates T is a known type, returns Slot<T>
 * ClaimSecureSlot<T>(level) → returns (SecureSlot<T>, SecurityToken)
 */
Type* type_check_claim_slot(ASTNode* call, SemanticContext* ctx);

/*
 * Write(slot, value)            — plain Slot<T>
 * Write(slot, value, token)     — SecureSlot<T>
 *
 * Rules enforced:
 *   R1: value type must match Slot inner type
 *   R2: SecureSlot requires exactly one token argument
 *   R3: token must be paired with this slot (not another slot's token)
 *   R4: slot must be in CLAIMED state
 */
bool type_check_write_slot(ASTNode* call, SemanticContext* ctx);

/*
 * Read(slot)         — plain Slot<T>, returns T
 * Read(slot, token)  — SecureSlot<T>, returns T
 *
 * Same rules as Write except no value argument.
 */
Type* type_check_read_slot(ASTNode* call, SemanticContext* ctx);

/*
 * Release(slot)         — plain Slot<T>
 * Release(slot, token)  — SecureSlot<T>
 *
 * Marks slot as RELEASED in symbol table.
 * Emits error if slot is already RELEASED.
 */
bool type_check_release_slot(ASTNode* call, SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Built-in function dispatch
 *
 * Called from type_check_call when the callee is an identifier
 * matching a built-in name.
 * ----------------------------------------------------------------- */

Type* type_check_builtin_call(ASTNode* call, BuiltinKind kind,
                               SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Utility
 * ----------------------------------------------------------------- */

/*
 * Check two types are compatible for assignment (from → to).
 * Emits a semantic_error if not.
 */
bool require_assignable(Type* from, Type* to,
                         const ASTNode* site, SemanticContext* ctx);

#endif /* PERGYRA_TYPE_CHECKER_H */
