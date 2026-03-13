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
#include "../common/string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/PassBuilder.h>

/* =================================================================
 * Internal context
 * ================================================================= */

#define MAX_SCOPE_DEPTH 64
#define MAX_SCOPE_VARS  256
#define MAX_FUNCTIONS   256
#define MAX_SLOT_VARS   128
#define MAX_CLASS_TYPES 64
#define MAX_CLASS_FIELDS 64

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

    /* Unique temp counter */
    int             tmp_counter;

    /* Error state */
    bool            has_error;
    char            error_msg[512];
} LLVMGenCtx;

/* =================================================================
 * Context lifecycle
 * ================================================================= */

static LLVMGenCtx *
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
    ctx->var_class_count = 0;
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
    }

    return ctx;
}

static void
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

    free(ctx);
}

/* =================================================================
 * Scope management
 * ================================================================= */

static void
llvm_scope_push(LLVMGenCtx *ctx)
{
    if (ctx->scope_depth >= MAX_SCOPE_DEPTH) {
        ctx->has_error = true;
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Scope depth overflow (max %d)", MAX_SCOPE_DEPTH);
        return;
    }
    ctx->scopes[ctx->scope_depth].count = 0;
    ctx->scope_depth++;
}

static void
llvm_scope_pop(LLVMGenCtx *ctx)
{
    if (ctx->scope_depth > 0)
        ctx->scope_depth--;
}

static void
llvm_scope_declare(LLVMGenCtx *ctx, const char *name,
                   LLVMValueRef alloca_val, LLVMTypeRef type)
{
    if (ctx->scope_depth == 0)
        return;

    LLVMScopeFrame *frame = &ctx->scopes[ctx->scope_depth - 1];
    if (frame->count >= MAX_SCOPE_VARS) {
        ctx->has_error = true;
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Too many variables in scope (max %d)", MAX_SCOPE_VARS);
        return;
    }

    frame->entries[frame->count].name   = name;
    frame->entries[frame->count].alloca = alloca_val;
    frame->entries[frame->count].type   = type;
    frame->count++;
}

static LLVMVarEntry *
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

static void
llvm_register_function(LLVMGenCtx *ctx, const char *name,
                       LLVMValueRef fn, LLVMTypeRef fn_type,
                       LLVMTypeRef ret_type)
{
    if (ctx->func_count >= MAX_FUNCTIONS)
        return;

    ctx->functions[ctx->func_count].name     = name;
    ctx->functions[ctx->func_count].fn       = fn;
    ctx->functions[ctx->func_count].fn_type  = fn_type;
    ctx->functions[ctx->func_count].ret_type = ret_type;
    ctx->func_count++;
}

static LLVMFuncEntry *
llvm_lookup_function(LLVMGenCtx *ctx, const char *name)
{
    for (int i = 0; i < ctx->func_count; i++) {
        if (strcmp(ctx->functions[i].name, name) == 0)
            return &ctx->functions[i];
    }
    return NULL;
}

/* Forward declaration for type mapping (used by slot helpers) */
static LLVMTypeRef pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);

/* =================================================================
 * Slot variable tracking
 * ================================================================= */

static void
llvm_register_slot_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *inner_type)
{
    if (ctx->slot_var_count >= MAX_SLOT_VARS)
        return;

    ctx->slot_vars[ctx->slot_var_count].var_name   = var_name;
    ctx->slot_vars[ctx->slot_var_count].inner_type = inner_type;
    ctx->slot_var_count++;
}

static const char *
llvm_lookup_slot_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->slot_vars[i].var_name, var_name) == 0)
            return ctx->slot_vars[i].inner_type;
    }
    return NULL;
}

static LLVMTypeRef
llvm_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    if (strcmp(inner, "Int") == 0)    return ctx->slot_type_Int;
    if (strcmp(inner, "Long") == 0)   return ctx->slot_type_Long;
    if (strcmp(inner, "Float") == 0)  return ctx->slot_type_Float;
    if (strcmp(inner, "Double") == 0) return ctx->slot_type_Double;
    if (strcmp(inner, "Bool") == 0)   return ctx->slot_type_Bool;
    if (strcmp(inner, "String") == 0) return ctx->slot_type_String;
    return ctx->slot_type_Int;
}

/* =================================================================
 * Class type tracking
 * ================================================================= */

static LLVMClassTypeEntry *
llvm_register_class(LLVMGenCtx *ctx, const char *class_name,
                    LLVMTypeRef struct_type)
{
    if (ctx->class_type_count >= MAX_CLASS_TYPES)
        return NULL;

    LLVMClassTypeEntry *entry = &ctx->class_types[ctx->class_type_count++];
    entry->class_name  = class_name;
    entry->struct_type = struct_type;
    entry->field_count = 0;
    return entry;
}

static void
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

static LLVMClassTypeEntry *
llvm_lookup_class(LLVMGenCtx *ctx, const char *class_name)
{
    for (int i = 0; i < ctx->class_type_count; i++) {
        if (strcmp(ctx->class_types[i].class_name, class_name) == 0)
            return &ctx->class_types[i];
    }
    return NULL;
}

static int
llvm_class_field_index(LLVMClassTypeEntry *entry, const char *field_name)
{
    for (int i = 0; i < entry->field_count; i++) {
        if (strcmp(entry->fields[i].field_name, field_name) == 0)
            return entry->fields[i].index;
    }
    return -1;
}

static void
llvm_register_var_class(LLVMGenCtx *ctx, const char *var_name,
                        const char *class_name)
{
    if (ctx->var_class_count >= MAX_SCOPE_VARS)
        return;

    ctx->var_classes[ctx->var_class_count].var_name   = var_name;
    ctx->var_classes[ctx->var_class_count].class_name = class_name;
    ctx->var_class_count++;
}

static const char *
llvm_lookup_var_class(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->var_class_count - 1; i >= 0; i--) {
        if (strcmp(ctx->var_classes[i].var_name, var_name) == 0)
            return ctx->var_classes[i].class_name;
    }
    return NULL;
}

/* =================================================================
 * Error / result helpers
 * ================================================================= */

static LLVMGenResult *
llvm_result_error(const char *message)
{
    LLVMGenResult *res = calloc(1, sizeof(LLVMGenResult));
    if (res == NULL)
        return NULL;

    res->success = false;
    res->error_message = pergyra_strdup(message);
    return res;
}

static LLVMGenResult *
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

static LLVMTypeRef
pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name)
{
    if (type_name == NULL)
        return ctx->type_void;

    if (strcmp(type_name, "Int") == 0)    return ctx->type_i32;
    if (strcmp(type_name, "Long") == 0)   return ctx->type_i64;
    if (strcmp(type_name, "Float") == 0)  return ctx->type_f32;
    if (strcmp(type_name, "Double") == 0) return ctx->type_f64;
    if (strcmp(type_name, "Bool") == 0)   return ctx->type_i1;
    if (strcmp(type_name, "String") == 0) return ctx->type_i8ptr;
    if (strcmp(type_name, "Void") == 0)   return ctx->type_void;

    /* Check if it's a registered class type */
    LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
    if (cls != NULL)
        return cls->struct_type;

    return ctx->type_i32;
}

static LLVMTypeRef
ast_type_to_llvm(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (type_node == NULL)
        return ctx->type_void;

    if (type_node->type == AST_TYPE && type_node->data.type.name != NULL)
        return pergyra_type_to_llvm(ctx, type_node->data.type.name);

    return ctx->type_i32;
}

/* =================================================================
 * Utility: temp name generation
 * ================================================================= */

static const char *
llvm_tmp_name(LLVMGenCtx *ctx)
{
    static char buf[32];
    snprintf(buf, sizeof(buf), "t%d", ctx->tmp_counter++);
    return buf;
}

/* =================================================================
 * Utility: create alloca at function entry
 * ================================================================= */

static LLVMValueRef
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

static void
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
    }
}

/* =================================================================
 * Forward declarations
 * ================================================================= */

static LLVMValueRef llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx);
static void         llvm_emit_statement(ASTNode *node, LLVMGenCtx *ctx);
static void         llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx);
static void         llvm_emit_with_stmt(ASTNode *node, LLVMGenCtx *ctx);
static void         llvm_emit_func_decl(ASTNode *node, LLVMGenCtx *ctx);

/* =================================================================
 * Expression emission
 * ================================================================= */

static LLVMValueRef
llvm_emit_number(ASTNode *node, LLVMGenCtx *ctx)
{
    double val = node->data.number.value;

    /* Check if integer fits in i32 */
    if (val == (int64_t)val && val >= -2147483648.0 && val <= 2147483647.0)
        return LLVMConstInt(ctx->type_i32, (unsigned long long)(int32_t)val, 1);

    /* Check if integer fits in i64 (beyond i32 range) */
    if (val == (double)(int64_t)val
        && val >= -9.2233720368547758e+18
        && val <=  9.2233720368547758e+18)
        return LLVMConstInt(ctx->type_i64, (unsigned long long)(int64_t)val, 1);

    return LLVMConstReal(ctx->type_f64, val);
}

static LLVMValueRef
llvm_emit_string(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *str = node->data.string.value;
    LLVMValueRef global = LLVMBuildGlobalStringPtr(ctx->builder, str,
                                                    llvm_tmp_name(ctx));
    return global;
}

static LLVMValueRef
llvm_emit_boolean(ASTNode *node, LLVMGenCtx *ctx)
{
    return LLVMConstInt(ctx->type_i1, node->data.boolean.value ? 1 : 0, 0);
}

static LLVMValueRef
llvm_emit_identifier(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.identifier.name;

    /* Look up in scope */
    LLVMVarEntry *entry = llvm_scope_lookup(ctx, name);
    if (entry != NULL)
        return LLVMBuildLoad2(ctx->builder, entry->type, entry->alloca,
                              llvm_tmp_name(ctx));

    /* Look up as function (for passing as value) */
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, name);
    if (fn != NULL)
        return fn->fn;

    /* Unknown — default to 0 */
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

static LLVMValueRef
llvm_emit_binary(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef left  = llvm_emit_expression(node->data.binary.left, ctx);
    LLVMValueRef right = llvm_emit_expression(node->data.binary.right, ctx);
    if (left == NULL || right == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMTypeRef left_type  = LLVMTypeOf(left);
    LLVMTypeRef right_type = LLVMTypeOf(right);

    /* Promote: if one side is double, convert the other */
    bool is_float = (left_type == ctx->type_f64 || left_type == ctx->type_f32
                  || right_type == ctx->type_f64 || right_type == ctx->type_f32);

    if (is_float) {
        if (left_type == ctx->type_i32)
            left = LLVMBuildSIToFP(ctx->builder, left, ctx->type_f64,
                                    llvm_tmp_name(ctx));
        if (right_type == ctx->type_i32)
            right = LLVMBuildSIToFP(ctx->builder, right, ctx->type_f64,
                                     llvm_tmp_name(ctx));
    }

    const char *tmp = llvm_tmp_name(ctx);

    switch (node->data.binary.op.type) {
    case TOKEN_PLUS:
        return is_float
            ? LLVMBuildFAdd(ctx->builder, left, right, tmp)
            : LLVMBuildAdd(ctx->builder, left, right, tmp);

    case TOKEN_MINUS:
        return is_float
            ? LLVMBuildFSub(ctx->builder, left, right, tmp)
            : LLVMBuildSub(ctx->builder, left, right, tmp);

    case TOKEN_STAR:
        return is_float
            ? LLVMBuildFMul(ctx->builder, left, right, tmp)
            : LLVMBuildMul(ctx->builder, left, right, tmp);

    case TOKEN_SLASH:
        return is_float
            ? LLVMBuildFDiv(ctx->builder, left, right, tmp)
            : LLVMBuildSDiv(ctx->builder, left, right, tmp);

    case TOKEN_PERCENT:
        return LLVMBuildSRem(ctx->builder, left, right, tmp);

    case TOKEN_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntEQ, left, right, tmp);

    case TOKEN_NOT_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealONE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntNE, left, right, tmp);

    case TOKEN_LESS:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOLT, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSLT, left, right, tmp);

    case TOKEN_LESS_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOLE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSLE, left, right, tmp);

    case TOKEN_GREATER:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOGT, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSGT, left, right, tmp);

    case TOKEN_GREATER_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOGE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSGE, left, right, tmp);

    case TOKEN_AND:
        return LLVMBuildAnd(ctx->builder, left, right, tmp);

    case TOKEN_OR:
        return LLVMBuildOr(ctx->builder, left, right, tmp);

    default:
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }
}

static LLVMValueRef
llvm_emit_unary(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef operand = llvm_emit_expression(node->data.unary.operand, ctx);
    if (operand == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    const char *tmp = llvm_tmp_name(ctx);

    switch (node->data.unary.op.type) {
    case TOKEN_MINUS:
        if (LLVMTypeOf(operand) == ctx->type_f64 ||
            LLVMTypeOf(operand) == ctx->type_f32)
            return LLVMBuildFNeg(ctx->builder, operand, tmp);
        return LLVMBuildNeg(ctx->builder, operand, tmp);

    case TOKEN_NOT:
        return LLVMBuildNot(ctx->builder, operand, tmp);

    default:
        return operand;
    }
}

static LLVMValueRef
llvm_emit_call(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.call.callee == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Method call: obj.method(args) */
    if (node->data.call.callee->type == AST_MEMBER_ACCESS) {
        ASTNode *obj_node = node->data.call.callee->data.member.object;
        const char *method_name = node->data.call.callee->data.member.name;

        if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
            && method_name != NULL) {
            const char *var_name = obj_node->data.identifier.name;
            const char *class_name = llvm_lookup_var_class(ctx, var_name);
            LLVMVarEntry *var = llvm_scope_lookup(ctx, var_name);

            if (class_name != NULL && var != NULL) {
                LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, class_name);
                if (cls != NULL) {
                    char full_name[256];
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                             class_name, method_name);
                    LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
                    if (fn != NULL) {
                        /* Build args: self ptr + user args */
                        size_t argc = node->data.call.arg_count;
                        LLVMValueRef *args = calloc(argc + 1,
                                                     sizeof(LLVMValueRef));
                        /* Get object pointer for self */
                        LLVMValueRef self_ptr = var->alloca;
                        if (var->type == LLVMPointerType(
                                cls->struct_type, 0)) {
                            self_ptr = LLVMBuildLoad2(ctx->builder,
                                var->type, var->alloca,
                                llvm_tmp_name(ctx));
                        }
                        args[0] = self_ptr;
                        for (size_t i = 0; i < argc; i++) {
                            args[i + 1] = llvm_emit_expression(
                                node->data.call.arguments[i], ctx);
                        }

                        LLVMValueRef result;
                        if (fn->ret_type == ctx->type_void) {
                            LLVMBuildCall2(ctx->builder, fn->fn_type,
                                fn->fn, args, (unsigned)(argc + 1), "");
                            result = LLVMConstInt(ctx->type_i32, 0, 0);
                        } else {
                            result = LLVMBuildCall2(ctx->builder,
                                fn->fn_type, fn->fn, args,
                                (unsigned)(argc + 1), llvm_tmp_name(ctx));
                        }
                        free(args);
                        return result;
                    }
                }
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Get callee name */
    const char *callee_name = NULL;
    if (node->data.call.callee->type == AST_IDENTIFIER)
        callee_name = node->data.call.callee->data.identifier.name;

    if (callee_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Built-in: Log */
    if (strcmp(callee_name, "Log") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef arg = llvm_emit_expression(
            node->data.call.arguments[0], ctx);
        if (arg == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        /* Select the right log function based on arg type */
        LLVMTypeRef arg_type = LLVMTypeOf(arg);
        const char *log_fn_name = "pgy_log_int";

        if (arg_type == ctx->type_i64)        log_fn_name = "pgy_log_long";
        else if (arg_type == ctx->type_f32)   log_fn_name = "pgy_log_float";
        else if (arg_type == ctx->type_f64)   log_fn_name = "pgy_log_double";
        else if (arg_type == ctx->type_i1)    log_fn_name = "pgy_log_bool";
        else if (arg_type == ctx->type_i8ptr) log_fn_name = "pgy_log_string";

        LLVMFuncEntry *log_fn = llvm_lookup_function(ctx, log_fn_name);
        if (log_fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { arg };
        LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn,
                       args, 1, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: ClaimSlot<T>() — handled mostly in let_decl, but standalone */
    if (strcmp(callee_name, "ClaimSlot") == 0
        || strcmp(callee_name, "ClaimSecureSlot") == 0) {
        /* Standalone ClaimSlot: default to Int */
        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_claim_Int");
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                               NULL, 0, llvm_tmp_name(ctx));
    }

    /* Built-in: Write(slot, value) */
    if (strcmp(callee_name, "Write") == 0) {
        if (node->data.call.arg_count < 2)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        /* Resolve slot inner type */
        const char *inner = "Int";
        ASTNode *slot_arg = node->data.call.arguments[0];
        if (slot_arg->type == AST_IDENTIFIER)
            inner = llvm_lookup_slot_inner(ctx, slot_arg->data.identifier.name);
        if (inner == NULL) inner = "Int";

        /* Get slot alloca pointer */
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (val == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca, val };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: Read(slot) */
    if (strcmp(callee_name, "Read") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *inner = "Int";
        ASTNode *slot_arg = node->data.call.arguments[0];
        if (slot_arg->type == AST_IDENTIFIER)
            inner = llvm_lookup_slot_inner(ctx, slot_arg->data.identifier.name);
        if (inner == NULL) inner = "Int";

        LLVMVarEntry *slot_var = NULL;
        if (slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                               args, 1, llvm_tmp_name(ctx));
    }

    /* Built-in: Release(slot) */
    if (strcmp(callee_name, "Release") == 0) {
        if (node->data.call.arg_count < 1)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        const char *inner = "Int";
        ASTNode *slot_arg = node->data.call.arguments[0];
        if (slot_arg->type == AST_IDENTIFIER)
            inner = llvm_lookup_slot_inner(ctx, slot_arg->data.identifier.name);
        if (inner == NULL) inner = "Int";

        LLVMVarEntry *slot_var = NULL;
        if (slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (slot_var == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_release_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMValueRef args[] = { slot_var->alloca };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Look up user function */
    LLVMFuncEntry *func = llvm_lookup_function(ctx, callee_name);
    if (func == NULL) {
        /* Unknown function — return 0 */
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Build arguments */
    size_t argc = node->data.call.arg_count;
    LLVMValueRef *args = NULL;
    if (argc > 0) {
        args = calloc(argc, sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++) {
            args[i] = llvm_emit_expression(node->data.call.arguments[i], ctx);
        }
    }

    LLVMValueRef result;
    if (func->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                       args, (unsigned)argc, "");
        result = LLVMConstInt(ctx->type_i32, 0, 0);
    } else {
        result = LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                                args, (unsigned)argc, llvm_tmp_name(ctx));
    }

    free(args);
    return result;
}

static LLVMValueRef
llvm_emit_assignment(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.assignment.target == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Member assignment: obj.field = value */
    if (node->data.assignment.target->type == AST_MEMBER_ACCESS) {
        ASTNode *member_node = node->data.assignment.target;
        ASTNode *obj_node = member_node->data.member.object;
        const char *field_name = member_node->data.member.name;

        if (obj_node != NULL && obj_node->type == AST_IDENTIFIER
            && field_name != NULL) {
            const char *var_name = obj_node->data.identifier.name;
            LLVMVarEntry *var = llvm_scope_lookup(ctx, var_name);
            const char *class_name = llvm_lookup_var_class(ctx, var_name);

            if (var != NULL && class_name != NULL) {
                LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, class_name);
                if (cls != NULL) {
                    int field_idx = llvm_class_field_index(cls, field_name);
                    if (field_idx >= 0) {
                        LLVMValueRef val = llvm_emit_expression(
                            node->data.assignment.value, ctx);
                        if (val == NULL)
                            return LLVMConstInt(ctx->type_i32, 0, 0);

                        /* self: alloca holds pointer-to-struct */
                        LLVMValueRef base = var->alloca;
                        if (var->type == LLVMPointerType(
                                cls->struct_type, 0)) {
                            base = LLVMBuildLoad2(ctx->builder,
                                var->type, var->alloca,
                                llvm_tmp_name(ctx));
                        }
                        LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
                            cls->struct_type, base,
                            (unsigned)field_idx, llvm_tmp_name(ctx));
                        LLVMBuildStore(ctx->builder, val, gep);
                        return val;
                    }
                }
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    const char *name = NULL;
    if (node->data.assignment.target->type == AST_IDENTIFIER)
        name = node->data.assignment.target->data.identifier.name;

    if (name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMVarEntry *var = llvm_scope_lookup(ctx, name);
    if (var == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMValueRef val = llvm_emit_expression(node->data.assignment.value, ctx);
    if (val == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMBuildStore(ctx->builder, val, var->alloca);
    return val;
}

static LLVMValueRef
llvm_emit_member_access(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *obj_node = node->data.member.object;
    const char *field_name = node->data.member.name;

    if (obj_node == NULL || field_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Resolve object variable */
    if (obj_node->type != AST_IDENTIFIER)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    const char *var_name = obj_node->data.identifier.name;
    LLVMVarEntry *var = llvm_scope_lookup(ctx, var_name);
    if (var == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Find class type for this variable */
    const char *class_name = llvm_lookup_var_class(ctx, var_name);
    if (class_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, class_name);
    if (cls == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    int field_idx = llvm_class_field_index(cls, field_name);
    if (field_idx < 0)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* For 'self', the alloca holds a pointer-to-struct (need to load first).
       For regular vars, the alloca IS the struct. */
    LLVMValueRef base_ptr = var->alloca;
    if (var->type == LLVMPointerType(cls->struct_type, 0)) {
        /* self case: load the struct pointer from the alloca */
        base_ptr = LLVMBuildLoad2(ctx->builder, var->type, var->alloca,
                                   llvm_tmp_name(ctx));
    }

    /* GEP to get field pointer, then load */
    LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder,
        cls->struct_type, base_ptr, (unsigned)field_idx,
        llvm_tmp_name(ctx));

    LLVMTypeRef field_type = cls->fields[field_idx].field_type;
    return LLVMBuildLoad2(ctx->builder, field_type, gep,
                           llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return NULL;

    switch (node->type) {
    case AST_NUMBER:        return llvm_emit_number(node, ctx);
    case AST_STRING:        return llvm_emit_string(node, ctx);
    case AST_BOOLEAN:       return llvm_emit_boolean(node, ctx);
    case AST_IDENTIFIER:    return llvm_emit_identifier(node, ctx);
    case AST_BINARY:        return llvm_emit_binary(node, ctx);
    case AST_UNARY:         return llvm_emit_unary(node, ctx);
    case AST_CALL:          return llvm_emit_call(node, ctx);
    case AST_ASSIGNMENT:    return llvm_emit_assignment(node, ctx);
    case AST_MEMBER_ACCESS: return llvm_emit_member_access(node, ctx);

    case AST_SPAWN_EXPR:
        /* MVP: direct call (no threading) */
        if (node->data.spawn_expr.function != NULL)
            return llvm_emit_expression(node->data.spawn_expr.function, ctx);
        return LLVMConstInt(ctx->type_i32, 0, 0);

    case AST_AWAIT_EXPR:
        /* MVP: evaluate inner expression directly */
        if (node->data.await_expr.expression != NULL)
            return llvm_emit_expression(node->data.await_expr.expression, ctx);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    case AST_LAMBDA_EXPR:
        /* MVP: emit body as expression if single expression */
        if (node->data.lambda_expr.body != NULL)
            return llvm_emit_expression(node->data.lambda_expr.body, ctx);
        return LLVMConstInt(ctx->type_i32, 0, 0);

    default:
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }
}

/* =================================================================
 * Statement emission
 * ================================================================= */

static void
llvm_emit_let_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode *type_ann = node->data.let_decl.type;
    ASTNode *init     = node->data.let_decl.initializer;

    /* Detect ClaimSlot / ClaimSecureSlot */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee = init->data.call.callee->data.identifier.name;
        if (strcmp(callee, "ClaimSlot") == 0
            || strcmp(callee, "ClaimSecureSlot") == 0) {
            /* Resolve inner type from type annotation */
            const char *inner = "Int";
            if (type_ann != NULL && type_ann->type == AST_TYPE) {
                /* Check for generic args: Slot<Int> */
                if (type_ann->data.type.generic_args != NULL
                    && type_ann->data.type.generic_args->count > 0)
                    inner = type_ann->data.type.generic_args->params[0]->name;
                else if (type_ann->data.type.name != NULL) {
                    /* Try to extract inner from type name like "Slot_Int" */
                    const char *tn = type_ann->data.type.name;
                    if (strncmp(tn, "Slot", 4) == 0)
                        inner = "Int"; /* default */
                }
            }

            LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
            LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, name);

            /* Call pgy_claim_T() and store result */
            char fn_name[64];
            snprintf(fn_name, sizeof(fn_name), "pgy_claim_%s", inner);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn != NULL) {
                LLVMValueRef claimed = LLVMBuildCall2(ctx->builder,
                    fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, claimed, alloca_val);
            }

            llvm_scope_declare(ctx, name, alloca_val, slot_ty);
            llvm_register_slot_var(ctx, name, inner);
            return;
        }
    }

    /* Detect class constructor: let v = ClassName(args...) */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee = init->data.call.callee->data.identifier.name;
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, callee);
        if (cls != NULL) {
            /* Allocate struct on stack */
            LLVMValueRef alloca_val = llvm_create_entry_alloca(
                ctx, cls->struct_type, name);

            /* Store each argument into corresponding field */
            size_t argc = init->data.call.arg_count;
            for (size_t i = 0; i < argc && (int)i < cls->field_count; i++) {
                LLVMValueRef arg = llvm_emit_expression(
                    init->data.call.arguments[i], ctx);
                LLVMValueRef gep = LLVMBuildStructGEP2(
                    ctx->builder, cls->struct_type, alloca_val,
                    (unsigned)cls->fields[i].index, llvm_tmp_name(ctx));
                LLVMBuildStore(ctx->builder, arg, gep);
            }

            llvm_scope_declare(ctx, name, alloca_val, cls->struct_type);
            llvm_register_var_class(ctx, name, callee);
            return;
        }
    }

    /* Determine type from annotation or initializer */
    LLVMTypeRef var_type = ctx->type_i32; /* default */
    if (type_ann != NULL)
        var_type = ast_type_to_llvm(ctx, type_ann);

    /* Create alloca at function entry */
    LLVMValueRef alloca = llvm_create_entry_alloca(ctx, var_type, name);

    /* Store initializer if present */
    if (init != NULL) {
        LLVMValueRef val = llvm_emit_expression(init, ctx);
        if (val != NULL) {
            LLVMTypeRef val_type = LLVMTypeOf(val);

            /* Type coercion between numeric types */
            if (var_type != val_type) {
                bool var_is_int = (var_type == ctx->type_i32 || var_type == ctx->type_i64);
                bool var_is_fp  = (var_type == ctx->type_f32 || var_type == ctx->type_f64);
                bool val_is_int = (val_type == ctx->type_i32 || val_type == ctx->type_i64);
                bool val_is_fp  = (val_type == ctx->type_f32 || val_type == ctx->type_f64);

                if (var_is_int && val_is_fp)
                    val = LLVMBuildFPToSI(ctx->builder, val, var_type,
                                           llvm_tmp_name(ctx));
                else if (var_is_fp && val_is_int)
                    val = LLVMBuildSIToFP(ctx->builder, val, var_type,
                                           llvm_tmp_name(ctx));
                else if (var_is_int && val_is_int)
                    val = (LLVMGetIntTypeWidth(var_type) > LLVMGetIntTypeWidth(val_type))
                        ? LLVMBuildSExt(ctx->builder, val, var_type, llvm_tmp_name(ctx))
                        : LLVMBuildTrunc(ctx->builder, val, var_type, llvm_tmp_name(ctx));
                else if (var_is_fp && val_is_fp)
                    val = (var_type == ctx->type_f64)
                        ? LLVMBuildFPExt(ctx->builder, val, var_type, llvm_tmp_name(ctx))
                        : LLVMBuildFPTrunc(ctx->builder, val, var_type, llvm_tmp_name(ctx));
            }

            LLVMBuildStore(ctx->builder, val, alloca);
        }
    }

    llvm_scope_declare(ctx, name, alloca, var_type);

    /* Track class type for member access */
    if (type_ann != NULL && type_ann->type == AST_TYPE
        && type_ann->data.type.name != NULL) {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx,
            type_ann->data.type.name);
        if (cls != NULL)
            llvm_register_var_class(ctx, name, type_ann->data.type.name);
    }
}

static void
llvm_emit_return_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.return_stmt.value != NULL) {
        LLVMValueRef val = llvm_emit_expression(node->data.return_stmt.value,
                                                 ctx);
        if (val != NULL) {
            /* Coerce to expected return type */
            LLVMTypeRef val_type = LLVMTypeOf(val);
            LLVMTypeRef ret_type = ctx->current_ret_type;
            if (ret_type != val_type && ret_type != ctx->type_void) {
                bool ret_is_int = (ret_type == ctx->type_i32 || ret_type == ctx->type_i64);
                bool ret_is_fp  = (ret_type == ctx->type_f32 || ret_type == ctx->type_f64);
                bool val_is_int = (val_type == ctx->type_i32 || val_type == ctx->type_i64);
                bool val_is_fp  = (val_type == ctx->type_f32 || val_type == ctx->type_f64);

                if (ret_is_int && val_is_fp)
                    val = LLVMBuildFPToSI(ctx->builder, val, ret_type,
                                           llvm_tmp_name(ctx));
                else if (ret_is_fp && val_is_int)
                    val = LLVMBuildSIToFP(ctx->builder, val, ret_type,
                                           llvm_tmp_name(ctx));
                else if (ret_is_int && val_is_int)
                    val = (LLVMGetIntTypeWidth(ret_type) > LLVMGetIntTypeWidth(val_type))
                        ? LLVMBuildSExt(ctx->builder, val, ret_type, llvm_tmp_name(ctx))
                        : LLVMBuildTrunc(ctx->builder, val, ret_type, llvm_tmp_name(ctx));
                else if (ret_is_fp && val_is_fp)
                    val = (ret_type == ctx->type_f64)
                        ? LLVMBuildFPExt(ctx->builder, val, ret_type, llvm_tmp_name(ctx))
                        : LLVMBuildFPTrunc(ctx->builder, val, ret_type, llvm_tmp_name(ctx));
            }
            LLVMBuildRet(ctx->builder, val);
        } else {
            LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0));
        }
    } else {
        if (ctx->current_ret_type == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else
            LLVMBuildRet(ctx->builder,
                          LLVMConstInt(ctx->current_ret_type, 0, 0));
    }
}

static void
llvm_emit_if_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef cond = llvm_emit_expression(node->data.if_stmt.condition, ctx);
    if (cond == NULL)
        return;

    /* Ensure cond is i1 */
    if (LLVMTypeOf(cond) != ctx->type_i1)
        cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond,
                              LLVMConstInt(LLVMTypeOf(cond), 0, 0),
                              llvm_tmp_name(ctx));

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef then_bb  = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "then");
    LLVMBasicBlockRef else_bb  = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "else");
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "ifcont");

    LLVMBuildCondBr(ctx->builder, cond, then_bb, else_bb);

    /* Then block */
    LLVMPositionBuilderAtEnd(ctx->builder, then_bb);
    if (node->data.if_stmt.then_branch != NULL)
        llvm_emit_statement(node->data.if_stmt.then_branch, ctx);
    /* Only branch to merge if no terminator (return) was emitted */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    /* Else block */
    LLVMPositionBuilderAtEnd(ctx->builder, else_bb);
    if (node->data.if_stmt.else_branch != NULL)
        llvm_emit_statement(node->data.if_stmt.else_branch, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    /* Merge */
    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

static void
llvm_emit_while_loop(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.body");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "while.exit");

    LLVMBuildBr(ctx->builder, cond_bb);

    /* Condition */
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef cond = llvm_emit_expression(node->data.while_loop.condition,
                                              ctx);
    if (cond != NULL && LLVMTypeOf(cond) != ctx->type_i1)
        cond = LLVMBuildICmp(ctx->builder, LLVMIntNE, cond,
                              LLVMConstInt(LLVMTypeOf(cond), 0, 0),
                              llvm_tmp_name(ctx));
    if (cond == NULL)
        cond = LLVMConstInt(ctx->type_i1, 0, 0);

    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    /* Body */
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    if (node->data.while_loop.body != NULL)
        llvm_emit_statement(node->data.while_loop.body, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, cond_bb);

    /* Exit */
    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);
}

static void
llvm_emit_for_loop(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *var_name = node->data.for_loop.variable;

    llvm_scope_push(ctx);

    /* Create loop variable */
    LLVMValueRef var_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32,
                                                        var_name);
    LLVMValueRef start = llvm_emit_expression(node->data.for_loop.range_start,
                                               ctx);
    if (start == NULL)
        start = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMBuildStore(ctx->builder, start, var_alloca);
    llvm_scope_declare(ctx, var_name, var_alloca, ctx->type_i32);

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef cond_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.cond");
    LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.body");
    LLVMBasicBlockRef incr_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.incr");
    LLVMBasicBlockRef exit_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "for.exit");

    LLVMBuildBr(ctx->builder, cond_bb);

    /* Condition: i < end */
    LLVMPositionBuilderAtEnd(ctx->builder, cond_bb);
    LLVMValueRef current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                           var_alloca, llvm_tmp_name(ctx));
    LLVMValueRef end = llvm_emit_expression(node->data.for_loop.range_end, ctx);
    if (end == NULL)
        end = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMValueRef cond = LLVMBuildICmp(ctx->builder, LLVMIntSLT, current, end,
                                       llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, cond, body_bb, exit_bb);

    /* Body */
    LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
    if (node->data.for_loop.body != NULL)
        llvm_emit_statement(node->data.for_loop.body, ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, incr_bb);

    /* Increment: i = i + 1 */
    LLVMPositionBuilderAtEnd(ctx->builder, incr_bb);
    LLVMValueRef cur2 = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                                        var_alloca, llvm_tmp_name(ctx));
    LLVMValueRef next = LLVMBuildAdd(ctx->builder, cur2,
                                      LLVMConstInt(ctx->type_i32, 1, 0),
                                      llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, var_alloca);
    LLVMBuildBr(ctx->builder, cond_bb);

    /* Exit */
    LLVMPositionBuilderAtEnd(ctx->builder, exit_bb);

    llvm_scope_pop(ctx);
}

static void
llvm_emit_match_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef subject = llvm_emit_expression(node->data.match_stmt.subject,
                                                 ctx);
    if (subject == NULL)
        return;

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "match.end");

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        if (mc == NULL || mc->type != AST_MATCH_CASE)
            continue;

        LLVMValueRef pattern = llvm_emit_expression(mc->data.match_case.pattern,
                                                     ctx);
        if (pattern == NULL)
            continue;

        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                                          subject, pattern,
                                          llvm_tmp_name(ctx));

        LLVMBasicBlockRef case_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "match.case");
        LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "match.next");

        LLVMBuildCondBr(ctx->builder, cmp, case_bb, next_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
        if (mc->data.match_case.body != NULL)
            llvm_emit_statement(mc->data.match_case.body, ctx);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, merge_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
    }

    /* Default case */
    if (node->data.match_stmt.default_body != NULL) {
        llvm_emit_statement(node->data.match_stmt.default_body, ctx);
    }
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

static void
llvm_emit_with_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *alias = node->data.with_stmt.alias;
    bool is_secure    = node->data.with_stmt.is_secure;

    const char *inner = "Int";
    if (node->data.with_stmt.slot_type != NULL
        && node->data.with_stmt.slot_type->type == AST_TYPE
        && node->data.with_stmt.slot_type->data.type.name != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;

    (void)is_secure; /* TODO: SecureSlot in later phase */

    LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
    LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, alias);

    /* Call pgy_claim_T() and store */
    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "pgy_claim_%s", inner);
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
    if (fn != NULL) {
        LLVMValueRef claimed = LLVMBuildCall2(ctx->builder,
            fn->fn_type, fn->fn, NULL, 0, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, claimed, alloca_val);
    }

    /* Push scope, register slot variable */
    llvm_scope_push(ctx);
    llvm_scope_declare(ctx, alias, alloca_val, slot_ty);
    llvm_register_slot_var(ctx, alias, inner);

    /* Emit body */
    if (node->data.with_stmt.body != NULL)
        llvm_emit_block(node->data.with_stmt.body, ctx);

    /* Auto-release */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        snprintf(fn_name, sizeof(fn_name), "pgy_release_%s", inner);
        LLVMFuncEntry *release_fn = llvm_lookup_function(ctx, fn_name);
        if (release_fn != NULL) {
            LLVMValueRef args[] = { alloca_val };
            LLVMBuildCall2(ctx->builder, release_fn->fn_type,
                           release_fn->fn, args, 1, "");
        }
    }

    llvm_scope_pop(ctx);
}

static void
llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    if (node->type != AST_BLOCK)
        return;

    llvm_scope_push(ctx);
    for (size_t i = 0; i < node->data.block.count; i++) {
        llvm_emit_statement(node->data.block.statements[i], ctx);
        /* Stop emitting after a terminator (return) */
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) != NULL)
            break;
    }
    llvm_scope_pop(ctx);
}

static void
llvm_emit_statement(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || ctx->has_error)
        return;

    /* If current block already has a terminator, skip */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) != NULL)
        return;

    switch (node->type) {
    case AST_LET_DECL:
        llvm_emit_let_decl(node, ctx);
        break;

    case AST_RETURN:
        llvm_emit_return_stmt(node, ctx);
        break;

    case AST_IF_STMT:
        llvm_emit_if_stmt(node, ctx);
        break;

    case AST_WHILE_LOOP:
        llvm_emit_while_loop(node, ctx);
        break;

    case AST_FOR_LOOP:
        llvm_emit_for_loop(node, ctx);
        break;

    case AST_MATCH_STMT:
        llvm_emit_match_stmt(node, ctx);
        break;

    case AST_WITH_STMT:
        llvm_emit_with_stmt(node, ctx);
        break;

    case AST_BLOCK:
        llvm_emit_block(node, ctx);
        break;

    case AST_ASYNC_BLOCK:
        /* MVP: emit contained statements sequentially */
        for (size_t i = 0; i < node->data.async_block.statement_count; i++)
            llvm_emit_statement(node->data.async_block.statements[i], ctx);
        break;

    case AST_PARALLEL_BLOCK:
        /* MVP: emit parallel tasks sequentially */
        for (size_t i = 0; i < node->data.parallel.task_count; i++)
            llvm_emit_statement(node->data.parallel.tasks[i], ctx);
        break;

    case AST_FUNC_DECL:
    case AST_CLASS_DECL:
    case AST_ACTOR_DECL:
    case AST_ABILITY_DECL:
    case AST_ROLE_DECL:
    case AST_PARTY_DECL:
    case AST_SYSTEMIC_DECL:
    case AST_WORLD_DECL:
    case AST_EVENT_DECL:
        /* Handled in program pass or declaration-only — skip here */
        break;

    /* Expression statements */
    case AST_CALL:
    case AST_ASSIGNMENT:
    case AST_BINARY:
    case AST_UNARY:
    case AST_IDENTIFIER:
    case AST_MEMBER_ACCESS:
    case AST_NUMBER:
    case AST_STRING:
    case AST_BOOLEAN:
        llvm_emit_expression(node, ctx);
        break;

    default:
        /* Unsupported — skip silently */
        break;
    }
}

/* =================================================================
 * Function declaration emission
 * ================================================================= */

static void
llvm_forward_declare_func(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.func_decl.name;
    size_t param_count = node->data.func_decl.param_count;

    /* Return type */
    LLVMTypeRef ret_type = ctx->type_void;
    if (node->data.func_decl.return_type != NULL)
        ret_type = ast_type_to_llvm(ctx, node->data.func_decl.return_type);

    /* Parameter types */
    LLVMTypeRef *param_types = NULL;
    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(LLVMTypeRef));
        for (size_t i = 0; i < param_count; i++) {
            FuncParam *p = node->data.func_decl.params[i];
            param_types[i] = (p->type != NULL)
                ? ast_type_to_llvm(ctx, p->type)
                : ctx->type_i32;
        }
    }

    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types,
                                            (unsigned)param_count, 0);
    LLVMValueRef fn = LLVMAddFunction(ctx->module, name, fn_type);
    llvm_register_function(ctx, name, fn, fn_type, ret_type);

    free(param_types);
}

static void
llvm_emit_func_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.func_decl.name;

    LLVMFuncEntry *entry = llvm_lookup_function(ctx, name);
    if (entry == NULL)
        return;

    LLVMValueRef fn = entry->fn;

    /* Save context */
    LLVMValueRef saved_fn       = ctx->current_function;
    LLVMTypeRef  saved_ret_type = ctx->current_ret_type;

    ctx->current_function = fn;
    ctx->current_ret_type = entry->ret_type;

    /* Create entry block */
    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);

    llvm_scope_push(ctx);

    /* Create allocas for parameters and store incoming values */
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        LLVMTypeRef pt = (p->type != NULL)
            ? ast_type_to_llvm(ctx, p->type)
            : ctx->type_i32;

        LLVMValueRef alloca = llvm_create_entry_alloca(ctx, pt, p->name);
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, (unsigned)i), alloca);
        llvm_scope_declare(ctx, p->name, alloca, pt);
    }

    /* Emit body */
    if (node->data.func_decl.body != NULL)
        llvm_emit_block(node->data.func_decl.body, ctx);

    /* Add implicit return if no terminator */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        if (entry->ret_type == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else
            LLVMBuildRet(ctx->builder,
                          LLVMConstInt(entry->ret_type, 0, 0));
    }

    llvm_scope_pop(ctx);

    /* Restore context */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;

    /* Position builder back to the calling context */
    if (saved_fn != NULL) {
        LLVMBasicBlockRef last_bb = LLVMGetLastBasicBlock(saved_fn);
        if (last_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last_bb);
    }
}

/* =================================================================
 * Program emission
 * ================================================================= */

static void
llvm_emit_program(ASTNode *program, LLVMGenCtx *ctx)
{
    if (program == NULL || program->type != AST_PROGRAM) {
        ctx->has_error = true;
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Expected AST_PROGRAM node");
        return;
    }

    /* Declare runtime functions */
    llvm_declare_runtime(ctx);

    /* Pass 0: Register class/struct types */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
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
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
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

    /* Pass 0a: Register Party/Systemic/World struct types + methods */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL) continue;

        const char *decl_name = NULL;
        ASTNode **shared_fields = NULL;
        size_t shared_count = 0;
        ASTNode **methods = NULL;
        size_t method_count = 0;

        if (stmt->type == AST_PARTY_DECL) {
            decl_name    = stmt->data.party_decl.name;
            shared_fields = stmt->data.party_decl.shared_fields;
            shared_count  = stmt->data.party_decl.shared_count;
            methods       = stmt->data.party_decl.methods;
            method_count  = stmt->data.party_decl.method_count;
        } else if (stmt->type == AST_SYSTEMIC_DECL) {
            decl_name    = stmt->data.systemic_decl.name;
            shared_fields = stmt->data.systemic_decl.shared_fields;
            shared_count  = stmt->data.systemic_decl.shared_count;
            methods       = stmt->data.systemic_decl.methods;
            method_count  = stmt->data.systemic_decl.method_count;
        } else if (stmt->type == AST_WORLD_DECL) {
            decl_name    = stmt->data.world_decl.name;
            shared_fields = stmt->data.world_decl.shared_fields;
            shared_count  = stmt->data.world_decl.shared_count;
            methods       = stmt->data.world_decl.methods;
            method_count  = stmt->data.world_decl.method_count;
        } else {
            continue;
        }

        /* Build struct from shared fields */
        size_t fc = shared_count;
        LLVMTypeRef *ftypes = calloc(fc > 0 ? fc : 1,
                                       sizeof(LLVMTypeRef));
        for (size_t j = 0; j < fc; j++) {
            ASTNode *sf = shared_fields[j];
            ASTNode *sf_type = sf->data.party_shared.type;
            ftypes[j] = (sf_type != NULL)
                ? ast_type_to_llvm(ctx, sf_type)
                : ctx->type_i32;
        }

        LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context,
                                                        decl_name);
        LLVMStructSetBody(struct_ty, ftypes,
                           (unsigned)fc, 0);

        LLVMClassTypeEntry *entry = llvm_register_class(ctx,
            decl_name, struct_ty);
        if (entry != NULL) {
            for (size_t j = 0; j < fc; j++) {
                ASTNode *sf = shared_fields[j];
                llvm_class_add_field(entry,
                    sf->data.party_shared.name,
                    ftypes[j], (int)j);
            }
        }
        free(ftypes);

        /* Forward-declare methods */
        for (size_t j = 0; j < method_count; j++) {
            ASTNode *method = methods[j];
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
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
            }

            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes,
                (unsigned)(user_pc + 1), 0);

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s",
                     decl_name, mname);
            LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                                fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn),
                                    fn, ft, ret);
            free(ptypes);
        }
    }

    /* Pass 0b: Register ability vtable types */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ABILITY_DECL)
            continue;

        const char *ab_name = stmt->data.ability_decl.name;
        size_t mc = stmt->data.ability_decl.method_count;

        /* Build vtable struct: { fn_ptr_1, fn_ptr_2, ... } */
        LLVMTypeRef *vt_fields = calloc(mc > 0 ? mc : 1,
                                          sizeof(LLVMTypeRef));
        for (size_t j = 0; j < mc; j++) {
            ASTNode *method = stmt->data.ability_decl.methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL) {
                vt_fields[j] = ctx->type_i8ptr;
                continue;
            }

            LLVMTypeRef ret = ctx->type_void;
            if (method->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx,
                    method->data.func_decl.return_type);

            size_t pc = method->data.func_decl.param_count;
            LLVMTypeRef *ptypes = calloc(pc + 1, sizeof(LLVMTypeRef));
            ptypes[0] = ctx->type_i8ptr; /* self */
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = method->data.func_decl.params[k];
                ptypes[k + 1] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type)
                    : ctx->type_i32;
            }

            LLVMTypeRef fn_type = LLVMFunctionType(ret,
                ptypes, (unsigned)(pc + 1), 0);
            vt_fields[j] = LLVMPointerType(fn_type, 0);
            free(ptypes);
        }

        char vt_name[256];
        snprintf(vt_name, sizeof(vt_name), "%s_vtable", ab_name);
        LLVMTypeRef vt_struct = LLVMStructCreateNamed(ctx->context,
                                                        vt_name);
        LLVMStructSetBody(vt_struct, vt_fields, (unsigned)mc, 0);
        free(vt_fields);

        /* Register as class type so it's findable */
        LLVMClassTypeEntry *entry = llvm_register_class(ctx,
            vt_name, vt_struct);
        if (entry != NULL) {
            for (size_t j = 0; j < mc; j++) {
                ASTNode *method = stmt->data.ability_decl.methods[j];
                if (method != NULL && method->type == AST_FUNC_DECL)
                    llvm_class_add_field(entry,
                        method->data.func_decl.name,
                        LLVMStructGetTypeAtIndex(vt_struct, (unsigned)j),
                        (int)j);
            }
        }
    }

    /* Pass 0c: Forward-declare role methods + create vtable globals */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        const char *role_name = stmt->data.role_decl.name;

        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            for (size_t j = 0; j < impl->data.impl_ability.method_count;
                 j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                const char *mname = method->data.func_decl.name;
                size_t pc = method->data.func_decl.param_count;

                LLVMTypeRef ret = ctx->type_void;
                if (method->data.func_decl.return_type != NULL)
                    ret = ast_type_to_llvm(ctx,
                        method->data.func_decl.return_type);

                /* self + user params */
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
                        ? ast_type_to_llvm(ctx, p->type)
                        : ctx->type_i32;
                }

                LLVMTypeRef ft = LLVMFunctionType(ret, ptypes,
                    (unsigned)(user_pc + 1), 0);

                char fname[256];
                snprintf(fname, sizeof(fname), "%s_%s",
                         role_name, mname);
                LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                                    fname, ft);
                llvm_register_function(ctx, LLVMGetValueName(fn),
                                        fn, ft, ret);
                free(ptypes);
            }
        }
    }

    /* Pass 1: Forward-declare all user functions */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt != NULL && stmt->type == AST_FUNC_DECL)
            llvm_forward_declare_func(stmt, ctx);
    }

    /* Pass 2: Emit function bodies (standalone + class methods) */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt != NULL && stmt->type == AST_FUNC_DECL)
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
                LLVMValueRef saved_fn = ctx->current_function;
                LLVMTypeRef saved_ret = ctx->current_ret_type;
                ctx->current_function = fn;
                ctx->current_ret_type = entry->ret_type;

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
                }

                if (method->data.func_decl.body != NULL)
                    llvm_emit_block(method->data.func_decl.body, ctx);

                if (LLVMGetBasicBlockTerminator(
                        LLVMGetInsertBlock(ctx->builder)) == NULL) {
                    if (entry->ret_type == ctx->type_void)
                        LLVMBuildRetVoid(ctx->builder);
                    else
                        LLVMBuildRet(ctx->builder,
                            LLVMConstInt(entry->ret_type, 0, 0));
                }

                llvm_scope_pop(ctx);
                ctx->current_function = saved_fn;
                ctx->current_ret_type = saved_ret;

                if (saved_fn != NULL) {
                    LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
                    if (last != NULL)
                        LLVMPositionBuilderAtEnd(ctx->builder, last);
                }
            }
        }
    }

    /* Pass 2 (actors): Emit actor method bodies */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
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
            LLVMValueRef saved_fn = ctx->current_function;
            LLVMTypeRef saved_ret = ctx->current_ret_type;
            ctx->current_function = fn;
            ctx->current_ret_type = fentry->ret_type;

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
            }

            if (method->data.func_decl.body != NULL)
                llvm_emit_block(method->data.func_decl.body, ctx);

            if (LLVMGetBasicBlockTerminator(
                    LLVMGetInsertBlock(ctx->builder)) == NULL) {
                if (fentry->ret_type == ctx->type_void)
                    LLVMBuildRetVoid(ctx->builder);
                else
                    LLVMBuildRet(ctx->builder,
                        LLVMConstInt(fentry->ret_type, 0, 0));
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

    /* Pass 2b: Emit role method bodies + vtable globals */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL)
            continue;

        const char *role_name = stmt->data.role_decl.name;

        for (size_t ii = 0; ii < stmt->data.role_decl.impl_count; ii++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[ii];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            const char *ab_name = impl->data.impl_ability.ability_name;

            /* Emit method bodies */
            for (size_t j = 0; j < impl->data.impl_ability.method_count;
                 j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;

                char fname[256];
                snprintf(fname, sizeof(fname), "%s_%s",
                         role_name, method->data.func_decl.name);

                LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
                if (fentry == NULL) continue;

                LLVMValueRef fn = fentry->fn;
                LLVMValueRef saved_fn = ctx->current_function;
                LLVMTypeRef saved_ret = ctx->current_ret_type;
                ctx->current_function = fn;
                ctx->current_ret_type = fentry->ret_type;

                LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                    ctx->context, fn, "entry");
                LLVMPositionBuilderAtEnd(ctx->builder, bb);
                llvm_scope_push(ctx);

                /* self param */
                LLVMValueRef self_val = LLVMGetParam(fn, 0);
                LLVMValueRef self_alloca = llvm_create_entry_alloca(
                    ctx, ctx->type_i8ptr, "self.addr");
                LLVMBuildStore(ctx->builder, self_val, self_alloca);
                llvm_scope_declare(ctx, "self", self_alloca,
                                    ctx->type_i8ptr);

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
                }

                if (method->data.func_decl.body != NULL)
                    llvm_emit_block(method->data.func_decl.body, ctx);

                if (LLVMGetBasicBlockTerminator(
                        LLVMGetInsertBlock(ctx->builder)) == NULL) {
                    if (fentry->ret_type == ctx->type_void)
                        LLVMBuildRetVoid(ctx->builder);
                    else
                        LLVMBuildRet(ctx->builder,
                            LLVMConstInt(fentry->ret_type, 0, 0));
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

            /* Create vtable global constant */
            char vt_type_name[256];
            snprintf(vt_type_name, sizeof(vt_type_name),
                     "%s_vtable", ab_name);
            LLVMClassTypeEntry *vt_cls = llvm_lookup_class(ctx,
                vt_type_name);
            if (vt_cls != NULL) {
                size_t mc = impl->data.impl_ability.method_count;
                LLVMValueRef *vals = calloc(mc > 0 ? mc : 1,
                                              sizeof(LLVMValueRef));
                for (size_t j = 0; j < mc; j++) {
                    ASTNode *method = impl->data.impl_ability.methods[j];
                    if (method == NULL || method->type != AST_FUNC_DECL) {
                        vals[j] = LLVMConstNull(ctx->type_i8ptr);
                        continue;
                    }
                    char fname[256];
                    snprintf(fname, sizeof(fname), "%s_%s",
                             role_name, method->data.func_decl.name);
                    LLVMFuncEntry *fe = llvm_lookup_function(ctx, fname);
                    vals[j] = (fe != NULL) ? fe->fn
                        : LLVMConstNull(ctx->type_i8ptr);
                }

                LLVMValueRef vt_const = LLVMConstNamedStruct(
                    vt_cls->struct_type, vals, (unsigned)mc);

                char global_name[256];
                snprintf(global_name, sizeof(global_name),
                         "%s_%s_vtable_instance", role_name, ab_name);
                LLVMValueRef global = LLVMAddGlobal(ctx->module,
                    vt_cls->struct_type, global_name);
                LLVMSetInitializer(global, vt_const);
                LLVMSetGlobalConstant(global, 1);
                LLVMSetLinkage(global, LLVMInternalLinkage);

                free(vals);
            }
        }
    }

    /* Pass 2c: Emit Party/Systemic/World method bodies */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL) continue;

        const char *decl_name = NULL;
        ASTNode **methods = NULL;
        size_t method_count = 0;

        if (stmt->type == AST_PARTY_DECL) {
            decl_name    = stmt->data.party_decl.name;
            methods      = stmt->data.party_decl.methods;
            method_count = stmt->data.party_decl.method_count;
        } else if (stmt->type == AST_SYSTEMIC_DECL) {
            decl_name    = stmt->data.systemic_decl.name;
            methods      = stmt->data.systemic_decl.methods;
            method_count = stmt->data.systemic_decl.method_count;
        } else if (stmt->type == AST_WORLD_DECL) {
            decl_name    = stmt->data.world_decl.name;
            methods      = stmt->data.world_decl.methods;
            method_count = stmt->data.world_decl.method_count;
        } else {
            continue;
        }

        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, decl_name);

        for (size_t j = 0; j < method_count; j++) {
            ASTNode *method = methods[j];
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;

            char fname[256];
            snprintf(fname, sizeof(fname), "%s_%s",
                     decl_name, method->data.func_decl.name);

            LLVMFuncEntry *fentry = llvm_lookup_function(ctx, fname);
            if (fentry == NULL) continue;

            LLVMValueRef fn = fentry->fn;
            LLVMValueRef saved_fn = ctx->current_function;
            LLVMTypeRef saved_ret = ctx->current_ret_type;
            ctx->current_function = fn;
            ctx->current_ret_type = fentry->ret_type;

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
                llvm_register_var_class(ctx, "self", decl_name);
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
            }

            if (method->data.func_decl.body != NULL)
                llvm_emit_block(method->data.func_decl.body, ctx);

            if (LLVMGetBasicBlockTerminator(
                    LLVMGetInsertBlock(ctx->builder)) == NULL) {
                if (fentry->ret_type == ctx->type_void)
                    LLVMBuildRetVoid(ctx->builder);
                else
                    LLVMBuildRet(ctx->builder,
                        LLVMConstInt(fentry->ret_type, 0, 0));
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
    bool has_top_level = false;
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt != NULL && stmt->type != AST_FUNC_DECL
            && stmt->type != AST_CLASS_DECL
            && stmt->type != AST_ABILITY_DECL
            && stmt->type != AST_ROLE_DECL
            && stmt->type != AST_PARTY_DECL
            && stmt->type != AST_SYSTEMIC_DECL
            && stmt->type != AST_WORLD_DECL
            && stmt->type != AST_EVENT_DECL) {
            has_top_level = true;
            break;
        }
    }

    /* Create main() */
    LLVMTypeRef main_type = LLVMFunctionType(ctx->type_i32, NULL, 0, 0);
    LLVMValueRef main_fn = LLVMAddFunction(ctx->module, "main", main_type);
    ctx->current_function = main_fn;
    ctx->current_ret_type = ctx->type_i32;

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
        ctx->context, main_fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);

    llvm_scope_push(ctx);

    if (has_top_level) {
        for (size_t i = 0; i < program->data.program.count; i++) {
            ASTNode *stmt = program->data.program.statements[i];
            if (stmt != NULL && stmt->type != AST_FUNC_DECL
                && stmt->type != AST_CLASS_DECL
                && stmt->type != AST_ABILITY_DECL
                && stmt->type != AST_ROLE_DECL
                && stmt->type != AST_PARTY_DECL
                && stmt->type != AST_SYSTEMIC_DECL
                && stmt->type != AST_WORLD_DECL
                && stmt->type != AST_EVENT_DECL) {
                llvm_emit_statement(stmt, ctx);
                if (LLVMGetBasicBlockTerminator(
                        LLVMGetInsertBlock(ctx->builder)) != NULL)
                    break;
            }
        }
    }

    llvm_scope_pop(ctx);

    /* Return 0 from main (if no terminator yet) */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildRet(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0));
}

/* =================================================================
 * Optimization pass
 * ================================================================= */

static void
llvm_run_optimization(LLVMGenCtx *ctx)
{
    LLVMPassBuilderOptionsRef opts = LLVMCreatePassBuilderOptions();
    LLVMRunPasses(ctx->module, "default<O2>", NULL, opts);
    LLVMDisposePassBuilderOptions(opts);
}

/* =================================================================
 * Public API
 * ================================================================= */

LLVMGenResult *
llvm_codegen(ASTNode *ast, const char *module_name)
{
    LLVMGenCtx *ctx = llvm_ctx_create(module_name);
    if (ctx == NULL)
        return llvm_result_error("Out of memory");

    llvm_emit_program(ast, ctx);

    if (ctx->has_error) {
        LLVMGenResult *res = llvm_result_error(ctx->error_msg);
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
llvm_codegen_to_object(ASTNode *ast, const char *module_name,
                       const char *output_path)
{
    LLVMGenCtx *ctx = llvm_ctx_create(module_name);
    if (ctx == NULL)
        return llvm_result_error("Out of memory");

    llvm_emit_program(ast, ctx);

    if (ctx->has_error) {
        LLVMGenResult *res = llvm_result_error(ctx->error_msg);
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

    /* Optimize */
    llvm_run_optimization(ctx);

    /* Initialize all targets */
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

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
