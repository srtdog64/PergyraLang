/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend stdlib call lowering owner.
 */

#include "transpiler_expr_stdlib_builtin.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_abi_layout.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "codegen_type_mapping.h"
#include "transpiler_expr_stdlib_builtin_policy.h"
#include "transpiler_expr_stdlib_channel_builtin.h"
#include "transpiler_expr_stdlib_collection_builtin.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_expr_stdlib_misc_builtin.h"
#include "transpiler_expr_stdlib_scalar_builtin.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_inventory_view.h"
#include "codegen_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

static bool
transpiler_stdlib_copy_type_name(char *out, size_t out_size,
                                 const char *type_name)
{
    size_t len;

    if (out == NULL || out_size == 0 || type_name == NULL)
        return false;

    len = strlen(type_name);
    if (len >= out_size)
        return false;

    memcpy(out, type_name, len + 1);
    return true;
}

static const char *
transpiler_stdlib_slot_runtime_fn(TranspilerCtx *ctx,
                                  const char *inner_type,
                                  const char *operation)
{
    const char *runtime_fn = mir_abi_resource_runtime_fn_by_kind(
        MIR_RESOURCE_ABI_SLOT, inner_type, operation);
    if (runtime_fn != NULL)
        return runtime_fn;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "C stdlib Slot<T> Clone %s requires MIR ABI runtime function row",
        operation != NULL ? operation : "<unknown>");
    return NULL;
}

static bool
transpiler_resolve_unary_constructed_inner(TranspilerCtx *ctx,
                                           const char *type_name,
                                           const char *family,
                                           char *inner_buf,
                                           size_t inner_buf_size,
                                           const char **inner_out)
{
    const char *resolved_type = type_name;
    char resolved_buf[128];
    size_t family_len = family != NULL ? strlen(family) : 0;

    if (resolved_type != NULL
        && !(family_len > 0
             && strncmp(resolved_type, family, family_len) == 0
             && resolved_type[family_len] == '<')) {
        const char *target_type_name =
            transpiler_type_alias_target_type_name_from_headers(
                ctx, resolved_type);
        if (target_type_name != NULL) {
            bool copied = transpiler_stdlib_copy_type_name(
                resolved_buf, sizeof(resolved_buf), target_type_name);
            if (!copied) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: resolved %s type is too long",
                    family != NULL ? family : "constructed");
                return false;
            }
            resolved_type = resolved_buf;
        }
    }

    if (resolved_type != NULL
        && family_len > 0
        && strncmp(resolved_type, family, family_len) == 0
        && resolved_type[family_len] == '<') {
        if (slot_inner_type_name_copy(resolved_type, inner_buf,
                inner_buf_size)
            && inner_buf[0] != '\0'
            && strcmp(inner_buf, "Unknown") != 0) {
            if (inner_out != NULL)
                *inner_out = inner_buf;
            return true;
        }
    }
    return false;
}

/* Runtime array kernels are named by the SANITIZED element suffix
 * (Array<Int> element -> pgy_array_push_Array_Int), matching the literal and
 * index emit paths. The resolver hands back the raw inner type name, so a
 * nested-array element would otherwise leak literal angle brackets into the
 * callee name (pgy_array_push_Array<Int> -- invalid C). Scalar inners are
 * unchanged (sanitize is the identity on a bare name). */
static void
transpiler_array_inner_sanitize_suffix(char *inner_buf,
                                       size_t inner_buf_size,
                                       const char **inner_out)
{
    char sanitized[128];

    if (inner_out == NULL || *inner_out == NULL || inner_buf == NULL
        || inner_buf_size == 0)
        return;
    sanitize_c_suffix(*inner_out, sanitized, sizeof(sanitized));
    snprintf(inner_buf, inner_buf_size, "%s", sanitized);
    *inner_out = inner_buf;
}

static bool
transpiler_require_array_inner_type(TranspilerCtx *ctx, ASTNode *expr,
                                    const char *operation,
                                    bool allow_slice,
                                    char *inner_buf,
                                    size_t inner_buf_size,
                                    const char **inner_out)
{
    const char *type_name = transpiler_expr_infer_type_name(ctx, expr);
    if (transpiler_resolve_unary_constructed_inner(ctx, type_name, "Array",
            inner_buf, inner_buf_size, inner_out)) {
        transpiler_array_inner_sanitize_suffix(inner_buf, inner_buf_size,
                                               inner_out);
        return true;
    }
    if (allow_slice
        && transpiler_resolve_unary_constructed_inner(ctx, type_name, "Slice",
            inner_buf, inner_buf_size, inner_out)) {
        transpiler_array_inner_sanitize_suffix(inner_buf, inner_buf_size,
                                               inner_out);
        return true;
    }

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "C backend: %s requires concrete %s metadata",
        operation != NULL ? operation : "Array operation",
        allow_slice ? "Array<T> or Slice<T>" : "Array<T>");
    return false;
}

static char *
transpiler_stdlib_emit_arg(TranspilerCtx *ctx,
                           ASTNode *arg,
                           const char *builtin_name,
                           const char *role)
{
    char *rendered = emit_expression(arg, ctx);

    if (rendered != NULL)
        return rendered;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: stdlib builtin %s could not lower %s argument",
        builtin_name != NULL ? builtin_name : "(unknown)",
        role != NULL ? role : "operand");
    return NULL;
}

char *
emit_call_stdlib_builtin(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = ast_identifier_name(callee);
        size_t argc = ast_call_arg_count(call);
        TranspilerArrayStdlibOp array_op;
        TranspilerStdlibOp stdlib_op;
        ASTNode *arg0 = ast_call_argument(call, 0);
        ASTNode *arg1 = ast_call_argument(call, 1);
        ASTNode *arg2 = ast_call_argument(call, 2);
        char *scalar_builtin = emit_call_stdlib_scalar_builtin(fn, call, ctx);
        if (scalar_builtin != NULL)
            return scalar_builtin;

        array_op = transpiler_array_lookup(fn, argc);
        if (array_op == TRANSPILER_ARRAY_OP_LENGTH) {
            char *arg = transpiler_stdlib_emit_arg(ctx, arg0,
                "ArrayLength", "array");
            if (arg == NULL)
                return NULL;
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    arg0, "ArrayLength", true,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arg);
                return NULL;
            }
            char *result = strdup_fmt("((int32_t)(%s.length))", arg);
            free(arg);
            return result;
        }
        if (array_op == TRANSPILER_ARRAY_OP_SLICE_COPY) {
            char *slice = transpiler_stdlib_emit_arg(ctx, arg0,
                "SliceCopy", "slice");
            if (slice == NULL)
                return NULL;
            const char *slice_type = transpiler_expr_infer_type_name(ctx, arg0);
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_resolve_unary_constructed_inner(ctx,
                    slice_type, "Slice",
                    inner_buf, sizeof(inner_buf), &inner)) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: SliceCopy requires concrete Slice<T> metadata");
                free(slice);
                return NULL;
            }
            int tmp_id = ++ctx->tmp_counter;
            char *result = strdup_fmt(
                "({ PgySlice_%s _pgy_slice_copy_%d = %s; "
                "pgy_slice_copy_%s(&_pgy_slice_copy_%d); })",
                inner, tmp_id, slice,
                inner, tmp_id);
            free(slice);
            return result;
        }
        if (array_op == TRANSPILER_ARRAY_OP_PUSH) {
            if (!transpiler_require_c_addressable_storage(ctx, arg0,
                    "ArrayPush", "Array"))
                return NULL;
            char *arr = transpiler_stdlib_emit_arg(ctx, arg0,
                "ArrayPush", "array");
            char *val = arr != NULL
                ? transpiler_stdlib_emit_arg(ctx, arg1, "ArrayPush", "value")
                : NULL;
            if (arr == NULL || val == NULL) {
                free(arr);
                free(val);
                return NULL;
            }
            const char *suffix = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    arg0, "ArrayPush", false,
                    inner_buf, sizeof(inner_buf), &suffix)) {
                free(arr);
                free(val);
                return NULL;
            }
            char *result = strdup_fmt(
                "pgy_array_push_%s(&%s, %s)", suffix, arr, val);
            free(arr); free(val);
            return result;
        }
        if (array_op == TRANSPILER_ARRAY_OP_SET) {
            if (!transpiler_require_c_addressable_storage(ctx, arg0,
                    "ArraySet", "Array"))
                return NULL;
            char *arr = transpiler_stdlib_emit_arg(ctx, arg0,
                "ArraySet", "array");
            char *idx = arr != NULL
                ? transpiler_stdlib_emit_arg(ctx, arg1, "ArraySet", "index")
                : NULL;
            char *val = idx != NULL
                ? transpiler_stdlib_emit_arg(ctx, arg2, "ArraySet", "value")
                : NULL;
            if (arr == NULL || idx == NULL || val == NULL) {
                free(arr);
                free(idx);
                free(val);
                return NULL;
            }
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    arg0, "ArraySet", false,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arr); free(idx); free(val);
                return NULL;
            }
            char *result = strdup_fmt(
                "pgy_array_set_%s(&%s, %s, %s)", inner, arr, idx, val);
            free(arr); free(idx); free(val);
            return result;
        }
        if (array_op == TRANSPILER_ARRAY_OP_POP) {
            if (!transpiler_require_c_addressable_storage(ctx, arg0,
                    "ArrayPop", "Array"))
                return NULL;
            char *arr = transpiler_stdlib_emit_arg(ctx, arg0,
                "ArrayPop", "array");
            if (arr == NULL)
                return NULL;
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    arg0, "ArrayPop", false,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arr);
                return NULL;
            }
            char *result = strdup_fmt("pgy_array_pop_%s(&%s)", inner, arr);
            free(arr);
            return result;
        }
        if (array_op == TRANSPILER_ARRAY_OP_SORT) {
            char *arr = transpiler_stdlib_emit_arg(ctx, arg0,
                "ArraySort", "array");
            if (arr == NULL)
                return NULL;
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    arg0, "ArraySort", false,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arr);
                return NULL;
            }
            char *result = strdup_fmt(
                "({ pgy_array_sort_%s((%s).data, (%s).length); %s; })",
                inner, arr, arr, arr);
            free(arr);
            return result;
        }
        if (array_op == TRANSPILER_ARRAY_OP_MAP) {
            char *arr = transpiler_stdlib_emit_arg(ctx, arg0,
                "ArrayMap", "array");
            char *fn_arg = arr != NULL
                ? transpiler_stdlib_emit_arg(ctx, arg1, "ArrayMap",
                      "function")
                : NULL;
            if (arr == NULL || fn_arg == NULL) {
                free(arr);
                free(fn_arg);
                return NULL;
            }
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    arg0, "ArrayMap", false,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arr); free(fn_arg);
                return NULL;
            }
            int tmp_id = ++ctx->tmp_counter;
            char *result = strdup_fmt(
                "({ PgyArray_%s _pgy_map_%d = pgy_array_new_%s((%s).length); "
                "for (size_t _mi = 0; _mi < (%s).length; _mi++) "
                "pgy_array_push_%s(&_pgy_map_%d, %s((%s).data[_mi])); "
                "_pgy_map_%d; })",
                inner, tmp_id, inner, arr,
                arr,
                inner, tmp_id, fn_arg, arr,
                tmp_id);
            free(arr); free(fn_arg);
            return result;
        }
        if (array_op == TRANSPILER_ARRAY_OP_FILTER) {
            char *arr = transpiler_stdlib_emit_arg(ctx, arg0,
                "ArrayFilter", "array");
            char *fn_arg = arr != NULL
                ? transpiler_stdlib_emit_arg(ctx, arg1, "ArrayFilter",
                      "function")
                : NULL;
            if (arr == NULL || fn_arg == NULL) {
                free(arr);
                free(fn_arg);
                return NULL;
            }
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    arg0, "ArrayFilter", false,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arr); free(fn_arg);
                return NULL;
            }
            int tmp_id = ++ctx->tmp_counter;
            char *result = strdup_fmt(
                "({ PgyArray_%s _pgy_filt_%d = pgy_array_new_%s((%s).length); "
                "for (size_t _fi = 0; _fi < (%s).length; _fi++) "
                "if (%s((%s).data[_fi])) "
                "pgy_array_push_%s(&_pgy_filt_%d, (%s).data[_fi]); "
                "_pgy_filt_%d; })",
                inner, tmp_id, inner, arr,
                arr,
                fn_arg, arr,
                inner, tmp_id, arr,
                tmp_id);
            free(arr); free(fn_arg);
            return result;
        }
        if (array_op == TRANSPILER_ARRAY_OP_REVERSE) {
            char *arr = transpiler_stdlib_emit_arg(ctx, arg0,
                "ArrayReverse", "array");
            if (arr == NULL)
                return NULL;
            const char *arr_type = transpiler_expr_infer_type_name(ctx, arg0);
            const char *inner = NULL;
            const char *c_type = NULL;
            char inner_buf[128];
            char c_type_buf[128];
            if (transpiler_type_name_is_array(arr_type)) {
                if (slot_inner_type_name_copy(arr_type, inner_buf,
                        sizeof(inner_buf)))
                    inner = inner_buf;
                if (transpiler_require_type_name_c_type_copy(ctx, inner,
                        "ArrayReverse element", c_type_buf,
                        sizeof(c_type_buf))) {
                    c_type = c_type_buf;
                }
            }
            if (inner == NULL || c_type == NULL) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "cannot determine element type for ArrayReverse; "
                "explicit concrete Array<T> input is required");
                free(arr);
                return NULL;
            }
            char *result = strdup_fmt(
                "({ for (size_t _ri = 0; _ri < (%s).length / 2; _ri++) { "
                "%s _tmp = (%s).data[_ri]; "
                "(%s).data[_ri] = (%s).data[(%s).length - 1 - _ri]; "
                "(%s).data[(%s).length - 1 - _ri] = _tmp; } %s; })",
                arr, c_type, arr, arr, arr, arr, arr, arr, arr);
            free(arr);
            return result;
        }
        char *channel_builtin = emit_call_stdlib_channel_builtin(fn, call, ctx);
        if (channel_builtin != NULL)
            return channel_builtin;
        stdlib_op = transpiler_stdlib_lookup(fn, argc);
        if (stdlib_op == TRANSPILER_STDLIB_OP_CLONE) {
            char *src = transpiler_stdlib_emit_arg(ctx, arg0, "Clone",
                "value");
            if (src == NULL)
                return NULL;
            const char *tn = transpiler_expr_infer_type_name(ctx, arg0);
            if (tn != NULL && strncmp(tn, "Slot<", 5) == 0) {
                char inner_buf[128];
                const char *inner = NULL;
                const char *claim_fn;
                const char *read_fn;
                const char *write_fn;
                if (slot_inner_type_name_copy(tn, inner_buf, sizeof(inner_buf)))
                    inner = inner_buf;
                if (inner == NULL || inner[0] == '\0') {
                    free(src);
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C backend: Clone requires concrete Slot<T> metadata");
                    return NULL;
                }
                claim_fn = transpiler_stdlib_slot_runtime_fn(ctx, inner, "Claim");
                read_fn = transpiler_stdlib_slot_runtime_fn(ctx, inner, "Read");
                write_fn = transpiler_stdlib_slot_runtime_fn(ctx, inner, "Write");
                if (claim_fn == NULL || read_fn == NULL || write_fn == NULL) {
                    free(src);
                    return NULL;
                }
                char *result = strdup_fmt(
                    "({ PgySlot_%s _c = %s(); "
                    "%s(&_c, %s(&%s)); _c; })",
                    inner, claim_fn, write_fn, read_fn, src);
                free(src);
                return result;
            }
            return src;
        }
        if (stdlib_op == TRANSPILER_STDLIB_OP_PRINT) {
            char *arg = transpiler_stdlib_emit_arg(ctx, arg0, "Print",
                "value");
            if (arg == NULL)
                return NULL;
            char *result = strdup_fmt("printf(\"%%s\", %s)", arg);
            free(arg);
            return result;
        }
        if (stdlib_op == TRANSPILER_STDLIB_OP_TO_STRING) {
            char *arg = transpiler_stdlib_emit_arg(ctx, arg0, "ToString",
                "value");
            if (arg == NULL)
                return NULL;
            const char *arg_type = transpiler_expr_infer_type_name(ctx, arg0);
            TranspilerToStringKind to_string_kind =
                transpiler_to_string_kind(arg_type);
            char *result = NULL;
            if (to_string_kind == TRANSPILER_TO_STRING_KIND_STRING) {
                result = pergyra_strdup(arg);
            } else if (to_string_kind == TRANSPILER_TO_STRING_KIND_BOOL) {
                result = strdup_fmt("pgy_bool_to_string(%s)", arg);
            } else if (to_string_kind == TRANSPILER_TO_STRING_KIND_FLOAT) {
                result = strdup_fmt("pgy_float_to_string(%s)", arg);
            } else if (to_string_kind == TRANSPILER_TO_STRING_KIND_DOUBLE) {
                result = strdup_fmt("pgy_double_to_string(%s)", arg);
            } else if (to_string_kind == TRANSPILER_TO_STRING_KIND_LONG) {
                result = strdup_fmt("pgy_long_to_string(%s)", arg);
            } else {
                result = strdup_fmt("pgy_int_to_string(%s)", arg);
            }
            free(arg);
            return result;
        }
        char *collection_builtin =
            emit_call_stdlib_collection_builtin(fn, call, ctx);
        if (collection_builtin != NULL)
            return collection_builtin;
        char *misc_builtin = emit_call_stdlib_misc_builtin(fn, call, ctx);
        if (misc_builtin != NULL)
            return misc_builtin;
    }

    return NULL;
}
