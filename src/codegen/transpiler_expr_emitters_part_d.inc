static char *
emit_call_stdlib_builtin(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    /* Standard library built-in functions */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = callee->data.identifier.name;

        /* Math functions */
        if (strcmp(fn, "Abs") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((%s) < 0 ? -(%s) : (%s))", arg, arg, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Min") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("((%s) < (%s) ? (%s) : (%s))", a, b, a, b);
            free(a); free(b);
            return result;
        }
        if (strcmp(fn, "Max") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("((%s) > (%s) ? (%s) : (%s))", a, b, a, b);
            free(a); free(b);
            return result;
        }
        /* String functions */
        if (strcmp(fn, "StringLength") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((int32_t)strlen(%s))", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Contains") == 0 || strcmp(fn, "StringContains") == 0)
            && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringContains(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if ((strcmp(fn, "Replace") == 0 || strcmp(fn, "StringReplace") == 0)
            && call->data.call.arg_count == 3) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *old_s = emit_expression(call->data.call.arguments[1], ctx);
            char *new_s = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("StringReplace(%s, %s, %s)", s, old_s, new_s);
            free(s); free(old_s); free(new_s);
            return result;
        }
        if (strcmp(fn, "Substring") == 0 && call->data.call.arg_count == 3) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *start = emit_expression(call->data.call.arguments[1], ctx);
            char *len = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("Substring(%s, %s, %s)", s, start, len);
            free(s); free(start); free(len);
            return result;
        }
        if ((strcmp(fn, "Trim") == 0 || strcmp(fn, "StringTrim") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("StringTrim(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Upper") == 0 || strcmp(fn, "ToUpper") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToUpper(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Lower") == 0 || strcmp(fn, "ToLower") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToLower(%s)", arg);
            free(arg);
            return result;
        }
        if ((strcmp(fn, "Concat") == 0 || strcmp(fn, "StringConcat") == 0)
            && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringConcat(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        /* StringSplit / StringJoin / ToInt / ToFloat / Math */
        if ((strcmp(fn, "StringSplit") == 0 || strcmp(fn, "Split") == 0)
            && call->data.call.arg_count == 2) {
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *d = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringSplit(%s, %s)", s, d);
            free(s); free(d);
            return result;
        }
        if ((strcmp(fn, "StringJoin") == 0 || strcmp(fn, "Join") == 0)
            && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *sep = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("StringJoin(&%s, %s)", arr, sep);
            free(arr); free(sep);
            return result;
        }
        if (strcmp(fn, "ToInt") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToInt(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ToFloat") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("ToFloat(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Sqrt") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Sqrt(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Pow") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("Pow(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if ((strcmp(fn, "Floor") == 0 || strcmp(fn, "Ceil") == 0
            || strcmp(fn, "Round") == 0
            || strcmp(fn, "Sin") == 0 || strcmp(fn, "Cos") == 0
            || strcmp(fn, "Tan") == 0 || strcmp(fn, "Asin") == 0
            || strcmp(fn, "Acos") == 0 || strcmp(fn, "Atan") == 0
            || strcmp(fn, "Exp") == 0 || strcmp(fn, "MathLog") == 0
            || strcmp(fn, "Log10") == 0 || strcmp(fn, "Log2") == 0)
            && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("%s(%s)", fn, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Atan2") == 0 && call->data.call.arg_count == 2) {
            char *a = emit_expression(call->data.call.arguments[0], ctx);
            char *b = emit_expression(call->data.call.arguments[1], ctx);
            char *result = strdup_fmt("Atan2(%s, %s)", a, b);
            free(a); free(b);
            return result;
        }
        if (strcmp(fn, "Clamp") == 0 && call->data.call.arg_count == 3) {
            char *val = emit_expression(call->data.call.arguments[0], ctx);
            char *lo = emit_expression(call->data.call.arguments[1], ctx);
            char *hi = emit_expression(call->data.call.arguments[2], ctx);
            char *result = strdup_fmt("Clamp(%s, %s, %s)", val, lo, hi);
            free(val); free(lo); free(hi);
            return result;
        }
        if (strcmp(fn, "PI") == 0) return pergyra_strdup("PGY_PI");
        if (strcmp(fn, "E") == 0 && call->data.call.arg_count == 0)
            return pergyra_strdup("PGY_E");
        if (strcmp(fn, "Random") == 0) {
            if (call->data.call.arg_count >= 1) {
                char *arg = emit_expression(call->data.call.arguments[0], ctx);
                char *result = strdup_fmt("Random(%s)", arg);
                free(arg);
                return result;
            }
            return pergyra_strdup("Random(100)");
        }
        if (strcmp(fn, "SeedRandom") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("SeedRandom(%s)", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ArrayLength") == 0 && call->data.call.arg_count == 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((int32_t)(%s.length))", arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "ArrayPush") == 0 && call->data.call.arg_count == 2) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *suffix = infer_expression_type_name(
                ctx, call->data.call.arguments[1]);
            char *result = strdup_fmt(
                "pgy_array_push_%s(&%s, %s)", suffix, arr, val);
            free(arr); free(val);
            return result;
        }
        if (strcmp(fn, "ArraySet") == 0 && call->data.call.arg_count == 3) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *idx = emit_expression(call->data.call.arguments[1], ctx);
            char *val = emit_expression(call->data.call.arguments[2], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
            char *result = strdup_fmt(
                "pgy_array_set_%s(&%s, %s, %s)", inner, arr, idx, val);
            free(arr); free(idx); free(val);
            return result;
        }
        if (strcmp(fn, "ArrayPop") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("((%s).length > 0 ? (%s).length-- : 0)",
                arr, arr);
            free(arr);
            return result;
        }
        /* ArraySort ??hybrid sort using AlphaDev kernels for small arrays */
        if (strcmp(fn, "ArraySort") == 0 && call->data.call.arg_count == 1) {
            char *arr = emit_expression(call->data.call.arguments[0], ctx);
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
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
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
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
            const char *arr_type = infer_expression_type_name(ctx,
                call->data.call.arguments[0]);
            const char *inner = "Int";
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = slot_inner_type_name(arr_type);
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
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
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
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
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
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_try_send_%s(&%s, %s)", inner, ch, val);
            free(ch);
            free(val);
            return result;
        }
        if (strcmp(fn, "TrySendStatus") == 0 && call->data.call.arg_count == 2) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            char *val = emit_expression(call->data.call.arguments[1], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
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
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
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
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
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
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_close_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelReady") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_ready_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelLength") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_length_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelCapacity") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_capacity_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelSpace") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_space_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelFull") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
            char *result = strdup_fmt(
                "pgy_channel_full_%s(&%s)", inner, ch);
            free(ch);
            return result;
        }
        if (strcmp(fn, "ChannelClosed") == 0 && call->data.call.arg_count == 1) {
            char *ch = emit_expression(call->data.call.arguments[0], ctx);
            const char *inner = channel_inner_type_name(ctx,
                call->data.call.arguments[0]);
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
        /* HashMap builtins */
        if (strcmp(fn, "MapNew") == 0) {
            const char *hint = ctx->active_type_hint;
            const char *value = "Int";
            if (hint != NULL && strncmp(hint, "HashMap<", 8) == 0) {
                const char *hint_value = constructed_arg_name_at(hint, 1);
                if (hint_value != NULL)
                    value = hint_value;
            }
            ensure_collection_specialization(ctx, "Map", value);
            return strdup_fmt("pgy_map_new_%s()", collection_runtime_suffix(value));
        }
        if (strcmp(fn, "MapSet") == 0 && call->data.call.arg_count == 3) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            char *v = emit_expression(call->data.call.arguments[2], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char key_buf[64];
            char value_buf[64];
            const char *key = "String";
            const char *value = "Int";
            if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
                copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
                key = key_buf;
                value = value_buf;
            }
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt(
                strcmp(key, "Int") == 0 ? "pgy_map_set_i32_%s(&%s, %s, %s)"
                : strcmp(key, "Long") == 0 ? "pgy_map_set_i64_%s(&%s, %s, %s)"
                : strcmp(key, "Bool") == 0 ? "pgy_map_set_bool_%s(&%s, %s, %s)"
                : "pgy_map_set_%s(&%s, %s, %s)",
                collection_runtime_suffix(value), m, k, v);
            free(m); free(k); free(v);
            return result;
        }
        if (strcmp(fn, "MapGet") == 0 && call->data.call.arg_count == 2) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char key_buf[64];
            char value_buf[64];
            const char *key = "String";
            const char *value = "Int";
            if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
                copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
                key = key_buf;
                value = value_buf;
            }
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt(
                strcmp(key, "Int") == 0 ? "pgy_map_get_i32_%s(&%s, %s)"
                : strcmp(key, "Long") == 0 ? "pgy_map_get_i64_%s(&%s, %s)"
                : strcmp(key, "Bool") == 0 ? "pgy_map_get_bool_%s(&%s, %s)"
                : "pgy_map_get_%s(&%s, %s)",
                collection_runtime_suffix(value), m, k);
            free(m); free(k);
            return result;
        }
        if (strcmp(fn, "MapHas") == 0 && call->data.call.arg_count == 2) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char key_buf[64];
            char value_buf[64];
            const char *key = "String";
            const char *value = "Int";
            if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
                copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
                key = key_buf;
                value = value_buf;
            }
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt(
                strcmp(key, "Int") == 0 ? "pgy_map_has_i32_%s(&%s, %s)"
                : strcmp(key, "Long") == 0 ? "pgy_map_has_i64_%s(&%s, %s)"
                : strcmp(key, "Bool") == 0 ? "pgy_map_has_bool_%s(&%s, %s)"
                : "pgy_map_has_%s(&%s, %s)",
                collection_runtime_suffix(value), m, k);
            free(m); free(k);
            return result;
        }
        if (strcmp(fn, "MapRemove") == 0 && call->data.call.arg_count == 2) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char key_buf[64];
            char value_buf[64];
            const char *key = "String";
            const char *value = "Int";
            if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
                copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
                key = key_buf;
                value = value_buf;
            }
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt(
                strcmp(key, "Int") == 0 ? "pgy_map_remove_i32_%s(&%s, %s)"
                : strcmp(key, "Long") == 0 ? "pgy_map_remove_i64_%s(&%s, %s)"
                : strcmp(key, "Bool") == 0 ? "pgy_map_remove_bool_%s(&%s, %s)"
                : "pgy_map_remove_%s(&%s, %s)",
                collection_runtime_suffix(value), m, k);
            free(m); free(k);
            return result;
        }
        if (strcmp(fn, "MapSize") == 0 && call->data.call.arg_count == 1) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *value = (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0)
                ? constructed_arg_name_at(map_type, 1)
                : "Int";
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt("pgy_map_size_%s(&%s)",
                collection_runtime_suffix(value), m);
            free(m);
            return result;
        }
        if (strcmp(fn, "MapKeys") == 0 && call->data.call.arg_count == 1) {
            char *m = emit_expression(call->data.call.arguments[0], ctx);
            const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            char key_buf[64];
            char value_buf[64];
            const char *key = "String";
            const char *value = "Int";
            if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
                copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
                key = key_buf;
                value = value_buf;
            }
            ensure_collection_specialization(ctx, "Map", value);
            char *result = strdup_fmt(
                strcmp(key, "Int") == 0 ? "pgy_map_keys_i32_%s(&%s)"
                : strcmp(key, "Long") == 0 ? "pgy_map_keys_i64_%s(&%s)"
                : strcmp(key, "Bool") == 0 ? "pgy_map_keys_bool_%s(&%s)"
                : "pgy_map_keys_%s(&%s)",
                collection_runtime_suffix(value), m);
            free(m);
            return result;
        }
        /* List builtins */
        if (strcmp(fn, "ListNew") == 0)
        {
            const char *hint = ctx->active_type_hint;
            const char *inner = (hint != NULL && strncmp(hint, "List<", 5) == 0)
                ? slot_inner_type_name(hint)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            return strdup_fmt("pgy_list_new_%s()", collection_runtime_suffix(inner));
        }
        if (strcmp(fn, "ListPush") == 0 && call->data.call.arg_count == 2) {
            char *l = emit_expression(call->data.call.arguments[0], ctx);
            char *v = emit_expression(call->data.call.arguments[1], ctx);
            const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                ? slot_inner_type_name(list_type)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            char *r = strdup_fmt("pgy_list_push_%s(&%s, %s)",
                collection_runtime_suffix(inner), l, v);
            free(l); free(v); return r;
        }
        if (strcmp(fn, "ListGet") == 0 && call->data.call.arg_count == 2) {
            char *l = emit_expression(call->data.call.arguments[0], ctx);
            char *i = emit_expression(call->data.call.arguments[1], ctx);
            const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                ? slot_inner_type_name(list_type)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            char *r = strdup_fmt("pgy_list_get_%s(&%s, %s)",
                collection_runtime_suffix(inner), l, i);
            free(l); free(i); return r;
        }
        if (strcmp(fn, "ListSet") == 0 && call->data.call.arg_count == 3) {
            char *l = emit_expression(call->data.call.arguments[0], ctx);
            char *i = emit_expression(call->data.call.arguments[1], ctx);
            char *v = emit_expression(call->data.call.arguments[2], ctx);
            const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                ? slot_inner_type_name(list_type)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            char *r = strdup_fmt("pgy_list_set_%s(&%s, %s, %s)",
                collection_runtime_suffix(inner), l, i, v);
            free(l); free(i); free(v); return r;
        }
        if (strcmp(fn, "ListSize") == 0 && call->data.call.arg_count == 1) {
            char *l = emit_expression(call->data.call.arguments[0], ctx);
            const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                ? slot_inner_type_name(list_type)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            char *r = strdup_fmt("pgy_list_size_%s(&%s)",
                collection_runtime_suffix(inner), l);
            free(l); return r;
        }
        if (strcmp(fn, "ListRemove") == 0 && call->data.call.arg_count == 2) {
            char *l = emit_expression(call->data.call.arguments[0], ctx);
            char *i = emit_expression(call->data.call.arguments[1], ctx);
            const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                ? slot_inner_type_name(list_type)
                : "Int";
            ensure_collection_specialization(ctx, "List", inner);
            char *r = strdup_fmt("pgy_list_remove_%s(&%s, %s)",
                collection_runtime_suffix(inner), l, i);
            free(l); free(i); return r;
        }
        /* Set builtins ??generic (type inferred from Set<T> annotation) */
        #define SET_INFER_INNER(arg0) do { \
            const char *_st = infer_expression_type_name(ctx, arg0); \
            if (_st != NULL && strncmp(_st, "Set<", 4) == 0) \
                set_inner = slot_inner_type_name(_st); \
        } while (0)

        if (strcmp(fn, "SetNew") == 0) {
            const char *hint = ctx->active_type_hint;
            const char *set_inner = (hint != NULL && strncmp(hint, "Set<", 4) == 0)
                ? slot_inner_type_name(hint)
                : "String";
            ensure_collection_specialization(ctx, "Set", set_inner);
            return strdup_fmt("pgy_set_new_%s()", collection_runtime_suffix(set_inner));
        }
        if (strcmp(fn, "SetAdd") == 0 && call->data.call.arg_count == 2) {
            const char *set_inner = "String";
            SET_INFER_INNER(call->data.call.arguments[0]);
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            ensure_collection_specialization(ctx, "Set", set_inner);
            char *r = strdup_fmt("pgy_set_add_%s(&%s, %s)",
                collection_runtime_suffix(set_inner), s, k);
            free(s); free(k); return r;
        }
        if (strcmp(fn, "SetHas") == 0 && call->data.call.arg_count == 2) {
            const char *set_inner = "String";
            SET_INFER_INNER(call->data.call.arguments[0]);
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            ensure_collection_specialization(ctx, "Set", set_inner);
            char *r = strdup_fmt("pgy_set_has_%s(&%s, %s)",
                collection_runtime_suffix(set_inner), s, k);
            free(s); free(k); return r;
        }
        if (strcmp(fn, "SetRemove") == 0 && call->data.call.arg_count == 2) {
            const char *set_inner = "String";
            SET_INFER_INNER(call->data.call.arguments[0]);
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            char *k = emit_expression(call->data.call.arguments[1], ctx);
            ensure_collection_specialization(ctx, "Set", set_inner);
            char *r = strdup_fmt("pgy_set_remove_%s(&%s, %s)",
                collection_runtime_suffix(set_inner), s, k);
            free(s); free(k); return r;
        }
        if (strcmp(fn, "SetSize") == 0 && call->data.call.arg_count == 1) {
            const char *set_inner = "String";
            SET_INFER_INNER(call->data.call.arguments[0]);
            char *s = emit_expression(call->data.call.arguments[0], ctx);
            ensure_collection_specialization(ctx, "Set", set_inner);
            char *r = strdup_fmt("pgy_set_size_%s(&%s)",
                collection_runtime_suffix(set_inner), s);
            free(s); return r;
        }
        #undef SET_INFER_INNER
        /* Queue builtins */
        if (strcmp(fn, "QueueNew") == 0)
        {
            const char *hint = ctx->active_type_hint;
            const char *inner = (hint != NULL && strncmp(hint, "Queue<", 6) == 0)
                ? slot_inner_type_name(hint)
                : "Int";
            ensure_collection_specialization(ctx, "Queue", inner);
            return strdup_fmt("pgy_queue_new_%s()", collection_runtime_suffix(inner));
        }
        if (strcmp(fn, "QueuePush") == 0 && call->data.call.arg_count == 2) {
            char *q = emit_expression(call->data.call.arguments[0], ctx);
            char *v = emit_expression(call->data.call.arguments[1], ctx);
            const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
                ? slot_inner_type_name(queue_type)
                : "Int";
            ensure_collection_specialization(ctx, "Queue", inner);
            char *r = strdup_fmt("pgy_queue_push_%s(&%s, %s)",
                collection_runtime_suffix(inner), q, v);
            free(q); free(v); return r;
        }
        if (strcmp(fn, "QueuePop") == 0 && call->data.call.arg_count == 1) {
            char *q = emit_expression(call->data.call.arguments[0], ctx);
            const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
                ? slot_inner_type_name(queue_type)
                : "Int";
            ensure_collection_specialization(ctx, "Queue", inner);
            char *r = strdup_fmt("pgy_queue_pop_%s(&%s)",
                collection_runtime_suffix(inner), q);
            free(q); return r;
        }
        if (strcmp(fn, "QueueSize") == 0 && call->data.call.arg_count == 1) {
            char *q = emit_expression(call->data.call.arguments[0], ctx);
            const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
                ? slot_inner_type_name(queue_type)
                : "Int";
            ensure_collection_specialization(ctx, "Queue", inner);
            char *r = strdup_fmt("pgy_queue_size_%s(&%s)",
                collection_runtime_suffix(inner), q);
            free(q); return r;
        }
        if (strcmp(fn, "QueueEmpty") == 0 && call->data.call.arg_count == 1) {
            char *q = emit_expression(call->data.call.arguments[0], ctx);
            const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
                ? slot_inner_type_name(queue_type)
                : "Int";
            ensure_collection_specialization(ctx, "Queue", inner);
            char *r = strdup_fmt("pgy_queue_empty_%s(&%s)",
                collection_runtime_suffix(inner), q);
            free(q); return r;
        }
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

static char *
emit_call_event_builtin(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    if (callee->type == AST_IDENTIFIER) {
        const char *name = callee->data.identifier.name;
        if (find_event_decl(ctx, name) != NULL) {
            CodeBuf *args_buf = codebuf_create();
            for (size_t i = 0; i < call->data.call.arg_count; i++) {
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                if (i > 0)
                    codebuf_write(args_buf, ", ");
                codebuf_write(args_buf, "%s", arg);
                free(arg);
            }

            char *result = strdup_fmt("%s_INVOKE(&%s%s%s)",
                                      name, name,
                                      args_buf->len > 0 ? ", " : "",
                                      args_buf->data);
            codebuf_destroy(args_buf);
            return result;
        }
    }

    return NULL;
}
