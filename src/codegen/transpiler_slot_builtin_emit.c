#include "transpiler_slot_builtin_emit.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transpiler_context.h"
#include "transpiler_slot_target.h"
#include "transpiler_symbols.h"
#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"

static char *
slot_builtin_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int needed;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)needed + 1);
    if (buf == NULL) {
        va_end(ap2);
        return NULL;
    }
    vsnprintf(buf, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static bool
slot_builtin_require_arg_count(TranspilerCtx *ctx,
                               ASTNode *call,
                               const char *operation,
                               size_t required)
{
    size_t argc = ast_call_arg_count(call);
    if (argc >= required)
        return true;
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: %s requires %zu argument%s",
        operation != NULL ? operation : "slot operation",
        required,
        required == 1 ? "" : "s");
    return false;
}

char *
emit_builtin_claim_slot(ASTNode *call, TranspilerCtx *ctx)
{
    /*
     * ClaimSlot<T>() is handled in let_decl where the type is known.
     * If encountered standalone, emit a comment.
     */
    (void)call;
    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: standalone ClaimSlot expression is unsupported; ClaimSlot<T>() must lower through let binding");
    return pergyra_strdup("0");
}

char *
emit_builtin_claim_device_slot(ASTNode *call, TranspilerCtx *ctx)
{
    (void)call;
    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: standalone ClaimDeviceSlot expression is unsupported; ClaimDeviceSlot<T>() must lower through let binding");
    return pergyra_strdup("0");
}

char *
emit_builtin_write(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 2) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Write requires slot and value");
        return pergyra_strdup("0");
    }

    /* Resolve slot inner type from tracking table */
    char inner_buf[128];
    const char *inner = inner_buf;
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = ast_call_argument(call, 0);
    if (!transpiler_resolve_slot_target_copy(ctx, slot_arg,
            inner_buf, sizeof(inner_buf), &slot_name, &secure))
        return pergyra_strdup("0");

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    transpiler_refine_slot_target_from_emitted_expr(ctx, slot_expr, &slot_name, &secure);
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    char *value_expr = emit_expression(ast_call_argument(call, 1), ctx);

    char *result;
    if (ast_call_arg_count(call) >= 3) {
        /* SecureSlot: Write(slot, value, token) */
        char *token_expr = emit_expression(ast_call_argument(call, 2), ctx);
        result = slot_builtin_strdup_fmt(
            "pgy_secure_write_%s(%s, %s, &%s)",
            inner, slot_ref, value_expr, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        const char *token_name = require_slot_token_name(
            ctx, slot_name, "SecureSlot Write");
        if (token_name == NULL) {
            free(slot_ref);
            free(slot_expr);
            free(value_expr);
            return pergyra_strdup("0");
        }
        result = slot_builtin_strdup_fmt(
            "pgy_secure_write_%s(%s, %s, &%s)",
            inner, slot_ref, value_expr, token_name);
    } else {
        /* Plain slot: Write(slot, value) */
        result = slot_builtin_strdup_fmt(
            "pgy_write_%s(%s, %s)",
            inner, slot_ref, value_expr);
    }

    free(slot_ref);
    free(slot_expr);
    free(value_expr);
    return result;
}

char *
emit_builtin_view(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: View requires source slot");
        return pergyra_strdup("0");
    }

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(ast_call_argument(call, 0), ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    return slot_expr;
}

char *
emit_builtin_read(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Read requires slot");
        return pergyra_strdup("0");
    }

    /* Resolve slot inner type from tracking table */
    char inner_buf[128];
    const char *inner = inner_buf;
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = ast_call_argument(call, 0);
    if (!transpiler_resolve_slot_target_copy(ctx, slot_arg,
            inner_buf, sizeof(inner_buf), &slot_name, &secure))
        return pergyra_strdup("0");

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    transpiler_refine_slot_target_from_emitted_expr(ctx, slot_expr, &slot_name, &secure);
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    char *result;

    if (ast_call_arg_count(call) >= 2) {
        char *token_expr = emit_expression(ast_call_argument(call, 1), ctx);
        result = slot_builtin_strdup_fmt(
            "pgy_secure_read_%s(%s, &%s)",
            inner, slot_ref, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        const char *token_name = require_slot_token_name(
            ctx, slot_name, "SecureSlot Read");
        if (token_name == NULL) {
            free(slot_ref);
            free(slot_expr);
            return pergyra_strdup("0");
        }
        result = slot_builtin_strdup_fmt("pgy_secure_read_%s(%s, &%s)",
            inner, slot_ref, token_name);
    } else {
        result = slot_builtin_strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
    }

    free(slot_ref);
    free(slot_expr);
    return result;
}

char *
emit_builtin_release(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Release requires slot");
        return pergyra_strdup("0");
    }

    /* Resolve slot inner type from tracking table */
    char inner_buf[128];
    const char *inner = inner_buf;
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = ast_call_argument(call, 0);
    if (!transpiler_resolve_slot_target_copy(ctx, slot_arg,
            inner_buf, sizeof(inner_buf), &slot_name, &secure))
        return pergyra_strdup("0");

    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    transpiler_refine_slot_target_from_emitted_expr(ctx, slot_expr, &slot_name, &secure);
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    char *result;

    if (ast_call_arg_count(call) >= 2) {
        char *token_expr = emit_expression(ast_call_argument(call, 1), ctx);
        result = slot_builtin_strdup_fmt(
            "pgy_secure_release_%s(%s, &%s)",
            inner, slot_ref, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        const char *token_name = require_slot_token_name(
            ctx, slot_name, "SecureSlot Release");
        if (token_name == NULL) {
            free(slot_ref);
            free(slot_expr);
            return pergyra_strdup("0");
        }
        result = slot_builtin_strdup_fmt("pgy_secure_release_%s(%s, &%s)",
            inner, slot_ref, token_name);
    } else {
        result = slot_builtin_strdup_fmt("pgy_release_%s(%s)", inner, slot_ref);
    }

    /* Mark slot as explicitly released -> prevents auto-release at scope exit */
    if (slot_arg->type == AST_IDENTIFIER) {
        const char *sname = slot_name != NULL
            ? slot_name : ast_identifier_name(slot_arg);
        for (int i = 0; i < ctx->slot_var_count; i++) {
            if (strcmp(ctx->slot_vars[i].name, sname) == 0) {
                ctx->slot_vars[i].released = true;
                break;
            }
        }
    }

    free(slot_ref);
    free(slot_expr);
    return result;
}

char *
emit_builtin_device_write(ASTNode *call, TranspilerCtx *ctx)
{
    if (!slot_builtin_require_arg_count(ctx, call, "DeviceWrite", 2))
        return pergyra_strdup("0");
    ASTNode *slot_arg = ast_call_argument(call, 0);
    char inner_buf[128];
    const char *inner = inner_buf;
    if (!transpiler_resolve_device_slot_inner_copy_or_error(ctx, slot_arg,
            "DeviceWrite", inner_buf, sizeof(inner_buf)))
        return pergyra_strdup("0");
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *value_expr = emit_expression(ast_call_argument(call, 1), ctx);
    char *result;

    result = slot_builtin_strdup_fmt("pgy_device_write_%s(&%s, %s)", inner, slot_expr, value_expr);
    free(slot_expr);
    free(value_expr);
    return result;
}

char *
emit_builtin_device_read(ASTNode *call, TranspilerCtx *ctx)
{
    if (!slot_builtin_require_arg_count(ctx, call, "DeviceRead", 1))
        return pergyra_strdup("0");
    ASTNode *slot_arg = ast_call_argument(call, 0);
    char inner_buf[128];
    const char *inner = inner_buf;
    if (!transpiler_resolve_device_slot_inner_copy_or_error(ctx, slot_arg,
            "DeviceRead", inner_buf, sizeof(inner_buf)))
        return pergyra_strdup("0");
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *result;

    result = slot_builtin_strdup_fmt("pgy_device_read_%s(&%s)", inner, slot_expr);
    free(slot_expr);
    return result;
}

char *
emit_builtin_release_device_slot(ASTNode *call, TranspilerCtx *ctx)
{
    if (!slot_builtin_require_arg_count(ctx, call, "ReleaseDeviceSlot", 1))
        return pergyra_strdup("0");
    ASTNode *slot_arg = ast_call_argument(call, 0);
    char inner_buf[128];
    const char *inner = inner_buf;
    if (!transpiler_resolve_device_slot_inner_copy_or_error(ctx, slot_arg,
            "ReleaseDeviceSlot", inner_buf, sizeof(inner_buf)))
        return pergyra_strdup("0");
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *result;

    result = slot_builtin_strdup_fmt("pgy_release_device_%s(&%s)", inner, slot_expr);
    free(slot_expr);
    return result;
}

char *
emit_builtin_submit_device_read(ASTNode *call, TranspilerCtx *ctx)
{
    if (!slot_builtin_require_arg_count(ctx, call, "SubmitDeviceRead", 1))
        return pergyra_strdup("0");
    ASTNode *slot_arg = ast_call_argument(call, 0);
    char inner_buf[128];
    const char *inner = inner_buf;
    if (!transpiler_resolve_device_slot_inner_copy_or_error(ctx, slot_arg,
            "SubmitDeviceRead", inner_buf, sizeof(inner_buf)))
        return pergyra_strdup("0");
    bool saved_suppress = ctx->suppress_slot_auto_read;
    ctx->suppress_slot_auto_read = true;
    char *slot_expr = emit_expression(slot_arg, ctx);
    ctx->suppress_slot_auto_read = saved_suppress;
    char *result;

    result = slot_builtin_strdup_fmt("pgy_submit_device_read_%s(&%s)", inner, slot_expr);
    free(slot_expr);
    return result;
}
