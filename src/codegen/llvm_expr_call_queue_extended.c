#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_call_queue_extended.h"

#include <string.h>

#include "llvm_expr_call_collections_extended.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

typedef enum {
    LLVM_QUEUE_EXT_NONE = 0,
    LLVM_QUEUE_EXT_EMPTY,
    LLVM_QUEUE_EXT_POP,
    LLVM_QUEUE_EXT_PUSH,
    LLVM_QUEUE_EXT_SIZE,
} LLVMQueueExtendedOp;

typedef struct {
    const char *name;
    unsigned argc;
    LLVMQueueExtendedOp op;
} LLVMQueueExtendedSpec;

static const LLVMQueueExtendedSpec kQueueExtendedSpecs[] = {
    {"QueueEmpty", 1, LLVM_QUEUE_EXT_EMPTY},
    {"QueuePop", 1, LLVM_QUEUE_EXT_POP},
    {"QueuePush", 2, LLVM_QUEUE_EXT_PUSH},
    {"QueueSize", 1, LLVM_QUEUE_EXT_SIZE},
};

static int
llvm_queue_extended_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const LLVMQueueExtendedSpec *spec = (const LLVMQueueExtendedSpec *)entry;
    return strcmp(name, spec->name);
}

static LLVMQueueExtendedOp
llvm_queue_extended_lookup(const char *callee_name, unsigned argc)
{
    const LLVMQueueExtendedSpec *spec;

    if (callee_name == NULL)
        return LLVM_QUEUE_EXT_NONE;
    spec = (const LLVMQueueExtendedSpec *)bsearch(
        callee_name,
        kQueueExtendedSpecs,
        sizeof(kQueueExtendedSpecs) / sizeof(kQueueExtendedSpecs[0]),
        sizeof(kQueueExtendedSpecs[0]),
        llvm_queue_extended_spec_compare);
    if (spec == NULL || spec->argc != argc)
        return LLVM_QUEUE_EXT_NONE;
    return spec->op;
}

static bool
llvm_queue_error_out(LLVMGenCtx *ctx, ASTNode *node, LLVMValueRef *out,
                     LLVMValueRef recovery, const char *message)
{
    (void)recovery;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM queue builtin could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

bool
llvm_emit_queue_extended_call(ASTNode *node, LLVMGenCtx *ctx,
                              const char *callee_name,
                              LLVMValueRef *out)
{
    LLVMQueueExtendedOp op;

    if (node == NULL || node->type != AST_CALL)
        return false;
    op = llvm_queue_extended_lookup(callee_name,
        (unsigned)ast_call_arg_count(node));

    if (op == LLVM_QUEUE_EXT_PUSH) {
        ASTNode *queue_arg = ast_call_argument(node, 0);
        LLVMVarEntry *queue_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef value;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        queue_var = llvm_collection_required_receiver_var(ctx, node, queue_arg,
            callee_name, "queue", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (queue_var == NULL)
            return true;
        inner_name = llvm_lookup_queue_inner(ctx, ast_identifier_name(queue_arg));
        elem_ty = llvm_collection_required_value_type(ctx, node, "Queue",
            ast_identifier_name(queue_arg), inner_name, out);
        if (elem_ty == NULL)
            return true;
        value = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (value == NULL)
            return llvm_queue_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM QueuePush could not lower value expression");
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_queue_push_string_raw_export");
            if (fn == NULL) {
                *out = NULL;
                return true;
            }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                value
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
        }
        if (LLVMTypeOf(value) != elem_ty) {
            if ((elem_ty == ctx->type_i32 || elem_ty == ctx->type_i64)
                && (LLVMTypeOf(value) == ctx->type_f32 || LLVMTypeOf(value) == ctx->type_f64))
                value = LLVMBuildFPToSI(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
            else if ((elem_ty == ctx->type_f32 || elem_ty == ctx->type_f64)
                && (LLVMTypeOf(value) == ctx->type_i32 || LLVMTypeOf(value) == ctx->type_i64))
                value = LLVMBuildSIToFP(ctx->builder, value, elem_ty, llvm_tmp_name(ctx));
        }
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_queue_error_out(ctx, node, out,
                LLVMConstInt(ctx->type_i32, 0, 0),
                "LLVM QueuePush could not allocate element temporary");
        LLVMBuildStore(ctx->builder, value, tmp);
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_queue_push_raw_export");
        if (fn == NULL) {
            *out = NULL;
            return true;
        }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        { *out = LLVMConstInt(ctx->type_i32, 0, 0); return true; }
    }

    if (op == LLVM_QUEUE_EXT_POP) {
        ASTNode *queue_arg = ast_call_argument(node, 0);
        LLVMVarEntry *queue_var;
        const char *inner_name;
        LLVMTypeRef elem_ty;
        LLVMValueRef tmp;
        LLVMFuncEntry *fn;
        queue_var = llvm_collection_required_receiver_var(ctx, node, queue_arg,
            callee_name, "queue", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (queue_var == NULL)
            return true;
        inner_name = llvm_lookup_queue_inner(ctx, ast_identifier_name(queue_arg));
        elem_ty = llvm_collection_required_value_type(ctx, node, "Queue",
            ast_identifier_name(queue_arg), inner_name, out);
        if (elem_ty == NULL)
            return true;
        tmp = llvm_create_entry_alloca(ctx, elem_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_queue_error_out(ctx, node, out,
                LLVMConstNull(elem_ty),
                "LLVM QueuePop could not allocate result temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(elem_ty), tmp);
        if (inner_name != NULL && strcmp(inner_name, "String") == 0) {
            fn = llvm_required_collection_function(ctx, node, callee_name,
                "pgy_queue_pop_string_raw_export");
            if (fn == NULL)
                { *out = NULL; return true; }
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
                LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
            { *out = LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx)); return true; }
        }
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_queue_pop_raw_export");
        if (fn == NULL)
            { *out = NULL; return true; }
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            LLVMBuildBitCast(ctx->builder, tmp, ctx->type_i8ptr, llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        { *out = LLVMBuildLoad2(ctx->builder, elem_ty, tmp, llvm_tmp_name(ctx)); return true; }
    }

    if (op == LLVM_QUEUE_EXT_SIZE) {
        ASTNode *queue_arg = ast_call_argument(node, 0);
        LLVMVarEntry *queue_var;
        LLVMFuncEntry *fn;
        queue_var = llvm_collection_required_receiver_var(ctx, node, queue_arg,
            callee_name, "queue", LLVMConstInt(ctx->type_i32, 0, 0), out);
        if (queue_var == NULL)
            return true;
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_queue_size_raw_export");
        if (fn == NULL)
            { *out = NULL; return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx)); return true; }
        }
    }

    if (op == LLVM_QUEUE_EXT_EMPTY) {
        ASTNode *queue_arg = ast_call_argument(node, 0);
        LLVMVarEntry *queue_var;
        LLVMFuncEntry *fn;
        queue_var = llvm_collection_required_receiver_var(ctx, node, queue_arg,
            callee_name, "queue", LLVMConstInt(ctx->type_i1, 1, 0), out);
        if (queue_var == NULL)
            return true;
        fn = llvm_required_collection_function(ctx, node, callee_name,
            "pgy_queue_empty_raw_export");
        if (fn == NULL)
            { *out = NULL; return true; }
        {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, queue_var->alloca, ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            { *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, llvm_tmp_name(ctx)); return true; }
        }
    }

    return false;
}

#endif
