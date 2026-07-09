#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_slot_device_calls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_slot_type_policy.h"
#include "llvm_expr_identifier_slot_helpers.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"
#include "../compiler/mir_abi_layout.h"

typedef enum LLVMSlotBuiltinOp {
    LLVM_SLOT_BUILTIN_OP_NONE = 0,
    LLVM_SLOT_BUILTIN_OP_DEVICE_READ,
    LLVM_SLOT_BUILTIN_OP_DEVICE_WRITE,
    LLVM_SLOT_BUILTIN_OP_READ,
    LLVM_SLOT_BUILTIN_OP_RELEASE,
    LLVM_SLOT_BUILTIN_OP_RELEASE_DEVICE_SLOT,
    LLVM_SLOT_BUILTIN_OP_SUBMIT_DEVICE_READ,
    LLVM_SLOT_BUILTIN_OP_WRITE,
} LLVMSlotBuiltinOp;

typedef struct LLVMSlotBuiltinSpec {
    const char *name;
    LLVMSlotBuiltinOp op;
} LLVMSlotBuiltinSpec;

static int
llvm_slot_builtin_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMSlotBuiltinSpec *spec = (const LLVMSlotBuiltinSpec *)entry;

    return strcmp(name, spec->name);
}

static LLVMSlotBuiltinOp
llvm_slot_builtin_lookup(const char *callee_name)
{
    static const LLVMSlotBuiltinSpec kLLVMSlotBuiltinSpecs[] = {
        { "DeviceRead", LLVM_SLOT_BUILTIN_OP_DEVICE_READ },
        { "DeviceWrite", LLVM_SLOT_BUILTIN_OP_DEVICE_WRITE },
        { "Read", LLVM_SLOT_BUILTIN_OP_READ },
        { "Release", LLVM_SLOT_BUILTIN_OP_RELEASE },
        { "ReleaseDeviceSlot", LLVM_SLOT_BUILTIN_OP_RELEASE_DEVICE_SLOT },
        { "SubmitDeviceRead", LLVM_SLOT_BUILTIN_OP_SUBMIT_DEVICE_READ },
        { "Write", LLVM_SLOT_BUILTIN_OP_WRITE },
    };
    const LLVMSlotBuiltinSpec *match;

    if (callee_name == NULL)
        return LLVM_SLOT_BUILTIN_OP_NONE;

    match = (const LLVMSlotBuiltinSpec *)bsearch(&callee_name,
        kLLVMSlotBuiltinSpecs,
        sizeof(kLLVMSlotBuiltinSpecs) / sizeof(kLLVMSlotBuiltinSpecs[0]),
        sizeof(kLLVMSlotBuiltinSpecs[0]), llvm_slot_builtin_spec_compare);
    return match != NULL ? match->op : LLVM_SLOT_BUILTIN_OP_NONE;
}

static bool
llvm_slot_builtin_require_argc(ASTNode *node, LLVMGenCtx *ctx,
                               const char *callee_name,
                               size_t actual, size_t required,
                               LLVMValueRef *out)
{
    if (actual >= required)
        return true;
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM %s requires at least %zu argument(s)",
        callee_name != NULL ? callee_name : "slot operation",
        required);
    if (out != NULL)
        *out = NULL;
    return false;
}

static bool
llvm_slot_builtin_error_out(ASTNode *node, LLVMGenCtx *ctx,
                            const char *message, LLVMValueRef *out)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM slot builtin could not be lowered");
    }
    if (out != NULL)
        *out = NULL;
    return true;
}

static const char *
llvm_slot_runtime_expected_call_shape(MIRResourceAbiKind kind,
                                      const char *operation)
{
    bool secure = kind == MIR_RESOURCE_ABI_SECURE_SLOT;

    if (operation == NULL)
        return NULL;
    if (strcmp(operation, "Claim") == 0)
        return secure ? "token_ptr_to_container" : "returns_container";
    if (strcmp(operation, "Read") == 0)
        return secure ? "container_ptr_token_ptr_to_value"
                      : "container_ptr_to_value";
    if (strcmp(operation, "Write") == 0)
        return secure ? "container_ptr_value_token_ptr_to_void"
                      : "container_ptr_value_to_void";
    if (strcmp(operation, "Release") == 0)
        return secure ? "container_ptr_token_ptr_to_void"
                      : "container_ptr_to_void";
    if (strcmp(operation, "SubmitRead") == 0)
        return "container_ptr_to_task_handle";
    return NULL;
}

static const MIRResourceRuntimeRow *
llvm_slot_runtime_row_or_error(ASTNode *node,
                               LLVMGenCtx *ctx,
                               MIRResourceAbiKind kind,
                               const char *inner,
                               const char *operation,
                               const char *missing_message,
                               LLVMValueRef *out)
{
    const char *expected_shape =
        llvm_slot_runtime_expected_call_shape(kind, operation);
    const MIRResourceRuntimeRow *row =
        mir_abi_resource_runtime_row_by_kind(kind, inner, operation);

    if (row == NULL || row->runtime_fn == NULL || row->call_shape == NULL) {
        llvm_slot_builtin_error_out(node, ctx, missing_message, out);
        return NULL;
    }
    if (expected_shape != NULL &&
        strcmp(row->call_shape, expected_shape) != 0) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM slot builtin %s requires MIR ABI call shape %s",
            operation != NULL ? operation : "<unknown>",
            expected_shape);
        if (out != NULL)
            *out = NULL;
        return NULL;
    }
    return row;
}

bool
llvm_emit_slot_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                            const char *callee_name, LLVMValueRef *out)
{
    LLVMSlotBuiltinOp op;

    if (out == NULL)
        return false;
    *out = NULL;

    if (pgy_codegen_call_name_is_claim_slot(callee_name)
        || pgy_codegen_call_name_is_claim_secure_slot(callee_name)) {
        llvm_set_error_at_with_hints(ctx, node, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM standalone %s requires an explicitly typed binding; use 'let value: %s<T> = %s()'",
            callee_name,
            pgy_codegen_claim_slot_abi_prefix(callee_name),
            callee_name);
        return true;
    }

    if (pgy_codegen_call_name_is_claim_device_slot(callee_name)) {
        llvm_set_error_at_with_hints(ctx, node, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM standalone ClaimDeviceSlot requires an explicitly typed binding; use 'let value: DeviceSlot<T> = ClaimDeviceSlot()'");
        return true;
    }

    op = llvm_slot_builtin_lookup(callee_name);

    if (op == LLVM_SLOT_BUILTIN_OP_WRITE) {
        if (!llvm_slot_builtin_require_argc(node, ctx, callee_name,
                ast_call_arg_count(node), 2, out))
            return true;

        const char *inner = NULL;
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = ast_call_argument(node, 0);
        LLVMVarEntry slot_var;
        if (!llvm_resolve_slot_target(ctx, slot_arg, &slot_var, &inner,
                &source_name, &is_secure))
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM Write requires registered slot receiver", out);

        LLVMValueRef val = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (val == NULL)
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM Write could not lower value expression", out);

        const MIRResourceRuntimeRow *runtime_row =
            llvm_slot_runtime_row_or_error(node, ctx,
                is_secure ? MIR_RESOURCE_ABI_SECURE_SLOT
                          : MIR_RESOURCE_ABI_SLOT,
                inner, "Write",
                "LLVM Write requires MIR ABI runtime function row", out);
        if (runtime_row == NULL)
            return true;
        const char *runtime_fn = runtime_row->runtime_fn;
        LLVMFuncEntry *fn =
            runtime_fn != NULL ? llvm_lookup_function(ctx, runtime_fn) : NULL;
        if (fn == NULL && llvm_slot_inner_has_external_runtime_helpers(inner)) {
            llvm_required_runtime_function(ctx, node,
                is_secure ? "secure slot" : "slot", callee_name, runtime_fn);
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM Write requires registered external slot runtime function",
                out);
        }
        if (fn == NULL) {
            if (is_secure)
                llvm_emit_structural_secure_slot_write(ctx, &slot_var, val);
            else
                llvm_direct_slot_write(ctx, &slot_var, val);
            *out = llvm_void_expression_placeholder(ctx, node, callee_name);
            return true;
        }

        if (is_secure) {
            LLVMVarEntry token_var;
            if (!llvm_require_secure_token_var(ctx, node, source_name,
                    callee_name, &token_var))
                return llvm_slot_builtin_error_out(node, ctx,
                    "LLVM Write requires secure token metadata", out);
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, &slot_var),
                val,
                token_var.alloca
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        } else {
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, &slot_var),
                val
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        *out = llvm_void_expression_placeholder(ctx, node, callee_name);
        return true;
    }

    if (op == LLVM_SLOT_BUILTIN_OP_READ) {
        if (!llvm_slot_builtin_require_argc(node, ctx, callee_name,
                ast_call_arg_count(node), 1, out))
            return true;

        const char *inner = NULL;
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = ast_call_argument(node, 0);
        LLVMVarEntry slot_var;
        if (!llvm_resolve_slot_target(ctx, slot_arg, &slot_var, &inner,
                &source_name, &is_secure))
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM Read requires registered slot receiver", out);

        const MIRResourceRuntimeRow *runtime_row =
            llvm_slot_runtime_row_or_error(node, ctx,
                is_secure ? MIR_RESOURCE_ABI_SECURE_SLOT
                          : MIR_RESOURCE_ABI_SLOT,
                inner, "Read",
                "LLVM Read requires MIR ABI runtime function row", out);
        if (runtime_row == NULL)
            return true;
        const char *runtime_fn = runtime_row->runtime_fn;
        LLVMFuncEntry *fn =
            runtime_fn != NULL ? llvm_lookup_function(ctx, runtime_fn) : NULL;
        if (fn == NULL && llvm_slot_inner_has_external_runtime_helpers(inner)) {
            llvm_required_runtime_function(ctx, node,
                is_secure ? "secure slot" : "slot", callee_name, runtime_fn);
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM Read requires registered external slot runtime function",
                out);
        }
        if (fn == NULL) {
            if (is_secure)
                *out = llvm_emit_structural_secure_slot_read(ctx, &slot_var, inner);
            else
                *out = llvm_direct_slot_read(ctx, &slot_var, inner);
            return true;
        }

        if (is_secure) {
            LLVMVarEntry token_var;
            if (!llvm_require_secure_token_var(ctx, node, source_name,
                    callee_name, &token_var))
                return llvm_slot_builtin_error_out(node, ctx,
                    "LLVM Read requires secure token metadata", out);
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, &slot_var),
                token_var.alloca
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 2, llvm_tmp_name(ctx));
        } else {
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, &slot_var)
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 1, llvm_tmp_name(ctx));
        }
        return true;
    }

    if (op == LLVM_SLOT_BUILTIN_OP_RELEASE) {
        if (!llvm_slot_builtin_require_argc(node, ctx, callee_name,
                ast_call_arg_count(node), 1, out))
            return true;

        const char *inner = NULL;
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = ast_call_argument(node, 0);
        LLVMVarEntry slot_var;
        if (!llvm_resolve_slot_target(ctx, slot_arg, &slot_var, &inner,
                &source_name, &is_secure))
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM Release requires registered slot receiver", out);

        const MIRResourceRuntimeRow *runtime_row =
            llvm_slot_runtime_row_or_error(node, ctx,
                is_secure ? MIR_RESOURCE_ABI_SECURE_SLOT
                          : MIR_RESOURCE_ABI_SLOT,
                inner, "Release",
                "LLVM Release requires MIR ABI runtime function row", out);
        if (runtime_row == NULL)
            return true;
        const char *runtime_fn = runtime_row->runtime_fn;
        LLVMFuncEntry *fn =
            runtime_fn != NULL ? llvm_lookup_function(ctx, runtime_fn) : NULL;
        if (fn == NULL && llvm_slot_inner_has_external_runtime_helpers(inner)) {
            llvm_required_runtime_function(ctx, node,
                is_secure ? "secure slot" : "slot", callee_name, runtime_fn);
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM Release requires registered external slot runtime function",
                out);
        }

        if (fn == NULL) {
            if (is_secure)
                llvm_emit_structural_secure_slot_release(ctx, &slot_var);
            else
                llvm_direct_slot_release(ctx, &slot_var);
        } else if (is_secure) {
            LLVMVarEntry token_var;
            if (!llvm_require_secure_token_var(ctx, node, source_name,
                    callee_name, &token_var))
                return llvm_slot_builtin_error_out(node, ctx,
                    "LLVM Release requires secure token metadata", out);
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, &slot_var),
                token_var.alloca
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        } else {
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, &slot_var)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }

        if (source_name != NULL)
            llvm_mark_slot_released(ctx, source_name);

        *out = llvm_void_expression_placeholder(ctx, node, callee_name);
        return true;
    }

    if (op == LLVM_SLOT_BUILTIN_OP_DEVICE_WRITE) {
        if (!llvm_slot_builtin_require_argc(node, ctx, callee_name,
                ast_call_arg_count(node), 2, out))
            return true;

        ASTNode *slot_arg = ast_call_argument(node, 0);
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry slot_var;
        bool has_slot_var = false;
        const char *slot_name = ast_identifier_name(slot_arg);
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            has_slot_var = llvm_scope_lookup_snapshot(ctx, slot_name, &slot_var);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM DeviceWrite on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_name);
            return true;
        }
        if (!has_slot_var)
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM DeviceWrite requires registered DeviceSlot<T> receiver",
                out);

        const MIRResourceRuntimeRow *runtime_row =
            llvm_slot_runtime_row_or_error(node, ctx,
                MIR_RESOURCE_ABI_DEVICE_SLOT, inner, "Write",
                "LLVM DeviceWrite requires MIR ABI runtime function row", out);
        if (runtime_row == NULL)
            return true;
        const char *runtime_fn = runtime_row->runtime_fn;
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "device slot", callee_name, runtime_fn);
        LLVMValueRef val = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (fn == NULL)
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM DeviceWrite requires registered runtime function", out);
        if (val == NULL)
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM DeviceWrite could not lower value expression", out);

        LLVMValueRef args[] = { slot_var.alloca, val };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        *out = llvm_void_expression_placeholder(ctx, node, callee_name);
        return true;
    }

    if (op == LLVM_SLOT_BUILTIN_OP_DEVICE_READ
        || op == LLVM_SLOT_BUILTIN_OP_RELEASE_DEVICE_SLOT
        || op == LLVM_SLOT_BUILTIN_OP_SUBMIT_DEVICE_READ) {
        if (!llvm_slot_builtin_require_argc(node, ctx, callee_name,
                ast_call_arg_count(node), 1, out))
            return true;

        ASTNode *slot_arg = ast_call_argument(node, 0);
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry slot_var;
        bool has_slot_var = false;
        const char *slot_name = ast_identifier_name(slot_arg);
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            has_slot_var = llvm_scope_lookup_snapshot(ctx, slot_name, &slot_var);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM %s on '%s' requires a concrete DeviceSlot<T> inner type",
                callee_name, slot_name);
            return true;
        }
        if (!has_slot_var)
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM device slot operation requires registered DeviceSlot<T> receiver",
                out);

        const char *operation = NULL;
        if (op == LLVM_SLOT_BUILTIN_OP_DEVICE_READ) {
            operation = "Read";
        } else if (op == LLVM_SLOT_BUILTIN_OP_RELEASE_DEVICE_SLOT) {
            operation = "Release";
        } else {
            operation = "SubmitRead";
        }
        const MIRResourceRuntimeRow *runtime_row =
            llvm_slot_runtime_row_or_error(node, ctx,
                MIR_RESOURCE_ABI_DEVICE_SLOT, inner, operation,
                "LLVM device slot operation requires MIR ABI runtime function row",
                out);
        if (runtime_row == NULL)
            return true;
        const char *runtime_fn = runtime_row->runtime_fn;

        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "device slot", callee_name, runtime_fn);
        if (fn == NULL)
            return llvm_slot_builtin_error_out(node, ctx,
                "LLVM device slot operation requires registered runtime function",
                out);

        LLVMValueRef args[] = { slot_var.alloca };
        if (op == LLVM_SLOT_BUILTIN_OP_DEVICE_READ
            || op == LLVM_SLOT_BUILTIN_OP_SUBMIT_DEVICE_READ) {
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 1, llvm_tmp_name(ctx));
        } else {
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
            if (slot_arg->type == AST_IDENTIFIER)
                llvm_mark_device_slot_released(ctx, slot_name);
            *out = llvm_void_expression_placeholder(ctx, node, callee_name);
        }
        return true;
    }

    return false;
}

#endif
