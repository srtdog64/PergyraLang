                && strncmp(param_type, "SecureSlot<", 11) == 0;
            if (slot_param) {
                const char *inner = NULL;
                const char *slot_name = NULL;
                bool secure = false;
                bool saved_suppress = ctx->suppress_slot_auto_read;
                ctx->suppress_slot_auto_read = true;
                arg = emit_expression(call->data.call.arguments[i], ctx);
                ctx->suppress_slot_auto_read = saved_suppress;
                resolve_slot_target(ctx, call->data.call.arguments[i],
                    &inner, &slot_name, &secure);
                if (i > 0)
                    codebuf_write(args_buf, ", ");
                if (slot_name != NULL) {
                    char *slot_ref = slot_ref_expr(ctx, slot_name, arg);
                    codebuf_write(args_buf, "%s", slot_ref);
                    free(slot_ref);
                    if (secure_param)
                        codebuf_write(args_buf, ", %s_token", slot_name);
                    handled = true;
                }
            }
            free(param_type);
        }

        if (!handled)
            arg = emit_expression(call->data.call.arguments[i], ctx);
        if (!handled && i > 0)
            codebuf_write(args_buf, ", ");
        if (!handled) {
            /* Subject arguments: pass by pointer (reference semantics) */
            bool is_subject_arg = false;
            if (param != NULL && param->type != NULL
                && param->name != NULL && strcmp(param->name, "self") != 0) {
                char *ptn = render_type_name(param->type);
                if (ptn != NULL && is_pointer_self_host_type_name(ctx, ptn))
                    is_subject_arg = true;
                free(ptn);
            }
            if (!is_subject_arg && intent_param_type != NULL) {
                char *ptn = render_type_name(intent_param_type);
                if (ptn != NULL && is_pointer_self_host_type_name(ctx, ptn))
                    is_subject_arg = true;
                free(ptn);
            }
            if (is_subject_arg) {
                /* Don't add & if already a pointer (subject-ref param) */
                bool already_ptr = false;
                ASTNode *arg_node = call->data.call.arguments[i];
                if (arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
                    TypedVarEntry *entry = lookup_typed_entry(ctx,
                        arg_node->data.identifier.name);
                    if (entry != NULL && entry->is_subject_ref)
                        already_ptr = true;
                }
                if (already_ptr)
                    codebuf_write(args_buf, "%s", arg);
                else
                    codebuf_write(args_buf, "&%s", arg);
            } else {
                codebuf_write(args_buf, "%s", arg);
            }
        }
        free(arg);
    }

    char *result = strdup_fmt("%s(%s)", callee_str, args_buf->data);
    free(callee_str);
    codebuf_destroy(args_buf);
    return result;
}

char *
emit_spawn_expr(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *target = node->data.spawn_expr.function;
    ASTNode *call = NULL;
    ASTNode *callee = NULL;
    const char *function_name = NULL;
    const char *emitted_function_name = NULL;
    ASTNode *decl = NULL;
    size_t arg_count = 0;
    int wrapper_id = ++ctx->tmp_counter;
    char *wrapper_name = strdup_fmt("pgy_spawn_wrapper_%d", wrapper_id);
    char *args_type_name = NULL;
    char *return_type_name = infer_spawn_return_type_name(ctx, node);
    char *return_c_type = pergyra_strdup(pergyra_type_to_c(return_type_name));
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;

    if (target == NULL) {
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
    }

    if (target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
        arg_count = target->data.call.arg_count;
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        function_name = callee->data.identifier.name;
    if (function_name == NULL) {
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported spawn target at line %d", target->line);
        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
    }

    decl = find_function_decl(ctx, function_name);
    emitted_function_name = function_name;
    if (call != NULL && func_has_generic_params(decl)
        && infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count)) {
        const char *specialized = ensure_generic_specialization(ctx, decl, call);
        if (specialized != NULL)
            emitted_function_name = specialized;
    }
    if (arg_count > 0)
        args_type_name = strdup_fmt("PgySpawnArgs_%d", wrapper_id);

    if (args_type_name != NULL) {
        codebuf_write(ctx->decls, "\ntypedef struct {\n");
        for (size_t i = 0; i < arg_count; i++) {
            const char *arg_type = NULL;
            if (decl != NULL && i < decl->data.func_decl.param_count
                && decl->data.func_decl.params[i] != NULL
                && decl->data.func_decl.params[i]->type != NULL) {
                if (binding_count > 0) {
                    char *bound_type = render_type_name_with_bindings(ctx,
                        decl->data.func_decl.params[i]->type, bindings, binding_count);
                    arg_type = pergyra_type_to_c(bound_type);
                    codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
                    free(bound_type);
                    continue;
                }
                arg_type = pergyra_ast_type_to_c(decl->data.func_decl.params[i]->type);
            } else if (call != NULL) {
                arg_type = pergyra_type_to_c(infer_expression_type_name(
                    ctx, call->data.call.arguments[i]));
            }
            if (arg_type == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine spawn wrapper argument type for call '%s' at argument %llu",
                    function_name != NULL ? function_name : "<function>",
                    (unsigned long long) i);
                free(args_type_name);
                return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
            }
            codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
        }
        codebuf_write(ctx->decls, "} %s;\n", args_type_name);
    }

    codebuf_write(ctx->decls, "static void *%s(void *raw);\n", wrapper_name);
    codebuf_write(ctx->helpers, "\nstatic void *%s(void *raw)\n{\n", wrapper_name);
    if (args_type_name != NULL) {
        codebuf_write(ctx->helpers, "    %s *args = (%s *)raw;\n",
            args_type_name, args_type_name);
    } else {
        codebuf_write(ctx->helpers, "    (void)raw;\n");
    }

    if (strcmp(return_type_name, "Void") == 0) {
        codebuf_write(ctx->helpers, "    %s(", emitted_function_name);
    } else {
        codebuf_write(ctx->helpers,
            "    %s *result = (%s *)malloc(sizeof(%s));\n",
            return_c_type, return_c_type, return_c_type);
        codebuf_write(ctx->helpers,
            "    if (result == NULL) {\n"
            "        PGY_PANIC(\"spawn result allocation failed\");\n"
            "    }\n"
            "    *result = %s(",
            emitted_function_name);
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (i > 0)
            codebuf_write(ctx->helpers, ", ");
        codebuf_write(ctx->helpers, "args->arg%zu", i);
    }
    codebuf_write(ctx->helpers, ");\n");

    if (args_type_name != NULL)
        codebuf_write(ctx->helpers, "    free(args);\n");

    if (strcmp(return_type_name, "Void") == 0)
        codebuf_write(ctx->helpers, "    return NULL;\n");
    else
        codebuf_write(ctx->helpers, "    return result;\n");
    codebuf_write(ctx->helpers, "}\n");

    CodeBuf *expr = codebuf_create();
    if (expr == NULL) {
        free(wrapper_name);
        free(args_type_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("/* spawn alloc failed */");
    }

    {
        /* spawn blocking ??offload to dedicated blocking pool */
        const char *spawn_fn = node->data.spawn_expr.is_blocking
            ? "pgy_spawn_blocking" : "pgy_async_spawn";
        if (args_type_name == NULL) {
            codebuf_write(expr, "%s(%s, NULL)", spawn_fn, wrapper_name);
        } else {
            codebuf_write(expr,
                "({ %s *_pgy_args = (%s *)malloc(sizeof(%s)); "
                "if (_pgy_args == NULL) { PGY_PANIC(\"spawn arg allocation failed\"); } ",
                args_type_name, args_type_name, args_type_name);
            for (size_t i = 0; i < arg_count; i++) {
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                codebuf_write(expr, "_pgy_args->arg%zu = %s; ", i, arg);
                free(arg);
            }
            codebuf_write(expr, "%s(%s, _pgy_args); })", spawn_fn, wrapper_name);
        }
    }

    char *result = pergyra_strdup(expr->data);
    codebuf_destroy(expr);
    free(wrapper_name);
    free(args_type_name);
    free(return_type_name);
    free(return_c_type);
    return result;
}

char *
emit_channel_send(ASTNode *node, TranspilerCtx *ctx)
{
    char *ch  = emit_expression(node->data.channel_send.channel, ctx);
    char *val = emit_expression(node->data.channel_send.value, ctx);
    const char *inner = "Int";

    if (node->data.channel_send.channel != NULL
        && node->data.channel_send.channel->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            node->data.channel_send.channel->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
            inner = slot_inner_type_name(type_name);
    }

    char *result = strdup_fmt("pgy_channel_send_%s(&%s, %s)", inner, ch, val);
    free(ch);
    free(val);
    return result;
}

char *
emit_channel_recv(ASTNode *node, TranspilerCtx *ctx)
{
    char *ch = emit_expression(node->data.channel_recv.channel, ctx);
    const char *inner = "Int";

    if (node->data.channel_recv.channel != NULL
        && node->data.channel_recv.channel->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            node->data.channel_recv.channel->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
            inner = slot_inner_type_name(type_name);
    }

    char *result = strdup_fmt("pgy_channel_recv_val_%s(&%s)", inner, ch);
    free(ch);
    return result;
}

char *
emit_expression(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return pergyra_strdup("0");

    switch (node->type) {
    case AST_NUMBER:
        if (node->data.number.is_long)
            return strdup_fmt("%lldLL", (long long)(int64_t)node->data.number.value);
        if (node->data.number.value == (int64_t)node->data.number.value)
            return strdup_fmt("%lld", (long long)(int64_t)node->data.number.value);
        return strdup_fmt("%g", node->data.number.value);

    case AST_STRING:
    {
        char *escaped = escape_c_string_literal(node->data.string.value);
        char *result = strdup_fmt("\"%s\"", escaped);
        free(escaped);
        return result;
    }

    case AST_BOOLEAN:
        return pergyra_strdup(node->data.boolean.value ? "true" : "false");

    case AST_IDENTIFIER: {
        const char *id_name = node->data.identifier.name;
        /* Handle None used without parentheses as Option<Int> fallback */
        if (strcmp(id_name, "None") == 0) {
            const char *inner = "Int";
            if (ctx->expected_type != NULL && strncmp(ctx->expected_type, "Option<", 7) == 0) {
                inner = slot_inner_type_name(ctx->expected_type);
            }
            return strdup_fmt("None_%s()", inner);
        }
        /* Inside parallel wrapper: captured outer variables are accessed
         * through the context struct pointer.  (*_pctx->x) yields the
         * value, and &(*_pctx->x) collapses to _pctx->x (a pointer). */
        if (ctx->in_parallel_wrapper) {
            for (int i = 0; i < ctx->par_capture_slot_count; i++) {
                if (strcmp(ctx->par_capture_slot_names[i], id_name) == 0)
                    return strdup_fmt("(*_pctx->%s)", id_name);
            }
            for (int i = 0; i < ctx->par_capture_typed_count; i++) {
                if (strcmp(ctx->par_capture_typed_names[i], id_name) == 0)
                    return strdup_fmt("(*_pctx->%s)", id_name);
            }
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_class_has_field(ctx, id_name)) {
            return strdup_fmt(current_class_uses_self_cell(ctx)
                ? "self->%s"
                : "self.%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_relation_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_effect_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_zone_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        if (strcmp(id_name, "self") != 0
            && lookup_typed_var(ctx, id_name) == NULL
            && !is_slot_var(ctx, id_name)
            && current_world_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        const char *ssa_name = transpiler_resolve_active_ssa_name(ctx, id_name);
        if (ssa_name != NULL) {
            char *c_ssa_name = transpiler_make_c_ssa_name(ssa_name);
            const char *slot_type = lookup_typed_var(ctx, id_name);
            if (slot_type == NULL) {
                char base_name[128];
                size_t version = 0;
                if (transpiler_parse_versioned_name(id_name, base_name, sizeof(base_name), &version))
                    slot_type = lookup_typed_var(ctx, base_name);
            }
            if (slot_type != NULL && transpiler_type_name_is_slot_like(slot_type)
                && !ctx->suppress_slot_auto_read) {
                const char *inner = slot_inner_type_name(slot_type);
                bool secure = strncmp(slot_type, "SecureSlot<", 11) == 0;
                const char *token_name = secure
                    ? lookup_slot_token_name(ctx, id_name)
                    : NULL;
                char fallback_token[96];
                if (secure && token_name == NULL) {
                    snprintf(fallback_token, sizeof(fallback_token), "%s_token", id_name);
                    token_name = fallback_token;
                }
                char *result = secure
                    ? strdup_fmt("pgy_secure_read_%s(&%s, &%s)",
                                  inner, c_ssa_name, token_name)
                    : strdup_fmt("pgy_read_%s(&%s)", inner, c_ssa_name);
                free(c_ssa_name);
                return result;
            }
            return c_ssa_name;
        }
        {
            ASTNode *alias_expr = lookup_alias_expr(ctx, id_name);
            if (alias_expr != NULL)
                return emit_expression(alias_expr, ctx);
        }
        TypedVarEntry *projection_entry = lookup_typed_entry(ctx, id_name);
        /* Slot sugar: auto-Read ??emit pgy_read_T(&x) instead of x */
        if (!ctx->suppress_slot_auto_read && is_slot_var(ctx, id_name)) {
            const char *inner = lookup_slot_type(ctx, id_name);
            bool secure = lookup_slot_is_secure(ctx, id_name);
            if (inner == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for auto-read of '%s'",
                    id_name != NULL ? id_name : "<slot>");
                return pergyra_strdup("0");
            }
            char *slot_ref = slot_ref_expr(ctx, id_name, id_name);
            const char *token_name = secure
                ? lookup_slot_token_name(ctx, id_name)
                : NULL;
            char fallback_token[96];
            if (secure && token_name == NULL) {
                snprintf(fallback_token, sizeof(fallback_token), "%s_token", id_name);
                token_name = fallback_token;
            }
            char *result = secure
                ? strdup_fmt("pgy_secure_read_%s(%s, &%s)",
                              inner, slot_ref, token_name)
                : strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
            free(slot_ref);
            return result;
        }
        {
            const char *enum_variant = lookup_enum_variant_qualified_name(ctx, id_name);
            if (enum_variant != NULL)
                return pergyra_strdup(enum_variant);
        }
        if (projection_entry != NULL && projection_entry->is_projection_borrow) {
            const char *source_type = lookup_typed_var(ctx, projection_entry->source_slot);
            ASTNode *target_decl = find_class_decl(ctx, projection_entry->type_name);
            ASTNode *source_decl = source_type != NULL
                ? find_class_decl(ctx, source_type)
                : NULL;
            if (target_decl != NULL && source_decl != NULL) {
                return emit_projection_literal(ctx, target_decl, source_decl,
                    NULL, projection_entry->type_name, projection_entry->source_slot);
            }
        }
        return pergyra_strdup(id_name);
    }

    case AST_BINARY:
        return emit_binary(node, ctx);

    case AST_UNARY:
        return emit_unary(node, ctx);

    case AST_CALL:
        return emit_call(node, ctx);

    case AST_MEMBER_ACCESS: {
        if (node->data.member.object != NULL
            && node->data.member.object->type == AST_IDENTIFIER
            && node->data.member.name != NULL) {
            TypedVarEntry *entry = lookup_typed_entry(ctx,
                node->data.member.object->data.identifier.name);
            if (entry != NULL && entry->is_projection_borrow) {
                const char *source_type = lookup_typed_var(ctx, entry->source_slot);
                ASTNode *source_decl = source_type != NULL
                    ? find_class_decl(ctx, source_type)
                    : NULL;
                char *source_path = NULL;
                int source_status = 0;
                if (source_decl != NULL) {
                    source_status = resolve_projection_source_path_rec(
                        ctx, source_decl, node->data.member.name, 0, &source_path);
                }
                if (source_status == 1 && source_path != NULL) {
                    TypedVarEntry *source_entry = lookup_typed_entry(ctx, entry->source_slot);
                    char *result = strdup_fmt(
                        (source_entry != NULL && source_entry->is_subject_ref)
                            ? "%s->%s"
                            : "%s.%s",
                        entry->source_slot, source_path);
                    free(source_path);
                    return result;
                }
                free(source_path);
            }
        }
        char *obj = emit_expression(node->data.member.object, ctx);
        /* Enum variant access: Color.Red ??Color_Red */
        if (node->data.member.object->type == AST_IDENTIFIER
            && node->data.member.object->data.identifier.name[0] >= 'A'
            && node->data.member.object->data.identifier.name[0] <= 'Z') {
            char *result = strdup_fmt("%s_%s", obj, node->data.member.name);
            free(obj);
            return result;
        }
        if (node->data.member.object->type == AST_IDENTIFIER
            && strcmp(node->data.member.object->data.identifier.name, "self") == 0) {
            ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
            bool self_is_pointer = current_class_uses_self_cell(ctx)
                || (host_decl != NULL
                    && (host_decl->type == AST_RELATION_DECL
                        || host_decl->type == AST_EFFECT_DECL
                        || host_decl->type == AST_ZONE_DECL
                        || host_decl->type == AST_WORLD_DECL));
            char *result = strdup_fmt(self_is_pointer
                ? "%s->%s"
                : "%s.%s", obj, node->data.member.name);
            free(obj);
            return result;
        }
        /* Subject-ref parameter: use -> for member access */
        if (node->data.member.object->type == AST_IDENTIFIER) {
            TypedVarEntry *entry = lookup_typed_entry(ctx,
                node->data.member.object->data.identifier.name);
            if (entry != NULL && entry->is_subject_ref) {
                char *result = strdup_fmt("%s->%s", obj, node->data.member.name);
                free(obj);
                return result;
            }
        }
        char *result = strdup_fmt("%s.%s", obj, node->data.member.name);
        free(obj);
        return result;
    }

    case AST_ARRAY_ACCESS: {
        char *array = emit_expression(node->data.array_access.array, ctx);
        char *index = emit_expression(node->data.array_access.index, ctx);
        const char *array_type = infer_expression_type_name(ctx, node->data.array_access.array);
        char *result;
        if (strncmp(array_type, "Array<", 6) == 0) {
            const char *inner = slot_inner_type_name(array_type);
            int tmp_id = ++ctx->tmp_counter;
            result = strdup_fmt(
                "({ PgyArray_%s _pgy_arr_get_%d = %s; "
                "pgy_array_get_%s(&_pgy_arr_get_%d, %s); })",
                inner, tmp_id, array, inner, tmp_id, index);
        } else if (strncmp(array_type, "Slice<", 6) == 0) {
            const char *inner = slot_inner_type_name(array_type);
            int tmp_id = ++ctx->tmp_counter;
            result = strdup_fmt(
                "({ PgySlice_%s _pgy_slice_get_%d = %s; "
                "pgy_slice_get_%s(&_pgy_slice_get_%d, %s); })",
                inner, tmp_id, array, inner, tmp_id, index);
        } else {
            result = strdup_fmt("%s[%s]", array, index);
        }
        free(array);
        free(index);
        return result;
    }

    case AST_TUPLE_LITERAL: {
        /* Determine tuple type name from context. Priority:
         *   1) ctx->expected_type (let binding with annotation)
         *   2) ctx->current_return_type (inside `return` in tuple-returning fn)
         *   3) Build from element type names (best-effort fallback).
         * All three must start with '(' for tuple classification. */
        char tuple_name_buf[256];
        const char *tuple_name = NULL;
        if (ctx->expected_type != NULL && ctx->expected_type[0] == '(')
            tuple_name = ctx->expected_type;
        else if (ctx->current_return_type[0] == '(')
            tuple_name = ctx->current_return_type;
        if (tuple_name == NULL) {
            size_t off = 0;
            off += (size_t)snprintf(tuple_name_buf + off,
                sizeof(tuple_name_buf) - off, "(");
            for (size_t i = 0; i < node->data.tuple_literal.count; i++) {
                const char *et =
                    infer_expression_type_name(ctx, node->data.tuple_literal.elements[i]);
                if (et == NULL || et[0] == '\0') et = "Int";
                if (i > 0) {
                    off += (size_t)snprintf(tuple_name_buf + off,
                        sizeof(tuple_name_buf) - off, ", ");
                }
                off += (size_t)snprintf(tuple_name_buf + off,
                    sizeof(tuple_name_buf) - off, "%s", et);
            }
            (void)snprintf(tuple_name_buf + off,
                sizeof(tuple_name_buf) - off, ")");
            tuple_name = tuple_name_buf;
        }
        const char *ctype = pergyra_type_to_c(tuple_name);
        CodeBuf *out = codebuf_create();
        codebuf_write(out, "((%s){", ctype);
        for (size_t i = 0; i < node->data.tuple_literal.count; i++) {
            char *v = emit_expression(node->data.tuple_literal.elements[i], ctx);
            if (i > 0)
                codebuf_write(out, ", ");
            codebuf_write(out, ".f%zu = %s", i, v);
            free(v);
        }
        codebuf_write(out, "})");
        char *result = pergyra_strdup(out->data);
        codebuf_destroy(out);
        return result;
    }

    case AST_ARRAY_LITERAL: {
        const char *array_type = infer_expression_type_name(ctx, node);
        const char *inner = slot_inner_type_name(array_type);
        int tmp_id = ++ctx->tmp_counter;
        CodeBuf *buf = codebuf_create();
        codebuf_write(buf, "({ PgyArray_%s _pgy_arr_%d = pgy_array_new_%s(%zu); ",
            inner, tmp_id, inner, node->data.array_literal.count);
        for (size_t i = 0; i < node->data.array_literal.count; i++) {
            char *elem = emit_expression(node->data.array_literal.elements[i], ctx);
            codebuf_write(buf, "pgy_array_push_%s(&_pgy_arr_%d, %s); ",
                inner, tmp_id, elem);
            free(elem);
        }
        codebuf_write(buf, "_pgy_arr_%d; })", tmp_id);
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }

    case AST_ASSIGNMENT: {
        /* Slot sugar: x = 5 ??pgy_write_T(&x, 5) */
        if (node->data.assignment.target->type == AST_IDENTIFIER) {
            const char *tgt_name = node->data.assignment.target->data.identifier.name;
            if (is_slot_var(ctx, tgt_name)) {
                const char *inner = lookup_slot_type(ctx, tgt_name);
                bool secure = lookup_slot_is_secure(ctx, tgt_name);
                if (inner == NULL) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for assignment to '%s'",
                        tgt_name != NULL ? tgt_name : "<slot>");
                    return pergyra_strdup("0");
                }
                char *value = emit_expression(node->data.assignment.value, ctx);
                char *slot_ref = slot_ref_expr(ctx, tgt_name, tgt_name);
                char *result;
                if (secure) {
                    const char *token_name = lookup_slot_token_name(ctx, tgt_name);
                    char fallback_token[96];
                    if (token_name == NULL) {
                        snprintf(fallback_token, sizeof(fallback_token),
                                 "%s_token", tgt_name);
                        token_name = fallback_token;
                    }
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s)",
                        inner, slot_ref, value, token_name);
                } else {
                    result = strdup_fmt("pgy_write_%s(%s, %s)",
                        inner, slot_ref, value);
                }
                free(slot_ref);
                free(value);
                return result;
            }
        }
        char *target = emit_expression(node->data.assignment.target, ctx);
        char *value  = emit_expression(node->data.assignment.value,  ctx);
        char *invalidation = emit_assignment_projection_invalidation(
            ctx, node->data.assignment.target);
        char *post_sync = emit_world_embedded_assignment_sync(
            ctx, node->data.assignment.target);
        char *result;
        if (post_sync != NULL)
            result = strdup_fmt("({ %s%s = %s; %s%s; })",
                invalidation != NULL ? invalidation : "",
                target, value, post_sync, target);
        else if (invalidation != NULL)
            result = strdup_fmt("({ %s%s = %s; })", invalidation, target, value);
        else
            result = strdup_fmt("%s = %s", target, value);
        free(invalidation);
        free(post_sync);
        free(target);
        free(value);
        return result;
    }

    case AST_AWAIT_EXPR:
        {
            char *expr = emit_expression(node->data.await_expr.expression, ctx);
            const char *inner = lookup_future_inner_type(ctx,
                node->data.await_expr.expression);
            bool is_remote = is_remote_future_expr(ctx,
                node->data.await_expr.expression);
            char *result;
            if (strcmp(inner, "Void") == 0) {
                result = strdup_fmt("pgy_await_void(%s)", expr);
            } else if (is_remote) {
                /* RemoteFuture<T> ??Result<T>: wrap in PgyResult */
                result = strdup_fmt("pgy_await_result_take(%s, %s, %s)",
                    expr, inner, pergyra_type_to_c(inner));
            } else {
                result = strdup_fmt("pgy_await_take(%s, %s)",
                    expr, pergyra_type_to_c(inner));
            }
            free(expr);
            return result;
        }

    case AST_SPAWN_EXPR:
        return emit_spawn_expr(node, ctx);

    case AST_CHANNEL_SEND:
        return emit_channel_send(node, ctx);

    case AST_CHANNEL_RECV:
        return emit_channel_recv(node, ctx);

    case AST_EVENT_INVOKE: {
        char *event = emit_expression(node->data.event_invoke.event, ctx);
        CodeBuf *args = codebuf_create();
        for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
            char *arg = emit_expression(node->data.event_invoke.arguments[i], ctx);
            if (i > 0)
                codebuf_write(args, ", ");
            codebuf_write(args, "%s", arg);
            free(arg);
        }
        char *result = strdup_fmt("%s_INVOKE(&%s%s%s)",
                                  event,
                                  event,
                                  args->len > 0 ? ", " : "",
                                  args->data);
        free(event);
        codebuf_destroy(args);
        return result;
    }

    case AST_CONTEXT_ACCESS:
        if (node->data.context_access.role_slot_name != NULL) {
            return strdup_fmt("self->%s", node->data.context_access.role_slot_name);
        }
        return pergyra_strdup("self");

    case AST_PARTY_INSTANCE: {
        CodeBuf *assignments = codebuf_create();
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
            char *value = emit_expression(node->data.party_instance.assignments[i].value, ctx);
            if (i > 0)
                codebuf_write(assignments, ", ");
            codebuf_write(assignments, ".%s = %s",
                          node->data.party_instance.assignments[i].slot_name,
                          value);
            free(value);
        }
        char *result = strdup_fmt("(%s){%s}",
                                  node->data.party_instance.party_type,
                                  assignments->data);
        codebuf_destroy(assignments);
        return result;
    }

    case AST_LAMBDA_EXPR:
        return emit_lambda_expr(node, ctx);

    default:
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported expression node type %d at line %d",
            (int)node->type, node->line);
        return pergyra_strdup("0");
    }
}
