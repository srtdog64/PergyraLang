/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM event call lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_expr_event_calls.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

static LLVMValueRef
llvm_event_expr_error(LLVMGenCtx *ctx, ASTNode *node, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s", message != NULL ? message
                : "LLVM event expression lowering requires complete metadata");
    }
    return NULL;
}

static bool
llvm_event_call_helper_name(char *out,
    size_t out_size,
    const char *event_name,
    const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || event_name == NULL || suffix == NULL)
        return false;

    written = snprintf(out, out_size, "%s_%s", event_name, suffix);
    return written >= 0 && (size_t)written < out_size;
}

bool
llvm_emit_event_invocation_call(ASTNode *node, LLVMGenCtx *ctx,
                                const char *callee_name, LLVMValueRef *out)
{
    LLVMEventTypeEntry *evt = llvm_lookup_event(ctx, callee_name);
    char fname[256];
    LLVMFuncEntry *fn;
    LLVMValueRef ev_ptr;
    size_t arg_count;
    LLVMValueRef *args;

    if (out == NULL || evt == NULL)
        return false;

    if (!llvm_event_call_helper_name(fname, sizeof(fname),
            callee_name, "INVOKE")) {
        *out = llvm_event_expr_error(ctx, node,
            "LLVM event invocation helper name is too long");
        return true;
    }
    fn = llvm_lookup_function(ctx, fname);
    ev_ptr = LLVMGetNamedGlobal(ctx->module, callee_name);
    if (ev_ptr == NULL) {
        LLVMVarEntry *ev = llvm_scope_lookup(ctx, callee_name);
        if (ev != NULL)
            ev_ptr = ev->alloca;
    }
    if (fn == NULL || ev_ptr == NULL) {
        *out = llvm_event_expr_error(ctx, node,
            "LLVM event invocation call requires generated event function and storage");
        return true;
    }

    arg_count = ast_call_arg_count(node);
    if (arg_count > (size_t)UINT_MAX - 1U
        || arg_count > (SIZE_MAX / sizeof(LLVMValueRef)) - 1U) {
        *out = llvm_event_expr_error(ctx, node,
            "LLVM event invocation argument count exceeds backend ABI limits");
        return true;
    }
    args = pgy_arena_calloc(&ctx->scratch, (arg_count + 1) * sizeof(LLVMValueRef));
    if (args == NULL) {
        *out = llvm_event_expr_error(ctx, node,
            "LLVM event invocation call argument allocation failed");
        return true;
    }
    args[0] = ev_ptr;
    for (size_t j = 0; j < arg_count; j++) {
        args[j + 1] = llvm_emit_expression(ast_call_argument(node, j), ctx);
        if (args[j + 1] == NULL) {
            *out = llvm_event_expr_error(ctx, node,
                "LLVM event invocation call could not lower argument expression");
            return true;
        }
    }
    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args,
        (unsigned)(arg_count + 1), "");
    *out = LLVMConstInt(ctx->type_i32, 0, 0);
    return true;
}

LLVMValueRef
llvm_emit_event_subscribe_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *evt = ast_event_op_event(node);
    ASTNode *handler = ast_event_op_handler(node);
    const char *evt_name = NULL;
    if (evt != NULL && evt->type == AST_IDENTIFIER)
        evt_name = ast_identifier_name(evt);
    if (evt_name == NULL)
        return llvm_event_expr_error(ctx, node,
            "LLVM event subscribe requires an identifier event target");

    char fname[256];
    if (!llvm_event_call_helper_name(fname, sizeof(fname),
            evt_name, "SUBSCRIBE")) {
        return llvm_event_expr_error(ctx, node,
            "LLVM event subscribe helper name is too long");
    }
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
    LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
    LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
        : LLVMGetNamedGlobal(ctx->module, evt_name);
    LLVMValueRef hval = llvm_emit_expression(handler, ctx);

    if (fn == NULL || ev_ptr == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM event subscribe requires generated event function '%s' and event storage '%s'",
            fname, evt_name);
        return NULL;
    }
    if (hval == NULL)
        return llvm_event_expr_error(ctx, node,
            "LLVM event subscribe could not lower handler expression");

    LLVMValueRef args[] = { ev_ptr, hval };
    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

LLVMValueRef
llvm_emit_event_unsubscribe_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *evt = ast_event_op_event(node);
    ASTNode *handler = ast_event_op_handler(node);
    const char *evt_name = NULL;
    if (evt != NULL && evt->type == AST_IDENTIFIER)
        evt_name = ast_identifier_name(evt);
    if (evt_name == NULL)
        return llvm_event_expr_error(ctx, node,
            "LLVM event unsubscribe requires an identifier event target");

    char fname[256];
    if (!llvm_event_call_helper_name(fname, sizeof(fname),
            evt_name, "UNSUBSCRIBE")) {
        return llvm_event_expr_error(ctx, node,
            "LLVM event unsubscribe helper name is too long");
    }
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
    LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
    LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
        : LLVMGetNamedGlobal(ctx->module, evt_name);
    LLVMValueRef hval = llvm_emit_expression(handler, ctx);

    if (fn == NULL || ev_ptr == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM event unsubscribe requires generated event function '%s' and event storage '%s'",
            fname, evt_name);
        return NULL;
    }
    if (hval == NULL)
        return llvm_event_expr_error(ctx, node,
            "LLVM event unsubscribe could not lower handler expression");

    LLVMValueRef args[] = { ev_ptr, hval };
    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

LLVMValueRef
llvm_emit_event_invoke_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *evt = ast_event_invoke_event(node);
    const char *evt_name = NULL;
    if (evt != NULL && evt->type == AST_IDENTIFIER)
        evt_name = ast_identifier_name(evt);
    if (evt_name == NULL)
        return llvm_event_expr_error(ctx, node,
            "LLVM event invoke requires an identifier event target");

    char fname[256];
    if (!llvm_event_call_helper_name(fname, sizeof(fname),
            evt_name, "INVOKE")) {
        return llvm_event_expr_error(ctx, node,
            "LLVM event invoke helper name is too long");
    }
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
    LLVMVarEntry *ev = llvm_scope_lookup(ctx, evt_name);
    LLVMValueRef ev_ptr = (ev != NULL) ? ev->alloca
        : LLVMGetNamedGlobal(ctx->module, evt_name);
    if (fn == NULL || ev_ptr == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM event invoke requires generated event function '%s' and event storage '%s'",
            fname, evt_name);
        return NULL;
    }

    size_t ac = ast_event_invoke_arg_count(node);
    LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
        (ac + 1) * sizeof(LLVMValueRef));
    if (args == NULL)
        return llvm_event_expr_error(ctx, node,
            "LLVM event invoke argument allocation failed");
    args[0] = ev_ptr;
    for (size_t j = 0; j < ac; j++) {
        args[j + 1] = llvm_emit_expression(
            ast_event_invoke_argument(node, j), ctx);
        if (args[j + 1] == NULL)
            return llvm_event_expr_error(ctx, node,
                "LLVM event invoke could not lower argument expression");
    }
    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args,
        (unsigned)(ac + 1), "");
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

#endif /* PGY_LLVM_ENABLED */
