/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend internal header: shared types, context, and helpers.
 * Included by llvm_backend.c, llvm_expr.c, llvm_stmt.c, llvm_decl.c, llvm_domain.c
 */

#ifndef PGY_LLVM_INTERNAL_H
#define PGY_LLVM_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "../common/string_compat.h"
#include "../compiler/mir.h"
#include "../compiler/verified_projection_plan.h"
#include "../semantic/diag_codes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/PassBuilder.h>

#include "llvm_limits_internal.h"
#include "llvm_debug_flags.h"
#include "llvm_type_kind.h"
#include "llvm_result_spec.h"

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
    LLVMValueRef binding;
    const char *inner_type;
    bool        released;
    bool        is_secure;
} LLVMSlotVarEntry;

typedef struct
{
    const char *var_name;
    LLVMValueRef binding;
    const char *source_slot;
    const char *inner_type;
    bool        is_move_token;
} LLVMViewVarEntry;

typedef struct
{
    const char *var_name;
    LLVMValueRef binding;
    const char *inner_type;
    bool        released;
} LLVMDeviceSlotVarEntry;

typedef struct
{
    const char *var_name;
    LLVMValueRef binding;
    const char *inner_type;
    bool        is_remote;
} LLVMFutureVarEntry;

typedef struct
{
    const char *var_name;
    LLVMValueRef binding;
    const char *inner_type;
} LLVMChannelVarEntry;

typedef struct
{
    const char   *name;
    const char   *inner;
    LLVMValueRef  ptr;
} LLVMChannelTarget;

typedef struct
{
    const char *var_name;
    LLVMValueRef binding;
    const char *inner_type;
} LLVMRcVarEntry;

typedef struct
{
    const char *var_name;
    LLVMValueRef binding;
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
    struct LLVMGenCtx *owner_ctx;
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
    LLVMValueRef binding;
    LLVMTypeRef  elem_type;
    const char  *elem_name;
    int64_t      length;
} LLVMArrayVarEntry;

typedef struct
{
    const char *var_name;
    LLVMValueRef binding;
    const char *inner_type;
} LLVMListVarEntry;

typedef struct
{
    const char *var_name;
    LLVMValueRef binding;
    const char *inner_type;
} LLVMSetVarEntry;

typedef struct
{
    const char *var_name;
    LLVMValueRef binding;
    const char *inner_type;
} LLVMQueueVarEntry;

typedef struct
{
    const char *var_name;
    LLVMValueRef binding;
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
    const char **param_type_names;
    const char  *return_type_name;
    const MIRCallableSig **param_callable_sigs;
    const MIRCallableSig *return_callable_sig;
    const MIRCallableSig *value_callable_sig;
    bool        is_closure; /* var holds a closure value {fn,env} (docs/135) */
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
    LLVMVarEntry *entries;
    int           count;
    int           capacity;
    const char   *last_lookup_name;
    LLVMVarEntry *last_lookup;
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

#include "llvm_generic_registry_types_internal.h"

typedef struct
{
    int slot_var_count;
    int view_var_count;
    int device_slot_var_count;
    int future_var_count;
    int channel_var_count;
    int rc_var_count;
    int weak_var_count;
    int var_class_count;
    int projection_borrow_count;
    int array_var_count;
    int list_var_count;
    int set_var_count;
    int queue_var_count;
    int map_var_count;
    int callable_var_count;
} LLVMLexicalRegistrySnapshot;

typedef struct LLVMGenCtx
{
    const MIRProgram *mir;  /* MIR-based emission support */
    const PgyVerifiedProjectionPlanRow *projection_plan;
    const PgyVerifiedParallelCapturePlan *parallel_capture_plan;
    const PgySpawnLanePlan *spawn_lane_plan; /* AIR-carried spawn lane facts */
    const PgyRegionPlan *region_plan;         /* AIR-carried region facts */
    LLVMModuleRef   module;
    LLVMBuilderRef  builder;
    LLVMContextRef  context;

    /* Opt-in debug info (gated on mir->source_path). di_scope is the current
     * function's DISubprogram, used as the scope for per-statement locations. */
    LLVMDIBuilderRef di_builder;
    LLVMMetadataRef  di_file;
    LLVMMetadataRef  di_cu;
    LLVMMetadataRef  di_scope;
    bool             di_enabled;

    /* Scope stack: fixed depth, dynamic entries per scope. */
    LLVMScopeFrame  scopes[MAX_SCOPE_DEPTH];
    int             scope_depth;

    LLVMValueRef    current_function;
    LLVMTypeRef     current_ret_type;
    LLVMTypeRef     current_function_ret_type;
    const char     *current_return_type_name;

    /* Innermost active `transaction` saga: the i1 alloca holding the failed
     * flag and the epilogue block a `fail` branches to. Both NULL when no
     * transaction is open; saved/restored across nesting on the C call stack
     * by the transaction emitter. txn_counter mints unique block-name ids. */
    LLVMValueRef      current_txn_failed_flag;
    LLVMBasicBlockRef current_txn_end_bb;
    int               txn_counter;

    /* Active inout value-parameter copy-in/copy-out state for the current
     * function. Mirrors the C backend's mut_ref_param tracking: the inout
     * parameter arrives as a pointer (mut_ref_ptr), is copied into a value
     * local (mut_ref_alloca) of type mut_ref_pt, and is written back before
     * every return. */
    LLVMValueRef    mut_ref_ptr[64];
    LLVMValueRef    mut_ref_alloca[64];
    LLVMTypeRef     mut_ref_pt[64];
    int             mut_ref_count;
    ASTNode        *current_return_callable_type;
    const char     *current_within_zone_name;
    ASTNode        *current_func_decl;
    const MIRRoutine *current_mir_routine;
    const MIRInstruction *current_mir_instruction;
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
    LLVMTypeRef     type_allocator;
    LLVMTypeRef     type_text_builder;
    LLVMTypeRef     type_region;
    LLVMValueRef    region_alloca;
    LLVMValueRef    region_owner_function;
    uint32_t        region_scope_id;
    bool            region_scope_active;
    int             parallel_counter;
    /* Expression-form parallel join (docs/181 R2): while emitting such a
     * wrapper body, `give` stores through this per-task result slot. */
    LLVMValueRef    pjoin_give_ptr;
    LLVMTypeRef     pjoin_give_type;
    /* any-join (docs/181 R3): non-NULL state ptr redirects `give` to a
     * CAS on the shared decision cell plus a winner-only result store. */
    LLVMValueRef    pjoin_any_state_ptr;
    LLVMValueRef    pjoin_any_res_ptr;

    LLVMTypeRef     slot_type_Int;
    LLVMTypeRef     slot_type_Long;
    LLVMTypeRef     slot_type_Float;
    LLVMTypeRef     slot_type_Double;
    LLVMTypeRef     slot_type_Bool;
    LLVMTypeRef     slot_type_String;

    /* LLVM machine-layer projection: DeviceSlot<T> must not reuse the
     * ordinary Slot<T> named type even when the field layout is identical. */
    LLVMTypeRef     device_slot_type_Int;
    LLVMTypeRef     device_slot_type_Long;
    LLVMTypeRef     device_slot_type_Float;
    LLVMTypeRef     device_slot_type_Double;
    LLVMTypeRef     device_slot_type_Bool;
    LLVMTypeRef     device_slot_type_String;

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

    /* One-level nested arrays: Array<Array<T>> for scalar T. Element type is
     * the inner array struct (array_type_Int, ...) stored by value. */
    LLVMTypeRef     array_type_Array_Int;
    LLVMTypeRef     array_type_Array_Long;
    LLVMTypeRef     array_type_Array_Float;
    LLVMTypeRef     array_type_Array_Double;
    LLVMTypeRef     array_type_Array_Bool;
    LLVMTypeRef     array_type_Array_String;

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

    LLVMClassTypeEntry  **class_types;
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

    /* Loop tracking: fixed depth (bounded by scope depth). */
    LLVMBasicBlockRef loop_continue_blocks[MAX_SCOPE_DEPTH];
    LLVMBasicBlockRef loop_break_blocks[MAX_SCOPE_DEPTH];
    int              loop_defer_base_depth[MAX_SCOPE_DEPTH];
    const char      *loop_labels[MAX_SCOPE_DEPTH];
    int              loop_depth;

    ASTNode         *defer_bodies[MAX_SCOPE_DEPTH][MAX_DEFER_PER_SCOPE];
    const MIRInstruction *defer_mir_instructions[MAX_SCOPE_DEPTH]
                                                     [MAX_DEFER_PER_SCOPE];
    int              defer_body_counts[MAX_SCOPE_DEPTH];
    int              defer_scope_depth;

    int             lambda_counter;
    int             tmp_counter;

    /* Generic monomorphization: dynamic. */
    LLVMGenericTemplate  *generic_templates;
    int                   generic_template_count;
    int                   generic_template_capacity;

    LLVMMonoInstance     *mono_instances;
    int                   mono_count;
    int                   mono_capacity;

    /* Active type substitution map: small fixed size. */
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
    /* The active call ABI owns the concrete type expected at its argument
     * boundary.  This is scoped by the call-argument emitter and must not be
     * retained as a source-level semantic type fact. */
    LLVMTypeRef         expected_abi_type;
    ASTNode            *expected_callable_type;

    /* Slot sugar: suppress auto-Read when emitting slot handle arguments */
    bool            suppress_slot_auto_read;

    /* Error state: structured with optional source location. */
    bool            has_error;
    char            error_msg[512];
    uint32_t        error_line;    /* 0 = no location info */
    uint32_t        error_column;
    /* Stable diagnostic code attached to error_msg. non-owning; must be
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
     * Lifetime binds to the enclosing LLVMGenCtx; initialised in
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

#include "llvm_type_projection_internal.h"
#include "llvm_debug_info_internal.h"
#include "llvm_inventory_internal.h"
#include "llvm_internal_api.h"

#endif /* PGY_LLVM_ENABLED */
#endif /* PGY_LLVM_INTERNAL_H */
