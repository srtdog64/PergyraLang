#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_allocator_calls.h"

#include <stdlib.h>
#include <string.h>

typedef enum LLVMAllocatorOp {
    LLVM_ALLOCATOR_OP_NONE = 0,
    LLVM_ALLOCATOR_OP_DEBUG,
    LLVM_ALLOCATOR_OP_POOL,
    LLVM_ALLOCATOR_OP_SYSTEM,
    LLVM_ALLOCATOR_OP_TRACING,
} LLVMAllocatorOp;

typedef struct LLVMAllocatorSpec {
    const char *name;
    const char *runtime_name;
    size_t argc;
    LLVMAllocatorOp op;
} LLVMAllocatorSpec;

static int
llvm_allocator_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMAllocatorSpec *spec = (const LLVMAllocatorSpec *)entry;

    return strcmp(name, spec->name);
}

static const LLVMAllocatorSpec *
llvm_allocator_lookup(const char *callee_name)
{
    static const LLVMAllocatorSpec specs[] = {
        { "AllocatorDebug", "pgy_allocator_debug_init", 0,
          LLVM_ALLOCATOR_OP_DEBUG },
        { "AllocatorPool", "pgy_allocator_pool_init", 1,
          LLVM_ALLOCATOR_OP_POOL },
        { "AllocatorSystem", "pgy_allocator_system_init", 0,
          LLVM_ALLOCATOR_OP_SYSTEM },
        { "AllocatorTracing", "pgy_allocator_tracing_init", 0,
          LLVM_ALLOCATOR_OP_TRACING },
    };

    if (callee_name == NULL)
        return NULL;
    return (const LLVMAllocatorSpec *)bsearch(&callee_name,
        specs, sizeof(specs) / sizeof(specs[0]), sizeof(specs[0]),
        llvm_allocator_spec_compare);
}

static bool
llvm_allocator_emit_error(ASTNode *node, LLVMGenCtx *ctx,
                          const char *callee_name, const char *message,
                          LLVMValueRef *out)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            "LLVM allocator builtin '%s' %s",
            callee_name != NULL ? callee_name : "<allocator>",
            message != NULL ? message : "could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

bool
llvm_emit_allocator_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                                 const char *callee_name, LLVMValueRef *out)
{
    const LLVMAllocatorSpec *spec;
    LLVMFuncEntry *fn;
    LLVMValueRef storage;
    LLVMValueRef args[2];
    size_t argc;

    if (out == NULL)
        return false;
    *out = NULL;
    spec = llvm_allocator_lookup(callee_name);
    if (spec == NULL)
        return false;

    argc = ast_call_arg_count(node);
    if (argc != spec->argc)
        return llvm_allocator_emit_error(node, ctx, callee_name,
            "has invalid argument count", out);

    fn = llvm_required_runtime_function(ctx, node,
        "allocator", callee_name, spec->runtime_name);
    if (fn == NULL)
        return true;

    storage = llvm_create_entry_alloca(ctx, ctx->type_allocator,
        llvm_tmp_name(ctx));
    if (storage == NULL)
        return llvm_allocator_emit_error(node, ctx, callee_name,
            "requires allocator storage", out);

    args[0] = storage;
    if (spec->op == LLVM_ALLOCATOR_OP_POOL) {
        LLVMValueRef cap = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (cap == NULL)
            return llvm_allocator_emit_error(node, ctx, callee_name,
                "could not lower capacity expression", out);
        if (LLVMTypeOf(cap) != ctx->type_i64)
            cap = LLVMBuildSExtOrBitCast(ctx->builder, cap, ctx->type_i64,
                llvm_tmp_name(ctx));
        args[1] = cap;
    }

    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args,
        (unsigned)(spec->op == LLVM_ALLOCATOR_OP_POOL ? 2 : 1), "");
    *out = LLVMBuildLoad2(ctx->builder, ctx->type_allocator, storage,
        llvm_tmp_name(ctx));
    return true;
}

#endif /* PGY_LLVM_ENABLED */
