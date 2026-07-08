/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend: converts lowered Pergyra HIR to C source code.
 *
 * Strategy:
 *   Pergyra Slot<T>         -> PgySlot_<T> struct  (pgy_runtime.h)
 *   ClaimSlot<T>()          -> pgy_claim_<t>()
 *   Write(slot, val)        -> pgy_write_<t>(&slot, val)
 *   Read(slot)              -> pgy_read_<t>(&slot)
 *   Release(slot)           -> pgy_release_<t>(&slot)
 *   with slot<T> as s { }   -> { PgySlot_T s = ...; ... pgy_release(&s); }
 *   Parallel { A() B() }    -> _Pragma("omp parallel sections") { ... }
 *   func F(x: Int) -> Int   -> int F(int x)
 *   class Foo { }           -> typedef struct Foo { ... } Foo;
 *   let x: Int = 42         -> int x = 42;
 */

#ifndef PERGYRA_TRANSPILER_H
#define PERGYRA_TRANSPILER_H

#include <stdio.h>
#include <stdbool.h>
#include "../parser/ast.h"
#include "../compiler/hir.h"
#include "../compiler/mir.h"
#include "../common/arena.h"

/* -----------------------------------------------------------------
 * Output buffer: grows dynamically.
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
void     codebuf_truncate(CodeBuf *buf, size_t len);
bool     codebuf_dump_file(const CodeBuf *buf, const char *path);

/* -----------------------------------------------------------------
 * Slot variable tracking: maps variable name to inner type name.
 * ----------------------------------------------------------------- */

/* Sized to fit self-host compiler-scale function locals.
 * Caps slot_vars / typed_vars / par_capture arrays in TranspilerCtx;
 * each ctx is allocated once per compile, so the memory cost is bounded. */
#define MAX_SLOT_VARS 4096
#define MAX_ALIAS_VARS 128
#define MAX_GENERIC_BINDINGS 32
#define MAX_GENERIC_SPECIALIZATIONS 128
#define MAX_COLLECTION_SPECIALIZATIONS 128
#define TRANSPILE_MAX_LOOP_DEPTH 64
#define TRANSPILE_MAX_SCOPE_DEPTH 128
#define TRANSPILE_MAX_DEFER_PER_SCOPE 64
#define TRANSPILE_MAX_MUT_REF_PARAMS 32

typedef struct
{
    char name[64];         /* variable name, e.g. "msg"     */
    char inner_type[32];   /* Pergyra type, e.g. "String"   */
    char token_name[64];   /* paired token local for secure slots */
    bool is_secure;        /* SecureSlot?                    */
    bool is_indirect;      /* passed/stored as slot pointer  */
    bool released;         /* explicit Release() was called  */
    bool is_self_field;    /* slot lives in self->name (object field) */
} SlotVarEntry;

typedef struct
{
    char name[64];
    char ssa_name[128];
    char type_name[128];
    char source_slot[64];
    bool is_view;
    bool is_move_token;
    bool source_secure;
    bool is_subject_ref;  /* subject parameter pointer; use -> for member access */
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

    /* Slot variable to inner type mapping. */
    SlotVarEntry slot_vars[MAX_SLOT_VARS];
    int          slot_var_count;
    int          last_slot_var_index;
    TypedVarEntry typed_vars[MAX_SLOT_VARS];
    int           typed_var_count;
    int           last_typed_var_index;
    AliasVarEntry alias_vars[MAX_ALIAS_VARS];
    int           alias_var_count;
    int           last_alias_var_index;
    ASTNodeType   last_decl_lookup_type;
    char          last_decl_lookup_name[128];
    ASTNode     **last_decl_lookup_inventory;
    size_t        last_decl_lookup_inventory_count;
    ASTNode      *last_decl_lookup_result;
    bool          last_decl_lookup_active_only;
    char          last_nominal_host_name[128];
    const MIRProgram *last_nominal_host_mir;
    ASTNode      *last_nominal_host_decl;

    /* Counter for unique parallel wrapper function names */
    unsigned int  parallel_id;

    /* Parallel variable capture: when emitting a parallel/async wrapper body,
     * identifiers from the outer scope are accessed through _pctx->name. */
    bool  in_parallel_wrapper;
    char  par_capture_slot_names[MAX_SLOT_VARS][64];
    int   par_capture_slot_count;
    char  par_capture_typed_names[MAX_SLOT_VARS][64];
    int   par_capture_typed_count;
    /* Per-arm snapshot mode (docs/178 Copy evidence): when set for index i,
     * the current wrapper reads `_pctx-><name>__snap` (the pre-parallel
     * value) instead of dereferencing the shared pointer member. */
    bool  par_capture_typed_snapshot[MAX_SLOT_VARS];

    /* Slot sugar: suppress auto-Read when emitting slot handle arguments */
    bool  suppress_slot_auto_read;

    char  current_return_type[128];
    ASTNode *current_return_callable_type;

    /* Defer counter for unique defer IDs */
    int   defer_counter;

    /* Loop label tracking for labeled break/continue. */
    const char *loop_labels[TRANSPILE_MAX_LOOP_DEPTH];
    char        loop_break_labels[TRANSPILE_MAX_LOOP_DEPTH][64];
    char        loop_continue_labels[TRANSPILE_MAX_LOOP_DEPTH][64];
    bool        loop_break_label_used[TRANSPILE_MAX_LOOP_DEPTH];
    bool        loop_continue_label_used[TRANSPILE_MAX_LOOP_DEPTH];
    int         loop_defer_base_depth[TRANSPILE_MAX_LOOP_DEPTH];
    int         loop_depth;

    /* Lexical defer stack. Defer bodies are emitted inline at scope exits
     * and returns so they can reference locals such as method `self`. */
    ASTNode    *defer_bodies[TRANSPILE_MAX_SCOPE_DEPTH][TRANSPILE_MAX_DEFER_PER_SCOPE];
    int         defer_body_counts[TRANSPILE_MAX_SCOPE_DEPTH];
    int         defer_scope_depth;

    /* Active inout value-parameter names for the current
     * function. Each inout value parameter is lowered to a pointer parameter
     * `<name>__mutref` with a copy-in local `<name>`; write-backs
     * `*<name>__mutref = <name>;` are emitted at every return through the
     * pre-return hook so the caller observes the mutation. */
    char        mut_ref_param_names[TRANSPILE_MAX_MUT_REF_PARAMS][64];
    char        mut_ref_param_ctypes[TRANSPILE_MAX_MUT_REF_PARAMS][64];
    int         mut_ref_param_count;

    /* Scratch arena for transpiler-local temporary strings.
     * Long-lived caches/metadata must not retain pointers from here. */
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

    /* While emitting a monomorphized generic class method body, map the
     * base generic class name to its active specialization name so the
     * self parameter renders the concrete struct type. */
    const char *active_generic_class_base_name;
    const char *active_generic_class_spec_name;

    /* Runtime collection helper specializations (List/Queue) emitted on demand. */
    CollectionSpecEntry collection_specs[MAX_COLLECTION_SPECIALIZATIONS];
    int                 collection_spec_count;

    /* Result<T, E> specializations for user-defined error types.
     * Each entry records a unique (T, E) pair seen during emission so the
     * codegen can emit a matching PGY_RESULT_DEFINE(...) in the helpers buf. */
    char   result_specs_suffix[32][128];  /* e.g. "Int_NetError" */
    char   result_specs_ok_ctype[32][128]; /* e.g. "int32_t" */
    char   result_specs_err_ctype[32][128]; /* e.g. "NetError" */
    int    result_spec_count;

    /* Option<T> specializations for user-defined inner types beyond the
     * runtime-predefined Int/Bool/String. Same pattern as Result. */
    char   option_specs_suffix[32][128];     /* e.g. "Status" */
    char   option_specs_inner_ctype[32][128];/* e.g. "Status" */
    int    option_spec_count;

    /* Tuple type specializations. Each entry is a unique set of
     * pergyra-element-type-names (strings), emitted as a C struct:
     *   typedef struct { T0 f0; T1 f1; ... } PgyTuple_<suffix>_t;
     * suffix is e.g. "Int_String". Element types stored space-separated. */
    char   tuple_specs_suffix[32][256];   /* e.g. "Int_String" */
    char   tuple_specs_elements[32][512]; /* e.g. "Int String" (space-separated pergyra names) */
    int    tuple_specs_arity[32];
    int    tuple_spec_count;

    /* Generic ability vtable specializations emitted on demand. */
    AbilityVtableSpecEntry ability_vtable_specs[MAX_ABILITY_VTABLE_SPECIALIZATIONS];
    int                    ability_vtable_spec_count;

    /* Current host emission context for implicit self-field access.
     * Declaration-side MIR-only lowering should flow through the active
     * host declaration rather than mirrored name shadow state. */
    ASTNode *current_host_decl;
    const ASTNode *current_func_decl;
    const char *current_overlay_receiver_expr;
    bool uses_intent_observability;
    const void *active_ssa_map;
    /* Persistent alias map for match-case bindings, valid for the whole
     * function emission. Each entry maps source binding name (e.g. "v") to
     * the SSA-renamed unique C variable name (e.g. "_pgy_match_v_42").
     * Used as a fallback when block-local ssa_map doesn't have the alias
     * — necessary for nested matches where outer binding is referenced in
     * inner-match subject expression in a successor block. */
    void *match_binding_alias_map;
    const char *active_type_hint;

    /* Expected target type for context-sensitive emission.
     * Set when emitting a let initializer so that `None` can resolve
     * to the correct type-specific constructor (e.g. None_String vs None_Int). */
    const char *expected_type;
    ASTNode *expected_callable_type;

    char *backend_error;
    /* Stable diagnostic code attached to backend_error. non-owning; must
     * be a string literal (e.g. "PGY_MIR_UNRESOLVED_LOCAL"). NULL when
     * the failing site has not been assigned a code. Propagated to
     * CompilerResult.error_code at transpiler.c:552. */
    const char *backend_error_code;
    /* Optional hint tags alongside backend_error_code. Both non-owning
     * string literals; NULL when the failing site did not provide them.
     * Propagated to CompilerResult.error_cause_ir / error_fix_source. */
    const char *backend_error_cause_ir;
    const char *backend_error_fix_source;

    /* transaction saga lowering: txn_counter mints a unique id per
     * `transaction` block; current_txn_id is the innermost active transaction
     * id (-1 when no transaction is open) so a `fail` statement can jump to the
     * right compensation epilogue. Saved/restored across nesting on the C call
     * stack by the transaction emitter. */
    int txn_counter;
    int current_txn_id;
} TranspilerCtx;

#include "transpiler_inventory_view.h"

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
    /* Stable diagnostic code (owning) attached to error_message.
     * NULL when the failing site has not been assigned a code.
     * Propagated from TranspilerCtx.backend_error_code. */
    char *error_code;
    /* Owning strdups of hint tags from TranspilerCtx.backend_error_*.
     * Both NULL when the failing site did not provide them. */
    char *error_cause_ir;
    char *error_fix_source;
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
void emit_async_block(ASTNode *node, TranspilerCtx *ctx);

/* Expressions return a C expression string (caller frees). */
char *emit_expression(ASTNode *node, TranspilerCtx *ctx);
char *emit_call(ASTNode *node, TranspilerCtx *ctx);
char *emit_binary(ASTNode *node, TranspilerCtx *ctx);
char *emit_unary(ASTNode *node, TranspilerCtx *ctx);

/* -----------------------------------------------------------------
 * Type mapping helpers
 * ----------------------------------------------------------------- */

/* "Int" -> "int", "String" -> "char*", "Slot<Int>" -> "PgySlot_Int" */
bool pergyra_type_to_c_copy(const char *pergyra_type_name,
                            char *out, size_t out_size);

/* "Int" -> "int", used for slot operation suffixes. */
const char *pergyra_primitive_to_c(const char *name);

/* "Slot<Int>" -> "Int", "SecureSlot<String>" -> "String" */
bool slot_inner_type_name_copy(const char *slot_type_name,
                               char *out,
                               size_t out_size);

/* -----------------------------------------------------------------
 * Built-in call emitters
 * ----------------------------------------------------------------- */

char *emit_builtin_log(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_log_banner(ASTNode *call, TranspilerCtx *ctx);
char *emit_builtin_log_raw(ASTNode *call, TranspilerCtx *ctx);

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

void emit_include_stmt(ASTNode *node, TranspilerCtx *ctx);
void emit_impl_ability(ASTNode *node, TranspilerCtx *ctx);

#endif /* PERGYRA_TRANSPILER_H */
