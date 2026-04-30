/* C backend stdlib call lowering owner. Included inside transpiler.c after expression emitter prerequisites. */
#include "transpiler_expr_stdlib_scalar_builtin.h"
#include "transpiler_expr_stdlib_collection_builtin.h"

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
            char *result = strdup_fmt("((%s).length > 0 ? (%s).length-- : 0)",
                arr, arr);
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
        /* Channel builtins */
        if (strcmp(fn, "TryRecv") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "TryRecv",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                return pergyra_strdup("0");
            }
            const char *c_inner = pergyra_type_to_c(inner);
            char *result = strdup_fmt(
                "({ %s _pgy_recv_tmp; "
                "pgy_channel_try_recv_%s(&%s, &_pgy_recv_tmp) "
                "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
                c_inner, inner, ch, inner, inner);
            free(ch);
            return result;
        }
        if (strcmp(fn, "RecvTimeout") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *timeout = emit_expression(call->data.call.arguments[1], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "RecvTimeout",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                free(timeout);
                return pergyra_strdup("0");
            }
            const char *c_inner = pergyra_type_to_c(inner);
            char *result = strdup_fmt(
                "({ %s _pgy_recv_tmp; "
                "pgy_channel_recv_timeout_%s(&%s, &_pgy_recv_tmp, (uint64_t)(%s)) "
                "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
                c_inner, inner, ch, timeout, inner, inner);
            free(ch);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "TrySend") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "TrySend",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                free(val);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_try_send_%s(&%s, %s)", inner, ch, val);
            free(ch);
            free(val);
            return result;
        }
        if (strcmp(fn, "TrySendStatus") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "TrySendStatus",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                free(val);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_try_send_status_%s(&%s, %s)", inner, ch, val);
            free(ch);
            free(val);
            return result;
        }
        if (strcmp(fn, "SendTimeout") == 0 && call->data.call.arg_count == 3) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            char *timeout = emit_expression(call->data.call.arguments[2], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "SendTimeout",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                free(val);
                free(timeout);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_send_timeout_%s(&%s, %s, (uint64_t)(%s))",
                inner, ch, val, timeout);
            free(ch);
            free(val);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "SendTimeoutStatus") == 0 && call->data.call.arg_count == 3) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            char *timeout = emit_expression(call->data.call.arguments[2], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "SendTimeoutStatus",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                free(val);
                free(timeout);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_send_timeout_status_%s(&%s, %s, (uint64_t)(%s))",
                inner, ch, val, timeout);
            free(ch);
            free(val);
            free(timeout);
            return result;
        }
        if (strcmp(fn, "Cancel") == 0 && call->data.call.arg_count == 1) {
            char *task = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("pgy_task_cancel(%s)", task);
            free(task);
            return result;
        }
        if (strcmp(fn, "IsCancelled") == 0 && call->data.call.arg_count == 0) {
            return strdup_fmt("pgy_task_is_cancelled()");
        }
        if (strcmp(fn, "ChannelClose") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "ChannelClose",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_close_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelReady") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "ChannelReady",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_ready_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelLength") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "ChannelLength",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_length_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelCapacity") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "ChannelCapacity",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_capacity_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelSpace") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "ChannelSpace",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_space_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelFull") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "ChannelFull",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_full_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelClosed") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char inner_buf[64];
            const char *inner = transpiler_require_channel_inner_type(ctx,
                call->data.call.arguments[0], "ChannelClosed",
                inner_buf, sizeof(inner_buf));
            if (inner == NULL) {
                free(ch);
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt(
                "pgy_channel_closed_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
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

        /* FSM builtins */
        if (strcmp(fn, "FsmNew") == 0)
            return pergyra_strdup("pgy_fsm_new()");
        if (strcmp(fn, "FsmAddState") == 0 && call->data.call.arg_count == 2) {
            char *f = emit_expression(call->data.call.arguments[0], ctx);
            char *n = emit_expression(call->data.call.arguments[1], ctx);
            char *r = strdup_fmt("pgy_fsm_add_state(&%s, %s)", f, n);
            free(f); free(n); return r;
        }
        if (strcmp(fn, "FsmTransition") == 0 && call->data.call.arg_count == 4) {
            char *f = emit_expression(call->data.call.arguments[0], ctx);
            char *from = emit_expression(call->data.call.arguments[1], ctx);
            char *inp = emit_expression(call->data.call.arguments[2], ctx);
            char *to = emit_expression(call->data.call.arguments[3], ctx);
            char *r = strdup_fmt("pgy_fsm_add_transition(&%s, %s, %s, %s)", f, from, inp, to);
            free(f); free(from); free(inp); free(to); return r;
        }
        if (strcmp(fn, "FsmStep") == 0 && call->data.call.arg_count == 2) {
            char *f = emit_expression(call->data.call.arguments[0], ctx);
            char *i = emit_expression(call->data.call.arguments[1], ctx);
            char *r = strdup_fmt("pgy_fsm_step(&%s, %s)", f, i);
            free(f); free(i); return r;
        }
        if (strcmp(fn, "FsmCurrent") == 0 && call->data.call.arg_count == 1) {
            char *f = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_fsm_current(&%s)", f);
            free(f); return r;
        }
        if (strcmp(fn, "FsmCurrentName") == 0 && call->data.call.arg_count == 1) {
            char *f = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_fsm_current_name(&%s)", f);
            free(f); return r;
        }
        /* Timer builtins */
        if (strcmp(fn, "TimerNew") == 0 && call->data.call.arg_count == 1) {
            char *d = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_timer_new(%s)", d);
            free(d); return r;
        }
        if (strcmp(fn, "TimerTick") == 0 && call->data.call.arg_count == 2) {
            char *t = emit_expression(call->data.call.arguments[0], ctx);
            char *d = emit_expression(call->data.call.arguments[1], ctx);
            char *r = strdup_fmt("pgy_timer_tick(&%s, %s)", t, d);
            free(t); free(d); return r;
        }
        if (strcmp(fn, "TimerRemaining") == 0 && call->data.call.arg_count == 1) {
            char *t = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_timer_remaining(&%s)", t);
            free(t); return r;
        }
        if (strcmp(fn, "TimerDone") == 0 && call->data.call.arg_count == 1) {
            char *t = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_timer_done(&%s)", t);
            free(t); return r;
        }
        if (strcmp(fn, "TimerReset") == 0 && call->data.call.arg_count == 1) {
            char *t = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_timer_reset(&%s)", t);
            free(t); return r;
        }
        if (strcmp(fn, "CooldownNew") == 0 && call->data.call.arg_count == 1) {
            char *c = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_cooldown_new(%s)", c);
            free(c); return r;
        }
        if (strcmp(fn, "CooldownTick") == 0 && call->data.call.arg_count == 2) {
            char *c = emit_expression(call->data.call.arguments[0], ctx);
            char *d = emit_expression(call->data.call.arguments[1], ctx);
            char *r = strdup_fmt("pgy_cooldown_tick(&%s, %s)", c, d);
            free(c); free(d); return r;
        }
        if (strcmp(fn, "CooldownReady") == 0 && call->data.call.arg_count == 1) {
            char *c = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_cooldown_ready(&%s)", c);
            free(c); return r;
        }
        if (strcmp(fn, "CooldownTrigger") == 0 && call->data.call.arg_count == 1) {
            char *c = emit_expression(call->data.call.arguments[0], ctx);
            char *r = strdup_fmt("pgy_cooldown_trigger(&%s)", c);
            free(c); return r;
        }
        if (strcmp(fn, "MapSetStr") == 0 && call->data.call.arg_count == 3) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            char *v = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("pgy_map_set_string(&%s, %s, %s)", m, k, v);
            free(m); free(k); free(v);
            return result;
        }
        if (strcmp(fn, "MapGetStr") == 0 && call->data.call.arg_count == 2) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("pgy_map_get_string(&%s, %s)", m, k);
            free(m); free(k);
            return result;
        }
    }

    return NULL;
}
