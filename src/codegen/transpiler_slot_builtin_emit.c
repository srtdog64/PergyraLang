#include "transpiler_slot_builtin_emit.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transpiler_context.h"
#include "transpiler_slot_target.h"
#include "transpiler_symbols.h"
#include "../common/string_compat.h"
#include "../compiler/mir_abi_layout.h"
#include "../compiler/mir_machine_layer.h"
#include "../semantic/diag_codes.h"

static char *
slot_builtin_heap_fmt(TranspilerCtx *ctx, const char *fmt, ...)
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
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: slot builtin expression formatting failed");
        return NULL;
    }

    buf = malloc((size_t)needed + 1);
    if (buf == NULL) {
        va_end(ap2);
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: slot builtin expression allocation failed");
        return NULL;
    }
    vsnprintf(buf, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static const MIRResourceRuntimeRow *
slot_builtin_runtime_row_by_kind(TranspilerCtx *ctx,
                                 MIRResourceAbiKind kind,
                                 const char *inner_type,
                                 const char *operation)
{
    const MIRResourceRuntimeRow *row =
        mir_abi_resource_runtime_row_by_kind(kind, inner_type, operation);
    if (row != NULL && row->runtime_fn != NULL && row->call_shape != NULL)
        if (ctx == NULL || ctx->active_mir_instruction == NULL
            || !rir_machine_contact_kind_is_present(
                ctx->active_mir_instruction->machine_contact_kind)
            || mir_machine_layer_fact_matches_runtime_operation(
                ctx->active_mir_instruction, row->resource_op_name))
            return row;

    if (row != NULL && ctx != NULL && ctx->active_mir_instruction != NULL
        && rir_machine_contact_kind_is_present(
            ctx->active_mir_instruction->machine_contact_kind)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C slot builtin runtime row disagrees with machine-layer runtime operation");
        return NULL;
    }

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "C source slot builtin %s requires MIR ABI runtime function row",
        operation != NULL ? operation : "<unknown>");
    return NULL;
}

static const char *
slot_builtin_runtime_fn_by_kind(TranspilerCtx *ctx,
                                MIRResourceAbiKind kind,
                                const char *inner_type,
                                const char *operation)
{
    const MIRResourceRuntimeRow *row =
        slot_builtin_runtime_row_by_kind(ctx, kind, inner_type, operation);
    return row != NULL ? row->runtime_fn : NULL;
}

static const char *
slot_builtin_runtime_fn(TranspilerCtx *ctx,
                        bool secure,
                        const char *inner_type,
                        const char *operation)
{
    return slot_builtin_runtime_fn_by_kind(
        ctx,
        secure ? MIR_RESOURCE_ABI_SECURE_SLOT : MIR_RESOURCE_ABI_SLOT,
        inner_type,
        operation);
}

static bool
slot_builtin_require_machine_fact(TranspilerCtx *ctx,
                                  RIRMachineContactKind expected)
{
    const MIRInstruction *inst = ctx != NULL ? ctx->active_mir_instruction : NULL;
    if (!transpiler_machine_layer_projection_is_bound(ctx)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "C backend machine builtin projection is not admitted (operation=%s)",
            rir_machine_contact_kind_name(expected));
        return false;
    }
    if (inst != NULL
        && inst->machine_contact_kind == expected
        && mir_machine_layer_fact_is_valid(inst)) {
        return true;
    }
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "C backend machine builtin requires MIR machine-layer fact (operation=%s)",
        rir_machine_contact_kind_name(expected));
    return false;
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

static char *
slot_builtin_emit_operand(TranspilerCtx *ctx,
                          ASTNode *expr,
                          const char *operation,
                          const char *role)
{
    char *lowered = emit_expression(expr, ctx);
    if (lowered != NULL)
        return lowered;

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: %s could not lower %s expression",
        operation != NULL ? operation : "slot operation",
        role != NULL ? role : "operand");
    return NULL;
}

static char *
slot_builtin_emit_slot_operand(TranspilerCtx *ctx,
                               ASTNode *expr,
                               const char *operation)
{
    bool saved_suppress = ctx->suppress_slot_auto_read;
    char *slot_expr;

    ctx->suppress_slot_auto_read = true;
    slot_expr = slot_builtin_emit_operand(ctx, expr, operation, "slot");
    ctx->suppress_slot_auto_read = saved_suppress;
    return slot_expr;
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
    return NULL;
}

char *
emit_builtin_claim_device_slot(ASTNode *call, TranspilerCtx *ctx)
{
    if (!slot_builtin_require_machine_fact(ctx, RIR_MACHINE_CONTACT_CLAIM))
        return NULL;
    (void)call;
    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: standalone ClaimDeviceSlot expression is unsupported; ClaimDeviceSlot<T>() must lower through let binding");
    return NULL;
}

char *
emit_builtin_write(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 2) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Write requires slot and value");
        return NULL;
    }

    /* Resolve slot inner type from tracking table */
    char inner_buf[128];
    const char *inner = inner_buf;
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = ast_call_argument(call, 0);
    if (!transpiler_resolve_slot_target_copy(ctx, slot_arg,
            inner_buf, sizeof(inner_buf), &slot_name, &secure))
        return NULL;

    char *slot_expr = slot_builtin_emit_slot_operand(ctx, slot_arg, "Write");
    if (slot_expr == NULL)
        return NULL;
    transpiler_refine_slot_target_from_emitted_expr(ctx, slot_expr, &slot_name, &secure);
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    if (slot_ref == NULL) {
        free(slot_expr);
        return NULL;
    }
    char *value_expr = slot_builtin_emit_operand(ctx,
        ast_call_argument(call, 1), "Write", "value");
    if (value_expr == NULL) {
        free(slot_ref);
        free(slot_expr);
        return NULL;
    }

    char *result;
    if (ast_call_arg_count(call) >= 3) {
        /* SecureSlot: Write(slot, value, token) */
        char *token_expr = slot_builtin_emit_operand(ctx,
            ast_call_argument(call, 2), "Write", "token");
        const char *write_fn;
        if (token_expr == NULL) {
            free(slot_ref);
            free(slot_expr);
            free(value_expr);
            return NULL;
        }
        write_fn = slot_builtin_runtime_fn(ctx, true, inner, "Write");
        if (write_fn == NULL) {
            free(token_expr);
            free(slot_ref);
            free(slot_expr);
            free(value_expr);
            return NULL;
        }
        result = slot_builtin_heap_fmt(ctx,
            "%s(%s, %s, &%s)",
            write_fn, slot_ref, value_expr, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        const char *write_fn;
        const char *token_name = require_slot_token_name(
            ctx, slot_name, "SecureSlot Write");
        if (token_name == NULL) {
            free(slot_ref);
            free(slot_expr);
            free(value_expr);
            return NULL;
        }
        write_fn = slot_builtin_runtime_fn(ctx, true, inner, "Write");
        if (write_fn == NULL) {
            free(slot_ref);
            free(slot_expr);
            free(value_expr);
            return NULL;
        }
        result = slot_builtin_heap_fmt(ctx,
            "%s(%s, %s, &%s)",
            write_fn, slot_ref, value_expr, token_name);
    } else {
        /* Plain slot: Write(slot, value) */
        const char *write_fn = slot_builtin_runtime_fn(
            ctx, false, inner, "Write");
        if (write_fn == NULL) {
            free(slot_ref);
            free(slot_expr);
            free(value_expr);
            return NULL;
        }
        result = slot_builtin_heap_fmt(ctx,
            "%s(%s, %s)",
            write_fn, slot_ref, value_expr);
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
        return NULL;
    }

    char *slot_expr = slot_builtin_emit_slot_operand(ctx,
        ast_call_argument(call, 0), "View");
    return slot_expr;
}

char *
emit_builtin_read(ASTNode *call, TranspilerCtx *ctx)
{
    if (ast_call_arg_count(call) < 1) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Read requires slot");
        return NULL;
    }

    /* Resolve slot inner type from tracking table */
    char inner_buf[128];
    const char *inner = inner_buf;
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = ast_call_argument(call, 0);
    if (!transpiler_resolve_slot_target_copy(ctx, slot_arg,
            inner_buf, sizeof(inner_buf), &slot_name, &secure))
        return NULL;

    char *slot_expr = slot_builtin_emit_slot_operand(ctx, slot_arg, "Read");
    if (slot_expr == NULL)
        return NULL;
    transpiler_refine_slot_target_from_emitted_expr(ctx, slot_expr, &slot_name, &secure);
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    if (slot_ref == NULL) {
        free(slot_expr);
        return NULL;
    }
    char *result;

    if (ast_call_arg_count(call) >= 2) {
        char *token_expr = slot_builtin_emit_operand(ctx,
            ast_call_argument(call, 1), "Read", "token");
        const char *read_fn;
        if (token_expr == NULL) {
            free(slot_ref);
            free(slot_expr);
            return NULL;
        }
        read_fn = slot_builtin_runtime_fn(ctx, true, inner, "Read");
        if (read_fn == NULL) {
            free(token_expr);
            free(slot_ref);
            free(slot_expr);
            return NULL;
        }
        result = slot_builtin_heap_fmt(ctx,
            "%s(%s, &%s)",
            read_fn, slot_ref, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        const char *read_fn;
        const char *token_name = require_slot_token_name(
            ctx, slot_name, "SecureSlot Read");
        if (token_name == NULL) {
            free(slot_ref);
            free(slot_expr);
            return NULL;
        }
        read_fn = slot_builtin_runtime_fn(ctx, true, inner, "Read");
        if (read_fn == NULL) {
            free(slot_ref);
            free(slot_expr);
            return NULL;
        }
        result = slot_builtin_heap_fmt(ctx, "%s(%s, &%s)",
            read_fn, slot_ref, token_name);
    } else {
        const char *read_fn = slot_builtin_runtime_fn(
            ctx, false, inner, "Read");
        if (read_fn == NULL) {
            free(slot_ref);
            free(slot_expr);
            return NULL;
        }
        result = slot_builtin_heap_fmt(ctx, "%s(%s)", read_fn, slot_ref);
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
        return NULL;
    }

    /* Resolve slot inner type from tracking table */
    char inner_buf[128];
    const char *inner = inner_buf;
    const char *slot_name = NULL;
    bool secure = false;
    ASTNode *slot_arg = ast_call_argument(call, 0);
    if (!transpiler_resolve_slot_target_copy(ctx, slot_arg,
            inner_buf, sizeof(inner_buf), &slot_name, &secure))
        return NULL;

    char *slot_expr = slot_builtin_emit_slot_operand(ctx, slot_arg, "Release");
    if (slot_expr == NULL)
        return NULL;
    transpiler_refine_slot_target_from_emitted_expr(ctx, slot_expr, &slot_name, &secure);
    char *slot_ref = slot_ref_expr(ctx, slot_name, slot_expr);
    if (slot_ref == NULL) {
        free(slot_expr);
        return NULL;
    }
    char *result;

    if (ast_call_arg_count(call) >= 2) {
        char *token_expr = slot_builtin_emit_operand(ctx,
            ast_call_argument(call, 1), "Release", "token");
        const char *release_fn;
        if (token_expr == NULL) {
            free(slot_ref);
            free(slot_expr);
            return NULL;
        }
        release_fn = slot_builtin_runtime_fn(ctx, true, inner, "Release");
        if (release_fn == NULL) {
            free(token_expr);
            free(slot_ref);
            free(slot_expr);
            return NULL;
        }
        result = slot_builtin_heap_fmt(ctx,
            "%s(%s, &%s)",
            release_fn, slot_ref, token_expr);
        free(token_expr);
    } else if (secure && slot_name != NULL) {
        const char *release_fn;
        const char *token_name = require_slot_token_name(
            ctx, slot_name, "SecureSlot Release");
        if (token_name == NULL) {
            free(slot_ref);
            free(slot_expr);
            return NULL;
        }
        release_fn = slot_builtin_runtime_fn(ctx, true, inner, "Release");
        if (release_fn == NULL) {
            free(slot_ref);
            free(slot_expr);
            return NULL;
        }
        result = slot_builtin_heap_fmt(ctx, "%s(%s, &%s)",
            release_fn, slot_ref, token_name);
    } else {
        const char *release_fn = slot_builtin_runtime_fn(
            ctx, false, inner, "Release");
        if (release_fn == NULL) {
            free(slot_ref);
            free(slot_expr);
            return NULL;
        }
        result = slot_builtin_heap_fmt(ctx, "%s(%s)", release_fn, slot_ref);
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
        return NULL;
    if (!slot_builtin_require_machine_fact(ctx, RIR_MACHINE_CONTACT_WRITE))
        return NULL;
    ASTNode *slot_arg = ast_call_argument(call, 0);
    char inner_buf[128];
    const char *inner = inner_buf;
    if (!transpiler_resolve_device_slot_inner_copy_or_error(ctx, slot_arg,
            "DeviceWrite", inner_buf, sizeof(inner_buf)))
        return NULL;
    char *slot_expr = slot_builtin_emit_slot_operand(ctx, slot_arg, "DeviceWrite");
    if (slot_expr == NULL)
        return NULL;
    char *value_expr = slot_builtin_emit_operand(ctx,
        ast_call_argument(call, 1), "DeviceWrite", "value");
    if (value_expr == NULL) {
        free(slot_expr);
        return NULL;
    }
    char *result;
    const char *write_fn = slot_builtin_runtime_fn_by_kind(
        ctx, MIR_RESOURCE_ABI_DEVICE_SLOT, inner, "Write");
    if (write_fn == NULL) {
        free(slot_expr);
        free(value_expr);
        return NULL;
    }

    result = slot_builtin_heap_fmt(ctx,
        "%s(&%s, %s)", write_fn, slot_expr, value_expr);
    free(slot_expr);
    free(value_expr);
    return result;
}

char *
emit_builtin_device_read(ASTNode *call, TranspilerCtx *ctx)
{
    if (!slot_builtin_require_arg_count(ctx, call, "DeviceRead", 1))
        return NULL;
    if (!slot_builtin_require_machine_fact(ctx, RIR_MACHINE_CONTACT_READ))
        return NULL;
    ASTNode *slot_arg = ast_call_argument(call, 0);
    char inner_buf[128];
    const char *inner = inner_buf;
    if (!transpiler_resolve_device_slot_inner_copy_or_error(ctx, slot_arg,
            "DeviceRead", inner_buf, sizeof(inner_buf)))
        return NULL;
    char *slot_expr = slot_builtin_emit_slot_operand(ctx, slot_arg, "DeviceRead");
    if (slot_expr == NULL)
        return NULL;
    char *result;
    const char *read_fn = slot_builtin_runtime_fn_by_kind(
        ctx, MIR_RESOURCE_ABI_DEVICE_SLOT, inner, "Read");
    if (read_fn == NULL) {
        free(slot_expr);
        return NULL;
    }

    result = slot_builtin_heap_fmt(ctx, "%s(&%s)", read_fn, slot_expr);
    free(slot_expr);
    return result;
}

char *
emit_builtin_release_device_slot(ASTNode *call, TranspilerCtx *ctx)
{
    if (!slot_builtin_require_arg_count(ctx, call, "ReleaseDeviceSlot", 1))
        return NULL;
    if (!slot_builtin_require_machine_fact(ctx, RIR_MACHINE_CONTACT_RELEASE))
        return NULL;
    ASTNode *slot_arg = ast_call_argument(call, 0);
    char inner_buf[128];
    const char *inner = inner_buf;
    if (!transpiler_resolve_device_slot_inner_copy_or_error(ctx, slot_arg,
            "ReleaseDeviceSlot", inner_buf, sizeof(inner_buf)))
        return NULL;
    char *slot_expr = slot_builtin_emit_slot_operand(ctx,
        slot_arg, "ReleaseDeviceSlot");
    if (slot_expr == NULL)
        return NULL;
    char *result;
    const char *release_fn = slot_builtin_runtime_fn_by_kind(
        ctx, MIR_RESOURCE_ABI_DEVICE_SLOT, inner, "Release");
    if (release_fn == NULL) {
        free(slot_expr);
        return NULL;
    }

    result = slot_builtin_heap_fmt(ctx, "%s(&%s)", release_fn, slot_expr);
    free(slot_expr);
    return result;
}

char *
emit_builtin_submit_device_read(ASTNode *call, TranspilerCtx *ctx)
{
    if (!slot_builtin_require_arg_count(ctx, call, "SubmitDeviceRead", 1))
        return NULL;
    if (!slot_builtin_require_machine_fact(ctx,
            RIR_MACHINE_CONTACT_SUBMIT_READ))
        return NULL;
    ASTNode *slot_arg = ast_call_argument(call, 0);
    char inner_buf[128];
    const char *inner = inner_buf;
    if (!transpiler_resolve_device_slot_inner_copy_or_error(ctx, slot_arg,
            "SubmitDeviceRead", inner_buf, sizeof(inner_buf)))
        return NULL;
    char *slot_expr = slot_builtin_emit_slot_operand(ctx,
        slot_arg, "SubmitDeviceRead");
    if (slot_expr == NULL)
        return NULL;
    char *result;
    const char *submit_fn = slot_builtin_runtime_fn_by_kind(
        ctx, MIR_RESOURCE_ABI_DEVICE_SLOT, inner, "SubmitRead");
    if (submit_fn == NULL) {
        free(slot_expr);
        return NULL;
    }

    result = slot_builtin_heap_fmt(ctx, "%s(&%s)", submit_fn, slot_expr);
    free(slot_expr);
    return result;
}
