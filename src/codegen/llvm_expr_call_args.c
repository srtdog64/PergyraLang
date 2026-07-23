/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM expression call argument lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include <limits.h>
#include <stdint.h>

LLVMValueRef
llvm_emit_function_call_args(LLVMGenCtx *ctx, LLVMFuncEntry *func,
                             ASTNode **arg_nodes, size_t argc)
{
    LLVMValueRef *args = NULL;
    LLVMValueRef result;
    unsigned param_count;

    if (ctx == NULL || func == NULL)
        return NULL;

    if (argc > (size_t)UINT_MAX || argc > SIZE_MAX / sizeof(LLVMValueRef)) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_GENERIC_ARG_LIST,
            "LLVM call helper argument count exceeds backend ABI limits for '%s'",
            LLVMGetValueName(func->fn));
        return NULL;
    }

    param_count = LLVMCountParams(func->fn);
    if ((size_t)param_count != argc) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ALIGN_ARG_TYPE,
            "LLVM call helper argument count does not match runtime ABI for '%s': expected %u, got %zu",
            LLVMGetValueName(func->fn), param_count, argc);
        return NULL;
    }

    if (argc > 0) {
        if (arg_nodes == NULL) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM call helper requires argument AST payloads for '%s'",
                LLVMGetValueName(func->fn));
            return NULL;
        }
        args = pgy_arena_calloc(&ctx->scratch, argc * sizeof(LLVMValueRef));
        if (args == NULL) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM call helper could not allocate argument storage for '%s'",
                LLVMGetValueName(func->fn));
            return NULL;
        }
        for (size_t i = 0; i < argc; i++) {
            LLVMTypeRef expected_abi_type =
                LLVMTypeOf(LLVMGetParam(func->fn, (unsigned)i));
            LLVMTypeRef saved_expected_abi_type = ctx->expected_abi_type;
            ctx->expected_abi_type = expected_abi_type;
            LLVMTypeRef arg_type = llvm_stmt_infer_expr_type(ctx, arg_nodes[i]);
            ctx->expected_abi_type = saved_expected_abi_type;
            if (ctx->has_error)
                return NULL;
            if (arg_type == ctx->type_void) {
                llvm_set_error_at_with_hints(ctx, arg_nodes[i],
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ALIGN_ARG_TYPE,
                    "LLVM call helper cannot pass a Void expression as argument %zu for '%s'",
                    i, LLVMGetValueName(func->fn));
                return NULL;
            }
            args[i] = llvm_emit_expression(arg_nodes[i], ctx);
            if (args[i] == NULL) {
                llvm_set_error_at_with_hints(ctx, arg_nodes[i],
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM call helper could not lower argument %zu for '%s'",
                    i, LLVMGetValueName(func->fn));
                return NULL;
            }
            if (LLVMTypeOf(args[i]) != expected_abi_type) {
                llvm_set_error_at_with_hints(ctx, arg_nodes[i],
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ALIGN_ARG_TYPE,
                    "LLVM call helper argument %zu does not match runtime ABI for '%s'",
                    i, LLVMGetValueName(func->fn));
                return NULL;
            }
        }
    }

    if (llvm_runtime_aggregate_return_is_sret_function(
            LLVMGetValueName(func->fn))) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM runtime aggregate-return call '%s' must be emitted through "
            "the aggregate-return ABI owner, not the generic by-value call path",
            LLVMGetValueName(func->fn));
        return NULL;
    }

    /*
     * Honest ABI guard: a 0-argument runtime function returning a struct/array
     * BY VALUE mis-lowers on the by-value call path and crashes. clang compiles
     * such a runtime fn with the sret ABI (void f(ptr sret)); the by-value-decl
     * vs sret-def mismatch is bitcast-reconciled at llvm-link, but the 0-arg
     * form does not survive it. Make the latent trap an explicit compile error
     * here instead of a silent segfault, so any future 0-arg struct-returner is
     * caught and routed through the runtime aggregate-return ABI owner.
     */
    if (argc == 0 && func->ret_type != NULL && func->ret_type != ctx->type_void) {
        LLVMTypeKind rk = LLVMGetTypeKind(func->ret_type);
        if (rk == LLVMStructTypeKind || rk == LLVMArrayTypeKind) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM 0-arg struct-returning runtime call '%s' must use the sret "
                "slot pattern (declare void(ptr sret), alloca a return slot, call, "
                "load), not the by-value path which mis-lowers and crashes",
                LLVMGetValueName(func->fn));
            return NULL;
        }
    }

    if (func->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                       args, (unsigned)argc, "");
        result = llvm_void_expression_placeholder(ctx, NULL,
            "function-call");
    } else {
        result = LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                                args, (unsigned)argc, llvm_tmp_name(ctx));
    }

    return result;
}

#endif /* PGY_LLVM_ENABLED */
