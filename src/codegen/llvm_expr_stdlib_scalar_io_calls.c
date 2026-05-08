#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_stdlib_scalar_io_calls.h"

#include <string.h>

#include "llvm_expr_string_coerce.h"
#include "llvm_internal_api.h"

static LLVMValueRef
llvm_stdlib_error_value(ASTNode *node, LLVMGenCtx *ctx,
                        const char *callee_name, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM stdlib call '%s' %s",
            callee_name != NULL ? callee_name : "<anonymous>",
            message != NULL ? message : "could not lower its argument");
    }
    return NULL;
}

static bool
llvm_emit_required_runtime_call_result(ASTNode *node, LLVMGenCtx *ctx,
                                       const char *family_name,
                                       const char *callee_name,
                                       const char *runtime_name,
                                       size_t arg_count,
                                       LLVMValueRef *out_result)
{
    LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
        family_name, callee_name, runtime_name);
    if (fn == NULL) {
        *out_result = NULL;
        return true;
    }
    *out_result = llvm_emit_function_call_args(ctx, fn,
        node->data.call.arguments, arg_count);
    if (*out_result == NULL)
        *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
            "could not lower runtime call arguments");
    return true;
}

bool
llvm_emit_stdlib_string_file_call(ASTNode *node, LLVMGenCtx *ctx,
                                  const char *callee_name,
                                  LLVMValueRef *out_result)
{
    if (node == NULL || ctx == NULL || callee_name == NULL || out_result == NULL)
        return false;

    if (strcmp(callee_name, "StringLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef s = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMFuncEntry *strlen_fn = llvm_lookup_function(ctx, "strlen");
        LLVMValueRef args[] = { s };
        LLVMValueRef len;
        if (s == NULL) {
            *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
                "could not lower string argument");
            return true;
        }
        if (strlen_fn == NULL) {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i64, params, 1, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, "strlen", ft);
            llvm_register_function(ctx, "strlen", fn, ft, ctx->type_i64);
            strlen_fn = llvm_lookup_function(ctx, "strlen");
        }
        len = LLVMBuildCall2(ctx->builder, strlen_fn->fn_type,
            strlen_fn->fn, args, 1, llvm_tmp_name(ctx));
        *out_result = LLVMBuildTrunc(ctx->builder, len, ctx->type_i32,
            llvm_tmp_name(ctx));
        return true;
    }

    if ((strcmp(callee_name, "Contains") == 0
         || strcmp(callee_name, "StringContains") == 0)
        && node->data.call.arg_count == 2) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib string", callee_name, "StringContains", 2, out_result);
    }

    if ((strcmp(callee_name, "Replace") == 0
         || strcmp(callee_name, "StringReplace") == 0)
        && node->data.call.arg_count == 3) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib string", callee_name, "StringReplace", 3, out_result);
    }

    if (strcmp(callee_name, "Substring") == 0
        && node->data.call.arg_count == 3) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib string", callee_name, "Substring", 3, out_result);
    }

    if ((strcmp(callee_name, "Trim") == 0
         || strcmp(callee_name, "StringTrim") == 0)
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib string", callee_name, "StringTrim", 1, out_result);
    }

    if ((strcmp(callee_name, "Upper") == 0
         || strcmp(callee_name, "ToUpper") == 0)
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib string", callee_name, "ToUpper", 1, out_result);
    }

    if ((strcmp(callee_name, "Lower") == 0
         || strcmp(callee_name, "ToLower") == 0)
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib string", callee_name, "ToLower", 1, out_result);
    }

    if ((strcmp(callee_name, "Concat") == 0
         || strcmp(callee_name, "StringConcat") == 0)
        && node->data.call.arg_count == 2) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib string", callee_name, "StringConcat", 2, out_result);
    }

    if (strcmp(callee_name, "ReadFile") == 0
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib io", callee_name, "pgy_read_file", 1, out_result);
    }

    if (strcmp(callee_name, "ToString") == 0
        && node->data.call.arg_count == 1) {
        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (value == NULL) {
            *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
                "could not lower value argument");
            return true;
        }
        *out_result = llvm_coerce_value_to_string(value, ctx);
        if (*out_result == NULL)
            *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
                "could not stringify value argument");
        return true;
    }

    if (strcmp(callee_name, "ToInt") == 0
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib scalar", callee_name, "ToInt", 1, out_result);
    }

    if (strcmp(callee_name, "ToFloat") == 0
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib scalar", callee_name, "ToFloat", 1, out_result);
    }

    if (strcmp(callee_name, "Random") == 0
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib scalar", callee_name, "Random", 1, out_result);
    }

    if (strcmp(callee_name, "WriteFile") == 0
        && node->data.call.arg_count == 2) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib io", callee_name, "pgy_write_file", 2, out_result);
    }

    if (strcmp(callee_name, "Input") == 0
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib io", callee_name, "pgy_input", 1, out_result);
    }

    return false;
}

bool
llvm_emit_stdlib_runtime_io_call(ASTNode *node, LLVMGenCtx *ctx,
                                 const char *callee_name,
                                 LLVMValueRef *out_result)
{
    if (node == NULL || ctx == NULL || callee_name == NULL || out_result == NULL)
        return false;

    if (strcmp(callee_name, "SeedRandom") == 0
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib runtime", callee_name, "SeedRandom", 1, out_result);
    }

    if (strcmp(callee_name, "FileOpen") == 0
        && node->data.call.arg_count == 2) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib file", callee_name, "pgy_file_open", 2, out_result);
    }

    if (strcmp(callee_name, "FileRead") == 0
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib file", callee_name, "pgy_file_read", 1, out_result);
    }

    if (strcmp(callee_name, "FileWrite") == 0
        && node->data.call.arg_count == 2) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib file", callee_name, "pgy_file_write", 2, out_result);
    }

    if (strcmp(callee_name, "FileClose") == 0
        && node->data.call.arg_count == 1) {
        return llvm_emit_required_runtime_call_result(node, ctx,
            "stdlib file", callee_name, "pgy_file_close", 1, out_result);
    }

    if (strcmp(callee_name, "Print") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef vt;
        LLVMFuncEntry *pf = llvm_required_runtime_function(ctx, node,
            "stdlib io", callee_name, "printf");
        if (val == NULL) {
            *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
                "could not lower print argument");
            return true;
        }
        vt = LLVMTypeOf(val);
        if (pf == NULL) {
            *out_result = NULL;
            return true;
        }
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
        *out_result = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "ReadLine") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "stdlib io", callee_name, "pgy_input");
        if (fn != NULL) {
            LLVMValueRef empty = LLVMBuildGlobalStringPtr(ctx->builder, "",
                ".readline_empty");
            LLVMValueRef args[] = { empty };
            *out_result = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                args, 1, "");
        } else {
            *out_result = NULL;
        }
        return true;
    }

    if (strcmp(callee_name, "Now") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "stdlib time", callee_name, "pgy_now_ms");
        if (fn != NULL) {
            *out_result = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                NULL, 0, "");
        } else {
            *out_result = NULL;
        }
        return true;
    }

    if (strcmp(callee_name, "Sleep") == 0 && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "stdlib time", callee_name, "pgy_sleep_ms");
        if (fn == NULL) {
            *out_result = NULL;
            return true;
        }
        {
            LLVMValueRef arg = llvm_emit_expression(node->data.call.arguments[0], ctx);
            if (arg == NULL) {
                *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
                    "could not lower sleep duration argument");
                return true;
            }
            LLVMValueRef args[] = { arg };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }
        *out_result = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    return false;
}

#endif
