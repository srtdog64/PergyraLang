/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend — Phase 1: expressions, functions, control flow
 *
 * This file is only compiled when PGY_LLVM_ENABLED is defined.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"
#include "../common/string_compat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/PassBuilder.h>

/* Types, context, and constants are defined in llvm_internal.h */

#if 0 /* --- removed: now in llvm_internal.h --- */
#define MAX_SCOPE_DEPTH 64
#define MAX_SCOPE_VARS  256
#define MAX_FUNCTIONS   256
#define MAX_SLOT_VARS   128
#define MAX_CLASS_TYPES 64
#define MAX_CLASS_FIELDS 64
#define MAX_EVENT_TYPES 32
#define PGY_EVENT_MAX_HANDLERS 16

typedef struct
{
    const char   *name;
    LLVMValueRef  alloca;
    LLVMTypeRef   type;
} LLVMVarEntry;

/* Slot variable tracking (name → inner type name) */
typedef struct
{
    const char *var_name;
    const char *inner_type;   /* "Int", "Long", "Float", "Double", "Bool", "String" */
    bool        released;     /* explicit Release() was called */
} LLVMSlotVarEntry;

/* Class type tracking (class name → struct type + field info) */
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
    LLVMClassFieldInfo fields[MAX_CLASS_FIELDS];
    int                field_count;
} LLVMClassTypeEntry;

/* Variable-to-class-type tracking */
typedef struct
{
    const char *var_name;
    const char *class_name;
} LLVMVarClassEntry;

/* Event type tracking */
typedef struct
{
    const char  *event_name;
    LLVMTypeRef  struct_type;     /* { [16 x ptr], i64 } */
    int          param_count;
    LLVMTypeRef  param_types[8];  /* handler parameter LLVM types */
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

typedef struct LLVMGenCtx
{
    LLVMModuleRef   module;
    LLVMBuilderRef  builder;
    LLVMContextRef  context;

    /* Scope stack (variable name → alloca mapping) */
    LLVMScopeFrame  scopes[MAX_SCOPE_DEPTH];
    int             scope_depth;

    /* Current function being emitted */
    LLVMValueRef    current_function;
    LLVMTypeRef     current_ret_type;

    /* Function table */
    LLVMFuncEntry   functions[MAX_FUNCTIONS];
    int             func_count;

    /* Cached types */
    LLVMTypeRef     type_i32;
    LLVMTypeRef     type_i64;
    LLVMTypeRef     type_f32;
    LLVMTypeRef     type_f64;
    LLVMTypeRef     type_i1;
    LLVMTypeRef     type_i8ptr;
    LLVMTypeRef     type_void;

    /* Thread pool */
    LLVMTypeRef     type_task_handle;  /* PgyTaskHandle = { i8* } */
    int             parallel_counter;  /* unique IDs for parallel wrappers */

    /* Slot struct types: { value_type, i1 } */
    LLVMTypeRef     slot_type_Int;
    LLVMTypeRef     slot_type_Long;
    LLVMTypeRef     slot_type_Float;
    LLVMTypeRef     slot_type_Double;
    LLVMTypeRef     slot_type_Bool;
    LLVMTypeRef     slot_type_String;

    /* Slot variable tracking */
    LLVMSlotVarEntry slot_vars[MAX_SLOT_VARS];
    int              slot_var_count;

    /* Class type registry */
    LLVMClassTypeEntry class_types[MAX_CLASS_TYPES];
    int                class_type_count;

    /* Variable → class name tracking */
    LLVMVarClassEntry  var_classes[MAX_SCOPE_VARS];
    int                var_class_count;

    /* Event type registry */
    LLVMEventTypeEntry event_types[MAX_EVENT_TYPES];
    int                event_type_count;

    /* Lambda counter */
    int             lambda_counter;

    /* Unique temp counter */
    int             tmp_counter;

    /* =================================================================
     * Generic monomorphization
     * ================================================================= */
#define MAX_GENERIC_FUNCS   64
#define MAX_MONO_INSTANCES 256

    /* Template storage: generic functions deferred from Pass 1 */
    struct {
        const char *name;
        ASTNode    *ast;
    } generic_templates[MAX_GENERIC_FUNCS];
    int generic_template_count;

    /* Monomorphized instances already emitted */
    struct {
        char name[256];
    } mono_instances[MAX_MONO_INSTANCES];
    int mono_count;

    /* Active type substitution map (T → concrete LLVM type) */
    struct {
        const char  *param_name;  /* "T" */
        LLVMTypeRef  llvm_type;   /* i32 */
        const char  *type_name;   /* "Int" */
    } type_subst[8];
    int type_subst_count;

    /* Slot sugar: suppress auto-Read when emitting slot handle arguments */
    bool            suppress_slot_auto_read;

    /* Error state */
    bool            has_error;
    char            error_msg[512];
} LLVMGenCtx;
#endif /* --- end removed block --- */

/* =================================================================
 * Type classification
 * ================================================================= */

PgyTypeKind
pgy_classify_type(const char *type_name)
{
    if (type_name == NULL)
        return PGY_TK_VOID;

    /* Primitives — exact match */
    switch (type_name[0]) {
    case 'I': if (strcmp(type_name, "Int") == 0)        return PGY_TK_INT;        break;
    case 'L': if (strcmp(type_name, "Long") == 0)       return PGY_TK_LONG;       break;
    case 'F':
        if (strcmp(type_name, "Float") == 0)            return PGY_TK_FLOAT;
        if (strncmp(type_name, "Future<", 7) == 0)     return PGY_TK_FUTURE;
        break;
    case 'D':
        if (strcmp(type_name, "Double") == 0)           return PGY_TK_DOUBLE;
        if (strncmp(type_name, "DeviceSlot<", 11) == 0) return PGY_TK_DEVICE_SLOT;
        break;
    case 'B':
        if (strcmp(type_name, "Bool") == 0)             return PGY_TK_BOOL;
        if (strncmp(type_name, "Box<", 4) == 0)        return PGY_TK_BOX;
        break;
    case 'S':
        if (strcmp(type_name, "String") == 0)           return PGY_TK_STRING;
        if (strncmp(type_name, "Slot<", 5) == 0)       return PGY_TK_SLOT;
        if (strcmp(type_name, "Slot") == 0)             return PGY_TK_SLOT;
        if (strncmp(type_name, "SecureSlot<", 11) == 0) return PGY_TK_SECURE_SLOT;
        if (strncmp(type_name, "Slice<", 6) == 0)      return PGY_TK_SLICE;
        break;
    case 'V': if (strcmp(type_name, "Void") == 0)       return PGY_TK_VOID;       break;
    case 'Q': if (strcmp(type_name, "QubitSlot") == 0)  return PGY_TK_QUBIT_SLOT; break;
    case 'R':
        if (strncmp(type_name, "ReadView<", 9) == 0)  return PGY_TK_SLOT;
        if (strncmp(type_name, "RemoteFuture<", 13) == 0) return PGY_TK_REMOTE_FUTURE;
        if (strncmp(type_name, "Result<", 7) == 0)     return PGY_TK_RESULT;
        if (strncmp(type_name, "Rc<", 3) == 0)         return PGY_TK_RC;
        break;
    case 'O':
        if (strncmp(type_name, "Option<", 7) == 0)     return PGY_TK_OPTION;
        break;
    case 'C':
        if (strncmp(type_name, "Channel<", 8) == 0)    return PGY_TK_CHANNEL;
        break;
    case 'W':
        if (strncmp(type_name, "WriteView<", 10) == 0) return PGY_TK_SLOT;
        if (strncmp(type_name, "Weak<", 5) == 0)       return PGY_TK_WEAK;
        break;
    case 'A':
        if (strncmp(type_name, "Array<", 6) == 0)      return PGY_TK_ARRAY;
        break;
    default:
        break;
    }
    return PGY_TK_UNKNOWN;
}

LLVMTypeRef
pgy_kind_to_llvm(LLVMGenCtx *ctx, PgyTypeKind kind)
{
    switch (kind) {
    case PGY_TK_INT:        return ctx->type_i32;
    case PGY_TK_LONG:       return ctx->type_i64;
    case PGY_TK_FLOAT:      return ctx->type_f32;
    case PGY_TK_DOUBLE:     return ctx->type_f64;
    case PGY_TK_BOOL:       return ctx->type_i1;
    case PGY_TK_STRING:     return ctx->type_i8ptr;
    case PGY_TK_QUBIT_SLOT: return ctx->type_i32;
    case PGY_TK_REMOTE_FUTURE: return ctx->type_task_handle;
    case PGY_TK_VOID:       return ctx->type_void;
    default:                 return NULL;
    }
}

const char *
pgy_kind_to_suffix(PgyTypeKind kind)
{
    switch (kind) {
    case PGY_TK_INT:    return "Int";
    case PGY_TK_LONG:   return "Long";
    case PGY_TK_FLOAT:  return "Float";
    case PGY_TK_DOUBLE: return "Double";
    case PGY_TK_BOOL:       return "Bool";
    case PGY_TK_STRING:     return "String";
    case PGY_TK_QUBIT_SLOT: return "QubitSlot";
    default:                return NULL;
    }
}

/* =================================================================
 * Context lifecycle
 * ================================================================= */

LLVMGenCtx *
llvm_ctx_create(const char *module_name)
{
    LLVMGenCtx *ctx = calloc(1, sizeof(LLVMGenCtx));
    if (ctx == NULL)
        return NULL;

    ctx->context = LLVMContextCreate();
    ctx->module  = LLVMModuleCreateWithNameInContext(module_name, ctx->context);
    ctx->builder = LLVMCreateBuilderInContext(ctx->context);

    ctx->type_i32   = LLVMInt32TypeInContext(ctx->context);
    ctx->type_i64   = LLVMInt64TypeInContext(ctx->context);
    ctx->type_f32   = LLVMFloatTypeInContext(ctx->context);
    ctx->type_f64   = LLVMDoubleTypeInContext(ctx->context);
    ctx->type_i1    = LLVMInt1TypeInContext(ctx->context);
    ctx->type_i8ptr = LLVMPointerTypeInContext(ctx->context, 0);
    ctx->type_void  = LLVMVoidTypeInContext(ctx->context);

    ctx->scope_depth   = 0;
    ctx->tmp_counter   = 0;
    ctx->func_count    = 0;
    ctx->slot_var_count = 0;
    ctx->class_type_count = 0;
    ctx->event_type_count = 0;
    ctx->lambda_counter = 0;
    ctx->var_class_count = 0;
    ctx->array_var_count = 0;
    ctx->enum_variant_count = 0;
    ctx->loop_depth = 0;
    ctx->current_ret_type = ctx->type_i32;

    /* Slot struct types: { value_type, i1 (occupied) } */
    {
        LLVMTypeRef fields_int[] = { ctx->type_i32, ctx->type_i1 };
        ctx->slot_type_Int = LLVMStructCreateNamed(ctx->context, "PgySlot_Int");
        LLVMStructSetBody(ctx->slot_type_Int, fields_int, 2, 0);

        LLVMTypeRef fields_long[] = { ctx->type_i64, ctx->type_i1 };
        ctx->slot_type_Long = LLVMStructCreateNamed(ctx->context, "PgySlot_Long");
        LLVMStructSetBody(ctx->slot_type_Long, fields_long, 2, 0);

        LLVMTypeRef fields_float[] = { ctx->type_f32, ctx->type_i1 };
        ctx->slot_type_Float = LLVMStructCreateNamed(ctx->context, "PgySlot_Float");
        LLVMStructSetBody(ctx->slot_type_Float, fields_float, 2, 0);

        LLVMTypeRef fields_double[] = { ctx->type_f64, ctx->type_i1 };
        ctx->slot_type_Double = LLVMStructCreateNamed(ctx->context, "PgySlot_Double");
        LLVMStructSetBody(ctx->slot_type_Double, fields_double, 2, 0);

        LLVMTypeRef fields_bool[] = { ctx->type_i1, ctx->type_i1 };
        ctx->slot_type_Bool = LLVMStructCreateNamed(ctx->context, "PgySlot_Bool");
        LLVMStructSetBody(ctx->slot_type_Bool, fields_bool, 2, 0);

        LLVMTypeRef fields_str[] = { ctx->type_i8ptr, ctx->type_i1 };
        ctx->slot_type_String = LLVMStructCreateNamed(ctx->context, "PgySlot_String");
        LLVMStructSetBody(ctx->slot_type_String, fields_str, 2, 0);

        LLVMTypeRef secure_fields_int[] = { ctx->type_i32, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_Int = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_Int");
        LLVMStructSetBody(ctx->secure_slot_type_Int, secure_fields_int, 3, 0);

        LLVMTypeRef secure_fields_long[] = { ctx->type_i64, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_Long = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_Long");
        LLVMStructSetBody(ctx->secure_slot_type_Long, secure_fields_long, 3, 0);

        LLVMTypeRef secure_fields_float[] = { ctx->type_f32, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_Float = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_Float");
        LLVMStructSetBody(ctx->secure_slot_type_Float, secure_fields_float, 3, 0);

        LLVMTypeRef secure_fields_double[] = { ctx->type_f64, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_Double = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_Double");
        LLVMStructSetBody(ctx->secure_slot_type_Double, secure_fields_double, 3, 0);

        LLVMTypeRef secure_fields_bool[] = { ctx->type_i1, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_Bool = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_Bool");
        LLVMStructSetBody(ctx->secure_slot_type_Bool, secure_fields_bool, 3, 0);

        LLVMTypeRef secure_fields_str[] = { ctx->type_i8ptr, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_String = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_String");
        LLVMStructSetBody(ctx->secure_slot_type_String, secure_fields_str, 3, 0);

        LLVMTypeRef token_fields[] = { ctx->type_i64, ctx->type_i1, ctx->type_i1 };
        ctx->secure_token_type_Int = LLVMStructCreateNamed(ctx->context, "PgyToken_Int");
        LLVMStructSetBody(ctx->secure_token_type_Int, token_fields, 3, 0);
        ctx->secure_token_type_Long = LLVMStructCreateNamed(ctx->context, "PgyToken_Long");
        LLVMStructSetBody(ctx->secure_token_type_Long, token_fields, 3, 0);
        ctx->secure_token_type_Float = LLVMStructCreateNamed(ctx->context, "PgyToken_Float");
        LLVMStructSetBody(ctx->secure_token_type_Float, token_fields, 3, 0);
        ctx->secure_token_type_Double = LLVMStructCreateNamed(ctx->context, "PgyToken_Double");
        LLVMStructSetBody(ctx->secure_token_type_Double, token_fields, 3, 0);
        ctx->secure_token_type_Bool = LLVMStructCreateNamed(ctx->context, "PgyToken_Bool");
        LLVMStructSetBody(ctx->secure_token_type_Bool, token_fields, 3, 0);
        ctx->secure_token_type_String = LLVMStructCreateNamed(ctx->context, "PgyToken_String");
        LLVMStructSetBody(ctx->secure_token_type_String, token_fields, 3, 0);

        LLVMTypeRef arr_fields_int[] = {
            LLVMPointerType(ctx->type_i32, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_Int = LLVMStructCreateNamed(ctx->context, "PgyArray_Int");
        LLVMStructSetBody(ctx->array_type_Int, arr_fields_int, 4, 0);

        LLVMTypeRef arr_fields_long[] = {
            LLVMPointerType(ctx->type_i64, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_Long = LLVMStructCreateNamed(ctx->context, "PgyArray_Long");
        LLVMStructSetBody(ctx->array_type_Long, arr_fields_long, 4, 0);

        LLVMTypeRef arr_fields_float[] = {
            LLVMPointerType(ctx->type_f32, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_Float = LLVMStructCreateNamed(ctx->context, "PgyArray_Float");
        LLVMStructSetBody(ctx->array_type_Float, arr_fields_float, 4, 0);

        LLVMTypeRef arr_fields_double[] = {
            LLVMPointerType(ctx->type_f64, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_Double = LLVMStructCreateNamed(ctx->context, "PgyArray_Double");
        LLVMStructSetBody(ctx->array_type_Double, arr_fields_double, 4, 0);

        LLVMTypeRef arr_fields_bool[] = {
            LLVMPointerType(ctx->type_i1, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_Bool = LLVMStructCreateNamed(ctx->context, "PgyArray_Bool");
        LLVMStructSetBody(ctx->array_type_Bool, arr_fields_bool, 4, 0);

        LLVMTypeRef arr_fields_string[] = {
            LLVMPointerType(ctx->type_i8ptr, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_String = LLVMStructCreateNamed(ctx->context, "PgyArray_String");
        LLVMStructSetBody(ctx->array_type_String, arr_fields_string, 4, 0);

        LLVMTypeRef slice_fields_int[] = { LLVMPointerType(ctx->type_i32, 0), ctx->type_i64 };
        ctx->slice_type_Int = LLVMStructCreateNamed(ctx->context, "PgySlice_Int");
        LLVMStructSetBody(ctx->slice_type_Int, slice_fields_int, 2, 0);

        LLVMTypeRef slice_fields_long[] = { LLVMPointerType(ctx->type_i64, 0), ctx->type_i64 };
        ctx->slice_type_Long = LLVMStructCreateNamed(ctx->context, "PgySlice_Long");
        LLVMStructSetBody(ctx->slice_type_Long, slice_fields_long, 2, 0);

        LLVMTypeRef slice_fields_float[] = { LLVMPointerType(ctx->type_f32, 0), ctx->type_i64 };
        ctx->slice_type_Float = LLVMStructCreateNamed(ctx->context, "PgySlice_Float");
        LLVMStructSetBody(ctx->slice_type_Float, slice_fields_float, 2, 0);

        LLVMTypeRef slice_fields_double[] = { LLVMPointerType(ctx->type_f64, 0), ctx->type_i64 };
        ctx->slice_type_Double = LLVMStructCreateNamed(ctx->context, "PgySlice_Double");
        LLVMStructSetBody(ctx->slice_type_Double, slice_fields_double, 2, 0);

        LLVMTypeRef slice_fields_bool[] = { LLVMPointerType(ctx->type_i1, 0), ctx->type_i64 };
        ctx->slice_type_Bool = LLVMStructCreateNamed(ctx->context, "PgySlice_Bool");
        LLVMStructSetBody(ctx->slice_type_Bool, slice_fields_bool, 2, 0);

        LLVMTypeRef slice_fields_string[] = { LLVMPointerType(ctx->type_i8ptr, 0), ctx->type_i64 };
        ctx->slice_type_String = LLVMStructCreateNamed(ctx->context, "PgySlice_String");
        LLVMStructSetBody(ctx->slice_type_String, slice_fields_string, 2, 0);
    }

    return ctx;
}

void
llvm_ctx_destroy(LLVMGenCtx *ctx)
{
    if (ctx == NULL)
        return;

    if (ctx->builder != NULL)
        LLVMDisposeBuilder(ctx->builder);
    if (ctx->module != NULL)
        LLVMDisposeModule(ctx->module);
    if (ctx->context != NULL)
        LLVMContextDispose(ctx->context);

    /* Free dynamic arrays */
    free(ctx->functions);
    free(ctx->slot_vars);
    free(ctx->view_vars);
    free(ctx->device_slot_vars);
    free(ctx->future_vars);
    free(ctx->class_types);
    free(ctx->var_classes);
    free(ctx->array_vars);
    free(ctx->event_types);
    free(ctx->enum_variants);
    free(ctx->generic_templates);

    /* Free heap-allocated mono instance names */
    for (int i = 0; i < ctx->mono_count; i++)
        free(ctx->mono_instances[i].name);
    free(ctx->mono_instances);

    free(ctx);
}

/* =================================================================
 * Scope management
 * ================================================================= */

void
llvm_scope_push(LLVMGenCtx *ctx)
{
    if (ctx->scope_depth >= MAX_SCOPE_DEPTH) {
        llvm_set_error(ctx, "Scope depth overflow (max %d)", MAX_SCOPE_DEPTH);
        return;
    }
    ctx->scopes[ctx->scope_depth].count = 0;
    ctx->scope_depth++;
}

void
llvm_scope_pop(LLVMGenCtx *ctx)
{
    if (ctx->scope_depth > 0)
        ctx->scope_depth--;
}

void
llvm_scope_declare(LLVMGenCtx *ctx, const char *name,
                   LLVMValueRef alloca_val, LLVMTypeRef type)
{
    if (ctx->scope_depth == 0)
        return;

    LLVMScopeFrame *frame = &ctx->scopes[ctx->scope_depth - 1];
    if (frame->count >= MAX_SCOPE_VARS) {
        llvm_set_error(ctx, "Too many variables in scope (max %d)", MAX_SCOPE_VARS);
        return;
    }

    frame->entries[frame->count].name   = name;
    frame->entries[frame->count].alloca = alloca_val;
    frame->entries[frame->count].type   = type;
    frame->count++;
}

LLVMVarEntry *
llvm_scope_lookup(LLVMGenCtx *ctx, const char *name)
{
    for (int i = ctx->scope_depth - 1; i >= 0; i--) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = frame->count - 1; j >= 0; j--) {
            if (strcmp(frame->entries[j].name, name) == 0)
                return &frame->entries[j];
        }
    }
    return NULL;
}

/* =================================================================
 * Function table
 * ================================================================= */

void
llvm_register_function(LLVMGenCtx *ctx, const char *name,
                       LLVMValueRef fn, LLVMTypeRef fn_type,
                       LLVMTypeRef ret_type)
{
    PGY_DYNARR_ENSURE(ctx->functions, ctx->func_count,
                       ctx->func_capacity, LLVMFuncEntry);

    ctx->functions[ctx->func_count].name     = name;
    ctx->functions[ctx->func_count].fn       = fn;
    ctx->functions[ctx->func_count].fn_type  = fn_type;
    ctx->functions[ctx->func_count].ret_type = ret_type;
    ctx->func_count++;
}

LLVMFuncEntry *
llvm_lookup_function(LLVMGenCtx *ctx, const char *name)
{
    for (int i = 0; i < ctx->func_count; i++) {
        if (strcmp(ctx->functions[i].name, name) == 0)
            return &ctx->functions[i];
    }
    return NULL;
}

/* Forward declaration for type mapping (used by slot helpers) */
LLVMTypeRef pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);

/* =================================================================
 * Slot variable tracking
 * ================================================================= */

void
llvm_register_slot_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *inner_type,
                       bool is_secure)
{
    PGY_DYNARR_ENSURE(ctx->slot_vars, ctx->slot_var_count,
                       ctx->slot_var_capacity, LLVMSlotVarEntry);

    ctx->slot_vars[ctx->slot_var_count].var_name   = var_name;
    ctx->slot_vars[ctx->slot_var_count].inner_type = inner_type;
    ctx->slot_vars[ctx->slot_var_count].released   = false;
    ctx->slot_vars[ctx->slot_var_count].is_secure  = is_secure;
    ctx->slot_var_count++;
}

void
llvm_register_view_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *source_slot, const char *inner_type,
                       bool is_move_token)
{
    PGY_DYNARR_ENSURE(ctx->view_vars, ctx->view_var_count,
                      ctx->view_var_capacity, LLVMViewVarEntry);

    ctx->view_vars[ctx->view_var_count].var_name = var_name;
    ctx->view_vars[ctx->view_var_count].source_slot = source_slot;
    ctx->view_vars[ctx->view_var_count].inner_type = inner_type;
    ctx->view_vars[ctx->view_var_count].is_move_token = is_move_token;
    ctx->view_var_count++;
}

LLVMViewVarEntry *
llvm_lookup_view_var(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->view_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->view_vars[i].var_name, var_name) == 0)
            return &ctx->view_vars[i];
    }
    return NULL;
}

const char *
llvm_lookup_slot_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->slot_vars[i].var_name, var_name) == 0)
            return ctx->slot_vars[i].inner_type;
    }
    return NULL;
}

bool
llvm_lookup_slot_is_secure(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->slot_vars[i].var_name, var_name) == 0)
            return ctx->slot_vars[i].is_secure;
    }
    return false;
}

LLVMTypeRef
llvm_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->slot_type_Int;
    case PGY_TK_LONG:   return ctx->slot_type_Long;
    case PGY_TK_FLOAT:  return ctx->slot_type_Float;
    case PGY_TK_DOUBLE: return ctx->slot_type_Double;
    case PGY_TK_BOOL:   return ctx->slot_type_Bool;
    case PGY_TK_STRING: return ctx->slot_type_String;
    default: {
        LLVMTypeRef inner_ty = pergyra_type_to_llvm(ctx, inner);
        LLVMTypeRef fields[] = { inner_ty, LLVMInt1TypeInContext(ctx->context) };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    }
}

LLVMTypeRef
llvm_secure_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->secure_slot_type_Int;
    case PGY_TK_LONG:   return ctx->secure_slot_type_Long;
    case PGY_TK_FLOAT:  return ctx->secure_slot_type_Float;
    case PGY_TK_DOUBLE: return ctx->secure_slot_type_Double;
    case PGY_TK_BOOL:   return ctx->secure_slot_type_Bool;
    case PGY_TK_STRING: return ctx->secure_slot_type_String;
    default:            return ctx->secure_slot_type_Int;
    }
}

LLVMTypeRef
llvm_secure_token_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->secure_token_type_Int;
    case PGY_TK_LONG:   return ctx->secure_token_type_Long;
    case PGY_TK_FLOAT:  return ctx->secure_token_type_Float;
    case PGY_TK_DOUBLE: return ctx->secure_token_type_Double;
    case PGY_TK_BOOL:   return ctx->secure_token_type_Bool;
    case PGY_TK_STRING: return ctx->secure_token_type_String;
    default:            return ctx->secure_token_type_Int;
    }
}

LLVMTypeRef
llvm_array_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->array_type_Int;
    case PGY_TK_LONG:   return ctx->array_type_Long;
    case PGY_TK_FLOAT:  return ctx->array_type_Float;
    case PGY_TK_DOUBLE: return ctx->array_type_Double;
    case PGY_TK_BOOL:   return ctx->array_type_Bool;
    case PGY_TK_STRING: return ctx->array_type_String;
    default: {
        LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
        LLVMTypeRef fields[] = {
            LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
    }
    }
}

LLVMTypeRef
llvm_slice_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->slice_type_Int;
    case PGY_TK_LONG:   return ctx->slice_type_Long;
    case PGY_TK_FLOAT:  return ctx->slice_type_Float;
    case PGY_TK_DOUBLE: return ctx->slice_type_Double;
    case PGY_TK_BOOL:   return ctx->slice_type_Bool;
    case PGY_TK_STRING: return ctx->slice_type_String;
    default: {
        LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
        LLVMTypeRef fields[] = {
            LLVMPointerType(elem_ty, 0), ctx->type_i64
        };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    }
}

void
llvm_register_device_slot_var(LLVMGenCtx *ctx, const char *var_name,
                              const char *inner_type)
{
    PGY_DYNARR_ENSURE(ctx->device_slot_vars, ctx->device_slot_var_count,
                      ctx->device_slot_var_capacity, LLVMDeviceSlotVarEntry);

    ctx->device_slot_vars[ctx->device_slot_var_count].var_name = var_name;
    ctx->device_slot_vars[ctx->device_slot_var_count].inner_type = inner_type;
    ctx->device_slot_vars[ctx->device_slot_var_count].released = false;
    ctx->device_slot_var_count++;
}

const char *
llvm_lookup_device_slot_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->device_slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->device_slot_vars[i].var_name, var_name) == 0)
            return ctx->device_slot_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_mark_device_slot_released(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->device_slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->device_slot_vars[i].var_name, var_name) == 0) {
            ctx->device_slot_vars[i].released = true;
            return;
        }
    }
}

LLVMVarEntry *
llvm_lookup_secure_token_var(LLVMGenCtx *ctx, const char *slot_name)
{
    char token_name[256];
    if (slot_name == NULL)
        return NULL;
    snprintf(token_name, sizeof(token_name), "%s_token", slot_name);
    return llvm_scope_lookup(ctx, token_name);
}

void
llvm_register_future_var(LLVMGenCtx *ctx, const char *var_name,
                         const char *inner_type,
                         bool is_remote)
{
    PGY_DYNARR_ENSURE(ctx->future_vars, ctx->future_var_count,
                      ctx->future_var_capacity, LLVMFutureVarEntry);

    ctx->future_vars[ctx->future_var_count].var_name = var_name;
    ctx->future_vars[ctx->future_var_count].inner_type = inner_type;
    ctx->future_vars[ctx->future_var_count].is_remote = is_remote;
    ctx->future_var_count++;
}

const char *
llvm_lookup_future_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->future_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->future_vars[i].var_name, var_name) == 0)
            return ctx->future_vars[i].inner_type;
    }
    return NULL;
}

bool
llvm_lookup_future_is_remote(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->future_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->future_vars[i].var_name, var_name) == 0)
            return ctx->future_vars[i].is_remote;
    }
    return false;
}

void
llvm_register_channel_var(LLVMGenCtx *ctx, const char *var_name,
                          const char *inner_type)
{
    PGY_DYNARR_ENSURE(ctx->channel_vars, ctx->channel_var_count,
                      ctx->channel_var_capacity, LLVMChannelVarEntry);

    ctx->channel_vars[ctx->channel_var_count].var_name = var_name;
    ctx->channel_vars[ctx->channel_var_count].inner_type = inner_type;
    ctx->channel_var_count++;
}

const char *
llvm_lookup_channel_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->channel_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->channel_vars[i].var_name, var_name) == 0)
            return ctx->channel_vars[i].inner_type;
    }
    return NULL;
}

/* =================================================================
 * Class type tracking
 * ================================================================= */

LLVMClassTypeEntry *
llvm_register_class(LLVMGenCtx *ctx, const char *class_name,
                    LLVMTypeRef struct_type)
{
    PGY_DYNARR_ENSURE_RET(ctx->class_types, ctx->class_type_count,
                            ctx->class_type_capacity, LLVMClassTypeEntry);

    LLVMClassTypeEntry *entry = &ctx->class_types[ctx->class_type_count++];
    entry->class_name  = class_name;
    entry->struct_type = struct_type;
    entry->field_count = 0;
    return entry;
}

void
llvm_class_add_field(LLVMClassTypeEntry *entry, const char *field_name,
                     LLVMTypeRef field_type, int index)
{
    if (entry->field_count >= MAX_CLASS_FIELDS)
        return;

    entry->fields[entry->field_count].field_name = field_name;
    entry->fields[entry->field_count].field_type = field_type;
    entry->fields[entry->field_count].index      = index;
    entry->field_count++;
}

LLVMClassTypeEntry *
llvm_lookup_class(LLVMGenCtx *ctx, const char *class_name)
{
    for (int i = 0; i < ctx->class_type_count; i++) {
        if (strcmp(ctx->class_types[i].class_name, class_name) == 0)
            return &ctx->class_types[i];
    }
    return NULL;
}

int
llvm_class_field_index(LLVMClassTypeEntry *entry, const char *field_name)
{
    for (int i = 0; i < entry->field_count; i++) {
        if (strcmp(entry->fields[i].field_name, field_name) == 0)
            return entry->fields[i].index;
    }
    return -1;
}

void
llvm_register_var_class(LLVMGenCtx *ctx, const char *var_name,
                        const char *class_name)
{
    PGY_DYNARR_ENSURE(ctx->var_classes, ctx->var_class_count,
                       ctx->var_class_capacity, LLVMVarClassEntry);

    ctx->var_classes[ctx->var_class_count].var_name   = var_name;
    ctx->var_classes[ctx->var_class_count].class_name = class_name;
    ctx->var_class_count++;
}

const char *
llvm_lookup_var_class(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->var_class_count - 1; i >= 0; i--) {
        if (strcmp(ctx->var_classes[i].var_name, var_name) == 0)
            return ctx->var_classes[i].class_name;
    }
    return NULL;
}

void
llvm_register_array_var(LLVMGenCtx *ctx, const char *var_name,
                        LLVMTypeRef elem_type, int64_t length)
{
    PGY_DYNARR_ENSURE(ctx->array_vars, ctx->array_var_count,
                       ctx->array_var_capacity, LLVMArrayVarEntry);

    ctx->array_vars[ctx->array_var_count].var_name = var_name;
    ctx->array_vars[ctx->array_var_count].elem_type = elem_type;
    ctx->array_vars[ctx->array_var_count].length = length;
    ctx->array_var_count++;
}

LLVMArrayVarEntry *
llvm_lookup_array_var(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->array_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->array_vars[i].var_name, var_name) == 0)
            return &ctx->array_vars[i];
    }
    return NULL;
}

void
llvm_register_enum_variant(LLVMGenCtx *ctx, const char *enum_name,
                           const char *variant_name, int value)
{
    PGY_DYNARR_ENSURE(ctx->enum_variants, ctx->enum_variant_count,
                       ctx->enum_variant_capacity, LLVMEnumVariantEntry);

    ctx->enum_variants[ctx->enum_variant_count].enum_name = enum_name;
    ctx->enum_variants[ctx->enum_variant_count].variant_name = variant_name;
    ctx->enum_variants[ctx->enum_variant_count].value = value;
    ctx->enum_variant_count++;
}

LLVMEnumVariantEntry *
llvm_lookup_enum_variant(LLVMGenCtx *ctx, const char *variant_name)
{
    for (int i = ctx->enum_variant_count - 1; i >= 0; i--) {
        if (strcmp(ctx->enum_variants[i].variant_name, variant_name) == 0)
            return &ctx->enum_variants[i];
    }
    return NULL;
}

LLVMEnumVariantEntry *
llvm_lookup_enum_variant_qualified(LLVMGenCtx *ctx, const char *enum_name,
                                   const char *variant_name)
{
    for (int i = ctx->enum_variant_count - 1; i >= 0; i--) {
        if (strcmp(ctx->enum_variants[i].enum_name, enum_name) == 0
            && strcmp(ctx->enum_variants[i].variant_name, variant_name) == 0)
            return &ctx->enum_variants[i];
    }
    return NULL;
}

/* =================================================================
 * Error reporting helpers
 * ================================================================= */

void
llvm_set_error(LLVMGenCtx *ctx, const char *fmt, ...)
{
    if (ctx->has_error)
        return;  /* preserve first error */
    ctx->has_error = true;
    ctx->error_line = 0;
    ctx->error_column = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
    va_end(args);
}

void
llvm_set_error_at(LLVMGenCtx *ctx, ASTNode *node, const char *fmt, ...)
{
    if (ctx->has_error)
        return;
    ctx->has_error = true;
    ctx->error_line = (node != NULL) ? node->line : 0;
    ctx->error_column = (node != NULL) ? node->column : 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
    va_end(args);
}

/* =================================================================
 * Error / result helpers
 * ================================================================= */

LLVMGenResult *
llvm_result_error(const char *message)
{
    LLVMGenResult *res = calloc(1, sizeof(LLVMGenResult));
    if (res == NULL)
        return NULL;

    res->success = false;
    res->error_message = pergyra_strdup(message);
    return res;
}

LLVMGenResult *
llvm_result_success(char *ir_text)
{
    LLVMGenResult *res = calloc(1, sizeof(LLVMGenResult));
    if (res == NULL)
        return NULL;

    res->success = true;
    res->ir_text = ir_text;
    return res;
}

/* =================================================================
 * Pergyra type → LLVM type mapping
 * ================================================================= */

/* Resolve inner type for generic containers: "Result<Int>" → i32 */
LLVMTypeRef
llvm_resolve_inner_type(LLVMGenCtx *ctx, const char *type_name)
{
    /* Find the inner type name between < and > */
    const char *lt = strchr(type_name, '<');
    const char *gt = strrchr(type_name, '>');
    if (lt == NULL || gt == NULL || gt <= lt)
        return ctx->type_i32;

    char inner[128];
    size_t len = (size_t)(gt - lt - 1);
    if (len >= sizeof(inner)) len = sizeof(inner) - 1;
    memcpy(inner, lt + 1, len);
    inner[len] = '\0';

    LLVMTypeRef resolved = pgy_kind_to_llvm(ctx, pgy_classify_type(inner));
    return resolved != NULL ? resolved : ctx->type_i32;
}

LLVMTypeRef
pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name)
{
    if (type_name == NULL)
        return ctx->type_void;

    /* Check active type substitution (monomorphization) first */
    for (int i = 0; i < ctx->type_subst_count; i++) {
        if (strcmp(type_name, ctx->type_subst[i].param_name) == 0)
            return ctx->type_subst[i].llvm_type;
    }

    PgyTypeKind kind = pgy_classify_type(type_name);

    /* Primitive types — direct mapping */
    LLVMTypeRef primitive = pgy_kind_to_llvm(ctx, kind);
    if (primitive != NULL)
        return primitive;

    /* Generic container types */
    switch (kind) {
    case PGY_TK_RESULT: {
        LLVMTypeRef inner = llvm_resolve_inner_type(ctx, type_name);
        LLVMTypeRef fields[] = { ctx->type_i32, inner, ctx->type_i8ptr };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
    case PGY_TK_OPTION: {
        LLVMTypeRef inner = llvm_resolve_inner_type(ctx, type_name);
        LLVMTypeRef fields[] = { ctx->type_i32, inner };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    case PGY_TK_SLOT: {
        const char *inner_name = strchr(type_name, '<');
        if (inner_name != NULL) {
            inner_name++;
            char buf[64]; size_t l = strcspn(inner_name, ">");
            if (l >= sizeof(buf)) l = sizeof(buf) - 1;
            memcpy(buf, inner_name, l); buf[l] = '\0';
            return llvm_slot_struct_type(ctx, buf);
        }
        return llvm_slot_struct_type(ctx, "Int");
    }
    case PGY_TK_SECURE_SLOT: {
        const char *inner_name = strchr(type_name, '<');
        if (inner_name != NULL) {
            inner_name++;
            char buf[64]; size_t l = strcspn(inner_name, ">");
            if (l >= sizeof(buf)) l = sizeof(buf) - 1;
            memcpy(buf, inner_name, l); buf[l] = '\0';
            return llvm_secure_slot_struct_type(ctx, buf);
        }
        return llvm_secure_slot_struct_type(ctx, "Int");
    }
    case PGY_TK_DEVICE_SLOT: {
        const char *inner_name = strchr(type_name, '<');
        if (inner_name != NULL) {
            inner_name++;
            char buf[64]; size_t l = strcspn(inner_name, ">");
            if (l >= sizeof(buf)) l = sizeof(buf) - 1;
            memcpy(buf, inner_name, l); buf[l] = '\0';
            return llvm_slot_struct_type(ctx, buf);
        }
        return llvm_slot_struct_type(ctx, "Int");
    }
    case PGY_TK_REMOTE_FUTURE:
        return ctx->type_task_handle;
    case PGY_TK_ARRAY: {
        const char *inner_name = strchr(type_name, '<');
        if (inner_name != NULL) {
            inner_name++;
            char buf[64]; size_t l = strcspn(inner_name, ">");
            if (l >= sizeof(buf)) l = sizeof(buf) - 1;
            memcpy(buf, inner_name, l); buf[l] = '\0';
            return llvm_array_struct_type(ctx, buf);
        }
        return llvm_array_struct_type(ctx, "Int");
    }
    case PGY_TK_SLICE: {
        const char *inner_name = strchr(type_name, '<');
        if (inner_name != NULL) {
            inner_name++;
            char buf[64]; size_t l = strcspn(inner_name, ">");
            if (l >= sizeof(buf)) l = sizeof(buf) - 1;
            memcpy(buf, inner_name, l); buf[l] = '\0';
            return llvm_slice_struct_type(ctx, buf);
        }
        return llvm_slice_struct_type(ctx, "Int");
    }
    case PGY_TK_CHANNEL:
    case PGY_TK_BOX:
    case PGY_TK_RC:
    case PGY_TK_WEAK:
        return ctx->type_i8ptr;
    case PGY_TK_FUTURE:
        return ctx->type_task_handle;

    case PGY_TK_UNKNOWN:
    case PGY_TK_CLASS:
        break;
    default:
        break;
    }

    /* Check if it's a registered class type */
    LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
    if (cls != NULL)
        return cls->struct_type;

    return ctx->type_i32;
}

LLVMTypeRef
ast_type_to_llvm(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (type_node == NULL)
        return ctx->type_void;

    if (type_node->type == AST_TYPE && type_node->data.type.name != NULL) {
        const char *name = type_node->data.type.name;

        /* If the AST type has generic_args, build the full name
         * (e.g., "Result" + "<Int>" → "Result<Int>") */
        if (type_node->data.type.generic_args != NULL
            && type_node->data.type.generic_args->count > 0) {
            GenericParam *gp = type_node->data.type.generic_args->params[0];
            if (gp != NULL && gp->name != NULL) {
                static char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s<%s>",
                         name, gp->name);
                return pergyra_type_to_llvm(ctx, full_name);
            }
        }

        return pergyra_type_to_llvm(ctx, name);
    }

    return ctx->type_i32;
}

/* =================================================================
 * Utility: temp name generation
 * ================================================================= */

const char *
llvm_tmp_name(LLVMGenCtx *ctx)
{
    static char buf[32];
    snprintf(buf, sizeof(buf), "t%d", ctx->tmp_counter++);
    return buf;
}

/* =================================================================
 * Generic monomorphization helpers
 * ================================================================= */

ASTNode *
llvm_lookup_generic_template(LLVMGenCtx *ctx, const char *name)
{
    for (int i = 0; i < ctx->generic_template_count; i++) {
        if (strcmp(ctx->generic_templates[i].name, name) == 0)
            return ctx->generic_templates[i].ast;
    }
    return NULL;
}

bool
llvm_mono_already_emitted(LLVMGenCtx *ctx, const char *mangled)
{
    for (int i = 0; i < ctx->mono_count; i++) {
        if (strcmp(ctx->mono_instances[i].name, mangled) == 0)
            return true;
    }
    return false;
}

void
llvm_register_mono(LLVMGenCtx *ctx, const char *mangled)
{
    PGY_DYNARR_ENSURE(ctx->mono_instances, ctx->mono_count,
                       ctx->mono_capacity, LLVMMonoInstance);

    ctx->mono_instances[ctx->mono_count].name = pergyra_strdup(mangled);
    ctx->mono_count++;
}

/* Map LLVM type to Pergyra type name suffix.
 * Note: this uses pointer identity (not enum) since LLVMTypeRef is opaque. */
const char *
llvm_type_to_suffix(LLVMGenCtx *ctx, LLVMTypeRef ty)
{
    if (ty == ctx->type_i32)    return "Int";
    if (ty == ctx->type_i64)    return "Long";
    if (ty == ctx->type_f32)    return "Float";
    if (ty == ctx->type_f64)    return "Double";
    if (ty == ctx->type_i1)     return "Bool";
    if (ty == ctx->type_i8ptr)  return "String";
    return "Unknown";
}

/* Forward declarations for recursive emit */
void llvm_emit_func_decl(ASTNode *node, LLVMGenCtx *ctx);

/* =================================================================
 * Utility: create alloca at function entry
 * ================================================================= */

LLVMValueRef
llvm_create_entry_alloca(LLVMGenCtx *ctx, LLVMTypeRef type, const char *name)
{
    LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(ctx->current_function);
    LLVMValueRef first = LLVMGetFirstInstruction(entry);

    LLVMBuilderRef tmp_builder = LLVMCreateBuilderInContext(ctx->context);
    if (first != NULL)
        LLVMPositionBuilderBefore(tmp_builder, first);
    else
        LLVMPositionBuilderAtEnd(tmp_builder, entry);

    LLVMValueRef alloca = LLVMBuildAlloca(tmp_builder, type, name);
    LLVMDisposeBuilder(tmp_builder);
    return alloca;
}

/* =================================================================
 * Runtime function declarations
 * ================================================================= */

void
llvm_declare_runtime(LLVMGenCtx *ctx)
{
    struct { const char *name; LLVMTypeRef param; } log_fns[] = {
        { "pgy_log_int",    ctx->type_i32 },
        { "pgy_log_long",   ctx->type_i64 },
        { "pgy_log_float",  ctx->type_f32 },
        { "pgy_log_double", ctx->type_f64 },
        { "pgy_log_bool",   ctx->type_i1  },
        { "pgy_log_string", ctx->type_i8ptr },
    };

    if (ctx->type_task_handle == NULL) {
        LLVMTypeRef task_handle_fields[] = { ctx->type_i8ptr };
        ctx->type_task_handle = LLVMStructCreateNamed(ctx->context,
            "PgyTaskHandle");
        LLVMStructSetBody(ctx->type_task_handle, task_handle_fields, 1, 0);
    }

    for (size_t i = 0; i < sizeof(log_fns) / sizeof(log_fns[0]); i++) {
        LLVMTypeRef params[] = { log_fns[i].param };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, log_fns[i].name, ft);
        llvm_register_function(ctx, log_fns[i].name, fn, ft, ctx->type_void);
    }

    /* int printf(i8*, ...) */
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 1);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "printf", ft);
        llvm_register_function(ctx, "printf", fn, ft, ctx->type_i32);
    }

    /* String helpers and file I/O helpers */
    {
        struct {
            const char *name;
            LLVMTypeRef ret;
            LLVMTypeRef params[3];
            unsigned param_count;
        } builtins[] = {
            { "StringContains", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "StringReplace", ctx->type_i8ptr,
              { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr }, 3 },
            { "Substring", ctx->type_i8ptr,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32 }, 3 },
            { "StringTrim", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "ToUpper", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "ToLower", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "StringConcat", ctx->type_i8ptr,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "pgy_read_file", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "pgy_write_file", ctx->type_void,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "pgy_input", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "pgy_file_open", ctx->type_i32,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "pgy_file_read", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_file_write", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr }, 2 },
            { "pgy_file_close", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "ClaimQubit", ctx->type_i32,
              { 0 }, 0 },
            { "Measure", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "Entangle", ctx->type_void,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "QubitState", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "IsCollapsed", ctx->type_i1,
              { ctx->type_i32 }, 1 },
            { "ReleaseQubit", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "H", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "IntoClassical", ctx->type_i1,
              { ctx->type_i32 }, 1 },
        };

        for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
            LLVMTypeRef ft = LLVMFunctionType(
                builtins[i].ret, builtins[i].params,
                builtins[i].param_count, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, builtins[i].name, ft);
            llvm_register_function(ctx, builtins[i].name, fn, ft, builtins[i].ret);
        }
    }

    /* Slot runtime functions for each type */
    struct {
        const char *suffix;   /* Int, Long, etc. */
        LLVMTypeRef slot_ty;
        LLVMTypeRef val_ty;
    } slot_types[] = {
        { "Int",    ctx->slot_type_Int,    ctx->type_i32   },
        { "Long",   ctx->slot_type_Long,   ctx->type_i64   },
        { "Float",  ctx->slot_type_Float,  ctx->type_f32   },
        { "Double", ctx->slot_type_Double, ctx->type_f64   },
        { "Bool",   ctx->slot_type_Bool,   ctx->type_i1    },
        { "String", ctx->slot_type_String, ctx->type_i8ptr },
    };

    for (size_t i = 0; i < sizeof(slot_types) / sizeof(slot_types[0]); i++) {
        const char *suffix = slot_types[i].suffix;
        LLVMTypeRef slot_ty = slot_types[i].slot_ty;
        LLVMTypeRef val_ty  = slot_types[i].val_ty;
        LLVMTypeRef ptr_ty  = ctx->type_i8ptr; /* opaque ptr */
        char fn_name[64];

        /* PgySlot_T pgy_claim_T(void) → returns struct by value */
        {
            LLVMTypeRef ft = LLVMFunctionType(slot_ty, NULL, 0, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_claim_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx,
                LLVMGetValueName(fn), fn, ft, slot_ty);
        }

        /* void pgy_write_T(PgySlot_T*, val_ty) */
        {
            LLVMTypeRef params[] = { ptr_ty, val_ty };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx,
                LLVMGetValueName(fn), fn, ft, ctx->type_void);
        }

        /* val_ty pgy_read_T(PgySlot_T*) */
        {
            LLVMTypeRef params[] = { ptr_ty };
            LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 1, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_read_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx,
                LLVMGetValueName(fn), fn, ft, val_ty);
        }

        /* void pgy_release_T(PgySlot_T*) */
        {
            LLVMTypeRef params[] = { ptr_ty };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_release_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx,
                LLVMGetValueName(fn), fn, ft, ctx->type_void);
        }

        /* Device slot runtime mirrors Slot layout but lives on remote/device boundary */
        {
            LLVMTypeRef ft = LLVMFunctionType(slot_ty, NULL, 0, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_claim_device_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx,
                LLVMGetValueName(fn), fn, ft, slot_ty);
        }
        {
            LLVMTypeRef params[] = { ptr_ty, val_ty };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_device_write_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx,
                LLVMGetValueName(fn), fn, ft, ctx->type_void);
        }
        {
            LLVMTypeRef params[] = { ptr_ty };
            LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 1, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_device_read_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx,
                LLVMGetValueName(fn), fn, ft, val_ty);
        }
        {
            LLVMTypeRef params[] = { ptr_ty };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_release_device_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx,
                LLVMGetValueName(fn), fn, ft, ctx->type_void);
        }
        {
            LLVMTypeRef params[] = { ptr_ty };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle, params, 1, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_submit_device_read_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx,
                LLVMGetValueName(fn), fn, ft, ctx->type_task_handle);
        }
    }

    for (size_t i = 0; i < sizeof(slot_types) / sizeof(slot_types[0]); i++) {
        const char *suffix = slot_types[i].suffix;
        LLVMTypeRef arr_ty = llvm_array_struct_type(ctx, suffix);
        LLVMTypeRef arr_ptr_ty = LLVMPointerType(arr_ty, 0);
        LLVMTypeRef val_ty = slot_types[i].val_ty;
        char fn_name[64];

        {
            LLVMTypeRef params[] = { ctx->type_i64 };
            LLVMTypeRef ft = LLVMFunctionType(arr_ty, params, 1, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_array_new_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, arr_ty);
        }
        {
            LLVMTypeRef params[] = { arr_ptr_ty, val_ty };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
            snprintf(fn_name, sizeof(fn_name), "pgy_array_push_%s", suffix);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void);
        }
    }

    /* =================================================================
     * Thread pool runtime
     * pgy_pool_init_export(i64) → void
     * pgy_pool_shutdown_export() → void
     * pgy_spawn_export(fn_ptr, i8*) → { i8* }  (PgyTaskHandle)
     * pgy_async_spawn_export(fn_ptr, i8*) → { i8* }
     * pgy_async_detach_export({ i8* }) → void
     * pgy_await_export({ i8* }) → i8*
     * ================================================================= */

    /* void pgy_pool_init_export(i64) */
    {
        LLVMTypeRef params[] = { ctx->type_i64 };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                           "pgy_pool_init_export", ft);
        llvm_register_function(ctx, "pgy_pool_init_export",
                                fn, ft, ctx->type_void);
    }

    /* void pgy_pool_shutdown_export() */
    {
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, NULL, 0, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                           "pgy_pool_shutdown_export", ft);
        llvm_register_function(ctx, "pgy_pool_shutdown_export",
                                fn, ft, ctx->type_void);
    }

    /* fn_ptr type: i8* (*)(i8*) — represented as just i8* (opaque) */
    /* PgyTaskHandle pgy_spawn_export(i8*, i8*) */
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle,
                                           params, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                           "pgy_spawn_export", ft);
        llvm_register_function(ctx, "pgy_spawn_export",
                                fn, ft, ctx->type_task_handle);
    }

    /* PgyTaskHandle pgy_async_spawn_export(i8*, i8*) */
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle,
                                           params, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                           "pgy_async_spawn_export", ft);
        llvm_register_function(ctx, "pgy_async_spawn_export",
                                fn, ft, ctx->type_task_handle);
    }

    /* void pgy_async_detach_export(PgyTaskHandle) */
    {
        LLVMTypeRef params[] = { ctx->type_task_handle };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                           "pgy_async_detach_export", ft);
        llvm_register_function(ctx, "pgy_async_detach_export",
                                fn, ft, ctx->type_void);
    }

    /* i8* pgy_await_export(PgyTaskHandle) */
    {
        LLVMTypeRef params[] = { ctx->type_task_handle };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                           "pgy_await_export", ft);
        llvm_register_function(ctx, "pgy_await_export",
                                fn, ft, ctx->type_i8ptr);
    }

    /* i1 pgy_task_cancel_export(PgyTaskHandle) */
    {
        LLVMTypeRef params[] = { ctx->type_task_handle };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                           "pgy_task_cancel_export", ft);
        llvm_register_function(ctx, "pgy_task_cancel_export",
                                fn, ft, ctx->type_i1);
    }

    /* i1 pgy_task_is_cancelled_export() */
    {
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, NULL, 0, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                           "pgy_task_is_cancelled_export", ft);
        llvm_register_function(ctx, "pgy_task_is_cancelled_export",
                                fn, ft, ctx->type_i1);
    }

    /* i8* malloc(i64) */
    {
        LLVMTypeRef params[] = { ctx->type_i64 };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "malloc", ft);
        llvm_register_function(ctx, "malloc", fn, ft, ctx->type_i8ptr);
    }

    /* void free(i8*) */
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "free", ft);
        llvm_register_function(ctx, "free", fn, ft, ctx->type_void);
    }

    /* =================================================================
     * Channel runtime — all types (Int, String)
     * ================================================================= */
    struct {
        const char *suffix;
        LLVMTypeRef val_type;
    } chan_types[] = {
        { "Int",    ctx->type_i32    },
        { "String", ctx->type_i8ptr  },
    };

    for (size_t ci = 0; ci < sizeof(chan_types) / sizeof(chan_types[0]); ci++) {
        const char *suf = chan_types[ci].suffix;
        LLVMTypeRef vt = chan_types[ci].val_type;
        char fname[128];

        /* void pgy_channel_init_T(ptr, i64) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_init_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
                                    ctx->type_void);
        }

        /* i1 pgy_channel_send_T(ptr, val_type) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_send_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
                                    ctx->type_i1);
        }

        /* i1 pgy_channel_try_send_T(ptr, val_type) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_try_send_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
                                    ctx->type_i1);
        }

        /* i1 pgy_channel_send_timeout_T(ptr, val_type, i64) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr, vt, ctx->type_i64 };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_send_timeout_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
                                    ctx->type_i1);
        }

        /* val_type pgy_channel_recv_val_T(ptr) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(vt, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_recv_val_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt);
        }

        /* i1 pgy_channel_try_recv_T(ptr, val_type*) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr, LLVMPointerType(vt, 0) };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_try_recv_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1);
        }

        /* i1 pgy_channel_recv_timeout_T(ptr, val_type*, i64) */
        {
            LLVMTypeRef params[] = {
                ctx->type_i8ptr, LLVMPointerType(vt, 0), ctx->type_i64
            };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_recv_timeout_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
                                    ctx->type_i1);
        }

        /* i1 pgy_channel_ready_T(ptr) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_ready_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1);
        }

        /* i32 pgy_channel_length_T(ptr) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_length_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32);
        }

        /* i32 pgy_channel_capacity_T(ptr) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_capacity_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32);
        }

        /* i32 pgy_channel_space_T(ptr) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_space_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32);
        }

        /* i1 pgy_channel_full_T(ptr) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_full_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1);
        }

        /* i1 pgy_channel_closed_T(ptr) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_closed_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1);
        }

        /* void pgy_channel_close_T(ptr) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_close_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
                                    ctx->type_void);
        }

        /* void pgy_channel_destroy_T(ptr) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_destroy_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
                                    ctx->type_void);
        }
    }

    /* =================================================================
     * SecureSlot runtime (mirrors Slot but with token parameter)
     * ================================================================= */
    for (size_t si = 0; si < sizeof(slot_types) / sizeof(slot_types[0]); si++) {
        const char *suf = slot_types[si].suffix;
        LLVMTypeRef sty = llvm_secure_slot_struct_type(ctx, suf);
        LLVMTypeRef tty = llvm_secure_token_type(ctx, suf);
        LLVMTypeRef vt  = slot_types[si].val_ty;
        char fname[128];

        /* PgySecureSlot_T pgy_claim_secure_T(PgyToken_T*) */
        {
            LLVMTypeRef params[] = { LLVMPointerType(tty, 0) };
            LLVMTypeRef ft = LLVMFunctionType(sty, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_claim_secure_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, sty);
        }

        /* void pgy_secure_write_T(PgySecureSlot_T*, val_type, PgyToken_T*) */
        {
            LLVMTypeRef params[] = {
                LLVMPointerType(sty, 0), vt, LLVMPointerType(tty, 0)
            };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
            snprintf(fname, sizeof(fname), "pgy_secure_write_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
                                    ctx->type_void);
        }

        /* val_type pgy_secure_read_T(PgySecureSlot_T*, PgyToken_T*) */
        {
            LLVMTypeRef params[] = {
                LLVMPointerType(sty, 0), LLVMPointerType(tty, 0)
            };
            LLVMTypeRef ft = LLVMFunctionType(vt, params, 2, 0);
            snprintf(fname, sizeof(fname), "pgy_secure_read_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt);
        }

        /* void pgy_secure_release_T(PgySecureSlot_T*, PgyToken_T*) */
        {
            LLVMTypeRef params[] = {
                LLVMPointerType(sty, 0), LLVMPointerType(tty, 0)
            };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
            snprintf(fname, sizeof(fname), "pgy_secure_release_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
                                    ctx->type_void);
        }
    }
}

/* =================================================================
 * Event type registry helpers
 * ================================================================= */

LLVMEventTypeEntry *
llvm_lookup_event(LLVMGenCtx *ctx, const char *name)
{
    for (int i = 0; i < ctx->event_type_count; i++) {
        if (strcmp(ctx->event_types[i].event_name, name) == 0)
            return &ctx->event_types[i];
    }
    return NULL;
}

LLVMEventTypeEntry *
llvm_register_event(LLVMGenCtx *ctx, const char *name,
                    LLVMTypeRef struct_type,
                    int param_count, LLVMTypeRef *param_types)
{
    PGY_DYNARR_ENSURE_RET(ctx->event_types, ctx->event_type_count,
                            ctx->event_type_capacity, LLVMEventTypeEntry);

    LLVMEventTypeEntry *e = &ctx->event_types[ctx->event_type_count++];
    e->event_name  = name;
    e->struct_type = struct_type;
    e->param_count = param_count;
    for (int i = 0; i < param_count && i < MAX_EVENT_PARAMS; i++)
        e->param_types[i] = param_types[i];
    return e;
}

/* =================================================================
 * Program emission
 * ================================================================= */

static void
llvm_emit_program(const HIRProgram *hir, LLVMGenCtx *ctx)
{
    if (hir == NULL) {
        llvm_set_error(ctx, "Expected lowered HIR program");
        return;
    }

    ctx->hir = hir;

    /* Declare runtime functions */
    llvm_declare_runtime(ctx);

    /* Pass 0: Register class/struct types */
    for (size_t i = 0; i < hir->type_count; i++) {
        ASTNode *stmt = hir->types[i];
        if (stmt != NULL && stmt->type == AST_ENUM_DECL) {
            const char *enum_name = stmt->data.enum_decl.name;
            bool has_data = false;
            for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
                if (stmt->data.enum_decl.variant_param_counts != NULL
                    && stmt->data.enum_decl.variant_param_counts[j] > 0) {
                    has_data = true;
                }
                llvm_register_enum_variant(ctx, enum_name,
                    stmt->data.enum_decl.variants[j], (int)j);
            }

            if (has_data) {
                size_t variant_count = stmt->data.enum_decl.variant_count;
                LLVMTypeRef *enum_fields = calloc((variant_count + 1),
                    sizeof(LLVMTypeRef));
                LLVMTypeRef enum_ty =
                    LLVMStructCreateNamed(ctx->context, enum_name);
                LLVMClassTypeEntry *enum_entry =
                    llvm_register_class(ctx, enum_name, enum_ty);

                enum_fields[0] = ctx->type_i32;
                if (enum_entry != NULL)
                    llvm_class_add_field(enum_entry, "tag", ctx->type_i32, 0);

                for (size_t j = 0; j < variant_count; j++) {
                    const char *variant_name = stmt->data.enum_decl.variants[j];
                    size_t param_count = (stmt->data.enum_decl.variant_param_counts != NULL)
                        ? stmt->data.enum_decl.variant_param_counts[j] : 0;

                    if (param_count == 0) {
                        enum_fields[j + 1] = LLVMStructTypeInContext(ctx->context,
                            NULL, 0, 0);
                        continue;
                    }

                    char payload_name[256];
                    snprintf(payload_name, sizeof(payload_name), "%s$%s",
                        enum_name, variant_name);

                    LLVMTypeRef *payload_fields = calloc(param_count,
                        sizeof(LLVMTypeRef));
                    for (size_t p = 0; p < param_count; p++) {
                        ASTNode *pt = stmt->data.enum_decl.variant_params[j][p];
                        payload_fields[p] = (pt != NULL)
                            ? ast_type_to_llvm(ctx, pt)
                            : ctx->type_i32;
                    }

                    LLVMTypeRef payload_ty =
                        LLVMStructCreateNamed(ctx->context, payload_name);
                    LLVMStructSetBody(payload_ty, payload_fields,
                        (unsigned)param_count, 0);

                    LLVMClassTypeEntry *payload_entry =
                        llvm_register_class(ctx, pergyra_strdup(payload_name),
                            payload_ty);
                    if (payload_entry != NULL) {
                        for (size_t p = 0; p < param_count; p++) {
                            char field_name[32];
                            snprintf(field_name, sizeof(field_name), "_%zu", p);
                            llvm_class_add_field(payload_entry,
                                pergyra_strdup(field_name),
                                payload_fields[p], (int)p);
                        }
                    }

                    enum_fields[j + 1] = payload_ty;
                    if (enum_entry != NULL) {
                        llvm_class_add_field(enum_entry, variant_name,
                            payload_ty, (int)(j + 1));
                    }
                    free(payload_fields);
                }

                LLVMStructSetBody(enum_ty, enum_fields,
                    (unsigned)(variant_count + 1), 0);
                free(enum_fields);
            }
            continue;
        }
        if (stmt != NULL && stmt->type == AST_CLASS_DECL) {
            const char *cls_name = stmt->data.class_decl.name;
            size_t fc = stmt->data.class_decl.field_count;

            /* Build LLVM struct type */
            LLVMTypeRef *field_types = calloc(fc > 0 ? fc : 1,
                                               sizeof(LLVMTypeRef));
            for (size_t j = 0; j < fc; j++) {
                ClassField *f = stmt->data.class_decl.fields[j];
                field_types[j] = (f->type != NULL)
                    ? ast_type_to_llvm(ctx, f->type)
                    : ctx->type_i32;
            }

            LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context,
                                                            cls_name);
            LLVMStructSetBody(struct_ty, field_types,
                               (unsigned)fc, 0);

            LLVMClassTypeEntry *entry = llvm_register_class(ctx,
                cls_name, struct_ty);
            if (entry != NULL) {
                for (size_t j = 0; j < fc; j++) {
                    ClassField *f = stmt->data.class_decl.fields[j];
                    llvm_class_add_field(entry, f->name,
                        field_types[j], (int)j);
                }
            }

            free(field_types);

            /* Forward-declare methods as ClassName_MethodName(self*, ...) */
            for (size_t j = 0; j < stmt->data.class_decl.method_count; j++) {
                ASTNode *method = stmt->data.class_decl.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                const char *method_name = method->data.func_decl.name;
                size_t pc = method->data.func_decl.param_count;

                LLVMTypeRef ret_type = ctx->type_void;
                if (method->data.func_decl.return_type != NULL)
                    ret_type = ast_type_to_llvm(ctx,
                        method->data.func_decl.return_type);

                /* Count non-self user params */
                size_t user_pc = 0;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    user_pc++;
                }

                /* self pointer + user params */
                LLVMTypeRef *param_types = calloc(user_pc + 1,
                                                   sizeof(LLVMTypeRef));
                param_types[0] = ctx->type_i8ptr; /* self as opaque ptr */
                size_t pidx = 1;
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    param_types[pidx++] = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                }

                LLVMTypeRef ft = LLVMFunctionType(ret_type,
                    param_types, (unsigned)(user_pc + 1), 0);

                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s_%s",
                         cls_name, method_name);
                LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                                    full_name, ft);
                llvm_register_function(ctx, LLVMGetValueName(fn),
                                        fn, ft, ret_type);

                free(param_types);
            }
        }
    }

    /* Pass 0 (actors): Register actor types (same as class) */
    for (size_t i = 0; i < hir->actor_count; i++) {
        ASTNode *stmt = hir->actors[i];
        if (stmt == NULL || stmt->type != AST_ACTOR_DECL)
            continue;

        const char *aname = stmt->data.actor_decl.name;
        size_t fc = stmt->data.actor_decl.field_count;

        LLVMTypeRef *ftypes = calloc(fc > 0 ? fc : 1,
                                       sizeof(LLVMTypeRef));
        for (size_t j = 0; j < fc; j++) {
            ClassField *f = stmt->data.actor_decl.fields[j];
            ftypes[j] = (f->type != NULL)
                ? ast_type_to_llvm(ctx, f->type) : ctx->type_i32;
        }

        LLVMTypeRef sty = LLVMStructCreateNamed(ctx->context, aname);
        LLVMStructSetBody(sty, ftypes, (unsigned)fc, 0);

        LLVMClassTypeEntry *entry = llvm_register_class(ctx, aname, sty);
        if (entry != NULL) {
            for (size_t j = 0; j < fc; j++) {
                ClassField *f = stmt->data.actor_decl.fields[j];
                llvm_class_add_field(entry, f->name, ftypes[j], (int)j);
            }
        }
        free(ftypes);

        /* Forward-declare actor methods */
        for (size_t j = 0; j < stmt->data.actor_decl.method_count; j++) {
            ASTNode *method = stmt->data.actor_decl.methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;

            const char *mname = method->data.func_decl.name;
            size_t pc = method->data.func_decl.param_count;

            LLVMTypeRef ret = ctx->type_void;
            if (method->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx,
                    method->data.func_decl.return_type);

            size_t user_pc = 0;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0)
                    continue;
                user_pc++;
            }

            LLVMTypeRef *ptypes = calloc(user_pc + 1,
                                           sizeof(LLVMTypeRef));
            ptypes[0] = ctx->type_i8ptr;
            size_t pidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0)
                    continue;
                ptypes[pidx++] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type) : ctx->type_i32;
            }

            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes,
                (unsigned)(user_pc + 1), 0);

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s", aname, mname);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn),
                                    fn, ft, ret);
            free(ptypes);
        }
    }

    /* Passes 0a/0b/0c/0e delegated to llvm_emit_domain_passes() in
     * llvm_domain.c -- called after Pass 2 (actors) below. */

    /* Pass 0f: Emit extern "C" declarations as LLVM function prototypes */
    for (size_t i = 0; i < hir->item_count; i++) {
        ASTNode *stmt = hir->items[i].ast;
        if (stmt == NULL || stmt->type != AST_EXTERN_BLOCK)
            continue;

        for (size_t j = 0; j < stmt->data.extern_block.count; j++) {
            ASTNode *decl = stmt->data.extern_block.declarations[j];
            if (decl == NULL || decl->type != AST_FUNC_DECL)
                continue;

            const char *fname = decl->data.func_decl.name;
            if (llvm_lookup_function(ctx, fname) != NULL)
                continue; /* Already declared (e.g., runtime function) */

            LLVMTypeRef ret = ctx->type_void;
            if (decl->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx, decl->data.func_decl.return_type);

            size_t pc = decl->data.func_decl.param_count;
            LLVMTypeRef *ptypes = calloc(pc > 0 ? pc : 1,
                                           sizeof(LLVMTypeRef));
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = decl->data.func_decl.params[k];
                ptypes[k] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
            }

            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes, (unsigned)pc, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ret);
            free(ptypes);
        }
    }

    /* Pass 1: Forward-declare all user functions.
     * Generic functions (with generic_params) are stored as templates
     * for lazy monomorphization — NOT forward-declared as LLVM functions. */
    for (size_t i = 0; i < hir->function_count; i++) {
        ASTNode *stmt = hir->functions[i];
        if (stmt == NULL || stmt->type != AST_FUNC_DECL)
            continue;
        if (stmt->data.func_decl.generic_params != NULL
            && stmt->data.func_decl.generic_params->count > 0) {
            /* Store as template for lazy instantiation */
            PGY_DYNARR_ENSURE(ctx->generic_templates,
                               ctx->generic_template_count,
                               ctx->generic_template_capacity,
                               LLVMGenericTemplate);
            ctx->generic_templates[ctx->generic_template_count].name =
                stmt->data.func_decl.name;
            ctx->generic_templates[ctx->generic_template_count].ast = stmt;
            ctx->generic_template_count++;
        } else {
            llvm_forward_declare_func(stmt, ctx);
        }
    }

    /* Domain passes: ability, role, party, systemic, world, event.
     * Runs before regular function bodies so synthesized operator wrappers
     * are available during later expression lowering. */
    llvm_emit_domain_passes(hir, ctx);

    /* Pass 2: Emit function bodies (standalone + class methods).
     * Skip generic templates — they are instantiated lazily. */
    for (size_t i = 0; i < hir->item_count; i++) {
        ASTNode *stmt = hir->items[i].ast;
        if (stmt != NULL && stmt->type == AST_FUNC_DECL
            && llvm_lookup_generic_template(ctx, stmt->data.func_decl.name) == NULL)
            llvm_emit_func_decl(stmt, ctx);
        else if (stmt != NULL && stmt->type == AST_CLASS_DECL) {
            /* Emit method bodies */
            const char *cls_name = stmt->data.class_decl.name;
            LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, cls_name);

            for (size_t j = 0; j < stmt->data.class_decl.method_count; j++){
                ASTNode *method = stmt->data.class_decl.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s_%s",
                         cls_name, method->data.func_decl.name);

                LLVMFuncEntry *entry = llvm_lookup_function(ctx, full_name);
                if (entry == NULL) continue;

                LLVMValueRef fn = entry->fn;
                LLVMTypeRef ret_type = entry->ret_type;
                LLVMValueRef saved_fn = ctx->current_function;
                LLVMTypeRef saved_ret = ctx->current_ret_type;
                const char *saved_class_name = ctx->current_class_name;
                ctx->current_function = fn;
                ctx->current_ret_type = ret_type;
                ctx->current_class_name = cls_name;

                LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "entry");
                LLVMPositionBuilderAtEnd(ctx->builder, bb);

                llvm_scope_push(ctx);

                /* self param — store the incoming pointer */
                LLVMValueRef self_ptr = LLVMGetParam(fn, 0);
                LLVMTypeRef self_ptr_type = LLVMPointerType(
                    cls->struct_type, 0);
                LLVMValueRef self_alloca = llvm_create_entry_alloca(
                    ctx, self_ptr_type, "self.addr");
                LLVMBuildStore(ctx->builder, self_ptr, self_alloca);
                llvm_scope_declare(ctx, "self", self_alloca,
                                    self_ptr_type);
                llvm_register_var_class(ctx, "self", cls_name);

                /* User params (skip explicit 'self' params) */
                size_t pc = method->data.func_decl.param_count;
                unsigned llvm_pidx = 1; /* 0 = self */
                for (size_t k = 0; k < pc; k++) {
                    FuncParam *p = method->data.func_decl.params[k];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    LLVMTypeRef pt = (p->type != NULL)
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                    LLVMValueRef alloca = llvm_create_entry_alloca(
                        ctx, pt, p->name);
                    LLVMBuildStore(ctx->builder,
                        LLVMGetParam(fn, llvm_pidx++), alloca);
                    llvm_scope_declare(ctx, p->name, alloca, pt);
                    if (p->type != NULL && p->type->type == AST_TYPE
                        && p->type->data.type.name != NULL
                        && llvm_lookup_class(ctx, p->type->data.type.name) != NULL) {
                        llvm_register_var_class(ctx, p->name,
                                                p->type->data.type.name);
                    }
                }

                if (method->data.func_decl.body != NULL)
                    llvm_emit_block(method->data.func_decl.body, ctx);

                if (LLVMGetBasicBlockTerminator(
                        LLVMGetInsertBlock(ctx->builder)) == NULL) {
                    if (ret_type == ctx->type_void)
                        LLVMBuildRetVoid(ctx->builder);
                    else
                        LLVMBuildRet(ctx->builder,
                            LLVMConstInt(ret_type, 0, 0));
                }

                llvm_scope_pop(ctx);
                ctx->current_function = saved_fn;
                ctx->current_ret_type = saved_ret;
                ctx->current_class_name = saved_class_name;

                if (saved_fn != NULL) {
                    LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
                    if (last != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, last);
                }
            }
        }
    }

    /* Pass 2 (actors): Emit actor method bodies */
    for (size_t i = 0; i < hir->actor_count; i++) {
        ASTNode *stmt = hir->actors[i];
        if (stmt == NULL || stmt->type != AST_ACTOR_DECL)
            continue;

        const char *aname = stmt->data.actor_decl.name;
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, aname);

        for (size_t j = 0; j < stmt->data.actor_decl.method_count; j++) {
            ASTNode *method = stmt->data.actor_decl.methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s",
                     aname, method->data.func_decl.name);

            LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
            if (fentry == NULL) continue;

            LLVMValueRef fn = fentry->fn;
            LLVMTypeRef ret_type = fentry->ret_type;
            LLVMValueRef saved_fn = ctx->current_function;
            LLVMTypeRef saved_ret = ctx->current_ret_type;
            ctx->current_function = fn;
            ctx->current_ret_type = ret_type;

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);
            llvm_scope_push(ctx);

            /* self param */
            LLVMValueRef self_val = LLVMGetParam(fn, 0);
            if (cls != NULL) {
                LLVMTypeRef self_ptr_t = LLVMPointerType(
                    cls->struct_type, 0);
                LLVMValueRef sa = llvm_create_entry_alloca(
                    ctx, self_ptr_t, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, sa);
                llvm_scope_declare(ctx, "self", sa, self_ptr_t);
                llvm_register_var_class(ctx, "self", aname);
            } else {
                LLVMValueRef sa = llvm_create_entry_alloca(
                    ctx, ctx->type_i8ptr, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, sa);
                llvm_scope_declare(ctx, "self", sa, ctx->type_i8ptr);
            }

            /* User params */
            size_t pc = method->data.func_decl.param_count;
            unsigned lpidx = 1;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0)
                    continue;
                LLVMTypeRef pt = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
                LLVMValueRef a = llvm_create_entry_alloca(
                    ctx, pt, p->name);
                LLVMBuildStore(ctx->builder,
                    LLVMGetParam(fn, lpidx++), a);
                llvm_scope_declare(ctx, p->name, a, pt);
                if (p->type != NULL && p->type->type == AST_TYPE
                    && p->type->data.type.name != NULL
                    && llvm_lookup_class(ctx, p->type->data.type.name) != NULL) {
                    llvm_register_var_class(ctx, p->name,
                                            p->type->data.type.name);
                }
            }

            if (method->data.func_decl.body != NULL)
                llvm_emit_block(method->data.func_decl.body, ctx);

            if (LLVMGetBasicBlockTerminator(
                    LLVMGetInsertBlock(ctx->builder)) == NULL) {
                if (ret_type == ctx->type_void)
                    LLVMBuildRetVoid(ctx->builder);
                else
                    LLVMBuildRet(ctx->builder,
                        LLVMConstInt(ret_type, 0, 0));
            }

            llvm_scope_pop(ctx);
            ctx->current_function = saved_fn;
            ctx->current_ret_type = saved_ret;

            if (saved_fn != NULL) {
                LLVMBasicBlockRef last =
                    LLVMGetLastBasicBlock(saved_fn);
                if (last != NULL)
                    LLVMPositionBuilderAtEnd(ctx->builder, last);
            }
        }
    }

    /* Pass 3: Collect non-function/non-class top-level into main() */
    bool has_top_level = hir->executable_count > 0;

    /* Create main() */
    LLVMTypeRef main_type = LLVMFunctionType(ctx->type_i32, NULL, 0, 0);
    LLVMValueRef main_fn = LLVMAddFunction(ctx->module, "main", main_type);
    ctx->current_function = main_fn;
    ctx->current_ret_type = ctx->type_i32;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
        ctx->context, main_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);

    /* Initialize thread pool: pgy_pool_init_export(4) */
    {
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx,
                                     "pgy_pool_init_export");
        if (init_fn != NULL) {
            LLVMValueRef args[] = {
                LLVMConstInt(ctx->type_i64, 4, 0)
            };
            LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                           init_fn->fn, args, 1, "");
        }
    }

    llvm_scope_push(ctx);

    /* Initialize event globals (created in Pass 0e) */
    for (int i = 0; i < ctx->event_type_count; i++) {
        LLVMEventTypeEntry *evt = &ctx->event_types[i];
        char fname[256];
        snprintf(fname, sizeof(fname), "%s_INIT", evt->event_name);
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx, fname);

        LLVMValueRef gv = LLVMGetNamedGlobal(ctx->module, evt->event_name);
        if (init_fn != NULL && gv != NULL) {
            LLVMValueRef args[] = { gv };
            LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                           init_fn->fn, args, 1, "");
        }
    }

    /* If a Main() function exists, call it */
    {
        LLVMFuncEntry *main_user = llvm_lookup_function(ctx, "Main");
        if (main_user != NULL) {
            LLVMBuildCall2(ctx->builder, main_user->fn_type,
                           main_user->fn, NULL, 0, "");
        }
    }

    if (has_top_level) {
        for (size_t i = 0; i < hir->executable_count; i++) {
            ASTNode *stmt = hir->executables[i];
            if (stmt != NULL) {
                llvm_emit_statement(stmt, ctx);
                if (LLVMGetBasicBlockTerminator(
                        LLVMGetInsertBlock(ctx->builder)) != NULL)
                    break;
            }
        }
    }

    llvm_scope_pop(ctx);

    /* Shutdown thread pool before returning */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
    {
        LLVMFuncEntry *shutdown_fn = llvm_lookup_function(ctx,
                                         "pgy_pool_shutdown_export");
        if (shutdown_fn != NULL)
            LLVMBuildCall2(ctx->builder, shutdown_fn->fn_type,
                           shutdown_fn->fn, NULL, 0, "");

        /* Return 0 from main */
        LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0));
    }
}

/* =================================================================
 * Optimization pass
 * ================================================================= */

static void
llvm_run_optimization(LLVMGenCtx *ctx)
{
    /* Initialize targets so we can create a target machine */
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    char *triple = LLVMGetDefaultTargetTriple();
    LLVMTargetRef target = NULL;
    char *target_error = NULL;
    LLVMTargetMachineRef machine = NULL;

    if (!LLVMGetTargetFromTriple(triple, &target, &target_error)) {
        machine = LLVMCreateTargetMachine(
            target, triple, "generic", "",
            LLVMCodeGenLevelDefault,
            LLVMRelocDefault,
            LLVMCodeModelDefault);
    }
    if (target_error != NULL)
        LLVMDisposeMessage(target_error);

    /* Run the new pass manager pipeline:
     * default<O2> includes: mem2reg, instcombine, reassociate,
     * gvn, simplifycfg, inline, etc. */
    LLVMPassBuilderOptionsRef opts = LLVMCreatePassBuilderOptions();
    LLVMRunPasses(ctx->module, "default<O2>", machine, opts);
    LLVMDisposePassBuilderOptions(opts);

    if (machine != NULL)
        LLVMDisposeTargetMachine(machine);
    LLVMDisposeMessage(triple);
}

/* =================================================================
 * Public API
 * ================================================================= */

LLVMGenResult *
llvm_codegen(const HIRProgram *hir, const char *module_name)
{
    LLVMGenCtx *ctx = llvm_ctx_create(module_name);
    if (ctx == NULL)
        return llvm_result_error("Out of memory");

    llvm_emit_program(hir, ctx);

    if (ctx->has_error) {
        char msg[1024];
        if (ctx->error_line > 0) {
            snprintf(msg, sizeof(msg), "line %u:%u: %s",
                     ctx->error_line, ctx->error_column, ctx->error_msg);
        } else {
            snprintf(msg, sizeof(msg), "%s", ctx->error_msg);
        }
        LLVMGenResult *res = llvm_result_error(msg);
        llvm_ctx_destroy(ctx);
        return res;
    }

    /* Verify module */
    char *verify_error = NULL;
    if (LLVMVerifyModule(ctx->module, LLVMReturnStatusAction, &verify_error)) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "LLVM verify failed: %s",
                 verify_error ? verify_error : "(unknown)");
        LLVMDisposeMessage(verify_error);
        LLVMGenResult *res = llvm_result_error(msg);
        llvm_ctx_destroy(ctx);
        return res;
    }
    LLVMDisposeMessage(verify_error);

    /* Get IR text */
    char *ir = LLVMPrintModuleToString(ctx->module);
    char *ir_copy = pergyra_strdup(ir);
    LLVMDisposeMessage(ir);

    LLVMGenResult *res = llvm_result_success(ir_copy);
    llvm_ctx_destroy(ctx);
    return res;
}

LLVMGenResult *
llvm_codegen_to_object(const HIRProgram *hir, const char *module_name,
                       const char *output_path)
{
    LLVMGenCtx *ctx = llvm_ctx_create(module_name);
    if (ctx == NULL)
        return llvm_result_error("Out of memory");

    llvm_emit_program(hir, ctx);

    if (ctx->has_error) {
        char msg[1024];
        if (ctx->error_line > 0) {
            snprintf(msg, sizeof(msg), "line %u:%u: %s",
                     ctx->error_line, ctx->error_column, ctx->error_msg);
        } else {
            snprintf(msg, sizeof(msg), "%s", ctx->error_msg);
        }
        LLVMGenResult *res = llvm_result_error(msg);
        llvm_ctx_destroy(ctx);
        return res;
    }

    /* Verify module */
    char *verify_error = NULL;
    if (LLVMVerifyModule(ctx->module, LLVMReturnStatusAction, &verify_error)) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "LLVM verify failed: %s",
                 verify_error ? verify_error : "(unknown)");
        LLVMDisposeMessage(verify_error);
        LLVMGenResult *res = llvm_result_error(msg);
        llvm_ctx_destroy(ctx);
        return res;
    }
    LLVMDisposeMessage(verify_error);

    /* Optimize (also initializes all targets) */
    llvm_run_optimization(ctx);

    /* Get native target */
    char *triple = LLVMGetDefaultTargetTriple();
    LLVMTargetRef target;
    char *target_error = NULL;

    if (LLVMGetTargetFromTriple(triple, &target, &target_error)) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "Cannot get target: %s",
                 target_error ? target_error : "(unknown)");
        LLVMDisposeMessage(target_error);
        LLVMDisposeMessage(triple);
        LLVMGenResult *res = llvm_result_error(msg);
        llvm_ctx_destroy(ctx);
        return res;
    }

    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
        target, triple, "generic", "",
        LLVMCodeGenLevelDefault,
        LLVMRelocDefault,
        LLVMCodeModelDefault);

    LLVMSetModuleDataLayout(ctx->module,
        LLVMCreateTargetDataLayout(machine));
    LLVMSetTarget(ctx->module, triple);

    /* Emit object file */
    char *emit_error = NULL;
    if (LLVMTargetMachineEmitToFile(machine, ctx->module,
                                     (char *)output_path,
                                     LLVMObjectFile, &emit_error)) {
        char msg[1024];
        snprintf(msg, sizeof(msg), "Object emit failed: %s",
                 emit_error ? emit_error : "(unknown)");
        LLVMDisposeMessage(emit_error);
        LLVMDisposeTargetMachine(machine);
        LLVMDisposeMessage(triple);
        LLVMGenResult *res = llvm_result_error(msg);
        llvm_ctx_destroy(ctx);
        return res;
    }

    LLVMDisposeTargetMachine(machine);
    LLVMDisposeMessage(triple);

    LLVMGenResult *res = llvm_result_success(NULL);
    llvm_ctx_destroy(ctx);
    return res;
}

void
llvm_gen_result_destroy(LLVMGenResult *res)
{
    if (res == NULL)
        return;

    free(res->error_message);
    free(res->ir_text);
    free(res);
}

#endif /* PGY_LLVM_ENABLED */
