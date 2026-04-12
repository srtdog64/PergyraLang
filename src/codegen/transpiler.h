/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend — converts lowered Pergyra HIR to C source code.
 *
 * Strategy:
 *   Pergyra Slot<T>          → PgySlot_<T> struct  (pgy_runtime.h)
 *   ClaimSlot<T>()           → pgy_claim_<t>()
 *   Write(slot, val)         → pgy_write_<t>(&slot, val)
 *   Read(slot)               → pgy_read_<t>(&slot)
 *   Release(slot)            → pgy_release_<t>(&slot)
 *   with slot<T> as s { }   → { PgySlot_T s = ...; ... pgy_release(&s); }
 *   Parallel { A() B() }    → _Pragma("omp parallel sections") { ... }
 *   func F(x: Int) -> Int   → int F(int x)
 *   class Foo { }           → typedef struct Foo { ... } Foo;
 *   let x: Int = 42         → int x = 42;
 */

#ifndef PERGYRA_TRANSPILER_H
#define PERGYRA_TRANSPILER_H

#include <stdio.h>
#include <stdbool.h>
#include "../parser/ast.h"
#include "../compiler/hir.h"
#include "../compiler/mir.h"
#include "../semantic/type_system.h"
#include "../semantic/semantic.h"
#include "../common/arena.h"

/* -----------------------------------------------------------------
 * Output buffer — grows dynamically
 * ----------------------------------------------------------------- */

typedef struct
{
    char  *data;
    size_t len;
    size_t cap;
} CodeBuf;

CodeBuf *codebuf_create(void);
void     codebuf_destroy(CodeBuf *buf);
void     codebuf_write(CodeBuf *buf, const char *fmt, ...);
void     codebuf_write_raw(CodeBuf *buf, const char *s, size_t n);
bool     codebuf_dump_file(const CodeBuf *buf, const char *path);

/* -----------------------------------------------------------------
 * Slot variable tracking — maps variable name → inner type name
 * ----------------------------------------------------------------- */

#define MAX_SLOT_VARS 256
#define MAX_ALIAS_VARS 128
#define MAX_GENERIC_BINDINGS 32
#define MAX_GENERIC_SPECIALIZATIONS 128
#define MAX_COLLECTION_SPECIALIZATIONS 128
#define TRANSPILE_MAX_LOOP_DEPTH 64

typedef struct
{
    char name[64];         /* variable name, e.g. "msg"     */
    char inner_type[32];   /* Pergyra type, e.g. "String"   */
    bool is_secure;        /* SecureSlot?                    */
    bool is_indirect;      /* passed/stored as slot pointer  */
    bool released;         /* explicit Release() was called  */
} SlotVarEntry;

typedef struct
{
    char name[64];
    char type_name[128];
    char source_slot[64];
    bool is_view;
    bool is_move_token;
    bool source_secure;
    bool is_subject_ref;  /* subject parameter — pointer, use -> for member access */
    bool is_projection_borrow; /* object projection borrowed from a local source */
} TypedVarEntry;

typedef struct
{
    char name[64];
    ASTNode *target_expr;
} AliasVarEntry;

typedef struct
{
    char name[32];
    char concrete_type[128];
} GenericBindingEntry;

typedef struct
{
    const ASTNode *func_decl;
    char           specialized_name[128];
    bool           emitting;
} GenericSpecializationEntry;

#define MAX_GENERIC_CLASS_SPECIALIZATIONS 64
#define MAX_ABILITY_VTABLE_SPECIALIZATIONS 128

typedef struct
{
    const ASTNode *class_decl;
    char           specialized_name[128];
    bool           emitted;
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t         binding_count;
} GenericClassSpecEntry;

typedef struct
{
    char kind[16];
    char suffix[128];
} CollectionSpecEntry;

typedef struct
{
    char name[128];
} AbilityVtableSpecEntry;

/* -----------------------------------------------------------------
 * Transpiler context
 * ----------------------------------------------------------------- */

typedef struct
{
    CodeBuf *out;          /* main output buffer            */
    CodeBuf *decls;        /* prototypes and forward decls  */
    CodeBuf          *helpers;      /* late helper definitions       */
    int               indent;       /* current indent level          */
    bool              in_parallel;  /* inside a Parallel block       */
    const MIRProgram *mir;          /* MIR program (required)        */

    /* Unique counter for anonymous temp variables */
    int      tmp_counter;

    /* Slot variable → inner type mapping */
    SlotVarEntry slot_vars[MAX_SLOT_VARS];
    int          slot_var_count;
    TypedVarEntry typed_vars[MAX_SLOT_VARS];
    int           typed_var_count;
    AliasVarEntry alias_vars[MAX_ALIAS_VARS];
    int           alias_var_count;

    /* Counter for unique parallel wrapper function names */
    unsigned int  parallel_id;

    /* Parallel variable capture: when emitting a parallel/async wrapper body,
     * identifiers from the outer scope are accessed through _pctx->name. */
    bool  in_parallel_wrapper;
    char  par_capture_slot_names[MAX_SLOT_VARS][64];
    int   par_capture_slot_count;
    char  par_capture_typed_names[MAX_SLOT_VARS][64];
    int   par_capture_typed_count;

    /* Slot sugar: suppress auto-Read when emitting slot handle arguments */
    bool  suppress_slot_auto_read;

    char  current_return_type[128];

    /* Defer counter for unique defer IDs */
    int   defer_counter;

    /* Loop label tracking for labeled break/continue. */
    const char *loop_labels[TRANSPILE_MAX_LOOP_DEPTH];
    char        loop_break_labels[TRANSPILE_MAX_LOOP_DEPTH][64];
    char        loop_continue_labels[TRANSPILE_MAX_LOOP_DEPTH][64];
    bool        loop_break_label_used[TRANSPILE_MAX_LOOP_DEPTH];
    bool        loop_continue_label_used[TRANSPILE_MAX_LOOP_DEPTH];
    int         loop_depth;

    /* Memory arena for expression string allocation.
     * Strings allocated here are freed in bulk at context destruction. */
    PgyArena arena;

    /* Active generic bindings while emitting a monomorphized function. */
    GenericBindingEntry generic_bindings[MAX_GENERIC_BINDINGS];
    int                 generic_binding_count;

    /* Generic specializations emitted on demand. */
    GenericSpecializationEntry generic_specializations[MAX_GENERIC_SPECIALIZATIONS];
    int                        generic_specialization_count;

    /* Generic class specializations (monomorphized struct + methods). */
    GenericClassSpecEntry generic_class_specs[MAX_GENERIC_CLASS_SPECIALIZATIONS];
    int                   generic_class_spec_count;

    /* Runtime collection helper specializations (List/Queue) emitted on demand. */
    CollectionSpecEntry collection_specs[MAX_COLLECTION_SPECIALIZATIONS];
    int                 collection_spec_count;

    /* Generic ability vtable specializations emitted on demand. */
    AbilityVtableSpecEntry ability_vtable_specs[MAX_ABILITY_VTABLE_SPECIALIZATIONS];
    int                    ability_vtable_spec_count;

    /* Current class method emission context for implicit self-field access. */
    const char *current_class_name;
    const char *current_relation_name;
    const char *current_effect_name;
    const char *current_zone_name;
    const char *current_world_name;
    const char *current_overlay_receiver_expr;
    bool uses_intent_observability;
    const void *active_ssa_map;
    const char *active_type_hint;

    /* Expected target type for context-sensitive emission.
     * Set when emitting a let initializer so that `None` can resolve
     * to the correct type-specific constructor (e.g. None_String vs None_Int). */
    const char *expected_type;

    char *backend_error;
} TranspilerCtx;

static inline void
transpiler_active_inventory(const TranspilerCtx *ctx,
                            ASTNodeType decl_type,
                            ASTNode ***nodes_out,
                            size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL) {
        switch (decl_type) {
        case AST_ABILITY_DECL: nodes = ctx->mir->abilities; count = ctx->mir->ability_count; break;
        case AST_FUNC_DECL: nodes = ctx->mir->functions; count = ctx->mir->function_count; break;
        case AST_INTENT_DECL: nodes = ctx->mir->intents; count = ctx->mir->intent_count; break;
        case AST_ROLE_DECL: nodes = ctx->mir->roles; count = ctx->mir->role_count; break;
        case AST_PARTY_DECL: nodes = ctx->mir->parties; count = ctx->mir->party_count; break;
        case AST_ROSTER_DECL: nodes = ctx->mir->rosters; count = ctx->mir->roster_count; break;
        case AST_WORLD_DECL: nodes = ctx->mir->worlds; count = ctx->mir->world_count; break;
        case AST_RELATION_DECL: nodes = ctx->mir->relations; count = ctx->mir->relation_count; break;
        case AST_EFFECT_DECL: nodes = ctx->mir->effects; count = ctx->mir->effect_count; break;
        case AST_ZONE_DECL: nodes = ctx->mir->zones; count = ctx->mir->zone_count; break;
        case AST_EVENT_DECL: nodes = ctx->mir->events; count = ctx->mir->event_count; break;
        case AST_CLASS_DECL:
        case AST_ENUM_DECL:
        case AST_TYPE_ALIAS:
            nodes = ctx->mir->types; count = ctx->mir->type_count; break;
        default:
            break;
        }
    }

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

static inline void
transpiler_active_externs(const TranspilerCtx *ctx,
                          ASTNode ***nodes_out,
                          size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL) {
        nodes = ctx->mir->externs;
        count = ctx->mir->extern_count;
    }

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

static inline void
transpiler_active_executables(const TranspilerCtx *ctx,
                              ASTNode ***nodes_out,
                              size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    /* MIR-only: top-level exec is represented by __pgy_top_level_exec. */
    (void)ctx;

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

static inline ASTNode *
transpiler_active_synthetic_executable_func(const TranspilerCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return mir_find_function_decl(ctx->mir, "__pgy_top_level_exec");
    return NULL;
}

static inline bool
transpiler_active_has_main_function(const TranspilerCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return ctx->mir->has_main_function;
    return false;
}

static inline bool
transpiler_active_has_top_level_exec(const TranspilerCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return ctx->mir->has_top_level_exec;
    return false;
}

TranspilerCtx *transpiler_ctx_create(void);
void           transpiler_ctx_destroy(TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Main entry point
 *
 * Usage:
 *   SemanticResult *sem = semantic_analyze(ast);
 *   MIRProgram *mir = mir_lower(sem->annotated_ast, rir, NULL);
 *   TranspileResult *res = transpile_from_mir(mir, "out.c");
 * ----------------------------------------------------------------- */

typedef struct
{
    bool  success;
    char *error_message;  /* NULL on success */
    bool  uses_intent_observability;
} TranspileResult;

TranspileResult *transpile_from_mir(const MIRProgram *mir,
                                    const char *output_path);
TranspileResult *transpile_with_mir(const HIRProgram *hir,
                                    const MIRProgram *mir,
                                    const char *output_path);
void             transpile_result_destroy(TranspileResult *res);

/* Test-only helpers: MIR emission eligibility with reason.
 * These are intentionally conservative and exposed for diagnostics in
 * transpiler tests. */
bool transpiler_can_emit_function_from_mir_with_reason_for_test(
    const ASTNode *func_decl,
    const MIRProgram *mir,
    char *reason,
    size_t reason_cap);
bool transpiler_can_emit_intent_cleanup_from_mir_with_reason_for_test(
    const ASTNode *intent_decl,
    const MIRProgram *mir,
    char *reason,
    size_t reason_cap);

/* -----------------------------------------------------------------
 * Per-node emitters (public for testing)
 * ----------------------------------------------------------------- */

void emit_program(TranspilerCtx *ctx);
void emit_statement(ASTNode *node, TranspilerCtx *ctx);
void emit_block(ASTNode *node, TranspilerCtx *ctx);

/* Declarations */
void emit_func_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_class_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_extern_block(ASTNode *node, TranspilerCtx *ctx);
void emit_let_decl(ASTNode *node, TranspilerCtx *ctx);

/* Statements */
void emit_if_stmt(ASTNode *node, TranspilerCtx *ctx);
void emit_for_loop(ASTNode *node, TranspilerCtx *ctx);
void emit_while_loop(ASTNode *node, TranspilerCtx *ctx);
void emit_return_stmt(ASTNode *node, TranspilerCtx *ctx);
void emit_with_stmt(ASTNode *node, TranspilerCtx *ctx);
void emit_parallel_block(ASTNode *node, TranspilerCtx *ctx);

/* Expressions — return a C expression string (caller frees) */
char *emit_expression(ASTNode *node, TranspilerCtx *ctx);
char *emit_call(ASTNode *node, TranspilerCtx *ctx);
char *emit_binary(ASTNode *node, TranspilerCtx *ctx);
char *emit_unary(ASTNode *node, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Type mapping helpers
 * ----------------------------------------------------------------- */

/* "Int" → "int", "String" → "char*", "Slot<Int>" → "PgySlot_Int" */
const char *pergyra_type_to_c(const char *pergyra_type_name);

/* "Int" → "int", used for slot operation suffixes */
const char *pergyra_primitive_to_c(const char *name);

/* "Slot<Int>" → "Int",  "SecureSlot<String>" → "String" */
const char *slot_inner_type_name(const char *slot_type_name);

/* -----------------------------------------------------------------
 * Built-in call emitters
 * ----------------------------------------------------------------- */

char *emit_builtin_claim_slot(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_write(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_read(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_release(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_log(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_log_banner(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_log_raw(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_rc(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx);
char *emit_builtin_allocator(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Role/Ability system emitters
 * ----------------------------------------------------------------- */

void emit_ability_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_role_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_party_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_roster_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_world_decl(ASTNode *node, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Async system emitters
 * ----------------------------------------------------------------- */

void emit_select_stmt(ASTNode *node, TranspilerCtx *ctx);
char *emit_spawn_expr(ASTNode *node, TranspilerCtx *ctx);
char *emit_channel_send(ASTNode *node, TranspilerCtx *ctx);
char *emit_channel_recv(ASTNode *node, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Event system emitters
 * ----------------------------------------------------------------- */

void emit_event_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_event_subscribe(ASTNode *node, TranspilerCtx *ctx);
void emit_event_unsubscribe(ASTNode *node, TranspilerCtx *ctx);
void emit_event_invoke(ASTNode *node, TranspilerCtx *ctx);
char *emit_lambda_expr(ASTNode *node, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Role/Ability system emitters
 * ----------------------------------------------------------------- */

void emit_ability_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_role_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_include_stmt(ASTNode *node, TranspilerCtx *ctx);
void emit_impl_ability(ASTNode *node, TranspilerCtx *ctx);

#endif /* PERGYRA_TRANSPILER_H */
