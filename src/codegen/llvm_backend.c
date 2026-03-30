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
    ctx->event_type_count = 0;
    ctx->lambda_counter = 0;
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

/* Resolve inner type for generic containers: "Result<Int>" → i32 */
static LLVMTypeRef
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

    if (strcmp(inner, "Int") == 0)    return ctx->type_i32;
    if (strcmp(inner, "Long") == 0)   return ctx->type_i64;
    if (strcmp(inner, "Float") == 0)  return ctx->type_f32;
    if (strcmp(inner, "Double") == 0) return ctx->type_f64;
    if (strcmp(inner, "Bool") == 0)   return ctx->type_i1;
    if (strcmp(inner, "String") == 0) return ctx->type_i8ptr;
    return ctx->type_i32;
}

static LLVMTypeRef
pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name)
{
    if (type_name == NULL)
        return ctx->type_void;

    /* Check active type substitution (monomorphization) */
    for (int i = 0; i < ctx->type_subst_count; i++) {
        if (strcmp(type_name, ctx->type_subst[i].param_name) == 0)
            return ctx->type_subst[i].llvm_type;
    }

    if (strcmp(type_name, "Int") == 0)    return ctx->type_i32;
    if (strcmp(type_name, "Long") == 0)   return ctx->type_i64;
    if (strcmp(type_name, "Float") == 0)  return ctx->type_f32;
    if (strcmp(type_name, "Double") == 0) return ctx->type_f64;
    if (strcmp(type_name, "Bool") == 0)   return ctx->type_i1;
    if (strcmp(type_name, "String") == 0) return ctx->type_i8ptr;
    if (strcmp(type_name, "Void") == 0)   return ctx->type_void;

    /* Generic container types */
    if (strncmp(type_name, "Result<", 7) == 0) {
        /* Result<T> → { T value, i1 ok } */
        LLVMTypeRef inner = llvm_resolve_inner_type(ctx, type_name);
        LLVMTypeRef fields[] = { inner, ctx->type_i1 };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    if (strncmp(type_name, "Slot<", 5) == 0
        || strncmp(type_name, "SecureSlot<", 11) == 0) {
        const char *inner_name = strchr(type_name, '<') + 1;
        char buf[64]; size_t l = strcspn(inner_name, ">");
        if (l >= sizeof(buf)) l = sizeof(buf) - 1;
        memcpy(buf, inner_name, l); buf[l] = '\0';
        return llvm_slot_struct_type(ctx, buf);
    }
    if (strncmp(type_name, "Channel<", 8) == 0
        || strncmp(type_name, "Future<", 7) == 0
        || strncmp(type_name, "Box<", 4) == 0
        || strncmp(type_name, "Rc<", 3) == 0
        || strncmp(type_name, "Weak<", 5) == 0
        || strncmp(type_name, "Array<", 6) == 0
        || strncmp(type_name, "Slice<", 6) == 0) {
        /* Opaque pointer for all container types */
        return ctx->type_i8ptr;
    }

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

static const char *
llvm_tmp_name(LLVMGenCtx *ctx)
{
    static char buf[32];
    snprintf(buf, sizeof(buf), "t%d", ctx->tmp_counter++);
    return buf;
}

/* =================================================================
 * Generic monomorphization helpers
 * ================================================================= */

static ASTNode *
llvm_lookup_generic_template(LLVMGenCtx *ctx, const char *name)
{
    for (int i = 0; i < ctx->generic_template_count; i++) {
        if (strcmp(ctx->generic_templates[i].name, name) == 0)
            return ctx->generic_templates[i].ast;
    }
    return NULL;
}

static bool
llvm_mono_already_emitted(LLVMGenCtx *ctx, const char *mangled)
{
    for (int i = 0; i < ctx->mono_count; i++) {
        if (strcmp(ctx->mono_instances[i].name, mangled) == 0)
            return true;
    }
    return false;
}

static void
llvm_register_mono(LLVMGenCtx *ctx, const char *mangled)
{
    if (ctx->mono_count < MAX_MONO_INSTANCES) {
        snprintf(ctx->mono_instances[ctx->mono_count].name,
                 sizeof(ctx->mono_instances[0].name), "%s", mangled);
        ctx->mono_count++;
    }
}

/* Map LLVM type to Pergyra type name suffix */
static const char *
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
static void llvm_emit_func_decl(ASTNode *node, LLVMGenCtx *ctx);

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

    /* =================================================================
     * Thread pool runtime
     * pgy_pool_init_export(i64) → void
     * pgy_pool_shutdown_export() → void
     * pgy_spawn_export(fn_ptr, i8*) → { i8* }  (PgyTaskHandle)
     * pgy_await_export({ i8* }) → i8*
     * ================================================================= */

    /* PgyTaskHandle = { i8* } */
    LLVMTypeRef task_handle_fields[] = { ctx->type_i8ptr };
    ctx->type_task_handle = LLVMStructCreateNamed(ctx->context,
                                                   "PgyTaskHandle");
    LLVMStructSetBody(ctx->type_task_handle, task_handle_fields, 1, 0);

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

    /* i8* pgy_await_export(PgyTaskHandle) */
    {
        LLVMTypeRef params[] = { ctx->type_task_handle };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module,
                                           "pgy_await_export", ft);
        llvm_register_function(ctx, "pgy_await_export",
                                fn, ft, ctx->type_i8ptr);
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

        /* val_type pgy_channel_recv_val_T(ptr) */
        {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(vt, params, 1, 0);
            snprintf(fname, sizeof(fname), "pgy_channel_recv_val_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt);
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
        LLVMTypeRef sty = slot_types[si].slot_ty;
        LLVMTypeRef vt  = slot_types[si].val_ty;
        char fname[128];

        /* PgySecureSlot_T pgy_claim_secure_T() */
        {
            LLVMTypeRef ft = LLVMFunctionType(sty, NULL, 0, 0);
            snprintf(fname, sizeof(fname), "pgy_claim_secure_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, sty);
        }

        /* void pgy_write_secure_T(PgySecureSlot_T*, val_type, i64 token) */
        {
            LLVMTypeRef params[] = {
                LLVMPointerType(sty, 0), vt, ctx->type_i64
            };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
            snprintf(fname, sizeof(fname), "pgy_write_secure_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
                                    ctx->type_void);
        }

        /* val_type pgy_read_secure_T(PgySecureSlot_T*, i64 token) */
        {
            LLVMTypeRef params[] = {
                LLVMPointerType(sty, 0), ctx->type_i64
            };
            LLVMTypeRef ft = LLVMFunctionType(vt, params, 2, 0);
            snprintf(fname, sizeof(fname), "pgy_read_secure_%s", suf);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
            llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt);
        }
    }
}

/* =================================================================
 * Event type registry helpers
 * ================================================================= */

static LLVMEventTypeEntry *
llvm_lookup_event(LLVMGenCtx *ctx, const char *name)
{
    for (int i = 0; i < ctx->event_type_count; i++) {
        if (strcmp(ctx->event_types[i].event_name, name) == 0)
            return &ctx->event_types[i];
    }
    return NULL;
}

static LLVMEventTypeEntry *
llvm_register_event(LLVMGenCtx *ctx, const char *name,
                    LLVMTypeRef struct_type,
                    int param_count, LLVMTypeRef *param_types)
{
    if (ctx->event_type_count >= MAX_EVENT_TYPES)
        return NULL;
    LLVMEventTypeEntry *e = &ctx->event_types[ctx->event_type_count++];
    e->event_name  = name;
    e->struct_type = struct_type;
    e->param_count = param_count;
    for (int i = 0; i < param_count && i < 8; i++)
        e->param_types[i] = param_types[i];
    return e;
}

/* =================================================================
 * Forward declarations
 * ================================================================= */

static LLVMValueRef llvm_emit_expression(ASTNode *node, LLVMGenCtx *ctx);
static void         llvm_emit_statement(ASTNode *node, LLVMGenCtx *ctx);
static void         llvm_emit_block(ASTNode *node, LLVMGenCtx *ctx);
static void         llvm_emit_with_stmt(ASTNode *node, LLVMGenCtx *ctx);
static void         llvm_emit_func_decl(ASTNode *node, LLVMGenCtx *ctx);
static void         llvm_emit_parallel_block(ASTNode *node, LLVMGenCtx *ctx);

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
                        /* Self is always passed as i8* (opaque ptr).
                         * var->alloca is ptr-to-struct, which is ptr. */
                        args[0] = var->alloca;
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

    /* Event invocation: OnHit(42) → OnHit_INVOKE(&OnHit, 42) */
    {
        LLVMEventTypeEntry *evt = llvm_lookup_event(ctx, callee_name);
        if (evt != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", callee_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMValueRef ev_ptr = LLVMGetNamedGlobal(ctx->module, callee_name);
            if (ev_ptr == NULL) {
                LLVMVarEntry *ev = llvm_scope_lookup(ctx, callee_name);
                if (ev != NULL) ev_ptr = ev->alloca;
            }
            if (fn != NULL && ev_ptr != NULL) {
                size_t ac = node->data.call.arg_count;
                LLVMValueRef *args = calloc(ac + 1, sizeof(LLVMValueRef));
                args[0] = ev_ptr;
                for (size_t j = 0; j < ac; j++)
                    args[j + 1] = llvm_emit_expression(
                        node->data.call.arguments[j], ctx);
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, (unsigned)(ac + 1), "");
                free(args);
                return LLVMConstInt(ctx->type_i32, 0, 0);
            }
        }
    }

    /* Built-in: Abs(x) → select(x < 0, -x, x) */
    if (strcmp(callee_name, "Abs") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef x = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef zero = LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef neg = LLVMBuildNeg(ctx->builder, x, llvm_tmp_name(ctx));
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, x, zero,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, neg, x, llvm_tmp_name(ctx));
    }

    /* Built-in: Min(a, b) → select(a < b, a, b) */
    if (strcmp(callee_name, "Min") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef a = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef b = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, a, b,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
    }

    /* Built-in: Max(a, b) → select(a > b, a, b) */
    if (strcmp(callee_name, "Max") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef a = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef b = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, a, b,
                                          llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
    }

    /* Built-in: StringLength(s) → call strlen */
    if (strcmp(callee_name, "StringLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef s = llvm_emit_expression(node->data.call.arguments[0], ctx);
        /* Declare strlen if not already */
        LLVMFuncEntry *strlen_fn = llvm_lookup_function(ctx, "strlen");
        if (strlen_fn == NULL) {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i64, params, 1, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, "strlen", ft);
            llvm_register_function(ctx, "strlen", fn, ft, ctx->type_i64);
            strlen_fn = llvm_lookup_function(ctx, "strlen");
        }
        LLVMValueRef args[] = { s };
        LLVMValueRef len = LLVMBuildCall2(ctx->builder, strlen_fn->fn_type,
            strlen_fn->fn, args, 1, llvm_tmp_name(ctx));
        return LLVMBuildTrunc(ctx->builder, len, ctx->type_i32, llvm_tmp_name(ctx));
    }

    /* Built-in: Print(s) → printf("%s", s) */
    if (strcmp(callee_name, "Print") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef vt = LLVMTypeOf(val);
        LLVMFuncEntry *pf = llvm_lookup_function(ctx, "printf");
        if (pf != NULL) {
            if (vt == ctx->type_i8ptr) {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder,
                    "%s", ".fmt_s");
                LLVMValueRef args[] = { fmt, val };
                LLVMBuildCall2(ctx->builder, pf->fn_type, pf->fn, args, 2, "");
            } else {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder,
                    "%d", ".fmt_d");
                LLVMValueRef args[] = { fmt, val };
                LLVMBuildCall2(ctx->builder, pf->fn_type, pf->fn, args, 2, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Built-in: Ok(value) → { .ok=true, .value=value } */
    if (strcmp(callee_name, "Ok") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        /* Result struct: { i32 value, i1 ok } — simplified for Int */
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, ctx->type_i1 }, 2, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r, val, 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i1, 1, 0), 1, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: Err(value) → { .ok=false, .value=value } */
    if (strcmp(callee_name, "Err") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, ctx->type_i1 }, 2, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r, val, 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i1, 0, 0), 1, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: IsOk(result) → extract ok field */
    if (strcmp(callee_name, "IsOk") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
    }

    /* Built-in: IsErr(result) → !ok */
    if (strcmp(callee_name, "IsErr") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef ok = LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
        return LLVMBuildNot(ctx->builder, ok, llvm_tmp_name(ctx));
    }

    /* Built-in: Unwrap(result) → extract value field */
    if (strcmp(callee_name, "Unwrap") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOr(result, default) → ok ? value : default */
    if (strcmp(callee_name, "UnwrapOr") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef def = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef ok = LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, ok, val, def, llvm_tmp_name(ctx));
    }

    /* Check if callee is a generic template — if so, monomorphize */
    ASTNode *generic_ast = llvm_lookup_generic_template(ctx, callee_name);
    if (generic_ast != NULL) {
        /* Evaluate arguments first to determine concrete types */
        size_t argc = node->data.call.arg_count;
        LLVMValueRef *args = calloc(argc > 0 ? argc : 1, sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(node->data.call.arguments[i], ctx);

        /* Build mangled name from argument types: Identity_Int */
        char mangled[256];
        snprintf(mangled, sizeof(mangled), "%s", callee_name);
        for (size_t i = 0; i < argc; i++) {
            LLVMTypeRef at = (args[i] != NULL) ? LLVMTypeOf(args[i]) : ctx->type_i32;
            const char *suf = llvm_type_to_suffix(ctx, at);
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "%s_%s", mangled, suf);
            snprintf(mangled, sizeof(mangled), "%s", tmp);
        }

        /* Instantiate if not already emitted */
        if (!llvm_mono_already_emitted(ctx, mangled)) {
            llvm_register_mono(ctx, mangled);

            /* Set type substitution map */
            GenericParams *gp = generic_ast->data.func_decl.generic_params;
            int saved_subst = ctx->type_subst_count;
            ctx->type_subst_count = 0;
            for (size_t gi = 0; gi < gp->count && gi < 8; gi++) {
                /* Map T → type of corresponding argument */
                LLVMTypeRef concrete = (gi < argc && args[gi] != NULL)
                    ? LLVMTypeOf(args[gi]) : ctx->type_i32;
                ctx->type_subst[ctx->type_subst_count].param_name = gp->params[gi]->name;
                ctx->type_subst[ctx->type_subst_count].llvm_type = concrete;
                ctx->type_subst[ctx->type_subst_count].type_name = llvm_type_to_suffix(ctx, concrete);
                ctx->type_subst_count++;
            }

            /* Save builder state */
            LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
            LLVMValueRef saved_fn = ctx->current_function;
            LLVMTypeRef saved_ret = ctx->current_ret_type;

            /* Forward-declare the monomorphized function */
            LLVMTypeRef ret = ctx->type_void;
            if (generic_ast->data.func_decl.return_type != NULL)
                ret = ast_type_to_llvm(ctx, generic_ast->data.func_decl.return_type);

            size_t pc = generic_ast->data.func_decl.param_count;
            LLVMTypeRef *ptypes = calloc(pc > 0 ? pc : 1, sizeof(LLVMTypeRef));
            size_t real_pc = 0;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = generic_ast->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0) continue;
                ptypes[real_pc++] = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type) : ctx->type_i32;
            }
            LLVMTypeRef ft = LLVMFunctionType(ret, ptypes, (unsigned)real_pc, 0);
            LLVMValueRef mono_fn = LLVMAddFunction(ctx->module, mangled, ft);
            llvm_register_function(ctx, mangled, mono_fn, ft, ret);
            free(ptypes);

            /* Emit function body */
            ctx->current_function = mono_fn;
            ctx->current_ret_type = ret;
            LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
                ctx->context, mono_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry);
            llvm_scope_push(ctx);

            real_pc = 0;
            for (size_t k = 0; k < pc; k++) {
                FuncParam *p = generic_ast->data.func_decl.params[k];
                if (p->type == NULL && strcmp(p->name, "self") == 0) continue;
                LLVMTypeRef pt = (p->type != NULL)
                    ? ast_type_to_llvm(ctx, p->type) : ctx->type_i32;
                LLVMValueRef alloca = llvm_create_entry_alloca(ctx, pt, p->name);
                LLVMBuildStore(ctx->builder, LLVMGetParam(mono_fn, (unsigned)real_pc), alloca);
                llvm_scope_declare(ctx, p->name, alloca, pt);
                real_pc++;
            }

            if (generic_ast->data.func_decl.body != NULL)
                llvm_emit_block(generic_ast->data.func_decl.body, ctx);

            if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
                if (ret == ctx->type_void)
                    LLVMBuildRetVoid(ctx->builder);
                else
                    LLVMBuildRet(ctx->builder, LLVMConstInt(ret, 0, 0));
            }

            llvm_scope_pop(ctx);

            /* Restore state */
            ctx->type_subst_count = saved_subst;
            ctx->current_function = saved_fn;
            ctx->current_ret_type = saved_ret;
            if (saved_bb != NULL)
                LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
        }

        /* Call the monomorphized function */
        LLVMFuncEntry *mono_entry = llvm_lookup_function(ctx, mangled);
        LLVMValueRef result;
        if (mono_entry != NULL) {
            if (mono_entry->ret_type == ctx->type_void) {
                LLVMBuildCall2(ctx->builder, mono_entry->fn_type,
                    mono_entry->fn, args, (unsigned)argc, "");
                result = LLVMConstInt(ctx->type_i32, 0, 0);
            } else {
                result = LLVMBuildCall2(ctx->builder, mono_entry->fn_type,
                    mono_entry->fn, args, (unsigned)argc, llvm_tmp_name(ctx));
            }
        } else {
            result = LLVMConstInt(ctx->type_i32, 0, 0);
        }
        free(args);
        return result;
    }

    /* Look up user function */
    LLVMFuncEntry *func = llvm_lookup_function(ctx, callee_name);
    if (func == NULL) {
        fprintf(stderr, "[llvm] warning: unknown function '%s'\n", callee_name);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    /* Build arguments */
    size_t argc = node->data.call.arg_count;
    LLVMValueRef *args = NULL;
    if (argc > 0) {
        args = calloc(argc, sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++) {
            ASTNode *arg_node = node->data.call.arguments[i];
            /* If the function parameter expects ptr (self) and the
             * argument is an identifier holding a struct, pass the
             * alloca pointer directly instead of loading the value. */
            unsigned param_count = LLVMCountParams(func->fn);
            LLVMTypeRef param_ty = (i < param_count)
                ? LLVMTypeOf(LLVMGetParam(func->fn, (unsigned)i))
                : NULL;
            if (param_ty != NULL
                && LLVMGetTypeKind(param_ty) == LLVMPointerTypeKind
                && arg_node->type == AST_IDENTIFIER) {
                LLVMVarEntry *v = llvm_scope_lookup(ctx,
                    arg_node->data.identifier.name);
                if (v != NULL) {
                    args[i] = v->alloca; /* pass pointer, not loaded value */
                    continue;
                }
            }
            args[i] = llvm_emit_expression(arg_node, ctx);
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

    case AST_ARRAY_ACCESS: {
        /* arr[idx] → GEP + load */
        LLVMValueRef arr = llvm_emit_expression(
            node->data.array_access.array, ctx);
        LLVMValueRef idx = llvm_emit_expression(
            node->data.array_access.index, ctx);
        if (arr == NULL || idx == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        /* If arr is a pointer (i8* string or typed array pointer),
         * do a GEP + load */
        LLVMTypeRef arr_ty = LLVMTypeOf(arr);
        if (LLVMGetTypeKind(arr_ty) == LLVMPointerTypeKind) {
            /* String indexing: i8* → GEP i8 */
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                LLVMInt8TypeInContext(ctx->context),
                arr, &idx, 1, llvm_tmp_name(ctx));
            return LLVMBuildLoad2(ctx->builder,
                LLVMInt8TypeInContext(ctx->context),
                gep, llvm_tmp_name(ctx));
        }
        /* Array struct: get data pointer field (index 0), then GEP */
        if (LLVMGetTypeKind(arr_ty) == LLVMStructTypeKind) {
            LLVMValueRef data_ptr = LLVMBuildExtractValue(ctx->builder,
                arr, 0, llvm_tmp_name(ctx));
            LLVMTypeRef elem_ty = ctx->type_i32; /* default element type */
            LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
                elem_ty, data_ptr, &idx, 1, llvm_tmp_name(ctx));
            return LLVMBuildLoad2(ctx->builder, elem_ty,
                gep, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_CONTEXT_ACCESS: {
        /* context.GetRole("slotName") → load role slot from self (i8*)
         * self is in scope as the party/systemic method's first param */
        LLVMVarEntry *self_var = llvm_scope_lookup(ctx, "self");
        if (self_var == NULL)
            return LLVMConstNull(ctx->type_i8ptr);

        /* For now: return the self pointer cast — the role slot is
         * accessed through the party struct, which self points to */
        LLVMValueRef self_val = LLVMBuildLoad2(ctx->builder,
            ctx->type_i8ptr, self_var->alloca, llvm_tmp_name(ctx));
        return self_val;
    }

    case AST_PARTY_INSTANCE: {
        /* PartyType { slot1: val1, slot2: val2 }
         * → alloca struct, store fields, return value */
        const char *pty = node->data.party_instance.party_type;
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, pty);
        if (cls == NULL)
            return LLVMConstNull(ctx->type_i8ptr);

        LLVMValueRef alloca = llvm_create_entry_alloca(ctx,
            cls->struct_type, llvm_tmp_name(ctx));

        /* Zero-initialize */
        LLVMValueRef zero = LLVMConstNull(cls->struct_type);
        LLVMBuildStore(ctx->builder, zero, alloca);

        /* Store each assignment */
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            const char *slot_name = node->data.party_instance.assignments[i].slot_name;
            ASTNode *val_node = node->data.party_instance.assignments[i].value;

            /* Find field index */
            for (int f = 0; f < cls->field_count; f++) {
                if (strcmp(cls->fields[f].field_name, slot_name) == 0) {
                    LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                        ctx->builder, cls->struct_type, alloca,
                        (unsigned)cls->fields[f].index,
                        llvm_tmp_name(ctx));
                    LLVMValueRef val = llvm_emit_expression(val_node, ctx);
                    if (val != NULL)
                        LLVMBuildStore(ctx->builder, val, field_ptr);
                    break;
                }
            }
        }

        return LLVMBuildLoad2(ctx->builder, cls->struct_type,
            alloca, llvm_tmp_name(ctx));
    }

    case AST_TASK_GROUP: {
        /* TaskGroup { tasks... } → emit tasks sequentially (MVP) */
        for (size_t i = 0; i < node->data.task_group.task_count; i++) {
            if (node->data.task_group.tasks[i] != NULL)
                llvm_emit_expression(node->data.task_group.tasks[i], ctx);
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_CHANNEL_SEND: {
        /* ch <- value → pgy_channel_send_T(&ch, value) */
        LLVMVarEntry *ch_var = NULL;
        if (node->data.channel_send.channel != NULL
            && node->data.channel_send.channel->type == AST_IDENTIFIER) {
            ch_var = llvm_scope_lookup(ctx,
                node->data.channel_send.channel->data.identifier.name);
        }
        if (ch_var != NULL) {
            LLVMValueRef val = llvm_emit_expression(
                node->data.channel_send.value, ctx);
            /* Determine channel type suffix from value type */
            const char *suffix = "Int";
            if (val != NULL) {
                LLVMTypeRef vt = LLVMTypeOf(val);
                if (vt == ctx->type_i8ptr) suffix = "String";
            }
            char fname[128];
            snprintf(fname, sizeof(fname), "pgy_channel_send_%s", suffix);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            if (fn != NULL && val != NULL) {
                LLVMValueRef args[] = { ch_var->alloca, val };
                return LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 2, llvm_tmp_name(ctx));
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    case AST_CHANNEL_RECV: {
        /* <- ch → pgy_channel_recv_val_T(&ch) */
        LLVMVarEntry *ch_var = NULL;
        if (node->data.channel_recv.channel != NULL
            && node->data.channel_recv.channel->type == AST_IDENTIFIER) {
            ch_var = llvm_scope_lookup(ctx,
                node->data.channel_recv.channel->data.identifier.name);
        }
        if (ch_var != NULL) {
            /* Determine channel type from variable's LLVM type */
            const char *suffix = "Int";
            /* Default to Int; if channel var tracks String type,
             * the slot_var tracking would identify it */
            char fname[128];
            snprintf(fname, sizeof(fname), "pgy_channel_recv_val_%s", suffix);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            if (fn != NULL) {
                LLVMValueRef args[] = { ch_var->alloca };
                return LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 1, llvm_tmp_name(ctx));
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

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
    case AST_LAMBDA_EXPR: {
        /* Generate a static LLVM function and return its pointer */
        int lid = ctx->lambda_counter++;
        int pc = (int)node->data.lambda_expr.param_count;

        /* Determine return type */
        LLVMTypeRef ret_type = ctx->type_i32;
        if (node->data.lambda_expr.return_type != NULL)
            ret_type = ast_type_to_llvm(ctx, node->data.lambda_expr.return_type);
        else if (node->data.lambda_expr.body != NULL
                 && node->data.lambda_expr.body->type == AST_BLOCK)
            ret_type = ctx->type_void;

        /* Parameter types (default i32) */
        LLVMTypeRef lparams[8];
        for (int j = 0; j < pc && j < 8; j++) {
            ASTNode *p = node->data.lambda_expr.params[j];
            if (p->type == AST_LET_DECL && p->data.let_decl.type != NULL)
                lparams[j] = ast_type_to_llvm(ctx, p->data.let_decl.type);
            else
                lparams[j] = ctx->type_i32;
        }

        char lname[128];
        snprintf(lname, sizeof(lname), "pgy_lambda_%d", lid);
        LLVMTypeRef lft = LLVMFunctionType(ret_type,
            lparams, (unsigned)pc, 0);
        LLVMValueRef lfn = LLVMAddFunction(ctx->module, lname, lft);
        llvm_register_function(ctx, LLVMGetValueName(lfn),
            lfn, lft, ret_type);

        /* Save current builder state */
        LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
        LLVMValueRef saved_fn = ctx->current_function;
        LLVMTypeRef saved_ret = ctx->current_ret_type;

        ctx->current_function = lfn;
        ctx->current_ret_type = ret_type;

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, lfn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        llvm_scope_push(ctx);
        for (int j = 0; j < pc; j++) {
            ASTNode *p = node->data.lambda_expr.params[j];
            const char *pname = (p->type == AST_IDENTIFIER)
                ? p->data.identifier.name : p->data.let_decl.name;
            LLVMValueRef alloca = LLVMBuildAlloca(ctx->builder,
                lparams[j], pname);
            LLVMBuildStore(ctx->builder, LLVMGetParam(lfn, (unsigned)j),
                alloca);
            llvm_scope_declare(ctx, pname, alloca, lparams[j]);
        }

        if (node->data.lambda_expr.body != NULL) {
            if (node->data.lambda_expr.body->type == AST_BLOCK) {
                llvm_emit_block(node->data.lambda_expr.body, ctx);
            } else {
                LLVMValueRef val = llvm_emit_expression(
                    node->data.lambda_expr.body, ctx);
                if (ret_type != ctx->type_void)
                    LLVMBuildRet(ctx->builder, val);
                else
                    LLVMBuildRetVoid(ctx->builder);
            }
        }

        /* Ensure terminator exists */
        if (LLVMGetBasicBlockTerminator(
                LLVMGetInsertBlock(ctx->builder)) == NULL) {
            if (ret_type == ctx->type_void)
                LLVMBuildRetVoid(ctx->builder);
            else
                LLVMBuildRet(ctx->builder,
                    LLVMConstInt(ret_type, 0, 0));
        }

        llvm_scope_pop(ctx);

        /* Restore builder state */
        ctx->current_function = saved_fn;
        ctx->current_ret_type = saved_ret;
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

        return lfn;
    }

    case AST_EVENT_SUBSCRIBE: {
        /* event += handler → EventName_SUBSCRIBE(&event, handler) */
        ASTNode *evt = node->data.event_op.event;
        ASTNode *handler = node->data.event_op.handler;

        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_SUBSCRIBE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);
            LLVMValueRef hval = llvm_emit_expression(handler, ctx);

            if (fn != NULL && ev_ptr != NULL) {
                LLVMValueRef args[] = { ev_ptr, hval };
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 2, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_EVENT_UNSUBSCRIBE: {
        /* event -= handler → EventName_UNSUBSCRIBE(&event, handler) */
        ASTNode *evt = node->data.event_op.event;
        ASTNode *handler = node->data.event_op.handler;

        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_UNSUBSCRIBE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);
            LLVMValueRef hval = llvm_emit_expression(handler, ctx);

            if (fn != NULL && ev_ptr != NULL) {
                LLVMValueRef args[] = { ev_ptr, hval };
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, 2, "");
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    case AST_EVENT_INVOKE: {
        /* Emit(event, args...) → EventName_INVOKE(&event, args...) */
        ASTNode *evt = node->data.event_invoke.event;
        const char *evt_name = NULL;
        if (evt != NULL && evt->type == AST_IDENTIFIER)
            evt_name = evt->data.identifier.name;

        if (evt_name != NULL) {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", evt_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
            LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
            LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
                : LLVMGetNamedGlobal(ctx->module, evt_name);

            if (fn != NULL && ev_ptr != NULL) {
                size_t ac = node->data.event_invoke.arg_count;
                LLVMValueRef *args = calloc(ac + 1, sizeof(LLVMValueRef));
                args[0] = ev_ptr;
                for (size_t j = 0; j < ac; j++)
                    args[j + 1] = llvm_emit_expression(
                        node->data.event_invoke.arguments[j], ctx);
                LLVMBuildCall2(ctx->builder, fn->fn_type,
                    fn->fn, args, (unsigned)(ac + 1), "");
                free(args);
            }
        }
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    default:
        fprintf(stderr, "[llvm] warning: unhandled expression AST type %d\n",
                (int)node->type);
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

            /* Inline ClaimSlot: zero-init the struct and set claimed=true.
             * Avoids struct-return-by-value ABI mismatch between LLVM and C. */
            LLVMValueRef zero = LLVMConstNull(slot_ty);
            LLVMBuildStore(ctx->builder, zero, alloca_val);
            /* Set the 'claimed' field (index 1) to true */
            LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
                slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
                claimed_ptr);

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

    /* Detect Channel constructor: let ch: Channel<Int> = Channel(capacity) */
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && strcmp(init->data.call.callee->data.identifier.name, "Channel") == 0) {
        /* Allocate opaque channel as a large-enough byte array.
         * PgyChannel_Int_RT on the runtime side is ~128 bytes;
         * we allocate 256 bytes for safety. */
        LLVMTypeRef ch_type = LLVMArrayType(
            LLVMInt8TypeInContext(ctx->context), 256);
        LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, ch_type, name);

        /* Call pgy_channel_init_Int(ptr, capacity) */
        LLVMFuncEntry *init_fn = llvm_lookup_function(ctx,
            "pgy_channel_init_Int");
        if (init_fn != NULL) {
            LLVMValueRef cap = LLVMConstInt(ctx->type_i64, 16, 0);
            if (init->data.call.arg_count > 0)
                cap = LLVMBuildZExt(ctx->builder,
                    llvm_emit_expression(init->data.call.arguments[0], ctx),
                    ctx->type_i64, llvm_tmp_name(ctx));
            LLVMValueRef args[] = { alloca_val, cap };
            LLVMBuildCall2(ctx->builder, init_fn->fn_type,
                           init_fn->fn, args, 2, "");
        }
        llvm_scope_declare(ctx, name, alloca_val, ch_type);
        return;
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

    LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
    LLVMValueRef alloca_val = llvm_create_entry_alloca(ctx, slot_ty, alias);

    /* Inline claim: zero-init + set claimed=true (avoids ABI mismatch) */
    char fn_name[64];
    (void)is_secure; /* both secure and normal get same init pattern */
    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
    LLVMValueRef claimed_ptr = LLVMBuildStructGEP2(ctx->builder,
        slot_ty, alloca_val, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        claimed_ptr);

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

/* =================================================================
 * Parallel block — real concurrency via thread pool
 *
 * For each task, generate an LLVM function `_pgy_par_N(i8*) -> i8*`
 * that contains the task body, then spawn all + await all.
 * ================================================================= */

static void
llvm_emit_parallel_block(ASTNode *node, LLVMGenCtx *ctx)
{
    size_t count = node->data.parallel.task_count;
    if (count == 0)
        return;

    /* -----------------------------------------------------------
     * 1) Collect all variables from the current scope stack.
     *    These will be captured into a context struct so that
     *    wrapper functions can access them.
     * ----------------------------------------------------------- */
    typedef struct { const char *name; LLVMValueRef alloca; LLVMTypeRef type; } CapturedVar;
    CapturedVar captured[MAX_SCOPE_VARS];
    int n_captured = 0;

    for (int i = 0; i < ctx->scope_depth; i++) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = 0; j < frame->count && n_captured < MAX_SCOPE_VARS; j++) {
            captured[n_captured++] = (CapturedVar){
                frame->entries[j].name,
                frame->entries[j].alloca,
                frame->entries[j].type
            };
        }
    }

    /* -----------------------------------------------------------
     * 2) Build a context struct type: { ptr, ptr, ... }
     *    Each field is a pointer to the captured variable's alloca.
     *    In the wrapper, we GEP to get the pointer, then load/store
     *    through it — exactly like the C transpiler's approach.
     * ----------------------------------------------------------- */
    LLVMTypeRef *ctx_fields = calloc((size_t)n_captured, sizeof(LLVMTypeRef));
    for (int i = 0; i < n_captured; i++)
        ctx_fields[i] = ctx->type_i8ptr;   /* all fields are opaque ptr */

    char ctx_name[64];
    snprintf(ctx_name, sizeof(ctx_name), "_pgy_par_ctx_%d", ctx->parallel_counter);
    LLVMTypeRef ctx_struct_type = LLVMStructCreateNamed(ctx->context, ctx_name);
    LLVMStructSetBody(ctx_struct_type, ctx_fields, (unsigned)n_captured, 0);
    free(ctx_fields);

    /* -----------------------------------------------------------
     * 3) In the OUTER function: allocate + fill the context struct.
     * ----------------------------------------------------------- */
    LLVMValueRef ctx_alloca = LLVMBuildAlloca(ctx->builder, ctx_struct_type,
                                               "_pctx");
    for (int i = 0; i < n_captured; i++) {
        LLVMValueRef gep = LLVMBuildStructGEP2(ctx->builder, ctx_struct_type,
                                                 ctx_alloca, (unsigned)i,
                                                 llvm_tmp_name(ctx));
        /* Store the alloca address (pointer to the variable) */
        LLVMBuildStore(ctx->builder, captured[i].alloca, gep);
    }

    /* Cast context struct pointer to i8* for spawn argument */
    LLVMValueRef ctx_i8ptr = LLVMBuildBitCast(ctx->builder, ctx_alloca,
                                               ctx->type_i8ptr,
                                               llvm_tmp_name(ctx));

    /* -----------------------------------------------------------
     * 4) Generate wrapper functions for each parallel task.
     *    Each wrapper receives the context struct as i8* arg,
     *    casts it back, and GEPs to access captured variable pointers.
     * ----------------------------------------------------------- */
    LLVMValueRef    saved_fn  = ctx->current_function;
    LLVMTypeRef     saved_ret = ctx->current_ret_type;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);

    LLVMTypeRef wrapper_params[] = { ctx->type_i8ptr };
    LLVMTypeRef wrapper_type = LLVMFunctionType(ctx->type_i8ptr,
                                                 wrapper_params, 1, 0);

    LLVMValueRef *wrapper_fns = calloc(count, sizeof(LLVMValueRef));

    for (size_t i = 0; i < count; i++) {
        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "_pgy_par_%d_%zu",
                 ctx->parallel_counter, i);

        LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, wrapper_type);
        wrapper_fns[i] = fn;

        LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, entry);

        ctx->current_function = fn;
        ctx->current_ret_type = ctx->type_i8ptr;

        llvm_scope_push(ctx);

        /* Cast arg (i8*) back to context struct pointer */
        LLVMValueRef arg0 = LLVMGetParam(fn, 0);
        LLVMValueRef ctx_ptr = LLVMBuildBitCast(ctx->builder, arg0,
            LLVMPointerType(ctx_struct_type, 0), "_pctx");

        /* For each captured variable: GEP → load pointer → declare in scope.
         * The loaded pointer points to the original alloca, so
         * load/store through it accesses the outer variable. */
        for (int c = 0; c < n_captured; c++) {
            LLVMValueRef field_ptr = LLVMBuildStructGEP2(
                ctx->builder, ctx_struct_type, ctx_ptr, (unsigned)c,
                llvm_tmp_name(ctx));
            LLVMValueRef var_ptr = LLVMBuildLoad2(
                ctx->builder, ctx->type_i8ptr, field_ptr,
                llvm_tmp_name(ctx));
            /* Declare in wrapper scope — the "alloca" is actually the
             * loaded pointer to the outer function's alloca.  Since
             * llvm_emit_identifier does Load2(type, alloca, ...) and
             * store operations do Store(val, alloca), this transparent
             * pointer indirection works correctly. */
            llvm_scope_declare(ctx, captured[c].name, var_ptr, captured[c].type);
        }

        /* Emit the task body */
        llvm_emit_statement(node->data.parallel.tasks[i], ctx);

        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder))
                == NULL)
            LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));

        llvm_scope_pop(ctx);
    }

    ctx->parallel_counter++;

    /* Restore insertion point */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret;
    LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);

    /* -----------------------------------------------------------
     * 5) Spawn all tasks, await all.
     * ----------------------------------------------------------- */
    LLVMFuncEntry *spawn_fn = llvm_lookup_function(ctx, "pgy_spawn_export");
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");

    if (spawn_fn == NULL || await_fn == NULL) {
        /* Fallback: emit sequentially */
        for (size_t i = 0; i < count; i++)
            llvm_emit_statement(node->data.parallel.tasks[i], ctx);
        free(wrapper_fns);
        return;
    }

    LLVMValueRef *handles = calloc(count, sizeof(LLVMValueRef));
    for (size_t i = 0; i < count; i++) {
        LLVMValueRef fn_ptr = LLVMBuildBitCast(
            ctx->builder, wrapper_fns[i], ctx->type_i8ptr,
            llvm_tmp_name(ctx));

        LLVMValueRef args[] = { fn_ptr, ctx_i8ptr };
        handles[i] = LLVMBuildCall2(ctx->builder, spawn_fn->fn_type,
                                     spawn_fn->fn, args, 2,
                                     llvm_tmp_name(ctx));
    }

    for (size_t i = 0; i < count; i++) {
        LLVMValueRef args[] = { handles[i] };
        LLVMBuildCall2(ctx->builder, await_fn->fn_type,
                       await_fn->fn, args, 1, "");
    }

    free(handles);
    free(wrapper_fns);
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
        llvm_emit_parallel_block(node, ctx);
        break;

    case AST_SELECT_STMT:
        /* MVP: emit cases sequentially (first match wins) */
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
            if (node->data.select_stmt.cases[i] != NULL)
                llvm_emit_statement(node->data.select_stmt.cases[i], ctx);
        }
        if (node->data.select_stmt.default_case != NULL)
            llvm_emit_statement(node->data.select_stmt.default_case, ctx);
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
    case AST_IMPORT_DECL:
        /* Handled in program pass or declaration-only — skip here */
        break;

    case AST_EXTERN_BLOCK:
        /* extern "C" { func ...; } — handled in program pass (Pass 0) */
        break;

    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } — emit body directly, no safety wrappers */
        if (node->data.unsafe_block.body != NULL)
            llvm_emit_block(node->data.unsafe_block.body, ctx);
        break;

    case AST_DEFER_STMT:
        /* defer { ... } — emit at end of current scope
         * For now: emit inline (proper scope-exit requires goto-cleanup) */
        if (node->data.defer_stmt.body != NULL)
            llvm_emit_statement(node->data.defer_stmt.body, ctx);
        break;

    case AST_BIND_STMT:
        /* bind party.slot = Role; — runtime vtable swap
         * Minimal stub: emits nothing (vtable binding is compile-time) */
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
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
    case AST_SPAWN_EXPR:
    case AST_AWAIT_EXPR:
    case AST_ARRAY_ACCESS:
    case AST_PARTY_INSTANCE:
    case AST_CONTEXT_ACCESS:
    case AST_TASK_GROUP:
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
    case AST_EVENT_INVOKE:
        llvm_emit_expression(node, ctx);
        break;

    default:
        fprintf(stderr, "[llvm] warning: unhandled statement AST type %d\n",
                (int)node->type);
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
llvm_emit_program(const HIRProgram *hir, LLVMGenCtx *ctx)
{
    if (hir == NULL) {
        ctx->has_error = true;
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Expected lowered HIR program");
        return;
    }

    /* Declare runtime functions */
    llvm_declare_runtime(ctx);

    /* Pass 0: Register class/struct types */
    for (size_t i = 0; i < hir->type_count; i++) {
        ASTNode *stmt = hir->types[i];
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

    /* Pass 0a: Register Party/Systemic/World struct types + methods */
    for (size_t i = 0; i < hir->item_count; i++) {
        ASTNode *stmt = hir->items[i].ast;
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
    for (size_t i = 0; i < hir->ability_count; i++) {
        ASTNode *stmt = hir->abilities[i];
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
    for (size_t i = 0; i < hir->role_count; i++) {
        ASTNode *stmt = hir->roles[i];
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

    /* Pass 0e: Register event types and generate helper functions */
    for (size_t i = 0; i < hir->event_count; i++) {
        ASTNode *stmt = hir->events[i];
        if (stmt == NULL || stmt->type != AST_EVENT_DECL)
            continue;

        const char *ename = stmt->data.event_decl.name;
        int pc = (int)stmt->data.event_decl.param_count;

        /* Event struct: { [16 x ptr], i64 } → handlers + count */
        LLVMTypeRef handler_arr = LLVMArrayType(ctx->type_i8ptr,
                                                 PGY_EVENT_MAX_HANDLERS);
        LLVMTypeRef sfields[] = { handler_arr, ctx->type_i64 };
        char sname[256];
        snprintf(sname, sizeof(sname), "PgyEvent_%s", ename);
        LLVMTypeRef evt_struct = LLVMStructCreateNamed(ctx->context, sname);
        LLVMStructSetBody(evt_struct, sfields, 2, 0);

        /* Collect handler parameter types */
        LLVMTypeRef ptypes[8];
        for (int j = 0; j < pc && j < 8; j++) {
            ASTNode *p = stmt->data.event_decl.params[j];
            ptypes[j] = (p->data.let_decl.type != NULL)
                ? ast_type_to_llvm(ctx, p->data.let_decl.type)
                : ctx->type_i32;
        }
        llvm_register_event(ctx, ename, evt_struct, pc, ptypes);

        /* Handler function type: void(param_types...) */
        LLVMTypeRef handler_ft = LLVMFunctionType(ctx->type_void,
            ptypes, (unsigned)pc, 0);
        LLVMTypeRef handler_ptr_t = LLVMPointerTypeInContext(ctx->context, 0);
        (void)handler_ptr_t;

        /* --- Generate EventName_INIT(ptr) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INIT", ename);
            LLVMTypeRef init_params[] = { ctx->type_i8ptr };
            LLVMTypeRef init_ft = LLVMFunctionType(ctx->type_void,
                init_params, 1, 0);
            LLVMValueRef init_fn = LLVMAddFunction(ctx->module, fname, init_ft);
            llvm_register_function(ctx, LLVMGetValueName(init_fn),
                init_fn, init_ft, ctx->type_void);

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, init_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            /* memset(e, 0, sizeof(struct)) */
            LLVMValueRef e_ptr = LLVMGetParam(init_fn, 0);
            LLVMValueRef sz = LLVMSizeOf(evt_struct);
            LLVMBuildMemSet(ctx->builder, e_ptr,
                LLVMConstInt(LLVMInt8TypeInContext(ctx->context), 0, 0),
                sz, 0);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_SUBSCRIBE(ptr, handler_ptr) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_SUBSCRIBE", ename);
            LLVMTypeRef sub_params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
            LLVMTypeRef sub_ft = LLVMFunctionType(ctx->type_void,
                sub_params, 2, 0);
            LLVMValueRef sub_fn = LLVMAddFunction(ctx->module, fname, sub_ft);
            llvm_register_function(ctx, LLVMGetValueName(sub_fn),
                sub_fn, sub_ft, ctx->type_void);

            LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, bb);

            LLVMValueRef e_ptr = LLVMGetParam(sub_fn, 0);
            LLVMValueRef h_ptr = LLVMGetParam(sub_fn, 1);

            /* count_ptr = GEP(e, 0, 1) — the i64 count field */
            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            /* if (count < 16) { handlers[count] = h; count++; } */
            LLVMValueRef max_h = LLVMConstInt(ctx->type_i64,
                PGY_EVENT_MAX_HANDLERS, 0);
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, count, max_h, "cmp");

            LLVMBasicBlockRef then_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "then");
            LLVMBasicBlockRef end_bb = LLVMAppendBasicBlockInContext(
                ctx->context, sub_fn, "end");
            LLVMBuildCondBr(ctx->builder, cmp, then_bb, end_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, then_bb);
            /* handlers_ptr = GEP(e, 0, 0, count) */
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                count
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMBuildStore(ctx->builder, h_ptr, slot);

            /* count++ */
            LLVMValueRef new_count = LLVMBuildAdd(ctx->builder,
                count, LLVMConstInt(ctx->type_i64, 1, 0), "new_count");
            LLVMBuildStore(ctx->builder, new_count, count_ptr);
            LLVMBuildBr(ctx->builder, end_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, end_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_UNSUBSCRIBE(ptr, handler_ptr) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_UNSUBSCRIBE", ename);
            LLVMTypeRef unsub_params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
            LLVMTypeRef unsub_ft = LLVMFunctionType(ctx->type_void,
                unsub_params, 2, 0);
            LLVMValueRef unsub_fn = LLVMAddFunction(ctx->module, fname, unsub_ft);
            llvm_register_function(ctx, LLVMGetValueName(unsub_fn),
                unsub_fn, unsub_ft, ctx->type_void);

            LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);

            LLVMValueRef e_ptr = LLVMGetParam(unsub_fn, 0);
            LLVMValueRef h_ptr = LLVMGetParam(unsub_fn, 1);

            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            /* Loop: for (i = 0; i < count; i++) */
            LLVMValueRef i_alloca = LLVMBuildAlloca(ctx->builder,
                ctx->type_i64, "i");
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);

            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "loop");
            LLVMBasicBlockRef found_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "found");
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "next");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "done");

            LLVMBuildBr(ctx->builder, loop_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);

            LLVMValueRef iv = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_alloca, "iv");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, iv, count, "cmp");
            LLVMBasicBlockRef body_bb = LLVMAppendBasicBlockInContext(
                ctx->context, unsub_fn, "body");
            LLVMBuildCondBr(ctx->builder, cmp, body_bb, done_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, body_bb);
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                iv
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMValueRef val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, slot, "hval");
            LLVMValueRef eq = LLVMBuildICmp(ctx->builder,
                LLVMIntEQ, val, h_ptr, "eq");
            LLVMBuildCondBr(ctx->builder, eq, found_bb, next_bb);

            /* found: shift elements left, count-- */
            LLVMPositionBuilderAtEnd(ctx->builder, found_bb);
            /* Simple: set handlers[i] = handlers[count-1], count-- */
            LLVMValueRef last_idx_val = LLVMBuildSub(ctx->builder,
                count, LLVMConstInt(ctx->type_i64, 1, 0), "last");
            LLVMValueRef last_gep_idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                last_idx_val
            };
            LLVMValueRef last_slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, last_gep_idx, 3, "last_slot");
            LLVMValueRef last_val = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, last_slot, "last_val");
            LLVMBuildStore(ctx->builder, last_val, slot);
            LLVMBuildStore(ctx->builder, last_idx_val, count_ptr);
            LLVMBuildBr(ctx->builder, done_bb);

            /* next: i++ */
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);
        }

        /* --- Generate EventName_INVOKE(ptr, params...) → void --- */
        {
            char fname[256];
            snprintf(fname, sizeof(fname), "%s_INVOKE", ename);
            /* params: ptr (event), then handler params */
            LLVMTypeRef *inv_params = calloc((size_t)(pc + 1),
                sizeof(LLVMTypeRef));
            inv_params[0] = ctx->type_i8ptr;
            for (int j = 0; j < pc; j++)
                inv_params[j + 1] = ptypes[j];

            LLVMTypeRef inv_ft = LLVMFunctionType(ctx->type_void,
                inv_params, (unsigned)(pc + 1), 0);
            LLVMValueRef inv_fn = LLVMAddFunction(ctx->module, fname, inv_ft);
            llvm_register_function(ctx, LLVMGetValueName(inv_fn),
                inv_fn, inv_ft, ctx->type_void);

            LLVMBasicBlockRef entry_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);

            LLVMValueRef e_ptr = LLVMGetParam(inv_fn, 0);
            LLVMValueRef count_ptr = LLVMBuildStructGEP2(ctx->builder,
                evt_struct, e_ptr, 1, "count_ptr");
            LLVMValueRef count = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, count_ptr, "count");

            LLVMValueRef i_alloca = LLVMBuildAlloca(ctx->builder,
                ctx->type_i64, "i");
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i64, 0, 0), i_alloca);

            LLVMBasicBlockRef loop_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "loop");
            LLVMBasicBlockRef call_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "call");
            LLVMBasicBlockRef done_bb = LLVMAppendBasicBlockInContext(
                ctx->context, inv_fn, "done");

            LLVMBuildBr(ctx->builder, loop_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, loop_bb);

            LLVMValueRef iv = LLVMBuildLoad2(ctx->builder,
                ctx->type_i64, i_alloca, "iv");
            LLVMValueRef cmp = LLVMBuildICmp(ctx->builder,
                LLVMIntULT, iv, count, "cmp");
            LLVMBuildCondBr(ctx->builder, cmp, call_bb, done_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, call_bb);
            /* Load handler pointer */
            LLVMValueRef idx[] = {
                LLVMConstInt(ctx->type_i32, 0, 0),
                LLVMConstInt(ctx->type_i32, 0, 0),
                iv
            };
            LLVMValueRef slot = LLVMBuildGEP2(ctx->builder,
                evt_struct, e_ptr, idx, 3, "slot");
            LLVMValueRef hval = LLVMBuildLoad2(ctx->builder,
                ctx->type_i8ptr, slot, "hval");

            /* Call handler(params...) via indirect call */
            LLVMValueRef *call_args = calloc((size_t)pc, sizeof(LLVMValueRef));
            for (int j = 0; j < pc; j++)
                call_args[j] = LLVMGetParam(inv_fn, (unsigned)(j + 1));
            LLVMBuildCall2(ctx->builder, handler_ft, hval,
                call_args, (unsigned)pc, "");
            free(call_args);

            /* i++ */
            LLVMValueRef inc = LLVMBuildAdd(ctx->builder,
                iv, LLVMConstInt(ctx->type_i64, 1, 0), "inc");
            LLVMBuildStore(ctx->builder, inc, i_alloca);
            LLVMBuildBr(ctx->builder, loop_bb);

            LLVMPositionBuilderAtEnd(ctx->builder, done_bb);
            LLVMBuildRetVoid(ctx->builder);

            free(inv_params);
        }

        /* Create global variable for this event */
        LLVMValueRef gv = LLVMAddGlobal(ctx->module, evt_struct, ename);
        LLVMSetInitializer(gv, LLVMConstNull(evt_struct));
        LLVMSetLinkage(gv, LLVMInternalLinkage);
    }

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
            if (ctx->generic_template_count < MAX_GENERIC_FUNCS) {
                ctx->generic_templates[ctx->generic_template_count].name =
                    stmt->data.func_decl.name;
                ctx->generic_templates[ctx->generic_template_count].ast = stmt;
                ctx->generic_template_count++;
            }
        } else {
            llvm_forward_declare_func(stmt, ctx);
        }
    }

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
    for (size_t i = 0; i < hir->role_count; i++) {
        ASTNode *stmt = hir->roles[i];
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
    for (size_t i = 0; i < hir->item_count; i++) {
        ASTNode *stmt = hir->items[i].ast;
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
llvm_codegen_to_object(const HIRProgram *hir, const char *module_name,
                       const char *output_path)
{
    LLVMGenCtx *ctx = llvm_ctx_create(module_name);
    if (ctx == NULL)
        return llvm_result_error("Out of memory");

    llvm_emit_program(hir, ctx);

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
