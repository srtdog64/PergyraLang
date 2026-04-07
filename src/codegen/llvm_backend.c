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
    const HIRProgram  *hir;
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
    free(ctx->list_vars);
    free(ctx->queue_vars);
    free(ctx->map_vars);
    free(ctx->callable_vars);
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

static LLVMFuncEntry *
llvm_lookup_or_create_function(LLVMGenCtx *ctx, const char *name,
                              LLVMTypeRef fallback_type,
                              LLVMTypeRef fallback_ret_type)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    LLVMFuncEntry *entry = llvm_lookup_function(ctx, name);
    if (entry != NULL)
        return entry;

    if (ctx->module == NULL)
        return NULL;

    LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, name);
    LLVMTypeRef fn_type = NULL;
    LLVMTypeRef ret_type = fallback_ret_type;

    if (fn != NULL) {
        LLVMTypeRef value_type = LLVMTypeOf(fn);
        if (LLVMGetTypeKind(value_type) == LLVMPointerTypeKind)
            fn_type = LLVMGetElementType(value_type);
        else
            fn_type = value_type;
        if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind)
            ret_type = LLVMGetReturnType(fn_type);
    } else if (fallback_type != NULL && ctx->module != NULL) {
        fn = LLVMAddFunction(ctx->module, name, fallback_type);
        fn_type = fallback_type;
        if (ret_type == NULL)
            ret_type = LLVMGetReturnType(fallback_type);
    }

    if (fn == NULL || fn_type == NULL
        || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind || ret_type == NULL)
        return NULL;

    llvm_register_function(ctx, name, fn, fn_type, ret_type);
    return llvm_lookup_function(ctx, name);
}

/* Keep entry-point symbols alive across aggressive optimization passes.
 * The default LLVM pass pipeline may remove an otherwise unreferenced main(),
 * so we explicitly register it in llvm.compiler.used.
 */
static void
llvm_mark_function_as_used(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return;

    LLVMFuncEntry *entry = llvm_lookup_function(ctx, name);
    if (entry == NULL || entry->fn == NULL)
        return;

    LLVMTypeRef i8_type = LLVMInt8TypeInContext(ctx->context);
    LLVMTypeRef i8_ptr = LLVMPointerType(i8_type, 0);
    LLVMValueRef casted = LLVMConstBitCast(entry->fn, i8_ptr);

    LLVMValueRef used_elems[] = { casted };
    LLVMTypeRef used_ty = LLVMArrayType(i8_ptr, 1);
    LLVMValueRef used_init = LLVMConstArray(i8_ptr, used_elems, 1);

    const char *used_names[] = { "llvm.used", "llvm.compiler.used" };
    for (size_t i = 0; i < 2; i++) {
        const char *used_name = used_names[i];
        if (LLVMGetNamedGlobal(ctx->module, used_name) != NULL)
            continue;
        LLVMValueRef used_global = LLVMAddGlobal(ctx->module, used_ty, used_name);
        LLVMSetInitializer(used_global, used_init);
        LLVMSetGlobalConstant(used_global, true);
        LLVMSetSection(used_global, "llvm.metadata");
        LLVMSetLinkage(used_global, LLVMAppendingLinkage);
    }
}

/* Forward declaration for type mapping (used by slot helpers) */
LLVMTypeRef pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);
static bool llvm_can_forward_declare_type_early(LLVMGenCtx *ctx, ASTNode *type_node);
static bool llvm_can_forward_declare_func_early(LLVMGenCtx *ctx, ASTNode *func);

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
    default: {
        LLVMTypeRef inner_ty = pergyra_type_to_llvm(ctx, inner);
        LLVMTypeRef fields[] = {
            inner_ty, LLVMInt1TypeInContext(ctx->context), ctx->type_i64
        };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
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
    default: {
        LLVMTypeRef fields[] = { ctx->type_i64, ctx->type_i1, ctx->type_i1 };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
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

LLVMTypeRef
llvm_list_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner != NULL ? inner : "Int");
    LLVMTypeRef fields[] = {
        LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
}

LLVMTypeRef
llvm_queue_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner != NULL ? inner : "Int");
    LLVMTypeRef fields[] = {
        LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64,
        ctx->type_i64, ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 5, 0);
}

LLVMTypeRef
llvm_hashmap_struct_type(LLVMGenCtx *ctx, const char *value)
{
    LLVMTypeRef value_ty = pergyra_type_to_llvm(ctx, value != NULL ? value : "Int");
    LLVMTypeRef fields[] = {
        LLVMPointerType(ctx->type_i8ptr, 0),
        LLVMPointerType(value_ty, 0),
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0),
        ctx->type_i64,
        ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 5, 0);
}

LLVMValueRef
llvm_sizeof_type_i64(LLVMGenCtx *ctx, LLVMTypeRef type)
{
    LLVMValueRef zero_ptr = LLVMConstNull(LLVMPointerType(type, 0));
    LLVMValueRef one_idx = LLVMConstInt(ctx->type_i32, 1, 0);
    LLVMValueRef gep = LLVMConstGEP2(type, zero_ptr, &one_idx, 1);
    return LLVMConstPtrToInt(gep, ctx->type_i64);
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
                    LLVMTypeRef struct_type,
                    bool is_subject,
                    bool is_pointer_self_host)
{
    PGY_DYNARR_ENSURE_RET(ctx->class_types, ctx->class_type_count,
                            ctx->class_type_capacity, LLVMClassTypeEntry);

    LLVMClassTypeEntry *entry = &ctx->class_types[ctx->class_type_count++];
    entry->class_name  = class_name;
    entry->struct_type = struct_type;
    entry->is_subject  = is_subject;
    entry->is_pointer_self_host = is_pointer_self_host;
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
llvm_register_list_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *inner_type)
{
    PGY_DYNARR_ENSURE(ctx->list_vars, ctx->list_var_count,
                      ctx->list_var_capacity, LLVMListVarEntry);
    ctx->list_vars[ctx->list_var_count].var_name = var_name;
    ctx->list_vars[ctx->list_var_count].inner_type = inner_type;
    ctx->list_var_count++;
}

const char *
llvm_lookup_list_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->list_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->list_vars[i].var_name, var_name) == 0)
            return ctx->list_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_queue_var(LLVMGenCtx *ctx, const char *var_name,
                        const char *inner_type)
{
    PGY_DYNARR_ENSURE(ctx->queue_vars, ctx->queue_var_count,
                      ctx->queue_var_capacity, LLVMQueueVarEntry);
    ctx->queue_vars[ctx->queue_var_count].var_name = var_name;
    ctx->queue_vars[ctx->queue_var_count].inner_type = inner_type;
    ctx->queue_var_count++;
}

const char *
llvm_lookup_queue_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->queue_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->queue_vars[i].var_name, var_name) == 0)
            return ctx->queue_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_map_var(LLVMGenCtx *ctx, const char *var_name,
                      const char *value_type)
{
    PGY_DYNARR_ENSURE(ctx->map_vars, ctx->map_var_count,
                      ctx->map_var_capacity, LLVMMapVarEntry);
    ctx->map_vars[ctx->map_var_count].var_name = var_name;
    ctx->map_vars[ctx->map_var_count].value_type = value_type;
    ctx->map_var_count++;
}

const char *
llvm_lookup_map_value(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->map_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->map_vars[i].var_name, var_name) == 0)
            return ctx->map_vars[i].value_type;
    }
    return NULL;
}

void
llvm_register_callable_var(LLVMGenCtx *ctx, const char *var_name,
                           ASTNode *type_node)
{
    PGY_DYNARR_ENSURE(ctx->callable_vars, ctx->callable_var_count,
                      ctx->callable_var_capacity, LLVMCallableVarEntry);
    ctx->callable_vars[ctx->callable_var_count].var_name = var_name;
    ctx->callable_vars[ctx->callable_var_count].type_node = type_node;
    ctx->callable_var_count++;
}

ASTNode *
llvm_lookup_callable_var(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->callable_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->callable_vars[i].var_name, var_name) == 0)
            return ctx->callable_vars[i].type_node;
    }
    return NULL;
}

static char *llvm_render_type_name(ASTNode *type_node);
static LLVMGenCtx *g_llvm_type_render_ctx = NULL;

void
llvm_register_typed_var(LLVMGenCtx *ctx, const char *var_name,
                        ASTNode *type_node)
{
    const char *type_name;

    if (ctx == NULL || var_name == NULL || type_node == NULL)
        return;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        llvm_register_callable_var(ctx, var_name, type_node);
        return;
    }

    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return;

    type_name = type_node->data.type.name;

    if ((strcmp(type_name, "Array") == 0 || strcmp(type_name, "Slice") == 0)
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *elem_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        LLVMTypeRef elem_type = pergyra_type_to_llvm(ctx, elem_name);
        llvm_register_array_var(ctx, var_name, elem_type, -1);
        free(elem_name);
    }

    if (strcmp(type_name, "List") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_list_var(ctx, var_name, inner_name);
        return;
    }

    if (strcmp(type_name, "Queue") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_queue_var(ctx, var_name, inner_name);
        return;
    }

    if (strcmp(type_name, "HashMap") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 1
        && type_node->data.type.generic_args->params[1] != NULL
        && type_node->data.type.generic_args->params[1]->constraint != NULL) {
        char *value_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[1]->constraint);
        llvm_register_map_var(ctx, var_name, value_name);
        return;
    }

    if ((strcmp(type_name, "Future") == 0 || strcmp(type_name, "RemoteFuture") == 0)
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_future_var(ctx, var_name, inner_name,
            strcmp(type_name, "RemoteFuture") == 0);
        return;
    }

    if (strcmp(type_name, "Channel") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_channel_var(ctx, var_name, inner_name);
        return;
    }

    if (llvm_lookup_class(ctx, type_name) != NULL)
        llvm_register_var_class(ctx, var_name, type_name);
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

static const char *
llvm_constructed_arg_name_at(const char *type_name, int arg_index)
{
    static char arg_buf[256];
    const char *lt;
    const char *p;
    int current = 0;

    if (type_name == NULL || arg_index < 0)
        return NULL;
    lt = strchr(type_name, '<');
    if (lt == NULL)
        return NULL;

    p = lt + 1;
    while (*p != '\0' && *p != '>') {
        const char *start = p;
        int depth = 0;
        size_t len;
        while (*p != '\0') {
            if (*p == '<')
                depth++;
            else if (*p == '>') {
                if (depth == 0)
                    break;
                depth--;
            } else if (*p == ',' && depth == 0) {
                break;
            }
            p++;
        }
        if (current == arg_index) {
            while (*start == ' ')
                start++;
            while (p > start && p[-1] == ' ')
                p--;
            len = (size_t)(p - start);
            if (len >= sizeof(arg_buf))
                len = sizeof(arg_buf) - 1;
            memcpy(arg_buf, start, len);
            arg_buf[len] = '\0';
            return arg_buf;
        }
        if (*p == ',')
            p++;
        while (*p == ' ')
            p++;
        current++;
    }

    return NULL;
}

static char *
llvm_render_type_name(ASTNode *type_node)
{
    ASTNode *alias_decl = NULL;

    if (type_node == NULL)
        return pergyra_strdup("Void");
    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return pergyra_strdup("Int");
    if (type_node->data.type.generic_args == NULL
        || type_node->data.type.generic_args->count == 0) {
        if (g_llvm_type_render_ctx != NULL && g_llvm_type_render_ctx->hir != NULL) {
            for (size_t i = 0; i < g_llvm_type_render_ctx->hir->type_count; i++) {
                ASTNode *stmt = g_llvm_type_render_ctx->hir->types[i];
                if (stmt != NULL && stmt->type == AST_TYPE_ALIAS
                    && stmt->data.type_alias.name != NULL
                    && strcmp(stmt->data.type_alias.name, type_node->data.type.name) == 0) {
                    alias_decl = stmt;
                    break;
                }
            }
        }
        if (alias_decl != NULL && alias_decl->data.type_alias.target_type != NULL)
            return llvm_render_type_name(alias_decl->data.type_alias.target_type);
        return pergyra_strdup(type_node->data.type.name);
    }

    char *result = pergyra_strdup(type_node->data.type.name);
    for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
        GenericParam *gp = type_node->data.type.generic_args->params[i];
        char *arg_name = NULL;
        char *grown;
        size_t need;

        if (gp == NULL)
            continue;
        if (gp->constraint != NULL)
            arg_name = llvm_render_type_name(gp->constraint);
        else if (gp->name != NULL)
            arg_name = pergyra_strdup(gp->name);
        else
            arg_name = pergyra_strdup("Int");

        need = strlen(result) + strlen(arg_name) + 4;
        grown = (char *)realloc(result, need);
        if (grown == NULL) {
            free(result);
            free(arg_name);
            return pergyra_strdup("Int");
        }
        result = grown;
        {
            size_t offset = strlen(result);
            if (i == 0) {
                result[offset++] = '<';
            } else {
                result[offset++] = ',';
                result[offset++] = ' ';
            }
            {
                size_t arg_len = strlen(arg_name);
                memcpy(result + offset, arg_name, arg_len);
                offset += arg_len;
            }
            result[offset] = '\0';
        }
        free(arg_name);
    }

    {
        size_t cur_len = strlen(result);
        char *grown = (char *)realloc(result, cur_len + 2);
        if (grown == NULL) {
            free(result);
            return pergyra_strdup("Int");
        }
        result = grown;
        result[cur_len] = '>';
        result[cur_len + 1] = '\0';
    }
    return result;
}

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

    if (strncmp(type_name, "List<", 5) == 0) {
        const char *inner = llvm_constructed_arg_name_at(type_name, 0);
        return llvm_list_struct_type(ctx, inner != NULL ? inner : "Int");
    }
    if (strncmp(type_name, "Queue<", 6) == 0) {
        const char *inner = llvm_constructed_arg_name_at(type_name, 0);
        return llvm_queue_struct_type(ctx, inner != NULL ? inner : "Int");
    }
    if (strncmp(type_name, "HashMap<", 8) == 0) {
        const char *value = llvm_constructed_arg_name_at(type_name, 1);
        return llvm_hashmap_struct_type(ctx, value != NULL ? value : "Int");
    }

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

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        size_t param_count = type_node->data.event_handler_type.param_count;
        LLVMTypeRef *param_types = NULL;
        LLVMTypeRef ret_type = ctx->type_void;
        LLVMTypeRef fn_type;

        if (type_node->data.event_handler_type.return_type != NULL)
            ret_type = ast_type_to_llvm(ctx,
                type_node->data.event_handler_type.return_type);

        if (param_count > 0) {
            param_types = calloc(param_count, sizeof(LLVMTypeRef));
            if (param_types == NULL)
                return LLVMPointerType(LLVMFunctionType(ret_type, NULL, 0, 0), 0);
            for (size_t i = 0; i < param_count; i++) {
                param_types[i] = ast_type_to_llvm(ctx,
                    type_node->data.event_handler_type.param_types[i]);
            }
        }

        fn_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
        free(param_types);
        return LLVMPointerType(fn_type, 0);
    }

    if (type_node->type == AST_TYPE && type_node->data.type.name != NULL) {
        char *full_name = llvm_render_type_name(type_node);
        LLVMTypeRef resolved = pergyra_type_to_llvm(ctx, full_name);
        free(full_name);
        return resolved;
    }

    return ctx->type_i32;
}

static bool
llvm_can_forward_declare_type_early(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return true;
    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return true;

    const char *name = type_node->data.type.name;
    PgyTypeKind kind = pgy_classify_type(name);
    if (pgy_kind_to_llvm(ctx, kind) != NULL)
        return true;

    switch (kind) {
    case PGY_TK_RESULT:
    case PGY_TK_OPTION:
    case PGY_TK_SLOT:
    case PGY_TK_SECURE_SLOT:
    case PGY_TK_DEVICE_SLOT:
    case PGY_TK_REMOTE_FUTURE:
    case PGY_TK_ARRAY:
    case PGY_TK_SLICE:
    case PGY_TK_CHANNEL:
    case PGY_TK_BOX:
    case PGY_TK_RC:
    case PGY_TK_WEAK:
    case PGY_TK_FUTURE:
        return true;
    case PGY_TK_UNKNOWN:
    case PGY_TK_CLASS:
        return llvm_lookup_class(ctx, name) != NULL;
    default:
        return true;
    }
}

static bool
llvm_can_forward_declare_func_early(LLVMGenCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    if (func->data.func_decl.generic_params != NULL
        && func->data.func_decl.generic_params->count > 0)
        return false;
    if (!llvm_can_forward_declare_type_early(ctx, func->data.func_decl.return_type))
        return false;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *p = func->data.func_decl.params[i];
        if (p == NULL || p->type == NULL)
            continue;
        if (!llvm_can_forward_declare_type_early(ctx, p->type))
            return false;
    }
    return true;
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
static void llvm_forward_declare_intent(ASTNode *node, LLVMGenCtx *ctx);
static void llvm_emit_intent_decl(ASTNode *node, LLVMGenCtx *ctx);

/* MIR-based emission */
static LLVMValueRef llvm_emit_func_from_mir(const MIRRoutine *routine, LLVMGenCtx *ctx);

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

static ASTNode *
llvm_find_intent_actor_local(ASTNode *intent, const char *alias)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && involves->data.intent_involves.alias != NULL
            && strcmp(involves->data.intent_involves.alias, alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

static ASTNode *
llvm_find_zone_decl_in_hir(LLVMGenCtx *ctx, const char *zone_name)
{
    if (ctx == NULL || ctx->hir == NULL || zone_name == NULL)
        return NULL;

    for (size_t i = 0; i < ctx->hir->zone_count; i++) {
        ASTNode *zone = ctx->hir->zones[i];
        if (zone != NULL && zone->type == AST_ZONE_DECL
            && zone->data.zone_decl.name != NULL
            && strcmp(zone->data.zone_decl.name, zone_name) == 0) {
            return zone;
        }
    }
    return NULL;
}

static const char *
llvm_intent_actor_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = llvm_find_intent_actor_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
llvm_resolve_intent_zone_slot_name_for_zone(LLVMGenCtx *ctx, ASTNode *intent,
                                            const char *zone_type_name, const char *alias);

static const char *
llvm_intent_involves_type_name(ASTNode *involves)
{
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || involves->data.intent_involves.subject_type == NULL
        || involves->data.intent_involves.subject_type->type != AST_TYPE) {
        return NULL;
    }
    return involves->data.intent_involves.subject_type->data.type.name;
}

static bool
llvm_intent_involves_is_subject_participant(LLVMGenCtx *ctx, ASTNode *involves)
{
    const char *type_name = llvm_intent_involves_type_name(involves);

    if (ctx == NULL || type_name == NULL || ctx->hir == NULL)
        return false;

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_CLASS_DECL
            && stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, type_name) == 0
            && stmt->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT) {
            return true;
        }
        if (stmt->type == AST_ACTOR_DECL
            && stmt->data.actor_decl.name != NULL
            && strcmp(stmt->data.actor_decl.name, type_name) == 0) {
            return true;
        }
    }
    return false;
}

static bool
llvm_intent_involves_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *involves)
{
    const char *type_name = llvm_intent_involves_type_name(involves);
    if (ctx == NULL || type_name == NULL || ctx->hir == NULL)
        return false;

    {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
        if (cls != NULL && cls->is_pointer_self_host)
            return true;
    }

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt == NULL)
            continue;
        switch (stmt->type) {
        case AST_CLASS_DECL:
            if (stmt->data.class_decl.name != NULL
                && strcmp(stmt->data.class_decl.name, type_name) == 0
                && stmt->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL)
                return true;
            break;
        case AST_ACTOR_DECL:
        case AST_PARTY_DECL:
        case AST_SYSTEMIC_DECL:
        case AST_WORLD_DECL:
        case AST_RELATION_DECL:
        case AST_EFFECT_DECL:
        case AST_ZONE_DECL:
            if (((stmt->type == AST_ACTOR_DECL) && stmt->data.actor_decl.name != NULL
                    && strcmp(stmt->data.actor_decl.name, type_name) == 0)
                || ((stmt->type == AST_PARTY_DECL) && stmt->data.party_decl.name != NULL
                    && strcmp(stmt->data.party_decl.name, type_name) == 0)
                || ((stmt->type == AST_SYSTEMIC_DECL) && stmt->data.systemic_decl.name != NULL
                    && strcmp(stmt->data.systemic_decl.name, type_name) == 0)
                || ((stmt->type == AST_WORLD_DECL) && stmt->data.world_decl.name != NULL
                    && strcmp(stmt->data.world_decl.name, type_name) == 0)
                || ((stmt->type == AST_RELATION_DECL) && stmt->data.relation_decl.name != NULL
                    && strcmp(stmt->data.relation_decl.name, type_name) == 0)
                || ((stmt->type == AST_EFFECT_DECL) && stmt->data.effect_decl.name != NULL
                    && strcmp(stmt->data.effect_decl.name, type_name) == 0)
                || ((stmt->type == AST_ZONE_DECL) && stmt->data.zone_decl.name != NULL
                    && strcmp(stmt->data.zone_decl.name, type_name) == 0)) {
                return true;
            }
            break;
        default:
            break;
        }
    }

    return false;
}

static const char *
llvm_intent_step_effective_zone_alias(ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (step->data.intent_step.using_expr != NULL
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
        return step->data.intent_step.using_expr->data.identifier.name;
    }
    return step->data.intent_step.transfer_to_alias;
}

static const char *
llvm_intent_zone_binding_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = llvm_find_intent_actor_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
llvm_resolve_intent_zone_slot_name(LLVMGenCtx *ctx, ASTNode *intent,
                                   ASTNode *step, const char *alias)
{
    if (ctx == NULL || intent == NULL || step == NULL || alias == NULL
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE
        || step->data.intent_step.where_type->data.type.name == NULL) {
        return "<unbound>";
    }
    return llvm_resolve_intent_zone_slot_name_for_zone(ctx, intent,
        step->data.intent_step.where_type->data.type.name, alias);
}

static const char *
llvm_resolve_intent_zone_slot_name_for_zone(LLVMGenCtx *ctx, ASTNode *intent,
                                            const char *zone_type_name, const char *alias)
{
    ASTNode *zone_decl = NULL;
    const char *actor_type = NULL;
    ASTNode *named_match = NULL;
    ASTNode *typed_match = NULL;

    if (ctx == NULL || intent == NULL || zone_type_name == NULL || alias == NULL) {
        return "<unbound>";
    }

    zone_decl = llvm_find_zone_decl_in_hir(ctx, zone_type_name);
    actor_type = llvm_intent_actor_type_name(intent, alias);
    if (zone_decl == NULL)
        return "<unbound>";

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }
        if (strcmp(slot->data.domain_slot.slot_name, alias) == 0) {
            named_match = slot;
            break;
        }
        if (actor_type != NULL
            && slot->data.domain_slot.type != NULL
            && slot->data.domain_slot.type->type == AST_TYPE
            && slot->data.domain_slot.type->data.type.name != NULL
            && strcmp(slot->data.domain_slot.type->data.type.name, actor_type) == 0) {
            if (typed_match != NULL)
                typed_match = (ASTNode *)(uintptr_t)1;
            else
                typed_match = slot;
        }
    }

    if (named_match != NULL)
        return named_match->data.domain_slot.slot_name;
    if (typed_match != NULL && typed_match != (ASTNode *)(uintptr_t)1)
        return typed_match->data.domain_slot.slot_name;
    return "<unbound>";
}

static void
llvm_emit_intent_step_bind_bound_zone(LLVMGenCtx *ctx, ASTNode *intent, ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type_name;
    const char *from_alias;
    const char *from_zone_type_name;
    LLVMVarEntry *zone_var;
    LLVMValueRef zone_ptr;
    char sync_name[256];
    LLVMFuncEntry *sync_fn;
    LLVMClassTypeEntry *zone_cls;
    LLVMFuncEntry *trace_materialize_fn;
    LLVMFuncEntry *trace_transfer_fn;

    if (ctx == NULL || step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = llvm_intent_step_effective_zone_alias(step);
    zone_type_name = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type_name == NULL)
        return;

    zone_var = llvm_scope_lookup(ctx, zone_alias);
    if (zone_var == NULL)
        return;
    if (LLVMGetTypeKind(zone_var->type) != LLVMPointerTypeKind)
        return;

    zone_ptr = LLVMBuildLoad2(ctx->builder, zone_var->type, zone_var->alloca, llvm_tmp_name(ctx));
    if (LLVMGetTypeKind(LLVMTypeOf(zone_ptr)) != LLVMPointerTypeKind)
        return;

    zone_cls = llvm_lookup_class(ctx, zone_type_name);
    trace_materialize_fn = llvm_lookup_function(ctx, "pgy_intent_trace_materialize_export");
    trace_transfer_fn = llvm_lookup_function(ctx, "pgy_intent_trace_transfer_export");
    from_alias = step->data.intent_step.transfer_from_alias;
    from_zone_type_name = llvm_intent_zone_binding_type_name(intent, from_alias);

    if (from_alias != NULL && from_zone_type_name != NULL) {
        LLVMVarEntry *from_zone_var = llvm_scope_lookup(ctx, from_alias);
        LLVMValueRef from_zone_ptr;
        LLVMClassTypeEntry *from_zone_cls = llvm_lookup_class(ctx, from_zone_type_name);
        char from_sync_name[256];
        LLVMFuncEntry *from_sync_fn;

        if (from_zone_var == NULL || LLVMGetTypeKind(from_zone_var->type) != LLVMPointerTypeKind)
            return;

        from_zone_ptr = LLVMBuildLoad2(ctx->builder, from_zone_var->type,
            from_zone_var->alloca, llvm_tmp_name(ctx));
        if (LLVMGetTypeKind(LLVMTypeOf(from_zone_ptr)) != LLVMPointerTypeKind)
            return;

        if (from_zone_cls != NULL) {
            for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
                const char *alias = step->data.intent_step.who_names[i];
                const char *from_slot_name = llvm_resolve_intent_zone_slot_name_for_zone(
                    ctx, intent, from_zone_type_name, alias);
                const char *to_slot_name = llvm_resolve_intent_zone_slot_name_for_zone(
                    ctx, intent, zone_type_name, alias);
                ASTNode *involves;
                LLVMVarEntry *actor_var;
                LLVMTypeRef actor_ptr_type;
                LLVMTypeRef actor_value_type;
                LLVMValueRef actor_ptr;
                LLVMValueRef actor_value;
                LLVMValueRef handle;

                if (alias == NULL)
                    continue;

                involves = llvm_find_intent_actor_local(intent, alias);
                actor_var = llvm_scope_lookup(ctx, alias);
                if (involves == NULL || actor_var == NULL
                    || involves->data.intent_involves.subject_type == NULL) {
                    continue;
                }

                actor_ptr_type = actor_var->type;
                actor_value_type = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
                actor_ptr = LLVMBuildLoad2(ctx->builder, actor_ptr_type, actor_var->alloca,
                    llvm_tmp_name(ctx));
                actor_value = LLVMBuildLoad2(ctx->builder, actor_value_type, actor_ptr,
                    llvm_tmp_name(ctx));
                handle = llvm_scope_lookup(ctx, "__intent_handle") != NULL
                    ? LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                        llvm_scope_lookup(ctx, "__intent_handle")->alloca,
                        llvm_tmp_name(ctx))
                    : LLVMConstInt(ctx->type_i32, 0, 0);

                if (from_slot_name != NULL && strcmp(from_slot_name, "<unbound>") != 0) {
                    int from_field_idx = llvm_class_field_index(from_zone_cls, from_slot_name);
                    if (from_field_idx >= 0) {
                        LLVMValueRef from_slot_ptr = LLVMBuildStructGEP2(ctx->builder,
                            from_zone_cls->struct_type, from_zone_ptr, (unsigned)from_field_idx,
                            llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, actor_value, from_slot_ptr);
                        if (trace_materialize_fn != NULL) {
                            LLVMValueRef args[] = {
                                handle,
                                LLVMBuildGlobalStringPtr(ctx->builder, alias, llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, from_slot_name, llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, from_zone_type_name, llvm_tmp_name(ctx))
                            };
                            LLVMBuildCall2(ctx->builder, trace_materialize_fn->fn_type,
                                trace_materialize_fn->fn, args, 4, "");
                        }
                    }
                }

                if (to_slot_name != NULL && strcmp(to_slot_name, "<unbound>") != 0
                    && zone_cls != NULL) {
                    int to_field_idx = llvm_class_field_index(zone_cls, to_slot_name);
                    if (to_field_idx >= 0) {
                        LLVMValueRef to_slot_ptr = LLVMBuildStructGEP2(ctx->builder,
                            zone_cls->struct_type, zone_ptr, (unsigned)to_field_idx,
                            llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, actor_value, to_slot_ptr);
                        if (trace_transfer_fn != NULL) {
                            LLVMValueRef transfer_args[] = {
                                handle,
                                LLVMBuildGlobalStringPtr(ctx->builder, alias, llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, from_zone_type_name, llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder,
                                    from_slot_name != NULL ? from_slot_name : "<unbound>",
                                    llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, zone_type_name, llvm_tmp_name(ctx)),
                                LLVMBuildGlobalStringPtr(ctx->builder, to_slot_name, llvm_tmp_name(ctx))
                            };
                            LLVMBuildCall2(ctx->builder, trace_transfer_fn->fn_type,
                                trace_transfer_fn->fn, transfer_args, 6, "");
                        }
                    }
                }
            }
        }

        snprintf(from_sync_name, sizeof(from_sync_name), "%s_sync", from_zone_type_name);
        from_sync_fn = llvm_lookup_function(ctx, from_sync_name);
        if (from_sync_fn != NULL) {
            LLVMValueRef args[] = { from_zone_ptr };
            LLVMBuildCall2(ctx->builder, from_sync_fn->fn_type, from_sync_fn->fn, args, 1, "");
        }
        if (strcmp(from_alias, zone_alias) != 0 || strcmp(from_zone_type_name, zone_type_name) != 0) {
            snprintf(sync_name, sizeof(sync_name), "%s_sync", zone_type_name);
            sync_fn = llvm_lookup_function(ctx, sync_name);
            if (sync_fn != NULL) {
                LLVMValueRef args[] = { zone_ptr };
                LLVMBuildCall2(ctx->builder, sync_fn->fn_type, sync_fn->fn, args, 1, "");
            }
        }
        return;
    }

    if (zone_cls != NULL) {
        for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
            const char *alias = step->data.intent_step.who_names[i];
            const char *slot_name = llvm_resolve_intent_zone_slot_name(ctx, intent, step, alias);
            ASTNode *involves;
            LLVMVarEntry *actor_var;
            LLVMTypeRef actor_ptr_type;
            LLVMTypeRef actor_value_type;
            LLVMValueRef actor_ptr;
            LLVMValueRef actor_value;
            LLVMValueRef slot_ptr;
            int field_idx;

            if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
                continue;

            field_idx = llvm_class_field_index(zone_cls, slot_name);
            if (field_idx < 0)
                continue;

            involves = llvm_find_intent_actor_local(intent, alias);
            actor_var = llvm_scope_lookup(ctx, alias);
            if (involves == NULL || actor_var == NULL
                || involves->data.intent_involves.subject_type == NULL) {
                continue;
            }

            actor_ptr_type = actor_var->type;
            actor_value_type = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
            actor_ptr = LLVMBuildLoad2(ctx->builder, actor_ptr_type, actor_var->alloca,
                llvm_tmp_name(ctx));
            actor_value = LLVMBuildLoad2(ctx->builder, actor_value_type, actor_ptr,
                llvm_tmp_name(ctx));
            slot_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type, zone_ptr,
                (unsigned)field_idx, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, actor_value, slot_ptr);

            if (trace_materialize_fn != NULL) {
                LLVMValueRef handle = llvm_scope_lookup(ctx, "__intent_handle") != NULL
                    ? LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                        llvm_scope_lookup(ctx, "__intent_handle")->alloca,
                        llvm_tmp_name(ctx))
                    : LLVMConstInt(ctx->type_i32, 0, 0);
                LLVMValueRef args[] = {
                    handle,
                    LLVMBuildGlobalStringPtr(ctx->builder, alias, llvm_tmp_name(ctx)),
                    LLVMBuildGlobalStringPtr(ctx->builder, slot_name, llvm_tmp_name(ctx)),
                    LLVMBuildGlobalStringPtr(ctx->builder, zone_type_name, llvm_tmp_name(ctx))
                };
                LLVMBuildCall2(ctx->builder, trace_materialize_fn->fn_type,
                    trace_materialize_fn->fn, args, 4, "");
            }
        }
    }

    snprintf(sync_name, sizeof(sync_name), "%s_sync", zone_type_name);
    sync_fn = llvm_lookup_function(ctx, sync_name);
    if (sync_fn != NULL) {
        LLVMValueRef args[] = { zone_ptr };
        LLVMBuildCall2(ctx->builder, sync_fn->fn_type, sync_fn->fn, args, 1, "");
    }
}

static bool
llvm_emit_intent_step_rebind_bound_zone_aliases(LLVMGenCtx *ctx, ASTNode *intent,
                                                ASTNode *step, LLVMValueRef *saved_allocas)
{
    const char *zone_alias;
    const char *zone_type_name;
    LLVMVarEntry *zone_var;
    LLVMValueRef zone_ptr;
    LLVMClassTypeEntry *zone_cls;
    bool rebound = false;

    if (ctx == NULL || intent == NULL || step == NULL || saved_allocas == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return false;
    }

    zone_alias = llvm_intent_step_effective_zone_alias(step);
    zone_type_name = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type_name == NULL)
        return false;

    zone_var = llvm_scope_lookup(ctx, zone_alias);
    if (zone_var == NULL || LLVMGetTypeKind(zone_var->type) != LLVMPointerTypeKind)
        return false;

    zone_ptr = LLVMBuildLoad2(ctx->builder, zone_var->type, zone_var->alloca, llvm_tmp_name(ctx));
    zone_cls = llvm_lookup_class(ctx, zone_type_name);
    if (zone_cls == NULL)
        return false;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = llvm_resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type_name, alias);
        LLVMVarEntry *actor_var;
        int field_idx;
        LLVMValueRef original_ptr;
        LLVMValueRef slot_ptr;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;

        actor_var = llvm_scope_lookup(ctx, alias);
        if (actor_var == NULL || LLVMGetTypeKind(actor_var->type) != LLVMPointerTypeKind)
            continue;

        field_idx = llvm_class_field_index(zone_cls, slot_name);
        if (field_idx < 0)
            continue;

        original_ptr = LLVMBuildLoad2(ctx->builder, actor_var->type, actor_var->alloca, llvm_tmp_name(ctx));
        saved_allocas[i] = LLVMBuildAlloca(ctx->builder, actor_var->type, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, original_ptr, saved_allocas[i]);
        slot_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type, zone_ptr,
            (unsigned)field_idx, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, slot_ptr, actor_var->alloca);
        rebound = true;
    }

    return rebound;
}

static void
llvm_emit_intent_step_sync_effective_zone(LLVMGenCtx *ctx, ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type_name;
    LLVMVarEntry *zone_var;
    LLVMValueRef zone_ptr;
    char sync_name[256];
    LLVMFuncEntry *sync_fn;

    if (ctx == NULL || step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = llvm_intent_step_effective_zone_alias(step);
    zone_type_name = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type_name == NULL)
        return;

    zone_var = llvm_scope_lookup(ctx, zone_alias);
    if (zone_var == NULL || LLVMGetTypeKind(zone_var->type) != LLVMPointerTypeKind)
        return;

    zone_ptr = LLVMBuildLoad2(ctx->builder, zone_var->type, zone_var->alloca, llvm_tmp_name(ctx));
    snprintf(sync_name, sizeof(sync_name), "%s_sync", zone_type_name);
    sync_fn = llvm_lookup_function(ctx, sync_name);
    if (sync_fn != NULL) {
        LLVMValueRef args[] = { zone_ptr };
        LLVMBuildCall2(ctx->builder, sync_fn->fn_type, sync_fn->fn, args, 1, "");
    }
}

static void
llvm_emit_intent_step_restore_bound_zone_aliases(LLVMGenCtx *ctx, ASTNode *intent,
                                                 ASTNode *step, LLVMValueRef *saved_allocas)
{
    const char *zone_type_name;

    if (ctx == NULL || intent == NULL || step == NULL || saved_allocas == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_type_name = step->data.intent_step.where_type->data.type.name;
    if (zone_type_name == NULL)
        return;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = llvm_resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type_name, alias);
        ASTNode *involves;
        LLVMVarEntry *actor_var;
        LLVMTypeRef actor_ptr_type;
        LLVMTypeRef actor_value_type;
        LLVMValueRef zone_bound_ptr;
        LLVMValueRef zone_bound_value;
        LLVMValueRef saved_ptr;

        if (saved_allocas[i] == NULL || alias == NULL || slot_name == NULL
            || strcmp(slot_name, "<unbound>") == 0) {
            continue;
        }

        actor_var = llvm_scope_lookup(ctx, alias);
        involves = llvm_find_intent_actor_local(intent, alias);
        if (actor_var == NULL || involves == NULL
            || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        actor_ptr_type = actor_var->type;
        actor_value_type = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
        zone_bound_ptr = LLVMBuildLoad2(ctx->builder, actor_ptr_type, actor_var->alloca, llvm_tmp_name(ctx));
        zone_bound_value = LLVMBuildLoad2(ctx->builder, actor_value_type, zone_bound_ptr, llvm_tmp_name(ctx));
        saved_ptr = LLVMBuildLoad2(ctx->builder, actor_ptr_type, saved_allocas[i], llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, zone_bound_value, saved_ptr);
        LLVMBuildStore(ctx->builder, saved_ptr, actor_var->alloca);
    }
}

static ASTNode *
llvm_find_subject_action_decl(LLVMGenCtx *ctx, const char *subject_name, const char *action_name)
{
    if (ctx == NULL || ctx->hir == NULL || subject_name == NULL || action_name == NULL)
        return NULL;
    for (size_t i = 0; i < ctx->hir->type_count; i++) {
        ASTNode *decl = ctx->hir->types[i];
        if (decl == NULL || decl->type != AST_CLASS_DECL
            || decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT
            || decl->data.class_decl.name == NULL
            || strcmp(decl->data.class_decl.name, subject_name) != 0) {
            continue;
        }
        for (size_t j = 0; j < decl->data.class_decl.method_count; j++) {
            ASTNode *method = decl->data.class_decl.methods[j];
            if (method != NULL && method->type == AST_FUNC_DECL
                && method->data.func_decl.is_action
                && method->data.func_decl.name != NULL
                && strcmp(method->data.func_decl.name, action_name) == 0) {
                return method;
            }
        }
    }
    return NULL;
}

static bool
llvm_intent_action_has_only_self(ASTNode *action_decl)
{
    size_t real_pc = 0;
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return false;
    for (size_t i = 0; i < action_decl->data.func_decl.param_count; i++) {
        FuncParam *p = action_decl->data.func_decl.params[i];
        if (p == NULL || p->name == NULL)
            continue;
        if (p->type == NULL && strcmp(p->name, "self") == 0)
            continue;
        real_pc++;
    }
    return real_pc == 0;
}

static void
llvm_forward_declare_intent(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name;
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef fn_type;
    LLVMValueRef fn;

    if (node == NULL || node->type != AST_INTENT_DECL || ctx == NULL)
        return;
    name = node->data.intent_decl.name;
    if (name == NULL || llvm_lookup_function(ctx, name) != NULL)
        return;

    if (node->data.intent_decl.involve_count > 0) {
        param_types = calloc(node->data.intent_decl.involve_count, sizeof(LLVMTypeRef));
        for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
            ASTNode *involves = node->data.intent_decl.involves[i];
            LLVMTypeRef pt = ctx->type_i32;
            if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
                pt = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
                if (llvm_intent_involves_uses_pointer_self(ctx, involves))
                    pt = LLVMPointerType(pt, 0);
            }
            param_types[i] = pt;
        }
    }

    fn_type = LLVMFunctionType(ctx->type_i1, param_types,
        (unsigned)node->data.intent_decl.involve_count, 0);
    fn = LLVMAddFunction(ctx->module, name, fn_type);
    llvm_register_function(ctx, name, fn, fn_type, ctx->type_i1);
    free(param_types);
}

static void
llvm_emit_intent_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMFuncEntry *entry;
    LLVMFuncEntry *enter_fn;
    LLVMFuncEntry *exit_fn;
    LLVMFuncEntry *trace_step_fn;
    LLVMFuncEntry *trace_bind_fn;
    LLVMFuncEntry *trace_step_ok_fn;
    LLVMFuncEntry *trace_fail_fn;
    LLVMValueRef fn;
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret_type;
    LLVMBasicBlockRef entry_bb;
    LLVMBasicBlockRef run_bb;
    LLVMBasicBlockRef fail_enter_bb;
    LLVMBasicBlockRef fail_bb;
    LLVMBasicBlockRef cleanup_bb;
    LLVMBasicBlockRef compensate_bb;
    LLVMBasicBlockRef maybe_exit_bb;
    LLVMBasicBlockRef do_exit_bb;
    LLVMBasicBlockRef ret_bb;
    LLVMValueRef result_alloca;
    LLVMValueRef failed_alloca;
    LLVMValueRef fail_reason_alloca;
    LLVMValueRef handle_alloca;
    LLVMValueRef subjects_ptr;
    LLVMValueRef *completed_allocas = NULL;
    size_t subject_count = 0;
    bool has_compensate_steps = false;

    if (node == NULL || node->type != AST_INTENT_DECL || ctx == NULL)
        return;
    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        if (step != NULL && step->type == AST_INTENT_STEP
            && step->data.intent_step.compensate_expr_count > 0) {
            has_compensate_steps = true;
            break;
        }
    }
    if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_NONE)
        has_compensate_steps = false;
    entry = llvm_lookup_function(ctx, node->data.intent_decl.name);
    if (entry == NULL)
        return;
    enter_fn = llvm_lookup_function(ctx, "pgy_intent_enter_export");
    exit_fn = llvm_lookup_function(ctx, "pgy_intent_exit_export");
    trace_step_fn = llvm_lookup_function(ctx, "pgy_intent_trace_step_export");
    trace_bind_fn = llvm_lookup_function(ctx, "pgy_intent_trace_bind_export");
    trace_step_ok_fn = llvm_lookup_function(ctx, "pgy_intent_trace_step_ok_export");
    trace_fail_fn = llvm_lookup_function(ctx, "pgy_intent_trace_fail_export");
    if (enter_fn == NULL || exit_fn == NULL
        || trace_step_fn == NULL || trace_bind_fn == NULL
        || trace_step_ok_fn == NULL || trace_fail_fn == NULL)
        return;

    fn = entry->fn;
    saved_fn = ctx->current_function;
    saved_ret_type = ctx->current_ret_type;
    ctx->current_function = fn;
    ctx->current_ret_type = ctx->type_i1;

    entry_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    run_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.run");
    fail_enter_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.fail.enter");
    fail_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.fail");
    cleanup_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.cleanup");
    compensate_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.compensate");
    maybe_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.maybe_exit");
    do_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.do_exit");
    ret_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.ret");
    LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);
    llvm_scope_push(ctx);

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        const char *alias = involves != NULL ? involves->data.intent_involves.alias : NULL;
        const char *type_name = (involves != NULL
            && involves->data.intent_involves.subject_type != NULL
            && involves->data.intent_involves.subject_type->type == AST_TYPE)
            ? involves->data.intent_involves.subject_type->data.type.name : NULL;
        LLVMTypeRef pt = ctx->type_i8ptr;
        if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
            pt = ast_type_to_llvm(ctx, involves->data.intent_involves.subject_type);
            if (llvm_intent_involves_uses_pointer_self(ctx, involves))
                pt = LLVMPointerType(pt, 0);
        }
        LLVMValueRef a = llvm_create_entry_alloca(ctx, pt, alias != NULL ? alias : "actor");
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), a);
        llvm_scope_declare(ctx, alias != NULL ? alias : "actor", a, pt);
        if (type_name != NULL)
            llvm_register_var_class(ctx, alias, type_name);
    }

    result_alloca = llvm_create_entry_alloca(ctx, ctx->type_i1, "__intent_result");
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), result_alloca);
    failed_alloca = llvm_create_entry_alloca(ctx, ctx->type_i1, "__intent_failed");
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), failed_alloca);
    fail_reason_alloca = llvm_create_entry_alloca(ctx, ctx->type_i8ptr, "__intent_fail_reason");
    LLVMBuildStore(ctx->builder, LLVMBuildGlobalStringPtr(ctx->builder, "", llvm_tmp_name(ctx)),
        fail_reason_alloca);
    handle_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32, "__intent_handle");
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), handle_alloca);
    llvm_scope_declare(ctx, "__intent_handle", handle_alloca, ctx->type_i32);
    if (has_compensate_steps && node->data.intent_decl.step_count > 0) {
        completed_allocas = calloc(node->data.intent_decl.step_count, sizeof(LLVMValueRef));
        for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
            completed_allocas[i] = llvm_create_entry_alloca(ctx, ctx->type_i1, "__intent_step_done");
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), completed_allocas[i]);
        }
    }

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        if (llvm_intent_involves_is_subject_participant(ctx,
                node->data.intent_decl.involves[i])) {
            subject_count++;
        }
    }

    if (subject_count > 0) {
        LLVMTypeRef subject_array_type = LLVMArrayType(ctx->type_i8ptr,
            (unsigned)subject_count);
        LLVMValueRef subjects_alloca = llvm_create_entry_alloca(ctx,
            subject_array_type, "__intent_subjects");
        LLVMValueRef zero = LLVMConstInt(ctx->type_i32, 0, 0);
        unsigned subject_index = 0;

        for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
            ASTNode *involves = node->data.intent_decl.involves[i];
            const char *alias = involves != NULL ? involves->data.intent_involves.alias : NULL;
            LLVMVarEntry *actor_var = llvm_scope_lookup(ctx, alias != NULL ? alias : "actor");
            LLVMValueRef indices[] = {
                zero,
                LLVMConstInt(ctx->type_i32, subject_index, 0)
            };
            LLVMValueRef actor_ptr = actor_var != NULL
                ? LLVMBuildLoad2(ctx->builder, actor_var->type, actor_var->alloca, llvm_tmp_name(ctx))
                : LLVMConstPointerNull(ctx->type_i8ptr);
            LLVMValueRef cast_actor = LLVMBuildBitCast(ctx->builder, actor_ptr,
                ctx->type_i8ptr, llvm_tmp_name(ctx));
            if (!llvm_intent_involves_is_subject_participant(ctx, involves))
                continue;
            LLVMValueRef elem_ptr = LLVMBuildGEP2(ctx->builder, subject_array_type,
                subjects_alloca, indices, 2, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, cast_actor, elem_ptr);
            subject_index++;
        }

        {
            LLVMValueRef indices[] = { zero, zero };
            subjects_ptr = LLVMBuildGEP2(ctx->builder, subject_array_type,
                subjects_alloca, indices, 2, llvm_tmp_name(ctx));
        }
    } else {
        subjects_ptr = LLVMConstPointerNull(LLVMPointerType(ctx->type_i8ptr, 0));
    }

    {
        LLVMValueRef priority = node->data.intent_decl.priority_expr != NULL
            ? llvm_emit_expression(node->data.intent_decl.priority_expr, ctx)
            : LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef enter_args[] = {
            LLVMBuildGlobalStringPtr(ctx->builder, node->data.intent_decl.name,
                llvm_tmp_name(ctx)),
            subjects_ptr,
            LLVMConstInt(ctx->type_i32, (unsigned)subject_count, 0),
            LLVMConstInt(ctx->type_i1, node->data.intent_decl.is_concurrent ? 1 : 0, 0),
            priority
        };
        LLVMValueRef handle = LLVMBuildCall2(ctx->builder, enter_fn->fn_type, enter_fn->fn,
            enter_args, 5, llvm_tmp_name(ctx));
        LLVMValueRef entered = LLVMBuildICmp(ctx->builder, LLVMIntNE, handle,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, handle, handle_alloca);
        LLVMBuildCondBr(ctx->builder, entered, run_bb, fail_enter_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, fail_enter_bb);
    {
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), failed_alloca);
        LLVMBuildStore(ctx->builder,
            LLVMBuildGlobalStringPtr(ctx->builder, "enter-conflict", llvm_tmp_name(ctx)),
            fail_reason_alloca);
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), result_alloca);
        LLVMBuildBr(ctx->builder, cleanup_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, run_bb);

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        LLVMValueRef *saved_actor_ptrs = NULL;
        bool rebound_aliases = false;
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    llvm_tmp_name(ctx)),
                LLVMBuildGlobalStringPtr(ctx->builder,
                    (step->data.intent_step.where_type != NULL
                        && step->data.intent_step.where_type->type == AST_TYPE
                        && step->data.intent_step.where_type->data.type.name != NULL)
                        ? step->data.intent_step.where_type->data.type.name : "<zone>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_step_fn->fn_type, trace_step_fn->fn, args, 3, "");
        }
        for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            const char *alias = step->data.intent_step.who_names[j];
            const char *slot_name = llvm_resolve_intent_zone_slot_name(ctx, node, step, alias);
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder, alias != NULL ? alias : "<actor>",
                    llvm_tmp_name(ctx)),
                LLVMBuildGlobalStringPtr(ctx->builder, slot_name != NULL ? slot_name : "<unbound>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_bind_fn->fn_type, trace_bind_fn->fn, args, 3, "");
        }
        llvm_emit_intent_step_bind_bound_zone(ctx, node, step);
        if (step->data.intent_step.who_count > 0) {
            saved_actor_ptrs = calloc(step->data.intent_step.who_count, sizeof(LLVMValueRef));
            if (saved_actor_ptrs != NULL)
                rebound_aliases = llvm_emit_intent_step_rebind_bound_zone_aliases(ctx, node, step, saved_actor_ptrs);
        }

        if (step->data.intent_step.pre_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.pre.ok");
            LLVMValueRef cond = llvm_emit_expression(step->data.intent_step.pre_expr, ctx);
            snprintf(reason, sizeof(reason), "pre:%s",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step->data.intent_step.invariant_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.invariant.pre.ok");
            LLVMValueRef cond = llvm_emit_expression(step->data.intent_step.invariant_expr, ctx);
            snprintf(reason, sizeof(reason), "invariant-pre:%s",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step->data.intent_step.on_expr_count > 0) {
            for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
                if (step->data.intent_step.on_exprs[j] != NULL)
                    (void)llvm_emit_expression(step->data.intent_step.on_exprs[j], ctx);
            }
        }
        if (step->data.intent_step.intent_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.subintent.ok");
            LLVMValueRef cond = llvm_emit_expression(step->data.intent_step.intent_expr, ctx);
            snprintf(reason, sizeof(reason), "intent:%s",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        } else if (step->data.intent_step.on_expr_count == 0) {
            for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
                const char *alias = step->data.intent_step.who_names[j];
                ASTNode *involves = llvm_find_intent_actor_local(node, alias);
                const char *subject_name = (involves != NULL
                    && involves->data.intent_involves.subject_type != NULL
                    && involves->data.intent_involves.subject_type->type == AST_TYPE)
                    ? involves->data.intent_involves.subject_type->data.type.name : NULL;
                ASTNode *action_decl = llvm_find_subject_action_decl(ctx, subject_name,
                    step->data.intent_step.name);
                if (subject_name != NULL && action_decl != NULL
                    && llvm_intent_action_has_only_self(action_decl)) {
                    char full_name[256];
                    LLVMFuncEntry *action_fn;
                    LLVMVarEntry *actor_var = llvm_scope_lookup(ctx, alias);
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                        subject_name, step->data.intent_step.name);
                    action_fn = llvm_lookup_function(ctx, full_name);
                    if (action_fn != NULL && actor_var != NULL) {
                        LLVMValueRef actor_ptr = LLVMBuildLoad2(ctx->builder,
                            actor_var->type, actor_var->alloca, llvm_tmp_name(ctx));
                        LLVMValueRef args[] = { actor_ptr };
                        if (action_fn->ret_type == ctx->type_void) {
                            LLVMBuildCall2(ctx->builder, action_fn->fn_type, action_fn->fn,
                                args, 1, "");
                        } else {
                            (void)LLVMBuildCall2(ctx->builder, action_fn->fn_type, action_fn->fn,
                                args, 1, llvm_tmp_name(ctx));
                        }
                    }
                }
            }
        }
        if (rebound_aliases)
            llvm_emit_intent_step_sync_effective_zone(ctx, step);
        else
            llvm_emit_intent_step_bind_bound_zone(ctx, node, step);
        if (rebound_aliases)
            llvm_emit_intent_step_restore_bound_zone_aliases(ctx, node, step, saved_actor_ptrs);

        if (completed_allocas != NULL) {
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), completed_allocas[i]);
        }

        if (step->data.intent_step.guard_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.guard.ok");
            LLVMValueRef cond = llvm_emit_expression(step->data.intent_step.guard_expr, ctx);
            snprintf(reason, sizeof(reason), "guard:%s",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step->data.intent_step.expect_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.expect.ok");
            LLVMValueRef cond = llvm_emit_expression(step->data.intent_step.expect_expr, ctx);
            snprintf(reason, sizeof(reason), "expect:%s",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step->data.intent_step.post_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.post.ok");
            LLVMValueRef cond = llvm_emit_expression(step->data.intent_step.post_expr, ctx);
            snprintf(reason, sizeof(reason), "post:%s",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step->data.intent_step.invariant_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.invariant.post.ok");
            LLVMValueRef cond = llvm_emit_expression(step->data.intent_step.invariant_expr, ctx);
            snprintf(reason, sizeof(reason), "invariant-post:%s",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }
        free(saved_actor_ptrs);
        saved_actor_ptrs = NULL;
        {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_step_ok_fn->fn_type, trace_step_ok_fn->fn, args, 2, "");
        }
    }

    {
        LLVMValueRef success = node->data.intent_decl.success_expr != NULL
            ? llvm_emit_expression(node->data.intent_decl.success_expr, ctx)
            : LLVMConstInt(ctx->type_i1, 1, 0);
        LLVMBuildStore(ctx->builder, success, result_alloca);
        LLVMBuildBr(ctx->builder, cleanup_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    {
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        LLVMValueRef reason = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
            fail_reason_alloca, llvm_tmp_name(ctx));
        LLVMValueRef trace_args[] = { handle, reason };
        LLVMBuildCall2(ctx->builder, trace_fail_fn->fn_type, trace_fail_fn->fn, trace_args, 2, "");
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), failed_alloca);
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), result_alloca);
        LLVMBuildBr(ctx->builder, cleanup_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, cleanup_bb);
    {
        LLVMValueRef failed = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            failed_alloca, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, failed, compensate_bb, maybe_exit_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, compensate_bb);
    if (completed_allocas != NULL) {
        for (size_t i = node->data.intent_decl.step_count; i-- > 0;) {
            ASTNode *step = node->data.intent_decl.steps[i];
            if (step == NULL || step->type != AST_INTENT_STEP
                || step->data.intent_step.compensate_expr_count == 0)
                continue;
            {
                LLVMBasicBlockRef do_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.comp.do");
                LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.comp.next");
                LLVMValueRef done = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    completed_allocas[i], llvm_tmp_name(ctx));
                LLVMBuildCondBr(ctx->builder, done, do_bb, next_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, do_bb);
                for (size_t j = step->data.intent_step.compensate_expr_count; j-- > 0;) {
                    if (step->data.intent_step.compensate_exprs[j] != NULL)
                        (void)llvm_emit_expression(step->data.intent_step.compensate_exprs[j], ctx);
                }
                llvm_emit_intent_step_bind_bound_zone(ctx, node, step);
                if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_CURRENT)
                    LLVMBuildBr(ctx->builder, maybe_exit_bb);
                else
                    LLVMBuildBr(ctx->builder, next_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            }
        }
    }
    LLVMBuildBr(ctx->builder, maybe_exit_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, maybe_exit_bb);
    {
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        LLVMValueRef entered = LLVMBuildICmp(ctx->builder, LLVMIntNE, handle,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, entered, do_exit_bb, ret_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, do_exit_bb);
    {
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        LLVMValueRef exit_args[] = { handle };
        LLVMBuildCall2(ctx->builder, exit_fn->fn_type, exit_fn->fn, exit_args, 1, "");
        LLVMBuildBr(ctx->builder, ret_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, ret_bb);
    {
        LLVMValueRef result = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            result_alloca, llvm_tmp_name(ctx));
        LLVMBuildRet(ctx->builder, result);
    }

    llvm_scope_pop(ctx);
    free(completed_allocas);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
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
        { "pgy_log_banner", ctx->type_i8ptr },
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
            LLVMTypeRef params[6];
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
            { "pgy_string_equals", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "ToInt", ctx->type_i32,
              { ctx->type_i8ptr }, 1 },
            { "ToFloat", ctx->type_f32,
              { ctx->type_i8ptr }, 1 },
            { "Random", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_int_to_string", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_enter_export", ctx->type_i32,
              { ctx->type_i8ptr, LLVMPointerType(ctx->type_i8ptr, 0), ctx->type_i32, ctx->type_i1, ctx->type_i32 }, 5 },
            { "pgy_intent_exit_export", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_trace_step_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr }, 3 },
            { "pgy_intent_trace_bind_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr }, 3 },
            { "pgy_intent_trace_step_ok_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr }, 2 },
            { "pgy_intent_trace_fail_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr }, 2 },
            { "pgy_intent_last_trace_export", ctx->type_i8ptr,
              { }, 0 },
            { "pgy_intent_last_failure_export", ctx->type_i8ptr,
              { }, 0 },
            { "pgy_intent_last_name_export", ctx->type_i8ptr,
              { }, 0 },
            { "pgy_intent_last_handle_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_last_trace_id_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_last_step_count_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_last_failed_export", ctx->type_i1,
              { }, 0 },
            { "pgy_intent_history_count_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_history_step_name_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_zone_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_phase_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_actor_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_slot_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_from_zone_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_from_slot_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_to_zone_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_to_slot_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_ok_export", ctx->type_i1,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_failure_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_count_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_active_name_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_handle_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_trace_id_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_priority_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_concurrent_export", ctx->type_i1,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_trace_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_trace_materialize_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr }, 4 },
            { "pgy_intent_trace_transfer_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr }, 6 },
            { "pgy_read_file", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "pgy_write_file", ctx->type_void,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "pgy_input", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "SeedRandom", ctx->type_void,
              { ctx->type_i32 }, 1 },
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
        LLVMTypeRef ptr_ty  = LLVMPointerType(slot_ty, 0);
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

    /* PgyTaskHandle pgy_spawn_blocking_export(i8*, i8*) */
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle,
                                           params, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                           "pgy_spawn_blocking_export", ft);
        llvm_register_function(ctx, "pgy_spawn_blocking_export",
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

    /* Raw collection runtime used by LLVM generic List/Queue/HashMap lowering. */
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_new_raw_export", ft);
        llvm_register_function(ctx, "pgy_list_new_raw_export", fn, ft, ctx->type_void);
        fn = LLVMAddFunction(ctx->module, "pgy_queue_new_raw_export", ft);
        llvm_register_function(ctx, "pgy_queue_new_raw_export", fn, ft, ctx->type_void);
        fn = LLVMAddFunction(ctx->module, "pgy_map_new_raw_export", ft);
        llvm_register_function(ctx, "pgy_map_new_raw_export", fn, ft, ctx->type_void);
    }
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_push_raw_export", ft);
        llvm_register_function(ctx, "pgy_list_push_raw_export", fn, ft, ctx->type_void);
        fn = LLVMAddFunction(ctx->module, "pgy_queue_push_raw_export", ft);
        llvm_register_function(ctx, "pgy_queue_push_raw_export", fn, ft, ctx->type_void);
    }
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr, ctx->type_i64 };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_get_raw_export", ft);
        llvm_register_function(ctx, "pgy_list_get_raw_export", fn, ft, ctx->type_void);
        fn = LLVMAddFunction(ctx->module, "pgy_list_set_raw_export", ft);
        llvm_register_function(ctx, "pgy_list_set_raw_export", fn, ft, ctx->type_void);
    }
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_size_raw_export", ft);
        llvm_register_function(ctx, "pgy_list_size_raw_export", fn, ft, ctx->type_i32);
        fn = LLVMAddFunction(ctx->module, "pgy_queue_size_raw_export", ft);
        llvm_register_function(ctx, "pgy_queue_size_raw_export", fn, ft, ctx->type_i32);
        fn = LLVMAddFunction(ctx->module, "pgy_map_size_raw_export", ft);
        llvm_register_function(ctx, "pgy_map_size_raw_export", fn, ft, ctx->type_i32);
        ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
        fn = LLVMAddFunction(ctx->module, "pgy_queue_empty_raw_export", ft);
        llvm_register_function(ctx, "pgy_queue_empty_raw_export", fn, ft, ctx->type_i1);
    }
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i64 };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_remove_raw_export", ft);
        llvm_register_function(ctx, "pgy_list_remove_raw_export", fn, ft, ctx->type_void);
    }
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_queue_pop_raw_export", ft);
        llvm_register_function(ctx, "pgy_queue_pop_raw_export", fn, ft, ctx->type_void);
    }
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_raw_export", ft);
        llvm_register_function(ctx, "pgy_map_set_raw_export", fn, ft, ctx->type_void);
    }
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_raw_export", ft);
        llvm_register_function(ctx, "pgy_map_get_raw_export", fn, ft, ctx->type_void);
    }
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_has_raw_export", ft);
        llvm_register_function(ctx, "pgy_map_has_raw_export", fn, ft, ctx->type_i1);
    }
    {
        LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_raw_export", ft);
        llvm_register_function(ctx, "pgy_map_remove_raw_export", fn, ft, ctx->type_void);
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
llvm_emit_mir_main_wrapper(const HIRProgram *hir, LLVMGenCtx *ctx)
{
    if (hir == NULL || ctx == NULL)
        return;

    LLVMFuncEntry *main_user = llvm_lookup_or_create_function(ctx, "Main", NULL, NULL);
    bool has_top_level = (hir->executable_count > 0)
        || hir->has_main_function
        || (main_user != NULL);
    if (!has_top_level)
        return;

    LLVMTypeRef main_type = LLVMFunctionType(ctx->type_i32, NULL, 0, 0);
    LLVMFuncEntry *main_entry = llvm_lookup_or_create_function(ctx, "main", main_type,
                                                               ctx->type_i32);
    LLVMValueRef main_fn = main_entry != NULL ? main_entry->fn : NULL;
    if (main_fn == NULL) {
        return;
    }
    LLVMSetLinkage(main_fn, LLVMExternalLinkage);
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
            LLVMValueRef args[] = { LLVMConstInt(ctx->type_i64, 4, 0) };
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

    if (main_user != NULL)
        LLVMBuildCall2(ctx->builder, main_user->fn_type,
                       main_user->fn, NULL, 0, "");

    if (hir->executable_count > 0) {
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

    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        LLVMFuncEntry *shutdown_fn = llvm_lookup_function(ctx,
                                         "pgy_pool_shutdown_export");
        if (shutdown_fn != NULL)
            LLVMBuildCall2(ctx->builder, shutdown_fn->fn_type,
                           shutdown_fn->fn, NULL, 0, "");
        LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0));
    }

    llvm_mark_function_as_used(ctx, "main");
}

static void
llvm_emit_program(const HIRProgram *hir, LLVMGenCtx *ctx)
{
    if (hir == NULL) {
        llvm_set_error(ctx, "Expected lowered HIR program");
        return;
    }

    ctx->hir = hir;
    g_llvm_type_render_ctx = ctx;

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
                    llvm_register_class(ctx, enum_name, enum_ty, false, false);

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
                            payload_ty, false, false);
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

            bool is_subject = stmt->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT;
            bool is_pointer_self_host = is_subject
                || stmt->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL;
            LLVMClassTypeEntry *entry = llvm_register_class(ctx,
                cls_name, struct_ty, is_subject, is_pointer_self_host);
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

                /* Pointer-self hosts receive self by pointer; plain classes by value. */
                LLVMTypeRef *param_types = calloc(user_pc + 1,
                                                   sizeof(LLVMTypeRef));
                param_types[0] = is_pointer_self_host
                    ? LLVMPointerType(struct_ty, 0)
                    : struct_ty;
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

        LLVMClassTypeEntry *entry = llvm_register_class(ctx, aname, sty, true, true);
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

    /* Pass 0g: Early forward-declare standalone helpers whose signatures are
     * already resolvable from primitives / plain structs. This lets domain
     * methods call file-scope factory/table helpers without depending on
     * later passes. */
    for (size_t i = 0; i < hir->function_count; i++) {
        ASTNode *stmt = hir->functions[i];
        if (llvm_can_forward_declare_func_early(ctx, stmt))
            llvm_forward_declare_func(stmt, ctx);
    }

    /* Domain passes: ability, role, party, systemic, relation/effect/zone/world,
     * event. Runs before the remaining standalone function forward declarations
     * so domain nominal types are visible in those signatures. */
    llvm_emit_domain_passes(hir, ctx);

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
        } else if (llvm_lookup_function(ctx, stmt->data.func_decl.name) == NULL) {
            llvm_forward_declare_func(stmt, ctx);
        }
    }
    for (size_t i = 0; i < hir->intent_count; i++)
        llvm_forward_declare_intent(hir->intents[i], ctx);

    /* Pass 2: Emit function bodies (standalone + class methods).
     * Skip generic templates — they are instantiated lazily. */
    for (size_t i = 0; i < hir->item_count; i++) {
        ASTNode *stmt = hir->items[i].ast;
        if (stmt != NULL && stmt->type == AST_FUNC_DECL
            && llvm_lookup_generic_template(ctx, stmt->data.func_decl.name) == NULL)
            llvm_emit_func_decl(stmt, ctx);
        else if (stmt != NULL && stmt->type == AST_INTENT_DECL)
            llvm_emit_intent_decl(stmt, ctx);
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

                /* Pointer-self hosts keep a self pointer; plain classes keep a self value. */
                LLVMValueRef self_val = LLVMGetParam(fn, 0);
                if (cls != NULL && cls->is_pointer_self_host) {
                    LLVMTypeRef self_ptr_type = LLVMPointerType(
                        cls->struct_type, 0);
                    LLVMValueRef self_alloca = llvm_create_entry_alloca(
                        ctx, self_ptr_type, "self.addr");
                    LLVMBuildStore(ctx->builder, self_val, self_alloca);
                    llvm_scope_declare(ctx, "self", self_alloca,
                                        self_ptr_type);
                } else {
                    LLVMValueRef self_alloca = llvm_create_entry_alloca(
                        ctx, cls != NULL ? cls->struct_type : LLVMTypeOf(self_val),
                        "self");
                    LLVMBuildStore(ctx->builder, self_val, self_alloca);
                    llvm_scope_declare(ctx, "self", self_alloca,
                                        cls != NULL ? cls->struct_type : LLVMTypeOf(self_val));
                }
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
                    llvm_register_typed_var(ctx, p->name, p->type);
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
                llvm_register_typed_var(ctx, p->name, p->type);
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
    llvm_emit_mir_main_wrapper(hir, ctx);

    g_llvm_type_render_ctx = NULL;
}

/* =================================================================
 * Optimization pass
 * ================================================================= */

static void
llvm_init_all_targets(void)
{
    static bool initialized = false;

    if (initialized)
        return;

    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    initialized = true;
}

static LLVMTargetMachineRef
llvm_create_host_machine(char **triple_out, char **cpu_out, char **features_out)
{
    char *triple;
    char *cpu;
    char *features;
    LLVMTargetRef target = NULL;
    char *target_error = NULL;
    LLVMTargetMachineRef machine = NULL;

    if (triple_out != NULL)
        *triple_out = NULL;
    if (cpu_out != NULL)
        *cpu_out = NULL;
    if (features_out != NULL)
        *features_out = NULL;

    llvm_init_all_targets();

    triple = LLVMGetDefaultTargetTriple();
    cpu = LLVMGetHostCPUName();
    features = LLVMGetHostCPUFeatures();

    if (triple != NULL && !LLVMGetTargetFromTriple(triple, &target, &target_error)) {
        machine = LLVMCreateTargetMachine(
            target,
            triple,
            cpu != NULL ? cpu : "generic",
            features != NULL ? features : "",
            LLVMCodeGenLevelAggressive,
            LLVMRelocDefault,
            LLVMCodeModelDefault);
    }

    if (target_error != NULL)
        LLVMDisposeMessage(target_error);

    if (machine == NULL) {
        if (triple != NULL)
            LLVMDisposeMessage(triple);
        if (cpu != NULL)
            LLVMDisposeMessage(cpu);
        if (features != NULL)
            LLVMDisposeMessage(features);
        return NULL;
    }

    if (triple_out != NULL)
        *triple_out = triple;
    else if (triple != NULL)
        LLVMDisposeMessage(triple);

    if (cpu_out != NULL)
        *cpu_out = cpu;
    else if (cpu != NULL)
        LLVMDisposeMessage(cpu);

    if (features_out != NULL)
        *features_out = features;
    else if (features != NULL)
        LLVMDisposeMessage(features);

    return machine;
}

static void
llvm_apply_target_machine(LLVMGenCtx *ctx, LLVMTargetMachineRef machine,
                          const char *triple)
{
    if (ctx == NULL || machine == NULL || triple == NULL)
        return;

    LLVMTargetDataRef layout = LLVMCreateTargetDataLayout(machine);
    LLVMSetModuleDataLayout(ctx->module, layout);
    LLVMSetTarget(ctx->module, triple);
    LLVMDisposeTargetData(layout);
}

static void
llvm_run_optimization(LLVMGenCtx *ctx, LLVMTargetMachineRef machine,
                      const char *triple, bool release_opt)
{
    llvm_apply_target_machine(ctx, machine, triple);

    /*
     * TODO(CI-LLVM): keep this conservative for now while entry-point
     * retention diagnostics are being validated.
     */
    (void)release_opt;
}

/* =================================================================
 * Public API
 * ================================================================= */

/* =================================================================
 * MIR-based program emission
 * ================================================================= */

static bool
llvm_validate_mir_for_codegen(const MIRProgram *mir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is NULL");
        return false;
    }

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        char *topology_error = NULL;

        if (routine->name == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("MIR routine is missing name");
            return false;
        }

        if (!mir_validate_emission_topology(routine, false, true, &topology_error)) {
            if (error_message != NULL) {
                if (topology_error != NULL) {
                    size_t msg_len = strlen(topology_error) + 128;
                    *error_message = calloc(1, msg_len);
                    if (*error_message != NULL) {
                        snprintf(*error_message, msg_len,
                                 "MIR routine '%s' emission topology invalid: %s",
                                 routine->name != NULL ? routine->name : "(anonymous)",
                                 topology_error);
                    }
                } else {
                    *error_message = pergyra_strdup(
                        "MIR emission topology validation failed");
                }
            }
            free(topology_error);
            return false;
        }
        free(topology_error);
    }
    return true;
}

static bool
llvm_emit_program_from_mir(const MIRProgram *mir, LLVMGenCtx *ctx,
                          bool allow_hir_fallback)
{
    if (mir == NULL || ctx == NULL)
        return false;

    /* Declare runtime functions first */
    llvm_declare_runtime(ctx);

    /* Pass 1: Forward declarations from HIR (for type info) */
    if (ctx->hir != NULL) {
        for (size_t i = 0; i < ctx->hir->item_count; i++) {
            ASTNode *stmt = ctx->hir->items[i].ast;
            if (stmt != NULL && stmt->type == AST_FUNC_DECL)
                llvm_forward_declare_func(stmt, ctx);
        }
        for (size_t i = 0; i < ctx->hir->intent_count; i++)
            llvm_forward_declare_intent(ctx->hir->intents[i], ctx);
    }

    /* Pass 2: Emit function bodies from MIR (skip empty MIR — handled in Pass 3) */
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        if (routine->kind == MIR_SCOPE_FUNCTION || routine->kind == MIR_SCOPE_INTENT) {
            /* Skip routines with empty MIR blocks — let Pass 3 HIR fallback handle them */
            bool mir_has_instructions = false;
            for (size_t bi = 0; bi < routine->block_count; bi++) {
                if (routine->blocks[bi].instruction_count > 0) {
                    mir_has_instructions = true;
                    break;
                }
            }
            if (mir_has_instructions) {
                llvm_emit_func_from_mir(routine, ctx);
            }
        }
    }

    /* Pass 3: Emit remaining HIR-based declarations (classes, structs, etc.) */
    if (ctx->hir != NULL) {
        /* Emit domain/class/struct type declarations */
        llvm_emit_domain_passes(ctx->hir, ctx);

        /* Emit any functions that didn't have MIR (fallback) */
        for (size_t i = 0; i < ctx->hir->item_count; i++) {
            ASTNode *stmt = ctx->hir->items[i].ast;
            if (stmt != NULL && stmt->type == AST_FUNC_DECL) {
                /* Check if this function was emitted via MIR (non-empty) */
                bool has_mir = false;
                for (size_t j = 0; j < mir->routine_count; j++) {
                    const MIRRoutine *routine = &mir->routines[j];
                    if (routine->hir_routine != NULL &&
                        routine->hir_routine->ast == stmt) {
                        /* Only count as "has MIR" if it actually has instructions */
                        for (size_t bi = 0; bi < routine->block_count; bi++) {
                            if (routine->blocks[bi].instruction_count > 0) {
                                has_mir = true;
                                break;
                            }
                        }
                        break;
                    }
                }
                if (!has_mir) {
                    /* MIR is missing or empty — fallback to HIR emission */
                    llvm_emit_func_decl(stmt, ctx);
                }
            } else if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                /* Check if this intent has MIR emission */
                bool has_mir = false;
                for (size_t j = 0; j < mir->routine_count; j++) {
                    const MIRRoutine *routine = &mir->routines[j];
                    if (routine->hir_routine != NULL &&
                        routine->hir_routine->ast == stmt) {
                        has_mir = true;
                        break;
                    }
                }
                if (!has_mir) {
                    /* MIR is missing — fallback to HIR emission */
                    llvm_emit_intent_decl(stmt, ctx);
                }
            }
        }
    }
    llvm_emit_mir_main_wrapper(ctx->hir, ctx);
    return !ctx->has_error;
}

/* =================================================================
 * Main entry points
 * ================================================================= */

LLVMGenResult *
llvm_codegen(const HIRProgram *hir, const char *module_name)
{
    return llvm_codegen_with_mir(hir, NULL, module_name);
}

LLVMGenResult *
llvm_codegen_with_mir(const HIRProgram *hir, const MIRProgram *mir, const char *module_name)
{
    LLVMGenCtx *ctx = llvm_ctx_create(module_name);
    if (ctx == NULL)
        return llvm_result_error("Out of memory");

    ctx->hir = hir;
    ctx->mir = mir;

    /* Emit using MIR if available, otherwise fallback to HIR */
    if (mir != NULL) {
        char *mir_error = NULL;
        if (!llvm_validate_mir_for_codegen(mir, &mir_error)) {
            LLVMGenResult *res = llvm_result_error(
                mir_error != NULL ? mir_error : "Invalid MIR program");
            free(mir_error);
            llvm_ctx_destroy(ctx);
            return res;
        }
        if (!llvm_emit_program_from_mir(mir, ctx, true)) {
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
    } else {
        llvm_emit_program(hir, ctx);
    }

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
                       const char *output_path,
                       bool release_opt)
{
    return llvm_codegen_to_object_with_mir(hir, NULL, module_name, output_path,
                                           release_opt);
}

LLVMGenResult *
llvm_codegen_to_object_with_mir(const HIRProgram *hir, const MIRProgram *mir, const char *module_name,
                               const char *output_path,
                               bool release_opt)
{
    LLVMGenCtx *ctx = llvm_ctx_create(module_name);
    if (ctx == NULL)
        return llvm_result_error("Out of memory");

    ctx->hir = hir;
    ctx->mir = mir;

    if (mir != NULL) {
        char *mir_error = NULL;
        if (!llvm_validate_mir_for_codegen(mir, &mir_error)) {
            LLVMGenResult *res = llvm_result_error(
                mir_error != NULL ? mir_error : "Invalid MIR program");
            free(mir_error);
            llvm_ctx_destroy(ctx);
            return res;
        }
        if (!llvm_emit_program_from_mir(mir, ctx, true)) {
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
    } else {
        llvm_emit_program(hir, ctx);
    }

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

    {
        const char *dump_path = getenv("PGY_LLVM_DUMP_OBJ_IR");
        if (dump_path != NULL) {
            char *pre_opt = LLVMPrintModuleToString(ctx->module);
            FILE *fp = fopen(dump_path, "w");
            if (fp != NULL) {
                fputs(pre_opt, fp);
                fclose(fp);
            }
            LLVMDisposeMessage(pre_opt);
        }
    }

    char *triple = NULL;
    char *cpu = NULL;
    char *features = NULL;
    LLVMTargetMachineRef machine = llvm_create_host_machine(&triple, &cpu, &features);
    if (machine == NULL) {
        LLVMGenResult *res = llvm_result_error("Cannot create LLVM target machine");
        llvm_ctx_destroy(ctx);
        return res;
    }

    /* Optimize and emit with the same native target machine. */
    llvm_run_optimization(ctx, machine, triple, release_opt);
    llvm_apply_target_machine(ctx, machine, triple);

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
        if (triple != NULL)
            LLVMDisposeMessage(triple);
        if (cpu != NULL)
            LLVMDisposeMessage(cpu);
        if (features != NULL)
            LLVMDisposeMessage(features);
        LLVMGenResult *res = llvm_result_error(msg);
        llvm_ctx_destroy(ctx);
        return res;
    }

    LLVMDisposeTargetMachine(machine);
    if (triple != NULL)
        LLVMDisposeMessage(triple);
    if (cpu != NULL)
        LLVMDisposeMessage(cpu);
    if (features != NULL)
        LLVMDisposeMessage(features);

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

/* =================================================================
 * MIR-based function emission (Phase 2)
 * ================================================================= */

/* Map MIR value name to LLVM alloca */
typedef struct {
    const char *mir_name;
    LLVMValueRef alloca;
    LLVMTypeRef type;
} LLVMMirVar;

static LLVMValueRef
llvm_mir_get_var(LLVMMirVar *vars, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (vars[i].mir_name && strcmp(vars[i].mir_name, name) == 0)
            return vars[i].alloca;
    }
    return NULL;
}

static LLVMTypeRef
llvm_mir_type_from_ast(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (type_node == NULL || type_node->type != AST_TYPE)
        return ctx->type_i32;
    const char *t = type_node->data.type.name;
    if (strcmp(t, "Int") == 0) return ctx->type_i32;
    if (strcmp(t, "Long") == 0) return ctx->type_i64;
    if (strcmp(t, "Float") == 0) return ctx->type_f32;
    if (strcmp(t, "Double") == 0) return ctx->type_f64;
    if (strcmp(t, "Bool") == 0) return ctx->type_i1;
    if (strcmp(t, "String") == 0) return ctx->type_i8ptr;
    if (strcmp(t, "Void") == 0) return ctx->type_void;
    return ctx->type_i32;
}

static void
llvm_emit_mir_block_with_exprs(const MIRBasicBlock *mir_block, const MIRRoutine *routine,
                               LLVMGenCtx *ctx, LLVMBasicBlockRef *llvm_blocks,
                               LLVMMirVar *vars, size_t var_count, ASTNode *func_decl)
{
    (void)routine;
    (void)func_decl;
    LLVMBasicBlockRef llvm_block = llvm_blocks[mir_block->id];
    LLVMPositionBuilderAtEnd(ctx->builder, llvm_block);
    bool emitted_terminator = false;

    /* Emit instructions */
    for (size_t i = 0; i < mir_block->instruction_count; i++) {
        const MIRInstruction *inst = &mir_block->instructions[i];
        switch (inst->kind) {
            case MIR_INST_RESOURCE_OP:
                /* Resource operations → runtime function calls */
                /* For now, emit as no-op */
                break;
            case MIR_INST_DEF:
                /* SSA definition - extract initializer from let/assignment,
                 * emit as full statement so the HIR fallback path handles
                 * variable declaration, type inference, and initializer. */
                if (inst->ast != NULL && inst->result_name != NULL) {
                    if (inst->ast->type == AST_LET_DECL
                        || inst->ast->type == AST_ASSIGNMENT) {
                        /* Emit the full let/assignment statement which
                         * handles variable creation and initialization. */
                        llvm_emit_statement(inst->ast, ctx);
                    } else {
                        /* Legacy path: ast is a raw expression */
                        LLVMValueRef alloca = llvm_mir_get_var(vars, var_count, inst->result_name);
                        if (alloca != NULL) {
                            LLVMValueRef val = llvm_emit_expression(inst->ast, ctx);
                            if (val != NULL)
                                LLVMBuildStore(ctx->builder, val, alloca);
                        }
                    }
                }
                break;
            case MIR_INST_PHI:
                /* PHI nodes - copy from predecessor values */
                break;
            case MIR_INST_BRANCH:
                if (inst->ast != NULL && mir_block->has_succ_true && mir_block->has_succ_false) {
                    /* Conditional branch with condition from AST */
                    LLVMValueRef cond = llvm_emit_expression(inst->ast, ctx);
                    if (cond != NULL) {
                        LLVMBasicBlockRef true_bb = llvm_blocks[mir_block->succ_true];
                        LLVMBasicBlockRef false_bb = llvm_blocks[mir_block->succ_false];
                        LLVMBuildCondBr(ctx->builder, cond, true_bb, false_bb);
                        emitted_terminator = true;
                    }
                } else if (mir_block->has_succ_true) {
                    /* Unconditional branch */
                    LLVMBasicBlockRef target = llvm_blocks[mir_block->succ_true];
                    LLVMBuildBr(ctx->builder, target);
                    emitted_terminator = true;
                }
                break;
            case MIR_INST_RETURN:
                if (inst->ast != NULL) {
                    LLVMValueRef val = llvm_emit_expression(inst->ast, ctx);
                    if (val != NULL) {
                        LLVMBuildRet(ctx->builder, val);
                        emitted_terminator = true;
                    } else
                        LLVMBuildRetVoid(ctx->builder);
                } else {
                    LLVMBuildRetVoid(ctx->builder);
                }
                emitted_terminator = true;
                break;
            case MIR_INST_CLEANUP_EDGE:
                /* Cleanup edges → runtime call */
                break;
            case MIR_INST_STMT:
                if (inst->ast != NULL) {
                    llvm_emit_statement(inst->ast, ctx);
                }
                break;
            default:
                break;
        }
    }

    if (!emitted_terminator) {
        if (ctx->current_ret_type == ctx->type_void) {
            LLVMBuildRetVoid(ctx->builder);
        } else {
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->current_ret_type));
        }
    }
}

static LLVMValueRef
llvm_emit_func_from_mir(const MIRRoutine *routine, LLVMGenCtx *ctx)
{
    if (routine == NULL || ctx == NULL || routine->hir_routine == NULL)
        return NULL;

    ASTNode *func_decl = routine->hir_routine->ast;
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return NULL;

    /* Build LLVM function type from AST */
    size_t param_count = func_decl->data.func_decl.param_count;
    LLVMTypeRef *param_types = calloc(param_count > 0 ? param_count : 1, sizeof(LLVMTypeRef));
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *p = func_decl->data.func_decl.params[i];
        if (p != NULL && p->type != NULL)
            param_types[i] = llvm_mir_type_from_ast(ctx, p->type);
        else
            param_types[i] = ctx->type_i32;
    }
    LLVMTypeRef ret_type = ctx->type_i32;
    if (func_decl->data.func_decl.return_type != NULL)
        ret_type = llvm_mir_type_from_ast(ctx, func_decl->data.func_decl.return_type);

    LLVMTypeRef func_type = LLVMFunctionType(ret_type, param_types,
                                             (unsigned)param_count, 0);
    LLVMFuncEntry *entry = llvm_lookup_or_create_function(ctx, routine->name,
                                                          func_type,
                                                          ret_type);
    LLVMValueRef fn = entry != NULL ? entry->fn : NULL;
    if (fn == NULL)
        return NULL;
    free(param_types);

    /* Collect SSA variables and create allocas */
    size_t var_capacity = 64;
    LLVMMirVar *vars = calloc(var_capacity, sizeof(LLVMMirVar));
    size_t var_count = 0;

    /* Create basic blocks */
    LLVMBasicBlockRef *llvm_blocks = calloc(routine->block_count, sizeof(LLVMBasicBlockRef));
    for (size_t i = 0; i < routine->block_count; i++) {
        char bb_name[64];
        snprintf(bb_name, sizeof(bb_name), "bb_%zu", i);
        llvm_blocks[i] = LLVMAppendBasicBlockInContext(ctx->context, fn, bb_name);
    }

    /* Entry block: create allocas for all SSA definitions */
    LLVMPositionBuilderAtEnd(ctx->builder, llvm_blocks[routine->entry_block]);
    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *mir_block = &routine->blocks[b];
        for (size_t j = 0; j < mir_block->instruction_count; j++) {
            const MIRInstruction *inst = &mir_block->instructions[j];
            if ((inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI) && inst->result_name != NULL) {
                LLVMTypeRef alloca_type = ctx->type_i32;
                if (inst->ast != NULL) {
                    /* Try to infer type from AST */
                    if (inst->ast->type == AST_NUMBER) {
                        /* Check if it has a type annotation */
                    }
                }
                if (var_count >= var_capacity) {
                    var_capacity *= 2;
                    vars = realloc(vars, var_capacity * sizeof(LLVMMirVar));
                }
                vars[var_count].mir_name = inst->result_name;
                vars[var_count].type = alloca_type;
                vars[var_count].alloca = LLVMBuildAlloca(ctx->builder, alloca_type, inst->result_name);
                var_count++;
            }
        }
    }

    /* Set context and emit blocks */
    LLVMValueRef saved_fn = ctx->current_function;
    LLVMTypeRef saved_ret = ctx->current_ret_type;
    ctx->current_function = fn;
    ctx->current_ret_type = ret_type;

    /* Skip entry block (already positioned for allocas), emit from successors */
    if (routine->entry_block < routine->block_count) {
        llvm_emit_mir_block_with_exprs(&routine->blocks[routine->entry_block], routine, ctx, llvm_blocks, vars, var_count, func_decl);
    }
    for (size_t i = 0; i < routine->block_count; i++) {
        if (i == routine->entry_block) continue;
        const MIRBasicBlock *mir_block = &routine->blocks[i];
        if (mir_block->is_reachable && !mir_block->is_cleanup) {
            llvm_emit_mir_block_with_exprs(mir_block, routine, ctx, llvm_blocks, vars, var_count, func_decl);
        }
    }

    /* Cleanup blocks */
    if (routine->has_cleanup_block) {
        for (size_t i = 0; i < routine->block_count; i++) {
            const MIRBasicBlock *mir_block = &routine->blocks[i];
            if (mir_block->is_cleanup && mir_block->is_reachable) {
                llvm_emit_mir_block_with_exprs(mir_block, routine, ctx, llvm_blocks, vars, var_count, func_decl);
            }
        }
    }

    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    free(vars);
    free(llvm_blocks);
    return fn;
}

#else /* !PGY_LLVM_ENABLED - stub implementations */

#include "llvm_backend.h"
#include <stdlib.h>

LLVMGenResult *llvm_codegen(const void *hir, const char *module_name) {
    LLVMGenResult *res = calloc(1, sizeof(LLVMGenResult));
    if (res) res->error_message = strdup("LLVM backend not enabled");
    return res;
}

LLVMGenResult *llvm_codegen_with_mir(const void *hir, const void *mir, const char *module_name) {
    return llvm_codegen(hir, module_name);
}

LLVMGenResult *llvm_codegen_to_object_with_mir(const void *hir, const void *mir, const char *module_name, const char *output_path, bool release_opt) {
    LLVMGenResult *res = calloc(1, sizeof(LLVMGenResult));
    if (res) res->error_message = strdup("LLVM backend not enabled");
    return res;
}

LLVMGenResult *llvm_codegen_to_object(const void *hir, const char *module_name, const char *output_path, bool release_opt) {
    return llvm_codegen_to_object_with_mir(hir, NULL, module_name, output_path, release_opt);
}

void llvm_gen_result_destroy(LLVMGenResult *res) {
    if (res) {
        free(res->error_message);
        free(res->ir_text);
        free(res);
    }
}

#endif /* PGY_LLVM_ENABLED */
