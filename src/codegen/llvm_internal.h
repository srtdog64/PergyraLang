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
} LLVMClassFieldInfo;

typedef struct
{
    const char        *class_name;
    LLVMTypeRef        struct_type;
    bool               is_subject;
    bool               is_pointer_self_host;
    bool               is_immutable;       /* true for object/tobject (read-only after construction) */
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
} LLVMQueueVarEntry;

typedef struct
{
    const char *var_name;
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

typedef struct LLVMGenCtx
{
    const HIRProgram *hir;
    const MIRProgram *mir;  /* MIR-based emission support */
    LLVMModuleRef   module;
    LLVMBuilderRef  builder;
    LLVMContextRef  context;

    /* Scope stack — fixed depth (nesting rarely exceeds 64) */
    LLVMScopeFrame  scopes[MAX_SCOPE_DEPTH];
    int             scope_depth;

    LLVMValueRef    current_function;
    LLVMTypeRef     current_ret_type;
    const char     *current_class_name;

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

    LLVMArrayVarEntry    *array_vars;
    int                   array_var_count;
    int                   array_var_capacity;

    LLVMListVarEntry     *list_vars;
    int                   list_var_count;
    int                   list_var_capacity;

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

    /* Slot sugar: suppress auto-Read when emitting slot handle arguments */
    bool            suppress_slot_auto_read;

    /* Error state — structured with optional source location */
    bool            has_error;
    char            error_msg[512];
    uint32_t        error_line;    /* 0 = no location info */
    uint32_t        error_column;
} LLVMGenCtx;

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
void llvm_register_queue_var(LLVMGenCtx *ctx, const char *var_name,
                             const char *inner_type);
const char *llvm_lookup_queue_inner(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_map_var(LLVMGenCtx *ctx, const char *var_name,
                      const char *value_type);
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
LLVMFuncEntry *llvm_lookup_function(LLVMGenCtx *ctx, const char *name);

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
LLVMClassTypeEntry *llvm_lookup_class(LLVMGenCtx *ctx, const char *class_name);
int                 llvm_class_field_index(LLVMClassTypeEntry *entry,
                                            const char *field_name);
void                llvm_register_var_class(LLVMGenCtx *ctx, const char *var_name,
                                             const char *class_name);
const char         *llvm_lookup_var_class(LLVMGenCtx *ctx, const char *var_name);
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

/* =================================================================
 * Emitters — domain (llvm_domain.c)
 * ================================================================= */
void llvm_emit_domain_passes(const HIRProgram *hir, LLVMGenCtx *ctx);

/* =================================================================
 * Runtime declaration (llvm_backend.c)
 * ================================================================= */
void llvm_declare_runtime(LLVMGenCtx *ctx);

#endif /* PGY_LLVM_ENABLED */
#endif /* PGY_LLVM_INTERNAL_H */
