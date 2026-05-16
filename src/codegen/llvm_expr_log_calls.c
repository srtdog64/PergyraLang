/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_log_calls.h"

#include <stdlib.h>
#include <string.h>

#include "llvm_expr_banner_string_helpers.h"
#include "llvm_expr_string_coerce.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

typedef enum LLVMLogOp {
    LLVM_LOG_OP_NONE = 0,
    LLVM_LOG_OP_BANNER,
    LLVM_LOG_OP_LOG,
    LLVM_LOG_OP_RAW,
} LLVMLogOp;

typedef struct LLVMLogSpec {
    const char *name;
    LLVMLogOp op;
} LLVMLogSpec;

static int
llvm_log_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMLogSpec *spec = (const LLVMLogSpec *)entry;

    return strcmp(name, spec->name);
}

static LLVMLogOp
llvm_log_lookup(const char *callee_name)
{
    static const LLVMLogSpec kLLVMLogSpecs[] = {
        { "Log", LLVM_LOG_OP_LOG },
        { "LogBanner", LLVM_LOG_OP_BANNER },
        { "LogBlock", LLVM_LOG_OP_BANNER },
        { "LogRaw", LLVM_LOG_OP_RAW },
    };
    const LLVMLogSpec *match;

    if (callee_name == NULL)
        return LLVM_LOG_OP_NONE;

    match = (const LLVMLogSpec *)bsearch(&callee_name, kLLVMLogSpecs,
        sizeof(kLLVMLogSpecs) / sizeof(kLLVMLogSpecs[0]),
        sizeof(kLLVMLogSpecs[0]), llvm_log_spec_compare);
    return match != NULL ? match->op : LLVM_LOG_OP_NONE;
}

static LLVMValueRef
llvm_log_error(LLVMGenCtx *ctx, ASTNode *node, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "%s", message != NULL ? message
                : "LLVM log operation requires a printable argument");
    }
    return NULL;
}

static const char *
llvm_log_function_for_type(LLVMGenCtx *ctx, LLVMTypeRef type,
                           bool multiline_log)
{
    if (multiline_log)
        return "pgy_log_banner";
    if (type == ctx->type_i64)
        return "pgy_log_long";
    if (type == ctx->type_f32)
        return "pgy_log_float";
    if (type == ctx->type_f64)
        return "pgy_log_double";
    if (type == ctx->type_i1)
        return "pgy_log_bool";
    if (type == ctx->type_i8ptr)
        return "pgy_log_string";
    return "pgy_log_int";
}

static LLVMFuncEntry *
llvm_required_log_function(LLVMGenCtx *ctx, ASTNode *node,
                           const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;

    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM log operation requires registered runtime function '%s'",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

static LLVMValueRef
llvm_emit_log_call(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *arg_node;
    bool multiline_log = false;
    LLVMTypeRef arg_type = NULL;
    const char *log_fn_name;
    LLVMValueRef arg = NULL;
    LLVMFuncEntry *log_fn;
    LLVMValueRef args[1];

    if (ast_call_arg_count(node) < 1)
        return llvm_log_error(ctx, node,
            "LLVM Log requires at least one argument");

    arg_node = ast_call_argument(node, 0);
    if (arg_node != NULL && arg_node->type == AST_STRING
        && ast_string_value(arg_node) != NULL) {
        const char *raw = ast_string_value(arg_node);
        multiline_log = (strchr(raw, '\n') != NULL) || (strchr(raw, '\r') != NULL);
        if (multiline_log) {
            char *normalized = llvm_normalize_banner_string_literal_scratch(
                raw, &ctx->scratch);
            arg = LLVMBuildGlobalStringPtr(ctx->builder,
                normalized != NULL ? normalized : raw,
                llvm_tmp_name(ctx));
        } else {
            arg = LLVMBuildGlobalStringPtr(ctx->builder, raw,
                                          llvm_tmp_name(ctx));
        }
        arg_type = ctx->type_i8ptr;
    } else {
        arg = llvm_emit_expression(arg_node, ctx);
        if (arg != NULL)
            arg_type = LLVMTypeOf(arg);
    }

    if (arg == NULL)
        return llvm_log_error(ctx, node,
            "LLVM Log could not lower its argument");

    log_fn_name = llvm_log_function_for_type(ctx, arg_type, multiline_log);
    log_fn = llvm_required_log_function(ctx, node, log_fn_name);
    if (log_fn == NULL)
        return NULL;

    args[0] = arg;
    LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn, args, 1, "");
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

static LLVMValueRef
llvm_emit_log_raw_call(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *arg_node;
    LLVMValueRef arg = NULL;
    LLVMTypeRef arg_type;
    const char *log_fn_name;
    LLVMFuncEntry *log_fn;
    LLVMValueRef args[1];

    if (ast_call_arg_count(node) < 1)
        return llvm_log_error(ctx, node,
            "LLVM LogRaw requires at least one argument");

    arg_node = ast_call_argument(node, 0);
    if (arg_node != NULL && arg_node->type == AST_STRING
        && ast_string_value(arg_node) != NULL) {
        arg = LLVMBuildGlobalStringPtr(ctx->builder,
            ast_string_value(arg_node), llvm_tmp_name(ctx));
    } else {
        arg = llvm_emit_expression(arg_node, ctx);
        if (arg == NULL)
            return llvm_log_error(ctx, node,
                "LLVM LogRaw could not lower its argument");
    }

    arg_type = LLVMTypeOf(arg);
    log_fn_name = llvm_log_function_for_type(ctx, arg_type, false);
    log_fn = llvm_required_log_function(ctx, node, log_fn_name);
    if (log_fn == NULL)
        return NULL;

    args[0] = arg;
    LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn, args, 1, "");
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

static LLVMValueRef
llvm_emit_log_banner_call(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *arg;
    LLVMValueRef log_arg = NULL;
    LLVMFuncEntry *log_fn;
    LLVMValueRef args[1];

    if (ast_call_arg_count(node) < 1)
        return llvm_log_error(ctx, node,
            "LLVM LogBanner requires at least one argument");

    arg = ast_call_argument(node, 0);
    if (arg == NULL)
        return llvm_log_error(ctx, node,
            "LLVM LogBanner requires a non-null argument");

    if (arg->type == AST_STRING) {
        char *normalized = llvm_normalize_banner_string_literal_scratch(
            ast_string_value(arg), &ctx->scratch);
        if (normalized != NULL) {
            log_arg = LLVMBuildGlobalStringPtr(ctx->builder, normalized,
                                               llvm_tmp_name(ctx));
        }
    } else {
        log_arg = llvm_emit_expression(arg, ctx);
        if (log_arg != NULL)
            log_arg = llvm_coerce_value_to_string(log_arg, ctx);
    }

    if (log_arg == NULL)
        return llvm_log_error(ctx, node,
            "LLVM LogBanner could not lower or stringify its argument");

    log_fn = llvm_required_log_function(ctx, node, "pgy_log_banner");
    if (log_fn == NULL)
        return NULL;

    args[0] = log_arg;
    LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn, args, 1, "");
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

bool
llvm_emit_log_family_call(ASTNode *node, LLVMGenCtx *ctx,
                          const char *callee_name, LLVMValueRef *out)
{
    LLVMLogOp op;

    if (out == NULL)
        return false;

    op = llvm_log_lookup(callee_name);

    if (op == LLVM_LOG_OP_LOG) {
        *out = llvm_emit_log_call(node, ctx);
        return true;
    }
    if (op == LLVM_LOG_OP_RAW) {
        *out = llvm_emit_log_raw_call(node, ctx);
        return true;
    }
    if (op == LLVM_LOG_OP_BANNER) {
        *out = llvm_emit_log_banner_call(node, ctx);
        return true;
    }
    return false;
}

#endif
