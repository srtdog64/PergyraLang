/* C backend stdlib call lowering owner. Included inside transpiler.c after expression emitter prerequisites. */
#include "transpiler_expr_stdlib_scalar_builtin.h"
#include "transpiler_expr_stdlib_collection_builtin.h"
#include "transpiler_expr_stdlib_misc_builtin.h"

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
        ASTNode *alias_decl = transpiler_find_type_alias_decl(ctx, resolved_type);
        if (alias_decl != NULL && alias_decl->data.type_alias.target_type != NULL) {
            ASTNode *target = resolve_type_alias_target(
                ctx, alias_decl->data.type_alias.target_type);
            char *rendered = render_type_name(target);
            if (rendered != NULL) {
                snprintf(resolved_buf, sizeof(resolved_buf), "%s", rendered);
                free(rendered);
                resolved_type = resolved_buf;
            }
        }
    }

    if (resolved_type != NULL
        && family_len > 0
        && strncmp(resolved_type, family, family_len) == 0
        && resolved_type[family_len] == '<') {
        const char *inner = slot_inner_type_name(resolved_type);
        if (inner != NULL && inner[0] != '\0') {
            snprintf(inner_buf, inner_buf_size, "%s", inner);
            if (inner_out != NULL)
                *inner_out = inner_buf;
            return true;
        }
    }
    return false;
}

static bool
transpiler_require_array_inner_type(TranspilerCtx *ctx, ASTNode *expr,
                                    const char *operation,
                                    bool allow_slice,
                                    char *inner_buf,
                                    size_t inner_buf_size,
                                    const char **inner_out)
{
    const char *type_name = infer_expression_type_name(ctx, expr);
    if (transpiler_resolve_unary_constructed_inner(ctx, type_name, "Array",
            inner_buf, inner_buf_size, inner_out)) {
        return true;
    }
    if (allow_slice
        && transpiler_resolve_unary_constructed_inner(ctx, type_name, "Slice",
            inner_buf, inner_buf_size, inner_out)) {
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

static const char *
transpiler_require_channel_inner_type(TranspilerCtx *ctx, ASTNode *expr,
                                      const char *operation,
                                      char *inner_buf,
                                      size_t inner_buf_size)
{
    const char *inner = NULL;
    const char *type_name = infer_expression_type_name(ctx, expr);
    if (transpiler_resolve_unary_constructed_inner(ctx, type_name, "Channel",
            inner_buf, inner_buf_size, &inner)) {
        return inner;
    }
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "C backend: %s requires concrete Channel<T> metadata",
        operation != NULL ? operation : "Channel operation");
    return NULL;
}

#include "transpiler_expr_stdlib_channel_builtin.h"

static char *
emit_call_stdlib_builtin(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    /* Standard library built-in functions */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;
        char *scalar_builtin = emit_call_stdlib_scalar_builtin(fn, call, ctx);
        if (scalar_builtin != NULL)
            return scalar_builtin;

        if (strcmp(fn, "ArrayLength") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    call->data.call.arguments[0], "ArrayLength", true,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arg);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt("((int32_t)(%s.length))", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ArrayPush") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *suffix = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    call->data.call.arguments[0], "ArrayPush", false,
                    inner_buf, sizeof(inner_buf), &suffix)) {
                free(arr);
                free(val);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_array_push_%s(&%s, %s)", suffix, arr, val);
            free(arr); free(val);
            return result;
        }
        if (strcmp(fn, "ArraySet") == 0 && call->data.call.arg_count == 3) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *idx = emit_expression(call->data.call.arguments[1], ctx);
            char *val = emit_expression(call->data.call.arguments[2], ctx);
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    call->data.call.arguments[0], "ArraySet", false,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arr); free(idx); free(val);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_array_set_%s(&%s, %s, %s)", inner, arr, idx, val);
            free(arr); free(idx); free(val);
            return result;
        }
        if (strcmp(fn, "ArrayPop") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    call->data.call.arguments[0], "ArrayPop", false,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arr);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt("pgy_array_pop_%s(&%s)", inner, arr);
            free(arr);
            return result;
        }
        /* ArraySort ??hybrid sort using AlphaDev kernels for small arrays */
        if (strcmp(fn, "ArraySort") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    call->data.call.arguments[0], "ArraySort", false,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arr);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "({ pgy_array_sort_%s((%s).data, (%s).length); %s; })",
                inner, arr, arr, arr);
            free(arr);
            return result;
        }
        /* ArrayMap ??apply function to each element, return new array */
        if (strcmp(fn, "ArrayMap") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *fn_arg = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    call->data.call.arguments[0], "ArrayMap", false,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arr); free(fn_arg);
                return pergyra_strdup("0");
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
        /* ArrayFilter ??keep elements where predicate returns true */
        if (strcmp(fn, "ArrayFilter") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *fn_arg = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = NULL;
            char inner_buf[64];
            if (!transpiler_require_array_inner_type(ctx,
                    call->data.call.arguments[0], "ArrayFilter", false,
                    inner_buf, sizeof(inner_buf), &inner)) {
                free(arr); free(fn_arg);
                return pergyra_strdup("0");
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
        /* ArrayReverse ??in-place reverse */
        if (strcmp(fn, "ArrayReverse") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = NULL;
            const char *c_type = NULL;
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0) {
                inner = slot_inner_type_name(arr_type);
                c_type = pergyra_type_to_c(inner);
            }
            if (inner == NULL || c_type == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine element type for ArrayReverse; explicit concrete Array<T> input is required");
                free(arr);
                return pergyra_strdup("0");
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
        /* Clone: explicit copy of Slot */
        if (strcmp(fn, "Clone") == 0 && call->data.call.arg_count == 1) {
            char *src = emit_expression(call->data.call.arguments[0], ctx);
            const char *tn = infer_expression_type_name(
                ctx, call->data.call.arguments[0]);
            if (tn != NULL && strncmp(tn, "Slot<", 5) == 0) {
                const char *inner = slot_inner_type_name(tn);
                char *result = strdup_fmt(
                    "({ PgySlot_%s _c = pgy_claim_%s(); "
                    "pgy_write_%s(&_c, pgy_read_%s(&%s)); _c; })",
                    inner, inner, inner, inner, src);
                free(src);
                return result;
            }
            return src;
        }
        /* Print (no newline) vs Log (with newline) */
        if (strcmp(fn, "Print") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("printf(\"%%s\", %s)", arg);
            free(arg);
            return result;
        }
        /* ToString */
        if (strcmp(fn, "ToString") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            const char *arg_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = NULL;
            if (arg_type != NULL && strcmp(arg_type, "String") == 0) {
                result = pergyra_strdup(arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Bool") == 0) {
                result = strdup_fmt("pgy_bool_to_string(%s)", arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Float") == 0) {
                result = strdup_fmt("pgy_float_to_string(%s)", arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Double") == 0) {
                result = strdup_fmt("pgy_float_to_string((float)(%s))", arg);
            } else if (arg_type != NULL && strcmp(arg_type, "Long") == 0) {
                result = strdup_fmt("pgy_int_to_string((int32_t)(%s))", arg);
            } else {
                result = strdup_fmt("pgy_int_to_string(%s)", arg);
            }
            free(arg);
            return result;
        }
        char *collection_builtin = emit_call_stdlib_collection_builtin(fn, call, ctx);
        if (collection_builtin != NULL)
            return collection_builtin;
        char *misc_builtin = emit_call_stdlib_misc_builtin(fn, call, ctx);
        if (misc_builtin != NULL)
            return misc_builtin;
    }

    return NULL;
}
