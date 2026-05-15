#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_slot_device_calls.h"

#include <stdio.h>
#include <string.h>

#include "codegen_slot_type_policy.h"
#include "llvm_expr_identifier_slot_helpers.h"
#include "llvm_internal_api.h"
#include "parser/ast_api.h"

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
llvm_slot_format_runtime_name(char *out, size_t out_size,
                              const char *prefix, const char *inner)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || inner == NULL)
        return false;
    written = snprintf(out, out_size, "%s_%s", prefix, inner);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_slot_report_runtime_name(ASTNode *node, LLVMGenCtx *ctx,
                              const char *callee_name, LLVMValueRef *out)
{
    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM %s runtime function name is too long",
        callee_name != NULL ? callee_name : "slot operation");
    if (out != NULL)
        *out = NULL;
    return true;
}

bool
llvm_emit_slot_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                            const char *callee_name, LLVMValueRef *out)
{
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

    if (strcmp(callee_name, "Write") == 0) {
        if (!llvm_slot_builtin_require_argc(node, ctx, callee_name,
                ast_call_arg_count(node), 2, out))
            return true;

        const char *inner = NULL;
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = ast_call_argument(node, 0);
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL) {
            *out = NULL;
            return true;
        }

        LLVMValueRef val = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (val == NULL) {
            *out = NULL;
            return true;
        }

        char fn_name[64];
        if (!llvm_slot_format_runtime_name(fn_name, sizeof(fn_name),
                is_secure ? "pgy_secure_write" : "pgy_write", inner))
            return llvm_slot_report_runtime_name(node, ctx, callee_name, out);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL && llvm_slot_inner_has_external_runtime_helpers(inner)) {
            llvm_required_runtime_function(ctx, node,
                is_secure ? "secure slot" : "slot", callee_name, fn_name);
            *out = NULL;
            return true;
        }
        if (fn == NULL) {
            if (is_secure)
                llvm_emit_structural_secure_slot_write(ctx, slot_var, val);
            else
                llvm_direct_slot_write(ctx, slot_var, val);
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        if (is_secure) {
            LLVMVarEntry *token_var = llvm_require_secure_token_var(ctx, node,
                source_name, callee_name);
            if (token_var == NULL) {
                *out = NULL;
                return true;
            }
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var),
                val,
                token_var->alloca
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        } else {
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var),
                val
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "Read") == 0) {
        if (!llvm_slot_builtin_require_argc(node, ctx, callee_name,
                ast_call_arg_count(node), 1, out))
            return true;

        const char *inner = NULL;
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = ast_call_argument(node, 0);
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL) {
            *out = NULL;
            return true;
        }

        char fn_name[64];
        if (!llvm_slot_format_runtime_name(fn_name, sizeof(fn_name),
                is_secure ? "pgy_secure_read" : "pgy_read", inner))
            return llvm_slot_report_runtime_name(node, ctx, callee_name, out);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL && llvm_slot_inner_has_external_runtime_helpers(inner)) {
            llvm_required_runtime_function(ctx, node,
                is_secure ? "secure slot" : "slot", callee_name, fn_name);
            *out = NULL;
            return true;
        }
        if (fn == NULL) {
            if (is_secure)
                *out = llvm_emit_structural_secure_slot_read(ctx, slot_var, inner);
            else
                *out = llvm_direct_slot_read(ctx, slot_var, inner);
            return true;
        }

        if (is_secure) {
            LLVMVarEntry *token_var = llvm_require_secure_token_var(ctx, node,
                source_name, callee_name);
            if (token_var == NULL) {
                *out = NULL;
                return true;
            }
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var),
                token_var->alloca
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 2, llvm_tmp_name(ctx));
        } else {
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var)
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 1, llvm_tmp_name(ctx));
        }
        return true;
    }

    if (strcmp(callee_name, "Release") == 0) {
        if (!llvm_slot_builtin_require_argc(node, ctx, callee_name,
                ast_call_arg_count(node), 1, out))
            return true;

        const char *inner = NULL;
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = ast_call_argument(node, 0);
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL) {
            *out = NULL;
            return true;
        }

        char fn_name[64];
        if (!llvm_slot_format_runtime_name(fn_name, sizeof(fn_name),
                is_secure ? "pgy_secure_release" : "pgy_release", inner))
            return llvm_slot_report_runtime_name(node, ctx, callee_name, out);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL && llvm_slot_inner_has_external_runtime_helpers(inner)) {
            llvm_required_runtime_function(ctx, node,
                is_secure ? "secure slot" : "slot", callee_name, fn_name);
            *out = NULL;
            return true;
        }

        if (fn == NULL) {
            if (is_secure)
                llvm_emit_structural_secure_slot_release(ctx, slot_var);
            else
                llvm_direct_slot_release(ctx, slot_var);
        } else if (is_secure) {
            LLVMVarEntry *token_var = llvm_require_secure_token_var(ctx, node,
                source_name, callee_name);
            if (token_var == NULL) {
                *out = NULL;
                return true;
            }
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var),
                token_var->alloca
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        } else {
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }

        if (source_name != NULL) {
            const char *sname = source_name;
            for (int ri = 0; ri < ctx->slot_var_count; ri++) {
                if (strcmp(ctx->slot_vars[ri].var_name, sname) == 0) {
                    ctx->slot_vars[ri].released = true;
                    break;
                }
            }
        }

        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "DeviceWrite") == 0) {
        if (!llvm_slot_builtin_require_argc(node, ctx, callee_name,
                ast_call_arg_count(node), 2, out))
            return true;

        ASTNode *slot_arg = ast_call_argument(node, 0);
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        const char *slot_name = ast_identifier_name(slot_arg);
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM DeviceWrite on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_name);
            return true;
        }
        if (slot_var == NULL) {
            *out = NULL;
            return true;
        }

        char fn_name[64];
        if (!llvm_slot_format_runtime_name(fn_name, sizeof(fn_name),
                "pgy_device_write", inner))
            return llvm_slot_report_runtime_name(node, ctx, callee_name, out);
        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "device slot", callee_name, fn_name);
        LLVMValueRef val = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (fn == NULL || val == NULL) {
            *out = NULL;
            return true;
        }

        LLVMValueRef args[] = { slot_var->alloca, val };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "DeviceRead") == 0
        || strcmp(callee_name, "ReleaseDeviceSlot") == 0
        || strcmp(callee_name, "SubmitDeviceRead") == 0) {
        if (!llvm_slot_builtin_require_argc(node, ctx, callee_name,
                ast_call_arg_count(node), 1, out))
            return true;

        ASTNode *slot_arg = ast_call_argument(node, 0);
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        const char *slot_name = ast_identifier_name(slot_arg);
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM %s on '%s' requires a concrete DeviceSlot<T> inner type",
                callee_name, slot_name);
            return true;
        }
        if (slot_var == NULL) {
            *out = NULL;
            return true;
        }

        char fn_name[64];
        if (strcmp(callee_name, "DeviceRead") == 0) {
            if (!llvm_slot_format_runtime_name(fn_name, sizeof(fn_name),
                    "pgy_device_read", inner))
                return llvm_slot_report_runtime_name(node, ctx, callee_name, out);
        } else if (strcmp(callee_name, "ReleaseDeviceSlot") == 0) {
            if (!llvm_slot_format_runtime_name(fn_name, sizeof(fn_name),
                    "pgy_release_device", inner))
                return llvm_slot_report_runtime_name(node, ctx, callee_name, out);
        } else {
            if (!llvm_slot_format_runtime_name(fn_name, sizeof(fn_name),
                    "pgy_submit_device_read", inner))
                return llvm_slot_report_runtime_name(node, ctx, callee_name, out);
        }

        LLVMFuncEntry *fn = llvm_required_runtime_function(ctx, node,
            "device slot", callee_name, fn_name);
        if (fn == NULL) {
            *out = NULL;
            return true;
        }

        LLVMValueRef args[] = { slot_var->alloca };
        if (strcmp(callee_name, "DeviceRead") == 0
            || strcmp(callee_name, "SubmitDeviceRead") == 0) {
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 1, llvm_tmp_name(ctx));
        } else {
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
            if (slot_arg->type == AST_IDENTIFIER)
                llvm_mark_device_slot_released(ctx, slot_name);
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
        }
        return true;
    }

    return false;
}

#endif
