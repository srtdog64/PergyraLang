/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend internal header ??shared types, context, and helpers.
 * Included by llvm_backend.c, llvm_expr.c, llvm_stmt.c, llvm_decl.c, llvm_domain.c
 */

#ifndef PGY_LLVM_INTERNAL_H
#define PGY_LLVM_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "../common/string_compat.h"
#include "../compiler/mir.h"
#include "../semantic/diag_codes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/PassBuilder.h>

#include "llvm_limits_internal.h"
#include "llvm_debug_flags.h"

/* =================================================================
 * Pergyra type classification ??eliminates repeated strcmp dispatch
 *
 * Call pgy_classify_type() once on a type name string, then use
 * switch() everywhere else for exhaustive, typo-proof dispatch.
 * ================================================================= */

typedef enum
{
    PGY_TK_INT,
    PGY_TK_LONG,
    PGY_TK_FLOAT,
    PGY_TK_DOUBLE,
    PGY_TK_BOOL,
    PGY_TK_STRING,
    PGY_TK_VOID,
    PGY_TK_QUBIT_SLOT,
    PGY_TK_REMOTE_FUTURE,
    PGY_TK_DEVICE_SLOT,

    /* Generic container types ??inner type parsed separately */
    PGY_TK_SLOT,
    PGY_TK_SECURE_SLOT,
    PGY_TK_RESULT,
    PGY_TK_OPTION,
    PGY_TK_CHANNEL,
    PGY_TK_FUTURE,
    PGY_TK_BOX,
    PGY_TK_RC,
    PGY_TK_WEAK,
    PGY_TK_ARRAY,
    PGY_TK_SLICE,

    PGY_TK_CLASS,        /* user-defined class (not matched by classifier) */
    PGY_TK_UNKNOWN       /* type param, unresolved, or unrecognized */
} PgyTypeKind;

typedef struct LLVMGenCtx LLVMGenCtx;

/* Classify a Pergyra type name string into its kind.
 * Handles both primitive ("Int") and generic ("Slot<Int>") forms.
 * Returns PGY_TK_UNKNOWN for unrecognized names. */
PgyTypeKind pgy_classify_type(const char *type_name);

/* Map a PgyTypeKind (primitive only) to its LLVM type in the context.
 * Returns NULL for non-primitive kinds. */
LLVMTypeRef pgy_kind_to_llvm(LLVMGenCtx *ctx, PgyTypeKind kind);

/* Map a PgyTypeKind (primitive only) to its Pergyra name suffix.
 * Returns NULL for non-primitive kinds. */
const char *pgy_kind_to_suffix(PgyTypeKind kind);
LLVMTypeRef llvm_array_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef llvm_slice_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef llvm_list_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef llvm_set_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef llvm_queue_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef llvm_hashmap_struct_type(LLVMGenCtx *ctx, const char *value);
LLVMValueRef llvm_sizeof_type_i64(LLVMGenCtx *ctx, LLVMTypeRef type);

/* =================================================================
 * Type definitions
 * ================================================================= */

typedef struct
{
    const char   *name;
    LLVMValueRef  alloca;
    LLVMTypeRef   type;
} LLVMVarEntry;

typedef struct
{
    const char *var_name;
    const char *inner_type;
    bool        released;
    bool        is_secure;
} LLVMSlotVarEntry;

typedef struct
{
    const char *var_name;
    const char *source_slot;
    const char *inner_type;
    bool        is_move_token;
} LLVMViewVarEntry;

typedef struct
{
    const char *var_name;
    const char *inner_type;
    bool        released;
} LLVMDeviceSlotVarEntry;

typedef struct
{
    const char *var_name;
    const char *inner_type;
    bool        is_remote;
} LLVMFutureVarEntry;

typedef struct
{
    const char *var_name;
    const char *inner_type;
} LLVMChannelVarEntry;

typedef struct
{
    const char *var_name;
    const char *inner_type;
} LLVMRcVarEntry;

typedef struct
{
    const char *var_name;
    const char *inner_type;
} LLVMWeakVarEntry;

typedef struct
{
    const char  *field_name;
    LLVMTypeRef  field_type;
    int          index;
    bool         is_subject_slot;
} LLVMClassFieldInfo;

typedef enum
{
    LLVM_DOMAIN_NONE,
    LLVM_DOMAIN_PROJECTION,
    LLVM_DOMAIN_ZONE,
    LLVM_DOMAIN_WORLD,
    LLVM_DOMAIN_SYSTEMIC
} LLVMDomainKind;

typedef enum
{
    PGY_PROP_CAUSE_NONE = 0,
    PGY_PROP_CAUSE_REFRESH = 1,
    PGY_PROP_CAUSE_APPLY = 2,
    PGY_PROP_CAUSE_MAINTAIN = 3,
    PGY_PROP_CAUSE_DETACH = 4,
    PGY_PROP_CAUSE_LINK = 5,
    PGY_PROP_CAUSE_UNLINK = 6,
    PGY_PROP_CAUSE_WORLD_ACTIVATE = 7,
    PGY_PROP_CAUSE_WORLD_MAINTAIN = 8,
    PGY_PROP_CAUSE_WORLD_DEACTIVATE = 9,
    PGY_PROP_CAUSE_WORLD_DERIVED = 10,
    PGY_PROP_CAUSE_ACTION = 11,
} PgyPropagationCause;

typedef struct
{
    const char        *class_name;
    LLVMTypeRef        struct_type;
    bool               is_subject;
    bool               is_pointer_self_host;
    bool               is_immutable;       /* storage-level immutability for object/tobject */
    bool               is_boundary_transfer_contract; /* true only for tobject */
    LLVMDomainKind     domain_kind;
    const char        *sync_function_name;
    LLVMClassFieldInfo fields[MAX_CLASS_FIELDS];
    int                field_count;
} LLVMClassTypeEntry;

typedef struct
{
    const char *var_name;
    const char *class_name;
} LLVMVarClassEntry;

typedef struct
{
    const char *var_name;
    const char *class_name;
    const char *source_name;
} LLVMProjectionBorrowEntry;

typedef struct
{
    const char  *var_name;
    LLVMTypeRef  elem_type;
    int64_t      length;
} LLVMArrayVarEntry;

typedef struct
{
    const char *var_name;
    const char *inner_type;
} LLVMListVarEntry;

typedef struct
{
    const char *var_name;
    const char *inner_type;
} LLVMSetVarEntry;

typedef struct
{
    const char *var_name;
    const char *inner_type;
} LLVMQueueVarEntry;

typedef struct
{
    const char *var_name;
    const char *key_type;
    const char *value_type;
} LLVMMapVarEntry;

typedef struct
{
    const char *var_name;
    ASTNode    *type_node;
    ASTNode   **param_types;
    size_t      param_count;
    ASTNode    *return_type;
} LLVMCallableVarEntry;

typedef struct
{
    const char *enum_name;
    const char *variant_name;
    int         value;
} LLVMEnumVariantEntry;

typedef struct
{
    const char  *event_name;
    LLVMTypeRef  struct_type;
    int          param_count;
    LLVMTypeRef  param_types[MAX_EVENT_PARAMS];
} LLVMEventTypeEntry;

typedef struct
{
    LLVMVarEntry entries[MAX_SCOPE_VARS];
    int          count;
} LLVMScopeFrame;

typedef struct
{
    const char   *name;
    LLVMValueRef  fn;
    LLVMTypeRef   fn_type;
    LLVMTypeRef   ret_type;
    bool          is_action;
    bool          action_self_only;
} LLVMFuncEntry;

/* Generic template entry (for lazy monomorphization) */
typedef struct
{
    const char *name;
    ASTNode    *ast;
} LLVMGenericTemplate;

/* Monomorphized instance tracking */
typedef struct
{
    char *name;   /* heap-allocated, freed in ctx_destroy */
} LLVMMonoInstance;

/* Type substitution entry (T ??concrete LLVM type) */
typedef struct
{
    const char  *param_name;  /* "T" */
    LLVMTypeRef  llvm_type;   /* i32 */
    const char  *type_name;   /* "Int" */
} LLVMTypeSubst;

/* Result<T, E> specialization cache ??parity with C backend's
 * ensure_result_specialization (transpiler_helpers_core_b.h).
 * LLVM has no preprocessor, so each unique (T, E) gets a named struct
 * {i32 tag, ok_ty value, err_ty err} created once and reused. */
#define MAX_LLVM_RESULT_SPECS 32
typedef struct
{
    char         suffix[128];   /* "Int_NetError" */
    char         ok_name[64];   /* "Int" */
    char         err_name[64];  /* "NetError" */
    LLVMTypeRef  struct_ty;     /* named struct */
    LLVMTypeRef  ok_ty;
    LLVMTypeRef  err_ty;
} LLVMResultSpecEntry;

/* Result<T, E> context-aware suffix extractor.
 * Inspects ctx->expected_type_name then the enclosing function's return
 * type. On success fills suffix_out ("Int_NetError"), ok_out ("Int"),
 * err_out ("NetError"). Returns false if no enclosing Result<T,E> type
 * is visible from context. */
bool llvm_result_suffix_from_context(LLVMGenCtx *ctx,
                                     char *suffix_out, size_t suffix_n,
                                     char *ok_out, size_t ok_n,
                                     char *err_out, size_t err_n);

/* Best-effort resolution of a source-level type name to an LLVM type.
 * Handles primitives, classes/subjects, and enums (i32). Returns NULL
 * on miss. */
LLVMTypeRef llvm_resolve_source_type(LLVMGenCtx *ctx, const char *type_name);

/* Fetch or create the cached {i32 tag, ok_ty, err_ty} named struct for
 * Result<ok_name, err_name>. Calls llvm_set_error and returns NULL on
 * cache overflow or type-resolution failure. */
LLVMResultSpecEntry *llvm_ensure_result_type(LLVMGenCtx *ctx,
                                             const char *ok_name,
                                             const char *err_name);

typedef struct LLVMGenCtx
{
    const MIRProgram *mir;  /* MIR-based emission support */
    LLVMModuleRef   module;
    LLVMBuilderRef  builder;
    LLVMContextRef  context;

    /* Scope stack ??fixed depth (nesting rarely exceeds 64) */
    LLVMScopeFrame  scopes[MAX_SCOPE_DEPTH];
    int             scope_depth;

    LLVMValueRef    current_function;
    LLVMTypeRef     current_ret_type;
    ASTNode        *current_func_decl;
    ASTNode        *current_host_decl;
    const char     *current_class_name;
    bool            uses_intent_observability;

    /* --- Dynamic arrays: pointer + count + capacity --- */

    LLVMFuncEntry        *functions;
    int                   func_count;
    int                   func_capacity;

    /* Cached primitive types (set once in ctx_create) */
    LLVMTypeRef     type_i32;
    LLVMTypeRef     type_i64;
    LLVMTypeRef     type_f32;
    LLVMTypeRef     type_f64;
    LLVMTypeRef     type_i1;
    LLVMTypeRef     type_i8ptr;
    LLVMTypeRef     type_void;

    LLVMTypeRef     type_task_handle;
    int             parallel_counter;

    LLVMTypeRef     slot_type_Int;
    LLVMTypeRef     slot_type_Long;
    LLVMTypeRef     slot_type_Float;
    LLVMTypeRef     slot_type_Double;
    LLVMTypeRef     slot_type_Bool;
    LLVMTypeRef     slot_type_String;

    LLVMTypeRef     pinned_slot_type_Int;
    LLVMTypeRef     pinned_slot_type_Long;
    LLVMTypeRef     pinned_slot_type_Float;
    LLVMTypeRef     pinned_slot_type_Double;
    LLVMTypeRef     pinned_slot_type_Bool;
    LLVMTypeRef     pinned_slot_type_String;

    LLVMTypeRef     secure_slot_type_Int;
    LLVMTypeRef     secure_slot_type_Long;
    LLVMTypeRef     secure_slot_type_Float;
    LLVMTypeRef     secure_slot_type_Double;
    LLVMTypeRef     secure_slot_type_Bool;
    LLVMTypeRef     secure_slot_type_String;

    LLVMTypeRef     pinned_secure_slot_type_Int;
    LLVMTypeRef     pinned_secure_slot_type_Long;
    LLVMTypeRef     pinned_secure_slot_type_Float;
    LLVMTypeRef     pinned_secure_slot_type_Double;
    LLVMTypeRef     pinned_secure_slot_type_Bool;
    LLVMTypeRef     pinned_secure_slot_type_String;

    LLVMTypeRef     secure_token_type_Int;
    LLVMTypeRef     secure_token_type_Long;
    LLVMTypeRef     secure_token_type_Float;
    LLVMTypeRef     secure_token_type_Double;
    LLVMTypeRef     secure_token_type_Bool;
    LLVMTypeRef     secure_token_type_String;

    LLVMTypeRef     array_type_Int;
    LLVMTypeRef     array_type_Long;
    LLVMTypeRef     array_type_Float;
    LLVMTypeRef     array_type_Double;
    LLVMTypeRef     array_type_Bool;
    LLVMTypeRef     array_type_String;

    LLVMTypeRef     slice_type_Int;
    LLVMTypeRef     slice_type_Long;
    LLVMTypeRef     slice_type_Float;
    LLVMTypeRef     slice_type_Double;
    LLVMTypeRef     slice_type_Bool;
    LLVMTypeRef     slice_type_String;

    LLVMSlotVarEntry     *slot_vars;
    int                   slot_var_count;
    int                   slot_var_capacity;

    LLVMViewVarEntry     *view_vars;
    int                   view_var_count;
    int                   view_var_capacity;

    LLVMDeviceSlotVarEntry *device_slot_vars;
    int                     device_slot_var_count;
    int                     device_slot_var_capacity;

    LLVMFutureVarEntry    *future_vars;
    int                    future_var_count;
    int                    future_var_capacity;

    LLVMChannelVarEntry   *channel_vars;
    int                    channel_var_count;
    int                    channel_var_capacity;

    LLVMRcVarEntry        *rc_vars;
    int                    rc_var_count;
    int                    rc_var_capacity;

    LLVMWeakVarEntry      *weak_vars;
    int                    weak_var_count;
    int                    weak_var_capacity;

    LLVMClassTypeEntry   *class_types;
    int                   class_type_count;
    int                   class_type_capacity;

    LLVMVarClassEntry    *var_classes;
    int                   var_class_count;
    int                   var_class_capacity;

    LLVMProjectionBorrowEntry *projection_borrows;
    int                        projection_borrow_count;
    int                        projection_borrow_capacity;

    LLVMArrayVarEntry    *array_vars;
    int                   array_var_count;
    int                   array_var_capacity;

    LLVMListVarEntry     *list_vars;
    int                   list_var_count;
    int                   list_var_capacity;

    LLVMSetVarEntry      *set_vars;
    int                   set_var_count;
    int                   set_var_capacity;

    LLVMQueueVarEntry    *queue_vars;
    int                   queue_var_count;
    int                   queue_var_capacity;

    LLVMMapVarEntry      *map_vars;
    int                   map_var_count;
    int                   map_var_capacity;

    LLVMCallableVarEntry *callable_vars;
    int                   callable_var_count;
    int                   callable_var_capacity;

    LLVMEventTypeEntry   *event_types;
    int                   event_type_count;
    int                   event_type_capacity;

    LLVMEnumVariantEntry *enum_variants;
    int                   enum_variant_count;
    int                   enum_variant_capacity;

    /* Loop tracking ??fixed depth (bounded by scope depth) */
    LLVMBasicBlockRef loop_continue_blocks[MAX_SCOPE_DEPTH];
    LLVMBasicBlockRef loop_break_blocks[MAX_SCOPE_DEPTH];
    int              loop_defer_base_depth[MAX_SCOPE_DEPTH];
    const char      *loop_labels[MAX_SCOPE_DEPTH];
    int              loop_depth;

    ASTNode         *defer_bodies[MAX_SCOPE_DEPTH][MAX_DEFER_PER_SCOPE];
    int              defer_body_counts[MAX_SCOPE_DEPTH];
    int              defer_scope_depth;

    int             lambda_counter;
    int             tmp_counter;

    /* Generic monomorphization ??dynamic */
    LLVMGenericTemplate  *generic_templates;
    int                   generic_template_count;
    int                   generic_template_capacity;

    LLVMMonoInstance     *mono_instances;
    int                   mono_count;
    int                   mono_capacity;

    /* Active type substitution map ??small fixed size */
    LLVMTypeSubst   type_subst[MAX_TYPE_SUBST];
    int             type_subst_count;

    /* Result<T, E> named-struct cache (parity with C backend's
     * ensure_result_specialization). Used by Ok/Err/IsOk/IsErr/Unwrap and
     * match destructuring. */
    LLVMResultSpecEntry result_specs[MAX_LLVM_RESULT_SPECS];
    int                 result_spec_count;

    /* Source-level type name of the current let-binding annotation, if any.
     * Scratch pointer: saved/restored around initializer emission in
     * llvm_emit_let_decl. Consulted first by Result<T,E> suffix resolution
     * before falling back to the enclosing function's return type. */
    const char         *expected_type_name;

    /* Slot sugar: suppress auto-Read when emitting slot handle arguments */
    bool            suppress_slot_auto_read;

    /* Error state ??structured with optional source location */
    bool            has_error;
    char            error_msg[512];
    uint32_t        error_line;    /* 0 = no location info */
    uint32_t        error_column;
    /* Stable diagnostic code attached to error_msg. non-owning ??must be
     * a string literal (e.g. "PGY_LLVM_SPEC_LIMIT"). NULL when the failing
     * site has not been assigned a code. Propagated to CompilerResult.error_code
     * when the LLVM pipeline rolls up its result. */
    const char     *error_code;
    /* Optional hint tags attached alongside error_code. Both non-owning
     * static literals, NULL-or-set together with `error_code`. Propagated
     * through LLVMGenResult and CompilerResult into the runner's JSON
     * emit (see type_checker.h::Diagnostic for field semantics). */
    const char     *error_cause_ir;
    const char     *error_fix_source;

    /* Pass-local scratch arena: reused across LLVM lowering passes.
     * Lifetime binds to the enclosing LLVMGenCtx ??initialised in
     * llvm_ctx_create(), destroyed in llvm_ctx_destroy().  Used for
     * transient type-ref / name-buffer assembly that never escapes into
     * the LLVM module, class registry, or MIR. */
    PgyArena        scratch;
    /* Context-lifetime persistent arena: for LLVM-owned metadata that
     * outlives a single local emission helper but must still die with the
     * enclosing codegen context. Used for callable signatures and other
     * registry-backed arrays that must not be scratch-owned. */
    PgyArena        persistent;
} LLVMGenCtx;

#include "llvm_inventory_internal.h"
#include "llvm_internal_api.h"

#endif /* PGY_LLVM_ENABLED */
#endif /* PGY_LLVM_INTERNAL_H */
