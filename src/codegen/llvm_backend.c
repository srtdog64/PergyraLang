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

static LLVMGenCtx *g_llvm_type_render_ctx = NULL;

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
    ctx->result_spec_count = 0;
    ctx->expected_type_name = NULL;

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

    if (g_llvm_type_render_ctx == ctx)
        g_llvm_type_render_ctx = NULL;

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
    free(ctx->channel_vars);
    free(ctx->class_types);
    free(ctx->var_classes);
    free(ctx->projection_borrows);
    free(ctx->array_vars);
    free(ctx->list_vars);
    free(ctx->set_vars);
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

/* Forward declaration for type mapping (used by slot helpers) */
LLVMTypeRef pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);
static bool llvm_can_forward_declare_type_early(LLVMGenCtx *ctx, ASTNode *type_node);
bool llvm_can_forward_declare_func_early(LLVMGenCtx *ctx, ASTNode *func);

static char *llvm_render_type_name(ASTNode *type_node);

void
llvm_set_type_render_ctx(LLVMGenCtx *ctx)
{
    g_llvm_type_render_ctx = ctx;
}

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

    if (strcmp(type_name, "Set") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_set_var(ctx, var_name, inner_name);
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
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL
        && type_node->data.type.generic_args->params[1] != NULL
        && type_node->data.type.generic_args->params[1]->constraint != NULL) {
        char *key_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        char *value_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[1]->constraint);
        llvm_register_map_var(ctx, var_name, key_name, value_name);
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

    if (llvm_lookup_class(ctx, type_name) != NULL
        || llvm_find_enum_decl(ctx, type_name) != NULL)
        llvm_register_var_class(ctx, var_name, type_name);
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
        ASTNode **types = NULL;
        size_t type_count = 0;
        if (g_llvm_type_render_ctx != NULL) {
            llvm_active_inventory(g_llvm_type_render_ctx, AST_TYPE_ALIAS, &types,
                                  &type_count);
        }
        if (types != NULL) {
            for (size_t i = 0; i < type_count; i++) {
                ASTNode *stmt = types[i];
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
    if (strncmp(type_name, "Set<", 4) == 0) {
        const char *inner = llvm_constructed_arg_name_at(type_name, 0);
        return llvm_set_struct_type(ctx, inner != NULL ? inner : "Int");
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
        /* llvm_constructed_arg_name_at returns a pointer into a static
         * scratch buffer; copy each arg immediately before the next call
         * clobbers it. */
        char ok_name_buf[128]  = {0};
        char err_name_buf[128] = {0};
        const char *ok_tmp  = llvm_constructed_arg_name_at(type_name, 0);
        if (ok_tmp != NULL)
            snprintf(ok_name_buf, sizeof(ok_name_buf), "%s", ok_tmp);
        const char *err_tmp = llvm_constructed_arg_name_at(type_name, 1);
        if (err_tmp != NULL)
            snprintf(err_name_buf, sizeof(err_name_buf), "%s", err_tmp);
        const char *ok_name  = ok_name_buf[0]  != '\0' ? ok_name_buf  : NULL;
        const char *err_name = err_name_buf[0] != '\0' ? err_name_buf : NULL;

        /* Legacy single-arg Result<T> defaults err to PgyError (i8ptr).
         * Two-arg Result<T, E> routes through the named-struct cache so
         * Ok/Err builders and match destructuring share one layout. */
        if (ok_name != NULL && err_name != NULL) {
            LLVMResultSpecEntry *spec =
                llvm_ensure_result_type(ctx, ok_name, err_name);
            if (spec != NULL && spec->struct_ty != NULL)
                return spec->struct_ty;
            /* fall through to legacy anonymous layout if resolution fails */
        }
        LLVMTypeRef ok_ty  = pergyra_type_to_llvm(ctx,
            ok_name  != NULL ? ok_name  : "Int");
        LLVMTypeRef err_ty = pergyra_type_to_llvm(ctx,
            err_name != NULL ? err_name : "PgyError");
        LLVMTypeRef fields[] = {
            ctx->type_i32,
            ok_ty  != NULL ? ok_ty  : ctx->type_i32,
            err_ty != NULL ? err_ty : ctx->type_i8ptr
        };
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

    /* Tuple type: anonymous struct { T0, T1, ... } */
    if (type_node->type == AST_TYPE
        && type_node->data.type.tuple_elements != NULL
        && type_node->data.type.tuple_element_count > 0) {
        size_t n = type_node->data.type.tuple_element_count;
        LLVMTypeRef *fields = calloc(n, sizeof(LLVMTypeRef));
        if (fields == NULL)
            return ctx->type_i32;
        for (size_t i = 0; i < n; i++)
            fields[i] = ast_type_to_llvm(ctx,
                type_node->data.type.tuple_elements[i]);
        LLVMTypeRef result = LLVMStructTypeInContext(ctx->context, fields,
            (unsigned)n, 0);
        free(fields);
        return result;
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

bool
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

/* MIR-based emission */
LLVMValueRef llvm_emit_func_from_mir(const MIRRoutine *routine, LLVMGenCtx *ctx);

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

#else /* !PGY_LLVM_ENABLED - stub implementations */

#include "llvm_backend.h"
#include <stdlib.h>

LLVMGenResult *llvm_codegen_from_mir(const void *mir, const char *module_name) {
    LLVMGenResult *res = calloc(1, sizeof(LLVMGenResult));
    (void)mir;
    (void)module_name;
    if (res) res->error_message = strdup("LLVM backend not enabled");
    return res;
}

LLVMGenResult *llvm_codegen_to_object_from_mir(const void *mir, const char *module_name, const char *output_path, bool release_opt) {
    LLVMGenResult *res = calloc(1, sizeof(LLVMGenResult));
    (void)mir;
    (void)module_name;
    (void)output_path;
    (void)release_opt;
    if (res) res->error_message = strdup("LLVM backend not enabled");
    return res;
}

void llvm_gen_result_destroy(LLVMGenResult *res) {
    if (res) {
        free(res->error_message);
        free(res->ir_text);
        free(res);
    }
}

#endif /* PGY_LLVM_ENABLED */
