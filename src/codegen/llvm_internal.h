/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend internal header — shared types, context, and helpers.
 * Included by llvm_backend.c, llvm_expr.c, llvm_stmt.c, llvm_decl.c, llvm_domain.c
 */

#ifndef PGY_LLVM_INTERNAL_H
#define PGY_LLVM_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "../common/string_compat.h"
#include "../compiler/mir.h"

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

/* =================================================================
 * Constants
 * ================================================================= */

/* Fixed limits — bounded by nesting depth, reasonable for any program */
#define MAX_SCOPE_DEPTH     64
#define MAX_SCOPE_VARS      256
#define MAX_CLASS_FIELDS    64
#define MAX_EVENT_PARAMS    8
#define MAX_DEFER_PER_SCOPE 64
#define PGY_EVENT_MAX_HANDLERS 16
#define MAX_TYPE_SUBST      8

/* =================================================================
 * Dynamic array growth macro — eliminates static array overflow
 *
 * Usage:  PGY_DYNARR_ENSURE(ptr, count, capacity, Type)
 *   Before inserting at ptr[count], call this to guarantee capacity.
 *   On allocation failure, sets ctx->has_error and returns/continues.
 * ================================================================= */
#define PGY_DYNARR_ENSURE(arr, cnt, cap, T)                          \
    do {                                                              \
        if ((cnt) >= (cap)) {                                         \
            int _new_cap = (cap) == 0 ? 16 : (cap) * 2;              \
            T *_new = realloc((arr), (size_t)_new_cap * sizeof(T));   \
            if (_new == NULL) {                                       \
                llvm_set_error(ctx, "out of memory growing " #arr);   \
                return;                                               \
            }                                                         \
            memset(_new + (cap), 0,                                   \
                   (size_t)(_new_cap - (cap)) * sizeof(T));           \
            (arr) = _new;                                             \
            (cap) = _new_cap;                                         \
        }                                                             \
    } while (0)

/* Variant that returns NULL instead of void (for functions returning pointers) */
#define PGY_DYNARR_ENSURE_RET(arr, cnt, cap, T)                     \
    do {                                                              \
        if ((cnt) >= (cap)) {                                         \
            int _new_cap = (cap) == 0 ? 16 : (cap) * 2;              \
            T *_new = realloc((arr), (size_t)_new_cap * sizeof(T));   \
            if (_new == NULL) {                                       \
                llvm_set_error(ctx, "out of memory growing " #arr);   \
                return NULL;                                          \
            }                                                         \
            memset(_new + (cap), 0,                                   \
                   (size_t)(_new_cap - (cap)) * sizeof(T));           \
            (arr) = _new;                                             \
            (cap) = _new_cap;                                         \
        }                                                             \
    } while (0)

/* =================================================================
 * Pergyra type classification — eliminates repeated strcmp dispatch
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

    /* Generic container types — inner type parsed separately */
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

/* Type substitution entry (T → concrete LLVM type) */
typedef struct
{
    const char  *param_name;  /* "T" */
    LLVMTypeRef  llvm_type;   /* i32 */
    const char  *type_name;   /* "Int" */
} LLVMTypeSubst;

/* Result<T, E> specialization cache — parity with C backend's
 * ensure_result_specialization (transpiler_helpers_core_b.inc:1620).
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

    /* Scope stack — fixed depth (nesting rarely exceeds 64) */
    LLVMScopeFrame  scopes[MAX_SCOPE_DEPTH];
    int             scope_depth;

    LLVMValueRef    current_function;
    LLVMTypeRef     current_ret_type;
    ASTNode        *current_func_decl;
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

    LLVMTypeRef     secure_slot_type_Int;
    LLVMTypeRef     secure_slot_type_Long;
    LLVMTypeRef     secure_slot_type_Float;
    LLVMTypeRef     secure_slot_type_Double;
    LLVMTypeRef     secure_slot_type_Bool;
    LLVMTypeRef     secure_slot_type_String;

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

    /* Loop tracking — fixed depth (bounded by scope depth) */
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

    /* Generic monomorphization — dynamic */
    LLVMGenericTemplate  *generic_templates;
    int                   generic_template_count;
    int                   generic_template_capacity;

    LLVMMonoInstance     *mono_instances;
    int                   mono_count;
    int                   mono_capacity;

    /* Active type substitution map — small fixed size */
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

    /* Error state — structured with optional source location */
    bool            has_error;
    char            error_msg[512];
    uint32_t        error_line;    /* 0 = no location info */
    uint32_t        error_column;
} LLVMGenCtx;

static inline const char *
llvm_bind_current_host_name(LLVMGenCtx *ctx, const char *host_name)
{
    const char *saved_name = NULL;

    if (ctx == NULL)
        return NULL;
    saved_name = ctx->current_class_name;
    ctx->current_class_name = host_name;
    return saved_name;
}

static inline void
llvm_restore_current_host_name(LLVMGenCtx *ctx, const char *saved_name)
{
    if (ctx == NULL)
        return;
    ctx->current_class_name = saved_name;
}

static inline void
llvm_active_inventory(const LLVMGenCtx *ctx,
                      ASTNodeType decl_type,
                      ASTNode ***nodes_out,
                      size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL) {
        switch (decl_type) {
        case AST_FUNC_DECL: nodes = ctx->mir->functions; count = ctx->mir->function_count; break;
        case AST_INTENT_DECL: nodes = ctx->mir->intents; count = ctx->mir->intent_count; break;
        case AST_ABILITY_DECL: nodes = ctx->mir->abilities; count = ctx->mir->ability_count; break;
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

static inline const char *
llvm_decl_node_name(ASTNode *node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_FUNC_DECL:
        return node->data.func_decl.name;
    case AST_INTENT_DECL:
        return node->data.intent_decl.name;
    case AST_ABILITY_DECL:
        return node->data.ability_decl.name;
    case AST_ROLE_DECL:
        return node->data.role_decl.name;
    case AST_PARTY_DECL:
        return node->data.party_decl.name;
    case AST_ROSTER_DECL:
        return node->data.roster_decl.name;
    case AST_WORLD_DECL:
        return node->data.world_decl.name;
    case AST_RELATION_DECL:
        return node->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return node->data.effect_decl.name;
    case AST_ZONE_DECL:
        return node->data.zone_decl.name;
    case AST_EVENT_DECL:
        return node->data.event_decl.name;
    case AST_CLASS_DECL:
        return node->data.class_decl.name;
    case AST_ENUM_DECL:
        return node->data.enum_decl.name;
    case AST_TYPE_ALIAS:
        return node->data.type_alias.name;
    default:
        return NULL;
    }
}

static inline ASTNode *
llvm_find_decl_in_active_inventory(const LLVMGenCtx *ctx,
                                   ASTNodeType decl_type,
                                   const char *name)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    if (ctx->mir != NULL) {
        decl_header = mir_find_decl_header(ctx->mir, name);
        if (decl_header != NULL && decl_header->ast_type == decl_type)
            return decl_header->ast;
    }

    llvm_active_inventory(ctx, decl_type, &nodes, &count);
    for (size_t i = 0; i < count; i++) {
        ASTNode *node = nodes != NULL ? nodes[i] : NULL;
        const char *node_name;
        if (node == NULL || node->type != decl_type)
            continue;
        node_name = llvm_decl_node_name(node);
        if (node_name != NULL && strcmp(node_name, name) == 0)
            return node;
    }

    return NULL;
}

static inline bool
llvm_param_is_implicit_self(const FuncParam *param)
{
    return param != NULL
        && param->type == NULL
        && param->name != NULL
        && strcmp(param->name, "self") == 0;
}

static inline bool
llvm_is_host_decl_type(ASTNodeType decl_type)
{
    switch (decl_type) {
    case AST_CLASS_DECL:
    case AST_ENUM_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_ZONE_DECL:
    case AST_WORLD_DECL:
        return true;
    default:
        return false;
    }
}

static inline ASTNode *
llvm_find_host_decl_in_active_inventory(const LLVMGenCtx *ctx, const char *name)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode *decl = NULL;

    if (ctx == NULL || name == NULL)
        return NULL;

    if (ctx->mir != NULL) {
        decl_header = mir_find_decl_header(ctx->mir, name);
        if (decl_header != NULL && llvm_is_host_decl_type(decl_header->ast_type))
            return decl_header->ast;
    }

    decl = llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_ENUM_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_RELATION_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_EFFECT_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_ZONE_DECL, name);
    if (decl != NULL)
        return decl;
    return llvm_find_decl_in_active_inventory(ctx, AST_WORLD_DECL, name);
}

static inline const char *
llvm_current_host_decl_name(const LLVMGenCtx *ctx)
{
    ASTNode *decl = NULL;

    if (ctx == NULL)
        return NULL;

    if (ctx->current_func_decl != NULL
        && ctx->current_func_decl->type == AST_FUNC_DECL
        && ctx->current_func_decl->data.func_decl.within_zone != NULL) {
        decl = llvm_find_decl_in_active_inventory(
            ctx, AST_ZONE_DECL, ctx->current_func_decl->data.func_decl.within_zone);
        if (decl != NULL)
            return decl->data.zone_decl.name;
        return ctx->current_func_decl->data.func_decl.within_zone;
    }

    if (ctx->current_class_name == NULL)
        return NULL;

    decl = llvm_find_host_decl_in_active_inventory(ctx, ctx->current_class_name);
    if (decl == NULL)
        return ctx->current_class_name;

    switch (decl->type) {
    case AST_CLASS_DECL:
        return decl->data.class_decl.name;
    case AST_ENUM_DECL:
        return decl->data.enum_decl.name;
    case AST_RELATION_DECL:
        return decl->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return decl->data.effect_decl.name;
    case AST_ZONE_DECL:
        return decl->data.zone_decl.name;
    case AST_WORLD_DECL:
        return decl->data.world_decl.name;
    default:
        return ctx->current_class_name;
    }
}

static inline void
llvm_host_decl_methods(const MIRDeclHeader *decl_header,
                       ASTNode *decl,
                       ASTNode ***methods_out,
                       size_t *method_count_out)
{
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (decl_header != NULL && decl_header->ast == decl
        && llvm_is_host_decl_type(decl_header->ast_type)) {
        methods = decl_header->methods;
        method_count = decl_header->method_count;
    }

    if (methods == NULL && decl != NULL) {
        switch (decl->type) {
        case AST_CLASS_DECL:
            methods = decl->data.class_decl.methods;
            method_count = decl->data.class_decl.method_count;
            break;
        case AST_ENUM_DECL:
            methods = decl->data.enum_decl.methods;
            method_count = decl->data.enum_decl.method_count;
            break;
        case AST_RELATION_DECL:
            methods = decl->data.relation_decl.methods;
            method_count = decl->data.relation_decl.method_count;
            break;
        case AST_EFFECT_DECL:
            methods = decl->data.effect_decl.methods;
            method_count = decl->data.effect_decl.method_count;
            break;
        case AST_ZONE_DECL:
            methods = decl->data.zone_decl.methods;
            method_count = decl->data.zone_decl.method_count;
            break;
        case AST_WORLD_DECL:
            methods = decl->data.world_decl.methods;
            method_count = decl->data.world_decl.method_count;
            break;
        default:
            break;
        }
    }

    if (methods_out != NULL)
        *methods_out = methods;
    if (method_count_out != NULL)
        *method_count_out = method_count;
}

static inline ASTNode *
llvm_find_host_method_decl_in_context(const LLVMGenCtx *ctx,
                                      const char *host_type_name,
                                      const char *method_name)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode *decl = NULL;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    if (ctx->mir != NULL) {
        decl_header = mir_find_decl_header(ctx->mir, host_type_name);
        if (decl_header != NULL && llvm_is_host_decl_type(decl_header->ast_type))
            decl = decl_header->ast;
    }

    if (decl == NULL)
        decl = llvm_find_host_decl_in_active_inventory(ctx, host_type_name);
    if (decl == NULL)
        return NULL;

    llvm_host_decl_methods(decl_header, decl, &methods, &method_count);
    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods != NULL ? methods[i] : NULL;
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

static inline void
llvm_find_host_decl_methods_in_context(const LLVMGenCtx *ctx,
                                       const char *host_type_name,
                                       ASTNode ***methods_out,
                                       size_t *method_count_out)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode *decl = NULL;

    if (methods_out != NULL)
        *methods_out = NULL;
    if (method_count_out != NULL)
        *method_count_out = 0;
    if (ctx == NULL || host_type_name == NULL)
        return;

    if (ctx->mir != NULL) {
        decl_header = mir_find_decl_header(ctx->mir, host_type_name);
        if (decl_header != NULL && llvm_is_host_decl_type(decl_header->ast_type))
            decl = decl_header->ast;
    }

    if (decl == NULL)
        decl = llvm_find_host_decl_in_active_inventory(ctx, host_type_name);
    if (decl == NULL)
        return;

    llvm_host_decl_methods(decl_header, decl, methods_out, method_count_out);
}

static inline void
llvm_active_nominal_inventory(const LLVMGenCtx *ctx,
                              ASTNode ***nodes_out,
                              size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL) {
        nodes = ctx->mir->types;
        count = ctx->mir->type_count;
    }

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

typedef struct
{
    ASTNode **abilities;
    ASTNode **relations;
    ASTNode **effects;
    ASTNode **zones;
    ASTNode **worlds;
    ASTNode **parties;
    ASTNode **rosters;
    ASTNode **roles;
    ASTNode **events;
    size_t ability_count;
    size_t relation_count;
    size_t effect_count;
    size_t zone_count;
    size_t world_count;
    size_t party_count;
    size_t roster_count;
    size_t role_count;
    size_t event_count;
} LLVMDomainInventory;

static inline void
llvm_active_domain_inventory(const LLVMGenCtx *ctx,
                             LLVMDomainInventory *inventory)
{
    if (inventory == NULL)
        return;
    memset(inventory, 0, sizeof(*inventory));
    llvm_active_inventory(ctx, AST_ABILITY_DECL,
        &inventory->abilities, &inventory->ability_count);
    llvm_active_inventory(ctx, AST_RELATION_DECL,
        &inventory->relations, &inventory->relation_count);
    llvm_active_inventory(ctx, AST_EFFECT_DECL,
        &inventory->effects, &inventory->effect_count);
    llvm_active_inventory(ctx, AST_ZONE_DECL,
        &inventory->zones, &inventory->zone_count);
    llvm_active_inventory(ctx, AST_WORLD_DECL,
        &inventory->worlds, &inventory->world_count);
    llvm_active_inventory(ctx, AST_PARTY_DECL,
        &inventory->parties, &inventory->party_count);
    llvm_active_inventory(ctx, AST_ROSTER_DECL,
        &inventory->rosters, &inventory->roster_count);
    llvm_active_inventory(ctx, AST_ROLE_DECL,
        &inventory->roles, &inventory->role_count);
    llvm_active_inventory(ctx, AST_EVENT_DECL,
        &inventory->events, &inventory->event_count);
}

static inline void
llvm_active_executables(const LLVMGenCtx *ctx,
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

static inline void
llvm_active_externs(const LLVMGenCtx *ctx,
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

static inline ASTNode *
llvm_active_synthetic_executable_func(const LLVMGenCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return mir_find_function_decl(ctx->mir, "__pgy_top_level_exec");
    return NULL;
}

static inline bool
llvm_active_has_main_function(const LLVMGenCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return ctx->mir->has_main_function;
    return false;
}

static inline bool
llvm_active_has_top_level_exec(const LLVMGenCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return ctx->mir->has_top_level_exec;
    return false;
}

/* =================================================================
 * Context lifecycle (llvm_backend.c)
 * ================================================================= */
LLVMGenCtx *llvm_ctx_create(const char *module_name);
void         llvm_ctx_destroy(LLVMGenCtx *ctx);

/* =================================================================
 * Scope management (llvm_backend.c)
 * ================================================================= */
void          llvm_scope_push(LLVMGenCtx *ctx);
void          llvm_scope_pop(LLVMGenCtx *ctx);
void          llvm_scope_declare(LLVMGenCtx *ctx, const char *name,
                                  LLVMValueRef alloca, LLVMTypeRef type);
LLVMVarEntry *llvm_scope_lookup(LLVMGenCtx *ctx, const char *name);
void          llvm_defer_scope_push(LLVMGenCtx *ctx);
void          llvm_defer_scope_pop(LLVMGenCtx *ctx);
void          llvm_emit_defers_from(LLVMGenCtx *ctx, int from_depth);

void llvm_register_list_var(LLVMGenCtx *ctx, const char *var_name,
                            const char *inner_type);
const char *llvm_lookup_list_inner(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_set_var(LLVMGenCtx *ctx, const char *var_name,
                           const char *inner_type);
const char *llvm_lookup_set_inner(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_queue_var(LLVMGenCtx *ctx, const char *var_name,
                             const char *inner_type);
const char *llvm_lookup_queue_inner(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_map_var(LLVMGenCtx *ctx, const char *var_name,
                      const char *key_type, const char *value_type);
const char *llvm_lookup_map_key(LLVMGenCtx *ctx, const char *var_name);
const char *llvm_lookup_map_value(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_callable_var(LLVMGenCtx *ctx, const char *var_name,
                                ASTNode *type_node);
ASTNode *llvm_lookup_callable_var(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_typed_var(LLVMGenCtx *ctx, const char *var_name,
                             ASTNode *type_node);

/* =================================================================
 * Function registry (llvm_backend.c)
 * ================================================================= */
void           llvm_register_function(LLVMGenCtx *ctx, const char *name,
                                       LLVMValueRef fn, LLVMTypeRef fn_type,
                                       LLVMTypeRef ret_type);
void           llvm_set_function_flags(LLVMGenCtx *ctx, const char *name,
                                       bool is_action, bool action_self_only);
LLVMFuncEntry *llvm_lookup_function(LLVMGenCtx *ctx, const char *name);
LLVMFuncEntry *llvm_lookup_or_create_function(LLVMGenCtx *ctx, const char *name,
                                              LLVMTypeRef fn_type,
                                              LLVMTypeRef ret_type);
void           llvm_mark_function_as_used(LLVMGenCtx *ctx, const char *name);

/* =================================================================
 * Slot tracking (llvm_backend.c)
 * ================================================================= */
void          llvm_register_slot_var(LLVMGenCtx *ctx, const char *var_name,
                                     const char *inner_type,
                                     bool is_secure);
const char   *llvm_lookup_slot_inner(LLVMGenCtx *ctx, const char *var_name);
bool          llvm_lookup_slot_is_secure(LLVMGenCtx *ctx, const char *var_name);
void          llvm_register_view_var(LLVMGenCtx *ctx, const char *var_name,
                                     const char *source_slot,
                                     const char *inner_type,
                                     bool is_move_token);
LLVMViewVarEntry *llvm_lookup_view_var(LLVMGenCtx *ctx, const char *var_name);
LLVMTypeRef   llvm_slot_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef   llvm_secure_slot_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef   llvm_secure_token_type(LLVMGenCtx *ctx, const char *inner);
void          llvm_register_device_slot_var(LLVMGenCtx *ctx, const char *var_name,
                                             const char *inner_type);
const char   *llvm_lookup_device_slot_inner(LLVMGenCtx *ctx,
                                             const char *var_name);
void          llvm_mark_device_slot_released(LLVMGenCtx *ctx,
                                              const char *var_name);
LLVMVarEntry *llvm_lookup_secure_token_var(LLVMGenCtx *ctx,
                                            const char *slot_name);
void          llvm_register_future_var(LLVMGenCtx *ctx, const char *var_name,
                                        const char *inner_type,
                                        bool is_remote);
const char   *llvm_lookup_future_inner(LLVMGenCtx *ctx, const char *var_name);
bool          llvm_lookup_future_is_remote(LLVMGenCtx *ctx,
                                            const char *var_name);
void          llvm_register_channel_var(LLVMGenCtx *ctx, const char *var_name,
                                        const char *inner_type);
const char   *llvm_lookup_channel_inner(LLVMGenCtx *ctx, const char *var_name);

/* =================================================================
 * Class type registry (llvm_backend.c)
 * ================================================================= */
LLVMClassTypeEntry *llvm_register_class(LLVMGenCtx *ctx, const char *class_name,
                                          LLVMTypeRef struct_type,
                                          bool is_subject,
                                          bool is_pointer_self_host);
void                llvm_class_add_field(LLVMClassTypeEntry *entry,
                                          const char *field_name,
                                          LLVMTypeRef field_type, int index);
void                llvm_class_add_field_ex(LLVMClassTypeEntry *entry,
                                            const char *field_name,
                                            LLVMTypeRef field_type, int index,
                                            bool is_subject_slot);
LLVMClassTypeEntry *llvm_lookup_class(LLVMGenCtx *ctx, const char *class_name);
LLVMClassTypeEntry *llvm_lookup_class_by_struct_type(LLVMGenCtx *ctx,
                                                     LLVMTypeRef struct_type);
int                 llvm_class_field_index(LLVMClassTypeEntry *entry,
                                            const char *field_name);
void                llvm_register_var_class(LLVMGenCtx *ctx, const char *var_name,
                                             const char *class_name);
const char         *llvm_lookup_var_class(LLVMGenCtx *ctx, const char *var_name);
void                llvm_register_projection_borrow(LLVMGenCtx *ctx,
                                                    const char *var_name,
                                                    const char *class_name,
                                                    const char *source_name);
LLVMProjectionBorrowEntry *llvm_lookup_projection_borrow(LLVMGenCtx *ctx,
                                                         const char *var_name);
void                llvm_register_array_var(LLVMGenCtx *ctx, const char *var_name,
                                             LLVMTypeRef elem_type, int64_t length);
LLVMArrayVarEntry  *llvm_lookup_array_var(LLVMGenCtx *ctx, const char *var_name);
void                llvm_register_enum_variant(LLVMGenCtx *ctx,
                                                const char *enum_name,
                                                const char *variant_name,
                                                int value);
LLVMEnumVariantEntry *llvm_lookup_enum_variant(LLVMGenCtx *ctx,
                                                const char *variant_name);
LLVMEnumVariantEntry *llvm_lookup_enum_variant_qualified(LLVMGenCtx *ctx,
                                                          const char *enum_name,
                                                          const char *variant_name);

/* =================================================================
 * Event type registry (llvm_backend.c)
 * ================================================================= */
LLVMEventTypeEntry *llvm_lookup_event(LLVMGenCtx *ctx, const char *name);
LLVMEventTypeEntry *llvm_register_event(LLVMGenCtx *ctx, const char *name,
                                          LLVMTypeRef struct_type,
                                          int param_count, LLVMTypeRef *param_types);

/* =================================================================
 * Type helpers (llvm_backend.c)
 * ================================================================= */
LLVMTypeRef   pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);
LLVMTypeRef   ast_type_to_llvm(LLVMGenCtx *ctx, ASTNode *type_node);
LLVMTypeRef   llvm_resolve_inner_type(LLVMGenCtx *ctx, const char *type_name);
const char   *llvm_tmp_name(LLVMGenCtx *ctx);
LLVMValueRef  llvm_create_entry_alloca(LLVMGenCtx *ctx, LLVMTypeRef type,
                                        const char *name);

/* =================================================================
 * Generic monomorphization helpers (llvm_backend.c)
 * ================================================================= */
ASTNode    *llvm_lookup_generic_template(LLVMGenCtx *ctx, const char *name);
bool        llvm_mono_already_emitted(LLVMGenCtx *ctx, const char *mangled);
void        llvm_register_mono(LLVMGenCtx *ctx, const char *mangled);
const char *llvm_type_to_suffix(LLVMGenCtx *ctx, LLVMTypeRef ty);

/* =================================================================
 * Error reporting helpers (llvm_backend.c)
 *
 * llvm_set_error    — internal error (no source location)
 * llvm_set_error_at — error with source location from AST node
 * ================================================================= */
void llvm_set_error(LLVMGenCtx *ctx, const char *fmt, ...);
void llvm_set_error_at(LLVMGenCtx *ctx, ASTNode *node, const char *fmt, ...);

/* =================================================================
 * Result helpers (llvm_backend.c)
 * ================================================================= */
LLVMGenResult *llvm_result_error(const char *message);
LLVMGenResult *llvm_result_success(char *ir_text);

/* =================================================================
 * Pipeline helpers (llvm_backend.c / llvm_api.c)
 * ================================================================= */
bool llvm_validate_mir_for_codegen(const MIRProgram *mir, char **error_message);
bool llvm_emit_program_from_mir(const MIRProgram *mir, LLVMGenCtx *ctx);
void llvm_declare_runtime(LLVMGenCtx *ctx);
void llvm_set_type_render_ctx(LLVMGenCtx *ctx);
bool llvm_can_forward_declare_func_early(LLVMGenCtx *ctx, ASTNode *func);
bool llvm_nominal_uses_immutable_projection_storage(NominalDeclKind kind);
bool llvm_nominal_is_boundary_transfer_contract(NominalDeclKind kind);
void llvm_forward_declare_intent(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_intent_decl(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_main_wrapper(LLVMGenCtx *ctx);
void llvm_register_enum_decl(LLVMGenCtx *ctx, ASTNode *stmt);
void llvm_register_nominal_decl(LLVMGenCtx *ctx, ASTNode *stmt);
ASTNode *llvm_find_enum_decl(LLVMGenCtx *ctx, const char *enum_name);
void llvm_register_active_nominal_types(LLVMGenCtx *ctx);
void llvm_register_active_extern_prototypes(LLVMGenCtx *ctx);
bool llvm_type_name_uses_pointer_self(LLVMGenCtx *ctx, const char *type_name);

static inline bool
llvm_ast_type_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL
        || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        return false;
    }
    return llvm_type_name_uses_pointer_self(ctx, type_node->data.type.name);
}

/* =================================================================
 * Emitters — expressions (llvm_expr.c)
 * ================================================================= */
LLVMValueRef llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx);

/* =================================================================
 * Emitters — statements (llvm_stmt.c)
 * ================================================================= */
void llvm_emit_statement(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx);

/* =================================================================
 * Emitters — declarations (llvm_decl.c)
 * ================================================================= */
void llvm_forward_declare_func(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_func_decl(ASTNode *node, LLVMGenCtx *ctx);
LLVMValueRef llvm_emit_func_from_mir(const MIRRoutine *routine, LLVMGenCtx *ctx);
bool llvm_intent_involves_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *involves);

/* =================================================================
 * Emitters — domain (llvm_domain.c)
 * ================================================================= */
void llvm_emit_domain_passes(LLVMGenCtx *ctx);

/* =================================================================
 * Runtime declaration (llvm_backend.c)
 * ================================================================= */
void llvm_declare_runtime(LLVMGenCtx *ctx);

#endif /* PGY_LLVM_ENABLED */
#endif /* PGY_LLVM_INTERNAL_H */
