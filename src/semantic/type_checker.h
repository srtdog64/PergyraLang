/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker — expression type inference and rule validation
 */

#ifndef PERGYRA_TYPE_CHECKER_H
#define PERGYRA_TYPE_CHECKER_H

#include <stdbool.h>
#include "../parser/ast.h"
#include "../semantic/type_system.h"
#include "../semantic/symbol_table.h"

/* Forward declarations */
typedef struct SemanticContext SemanticContext;
typedef struct Diagnostic      Diagnostic;

/*
 * Diagnostic severity
 */
typedef enum
{
    DIAG_ERROR,
    DIAG_WARNING
} DiagnosticLevel;

/*
 * One compiler message
 */
struct Diagnostic
{
    DiagnosticLevel level;
    uint32_t        line;
    uint32_t        col;
    char*           message;
};

/*
 * Semantic analysis context — passed through all check functions
 */
struct SemanticContext
{
    Scope*       scope;          /* Current scope                  */
    ASTNode*     program_root;   /* Root AST for cross-decl lookup */
    ASTNode*     current_relation; /* Enclosing relation decl       */
    ASTNode*     current_effect;   /* Enclosing effect decl         */
    ASTNode*     current_nominal_decl; /* Enclosing nominal decl      */
    ASTNode*     current_zone;   /* Enclosing zone decl when any   */
    ASTNode*     current_world;  /* Enclosing world decl when any  */
    Type*        current_return; /* Expected return type of func   */
    uint32_t     current_function_effects; /* Inferred effect mask    */
    bool         tracking_function_effects; /* Only inside function body */
    bool         in_async_func;  /* Inside async func              */
    bool         in_parallel;    /* Inside parallel block          */
    int          loop_depth;     /* Inside loop nesting            */
    int32_t      next_entangle_pool; /* Compile-time entangle pool counter */

    Diagnostic** diagnostics;
    size_t       diagnostic_count;
    size_t       diagnostic_capacity;

    char**       embedded_world_zone_names;
    size_t       embedded_world_zone_count;
    size_t       embedded_world_zone_capacity;

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
                    const char* fmt, ...);

void semantic_warning(SemanticContext* ctx, const ASTNode* node,
                      const char* fmt, ...);

void semantic_print_diagnostics(SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Top-level check entry points
 * ----------------------------------------------------------------- */

bool type_check_program(ASTNode* program, SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Declaration checkers
 * ----------------------------------------------------------------- */

bool type_check_func_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_class_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_extern_block(ASTNode* node, SemanticContext* ctx);
bool type_check_let_decl(ASTNode* node, SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Statement checkers
 * ----------------------------------------------------------------- */

bool type_check_statement(ASTNode* node, SemanticContext* ctx);
bool type_check_block(ASTNode* node, SemanticContext* ctx);
bool type_check_if_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_for_loop(ASTNode* node, SemanticContext* ctx);
bool type_check_while_loop(ASTNode* node, SemanticContext* ctx);
bool type_check_match_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_return_stmt(ASTNode* node, SemanticContext* ctx);
bool type_check_ability_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_role_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_party_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_systemic_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_world_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_relation_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_effect_decl(ASTNode* node, SemanticContext* ctx);
bool type_check_zone_decl(ASTNode* node, SemanticContext* ctx);

/* Async system checkers */
bool type_check_actor_decl(ASTNode* node, SemanticContext* ctx);
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
Type* type_check_assignment(ASTNode* expr, SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Resource-handle checkers (Pergyra core rules)
 *
 * Today the first concrete anchored resource family is Slot<T>/SecureSlot<T>.
 * Additional resource families can grow on top of the same ownership model.
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

typedef enum
{
    BUILTIN_CLAIM_SLOT,
    BUILTIN_CLAIM_SECURE_SLOT,
    BUILTIN_CLAIM_DEVICE_SLOT,
    BUILTIN_VIEW_READ,
    BUILTIN_VIEW_WRITE,
    BUILTIN_MOVE,
    BUILTIN_WRITE,
    BUILTIN_READ,
    BUILTIN_RELEASE,
    BUILTIN_DEVICE_WRITE,
    BUILTIN_DEVICE_READ,
    BUILTIN_RELEASE_DEVICE_SLOT,
    BUILTIN_SUBMIT_DEVICE_READ,
    BUILTIN_LOG,
    BUILTIN_LOG_BANNER,
    BUILTIN_LOG_BLOCK,
    BUILTIN_LOG_RAW,
    BUILTIN_RC_NEW,
    BUILTIN_RC_CLONE,
    BUILTIN_RC_DROP,
    BUILTIN_RC_DOWNGRADE,
    BUILTIN_RC_GET,
    BUILTIN_WEAK_UPGRADE,
    BUILTIN_WEAK_DROP,
    BUILTIN_ALLOCATOR_SYSTEM,
    BUILTIN_ALLOCATOR_TRACING,
    BUILTIN_ALLOCATOR_DEBUG,
    BUILTIN_ALLOCATOR_POOL,
    BUILTIN_BOX,
    BUILTIN_BOX_GET,
    BUILTIN_BOX_SET,
    BUILTIN_BOX_DROP,
    BUILTIN_BOX_IS_VALID,
    BUILTIN_BOX_ARRAY,
    BUILTIN_TO_OBJECT,
    BUILTIN_TO_DTO,
    BUILTIN_HAS_PROJECTION,
    BUILTIN_HAS_LAYER,
    BUILTIN_HAS_STATE,
    BUILTIN_HAS_ZONE,
    BUILTIN_HAS_ZONE_PROJECTION,
    BUILTIN_HAS_ZONE_LAYER,
    BUILTIN_HAS_ZONE_STATE,
    BUILTIN_PARALLEL,
    /* I/O built-ins */
    BUILTIN_FILE_OPEN,
    BUILTIN_FILE_READ,
    BUILTIN_FILE_WRITE,
    BUILTIN_FILE_CLOSE,
    BUILTIN_READ_FILE,
    BUILTIN_WRITE_FILE,
    BUILTIN_INPUT,
    BUILTIN_PRINT,
    BUILTIN_READ_LINE,
    BUILTIN_NOW,
    BUILTIN_SLEEP,
    BUILTIN_INTENT_LAST_TRACE,
    BUILTIN_INTENT_LAST_FAILURE,
    BUILTIN_INTENT_LAST_NAME,
    BUILTIN_INTENT_LAST_HANDLE,
    BUILTIN_INTENT_LAST_TRACE_ID,
    BUILTIN_INTENT_LAST_STEP_COUNT,
    BUILTIN_INTENT_LAST_FAILED,
    BUILTIN_INTENT_HISTORY_COUNT,
    BUILTIN_INTENT_HISTORY_STEP_NAME,
    BUILTIN_INTENT_HISTORY_STEP_ZONE,
    BUILTIN_INTENT_HISTORY_STEP_PHASE,
    BUILTIN_INTENT_HISTORY_STEP_ACTOR,
    BUILTIN_INTENT_HISTORY_STEP_SLOT,
    BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE,
    BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT,
    BUILTIN_INTENT_HISTORY_STEP_TO_ZONE,
    BUILTIN_INTENT_HISTORY_STEP_TO_SLOT,
    BUILTIN_INTENT_HISTORY_STEP_OK,
    BUILTIN_INTENT_HISTORY_STEP_FAILURE,
    BUILTIN_INTENT_ACTIVE_COUNT,
    BUILTIN_INTENT_ACTIVE_NAME,
    BUILTIN_INTENT_ACTIVE_HANDLE,
    BUILTIN_INTENT_ACTIVE_TRACE_ID,
    BUILTIN_INTENT_ACTIVE_PRIORITY,
    BUILTIN_INTENT_ACTIVE_CONCURRENT,
    BUILTIN_INTENT_ACTIVE_TRACE,
    BUILTIN_NOT_BUILTIN    /* Not a built-in — resolve as user function */
} BuiltinKind;

BuiltinKind builtin_resolve(const char* name);

Type* type_check_builtin_call(ASTNode* call, BuiltinKind kind,
                               SemanticContext* ctx);

/* -----------------------------------------------------------------
 * Utility
 * ----------------------------------------------------------------- */

/*
 * Resolve an AST type node (AST_TYPE) to a
 * Type* using the current scope's type definitions.
 */
Type* resolve_type_node(ASTNode* type_node, SemanticContext* ctx);

/*
 * Check two types are compatible for assignment (from → to).
 * Emits a semantic_error if not.
 */
bool require_assignable(Type* from, Type* to,
                         const ASTNode* site, SemanticContext* ctx);

#endif /* PERGYRA_TYPE_CHECKER_H */
