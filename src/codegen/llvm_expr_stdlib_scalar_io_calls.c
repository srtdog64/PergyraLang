#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_stdlib_scalar_io_calls.h"

#include <stdlib.h>
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
        ast_call_arguments(node, NULL), arg_count);
    if (*out_result == NULL)
        *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
            "could not lower runtime call arguments");
    return true;
}

typedef struct LLVMStdlibRuntimeCallSpec {
    const char *name;
    const char *family;
    const char *runtime_name;
    size_t      arg_count;
} LLVMStdlibRuntimeCallSpec;

typedef enum LLVMStdlibStringSpecialOp {
    LLVM_STDLIB_STRING_SPECIAL_NONE = 0,
    LLVM_STDLIB_STRING_SPECIAL_LENGTH,
    LLVM_STDLIB_STRING_SPECIAL_TO_STRING,
} LLVMStdlibStringSpecialOp;

typedef struct LLVMStdlibStringSpecialSpec {
    const char *name;
    size_t arg_count;
    LLVMStdlibStringSpecialOp op;
} LLVMStdlibStringSpecialSpec;

typedef enum LLVMStdlibIoSpecialOp {
    LLVM_STDLIB_IO_SPECIAL_NONE = 0,
    LLVM_STDLIB_IO_SPECIAL_NOW,
    LLVM_STDLIB_IO_SPECIAL_PRINT,
    LLVM_STDLIB_IO_SPECIAL_READ_LINE,
    LLVM_STDLIB_IO_SPECIAL_SLEEP,
} LLVMStdlibIoSpecialOp;

typedef struct LLVMStdlibIoSpecialSpec {
    const char *name;
    size_t arg_count;
    LLVMStdlibIoSpecialOp op;
} LLVMStdlibIoSpecialSpec;

static int
llvm_stdlib_runtime_call_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMStdlibRuntimeCallSpec *spec =
        (const LLVMStdlibRuntimeCallSpec *)entry;

    return strcmp(name, spec->name);
}

static int
llvm_stdlib_string_special_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMStdlibStringSpecialSpec *spec =
        (const LLVMStdlibStringSpecialSpec *)entry;

    return strcmp(name, spec->name);
}

static int
llvm_stdlib_io_special_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMStdlibIoSpecialSpec *spec =
        (const LLVMStdlibIoSpecialSpec *)entry;

    return strcmp(name, spec->name);
}

static const LLVMStdlibRuntimeCallSpec *
llvm_stdlib_string_file_runtime_call_lookup(const char *callee_name)
{
    static const LLVMStdlibRuntimeCallSpec kLLVMStdlibStringFileRuntimeSpecs[] = {
        { "Acos", "stdlib scalar", "Acos", 1 },
        { "Asin", "stdlib scalar", "Asin", 1 },
        { "Atan", "stdlib scalar", "Atan", 1 },
        { "Atan2", "stdlib scalar", "Atan2", 2 },
        { "Ceil", "stdlib scalar", "Ceil", 1 },
        { "Concat", "stdlib string", "StringConcat", 2 },
        { "Contains", "stdlib string", "StringContains", 2 },
        { "Cos", "stdlib scalar", "Cos", 1 },
        { "Exit", "stdlib process", "pgy_exit", 1 },
        { "Exp", "stdlib scalar", "Exp", 1 },
        { "FileExists", "stdlib io", "pgy_file_exists", 1 },
        { "Floor", "stdlib scalar", "Floor", 1 },
        { "Input", "stdlib io", "pgy_input", 1 },
        { "Log10", "stdlib scalar", "Log10", 1 },
        { "Log2", "stdlib scalar", "Log2", 1 },
        { "Lower", "stdlib string", "ToLower", 1 },
        { "MathLog", "stdlib scalar", "MathLog", 1 },
        { "Pow", "stdlib scalar", "Pow", 2 },
        { "Random", "stdlib scalar", "Random", 1 },
        { "ReadFile", "stdlib io", "pgy_read_file", 1 },
        { "Replace", "stdlib string", "StringReplace", 3 },
        { "Round", "stdlib scalar", "Round", 1 },
        { "Sin", "stdlib scalar", "Sin", 1 },
        { "Split", "stdlib string", "StringSplit", 2 },
        { "Sqrt", "stdlib scalar", "Sqrt", 1 },
        { "StringConcat", "stdlib string", "StringConcat", 2 },
        { "StringContains", "stdlib string", "StringContains", 2 },
        { "StringIndexOf", "stdlib string", "StringIndexOf", 2 },
        { "StringReplace", "stdlib string", "StringReplace", 3 },
        { "StringSplit", "stdlib string", "StringSplit", 2 },
        { "StringTrim", "stdlib string", "StringTrim", 1 },
        { "Substring", "stdlib string", "Substring", 3 },
        { "Tan", "stdlib scalar", "Tan", 1 },
        { "ToFloat", "stdlib scalar", "ToFloat", 1 },
        { "ToInt", "stdlib scalar", "ToInt", 1 },
        { "ToLower", "stdlib string", "ToLower", 1 },
        { "ToUpper", "stdlib string", "ToUpper", 1 },
        { "Trim", "stdlib string", "StringTrim", 1 },
        { "Upper", "stdlib string", "ToUpper", 1 },
        { "WriteFile", "stdlib io", "pgy_write_file", 2 },
    };

    if (callee_name == NULL)
        return NULL;

    return (const LLVMStdlibRuntimeCallSpec *)bsearch(
        &callee_name, kLLVMStdlibStringFileRuntimeSpecs,
        sizeof(kLLVMStdlibStringFileRuntimeSpecs)
            / sizeof(kLLVMStdlibStringFileRuntimeSpecs[0]),
        sizeof(kLLVMStdlibStringFileRuntimeSpecs[0]),
        llvm_stdlib_runtime_call_compare);
}

static const LLVMStdlibRuntimeCallSpec *
llvm_stdlib_runtime_io_call_lookup(const char *callee_name)
{
    static const LLVMStdlibRuntimeCallSpec kLLVMStdlibRuntimeIoSpecs[] = {
        { "FileClose", "stdlib file", "pgy_file_close", 1 },
        { "FileOpen", "stdlib file", "pgy_file_open", 2 },
        { "FileRead", "stdlib file", "pgy_file_read", 1 },
        { "FileWrite", "stdlib file", "pgy_file_write", 2 },
        { "SeedRandom", "stdlib runtime", "SeedRandom", 1 },
    };

    if (callee_name == NULL)
        return NULL;

    return (const LLVMStdlibRuntimeCallSpec *)bsearch(
        &callee_name, kLLVMStdlibRuntimeIoSpecs,
        sizeof(kLLVMStdlibRuntimeIoSpecs)
            / sizeof(kLLVMStdlibRuntimeIoSpecs[0]),
        sizeof(kLLVMStdlibRuntimeIoSpecs[0]),
        llvm_stdlib_runtime_call_compare);
}

static LLVMStdlibStringSpecialOp
llvm_stdlib_string_special_lookup(const char *callee_name, size_t argc)
{
    static const LLVMStdlibStringSpecialSpec kLLVMStdlibStringSpecialSpecs[] = {
        { "StringLength", 1, LLVM_STDLIB_STRING_SPECIAL_LENGTH },
        { "ToString", 1, LLVM_STDLIB_STRING_SPECIAL_TO_STRING },
    };
    const LLVMStdlibStringSpecialSpec *match;

    if (callee_name == NULL)
        return LLVM_STDLIB_STRING_SPECIAL_NONE;

    match = (const LLVMStdlibStringSpecialSpec *)bsearch(
        &callee_name, kLLVMStdlibStringSpecialSpecs,
        sizeof(kLLVMStdlibStringSpecialSpecs)
            / sizeof(kLLVMStdlibStringSpecialSpecs[0]),
        sizeof(kLLVMStdlibStringSpecialSpecs[0]),
        llvm_stdlib_string_special_compare);
    if (match == NULL || match->arg_count != argc)
        return LLVM_STDLIB_STRING_SPECIAL_NONE;
    return match->op;
}

static LLVMStdlibIoSpecialOp
llvm_stdlib_io_special_lookup(const char *callee_name, size_t argc)
{
    static const LLVMStdlibIoSpecialSpec kLLVMStdlibIoSpecialSpecs[] = {
        { "Now", 0, LLVM_STDLIB_IO_SPECIAL_NOW },
        { "Print", 1, LLVM_STDLIB_IO_SPECIAL_PRINT },
        { "ReadLine", 0, LLVM_STDLIB_IO_SPECIAL_READ_LINE },
        { "Sleep", 1, LLVM_STDLIB_IO_SPECIAL_SLEEP },
    };
    const LLVMStdlibIoSpecialSpec *match;

    if (callee_name == NULL)
        return LLVM_STDLIB_IO_SPECIAL_NONE;

    match = (const LLVMStdlibIoSpecialSpec *)bsearch(
        &callee_name, kLLVMStdlibIoSpecialSpecs,
        sizeof(kLLVMStdlibIoSpecialSpecs)
            / sizeof(kLLVMStdlibIoSpecialSpecs[0]),
        sizeof(kLLVMStdlibIoSpecialSpecs[0]),
        llvm_stdlib_io_special_compare);
    if (match == NULL || match->arg_count != argc)
        return LLVM_STDLIB_IO_SPECIAL_NONE;
    return match->op;
}

bool
llvm_emit_stdlib_string_file_call(ASTNode *node, LLVMGenCtx *ctx,
                                  const char *callee_name,
                                  LLVMValueRef *out_result)
{
    LLVMStdlibStringSpecialOp op;

    if (node == NULL || ctx == NULL || callee_name == NULL || out_result == NULL)
        return false;

    op = llvm_stdlib_string_special_lookup(callee_name, ast_call_arg_count(node));

    if (op == LLVM_STDLIB_STRING_SPECIAL_LENGTH) {
        LLVMValueRef s = llvm_emit_expression(ast_call_argument(node, 0), ctx);
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

    if (op == LLVM_STDLIB_STRING_SPECIAL_TO_STRING) {
        LLVMValueRef value = llvm_emit_expression(ast_call_argument(node, 0), ctx);
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

    /*
     * StringJoin / Join take Array<String> by *pointer* in the runtime ABI
     * (PgyArray_String *arr), so the generic stdlib registry path -- which
     * lowers args by value -- cannot dispatch them. Resolve the array
     * receiver to its alloca slot and pass that pointer directly.
     */
    if (ast_call_arg_count(node) == 2
        && (strcmp(callee_name, "StringJoin") == 0
            || strcmp(callee_name, "Join") == 0)) {
        ASTNode *receiver = ast_call_argument(node, 0);
        if (receiver == NULL || receiver->type != AST_IDENTIFIER
            || ast_identifier_name(receiver) == NULL) {
            *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
                "requires an identifier Array<String> receiver");
            return true;
        }
        const char *recv_name = ast_identifier_name(receiver);
        LLVMVarEntry *arr_var = llvm_scope_lookup(ctx, recv_name);
        if (arr_var == NULL || arr_var->alloca == NULL) {
            *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
                "Array<String> receiver has no LLVM alloca");
            return true;
        }
        LLVMValueRef sep = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (sep == NULL) {
            *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
                "could not lower separator argument");
            return true;
        }
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringJoin");
        if (fn == NULL) {
            *out_result = llvm_stdlib_error_value(node, ctx, callee_name,
                "StringJoin runtime function not declared in backend registry");
            return true;
        }
        LLVMValueRef args[] = { arr_var->alloca, sep };
        *out_result = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            args, 2, llvm_tmp_name(ctx));
        return true;
    }

    {
        const LLVMStdlibRuntimeCallSpec *spec =
            llvm_stdlib_string_file_runtime_call_lookup(callee_name);
        if (spec != NULL && ast_call_arg_count(node) == spec->arg_count) {
            return llvm_emit_required_runtime_call_result(node, ctx,
                spec->family, callee_name, spec->runtime_name,
                spec->arg_count, out_result);
        }
    }

    return false;
}

bool
llvm_emit_stdlib_runtime_io_call(ASTNode *node, LLVMGenCtx *ctx,
                                 const char *callee_name,
                                 LLVMValueRef *out_result)
{
    LLVMStdlibIoSpecialOp op;

    if (node == NULL || ctx == NULL || callee_name == NULL || out_result == NULL)
        return false;

    {
        const LLVMStdlibRuntimeCallSpec *spec =
            llvm_stdlib_runtime_io_call_lookup(callee_name);
        if (spec != NULL && ast_call_arg_count(node) == spec->arg_count) {
            return llvm_emit_required_runtime_call_result(node, ctx,
                spec->family, callee_name, spec->runtime_name,
                spec->arg_count, out_result);
        }
    }

    op = llvm_stdlib_io_special_lookup(callee_name, ast_call_arg_count(node));

    if (op == LLVM_STDLIB_IO_SPECIAL_PRINT) {
        LLVMValueRef val = llvm_emit_expression(ast_call_argument(node, 0), ctx);
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

    if (op == LLVM_STDLIB_IO_SPECIAL_READ_LINE) {
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

    if (op == LLVM_STDLIB_IO_SPECIAL_NOW) {
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

    if (op == LLVM_STDLIB_IO_SPECIAL_SLEEP) {
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "stdlib time", callee_name, "pgy_sleep_ms");
        if (fn == NULL) {
            *out_result = NULL;
            return true;
        }
        {
            LLVMValueRef arg = llvm_emit_expression(ast_call_argument(node, 0), ctx);
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
