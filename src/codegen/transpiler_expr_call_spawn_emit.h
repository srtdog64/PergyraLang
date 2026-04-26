static char *
emit_call_member_style(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    /* Method-call style slot operations: slot.Write(val), slot.Read(), slot.Release() */
    if (callee->type == AST_MEMBER_ACCESS) {
        const char *method = callee->data.member.name;
        ASTNode *obj = callee->data.member.object;

        if (obj != NULL && obj->type == AST_MEMBER_ACCESS && method != NULL) {
            ASTNode *party_node = obj->data.member.object;
            const char *slot_name = obj->data.member.name;

            if (party_node != NULL && party_node->type == AST_IDENTIFIER
                && slot_name != NULL) {
                const char *party_var = party_node->data.identifier.name;
                const char *party_type = lookup_typed_var(ctx, party_var);
                ASTNode *party_decl = find_party_decl(ctx, party_type);
                char *ability_name = NULL;

                if (party_decl != NULL) {
                    for (size_t i = 0; i < party_decl->data.party_decl.role_count; i++) {
                        ASTNode *rs = party_decl->data.party_decl.role_slots[i];
                        if (rs == NULL || strcmp(rs->data.role_slot.slot_name, slot_name) != 0)
                            continue;
                        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
                            ASTNode *ab = rs->data.role_slot.required_abilities[j];
                            ASTNode *ability_decl;
                            bool has_method = false;
                            char *ability_tag = NULL;
                            if (ab == NULL || ab->data.type.name == NULL)
                                continue;
                            ability_decl = find_ability_decl(ctx, ab->data.type.name);
                            if (ability_decl != NULL) {
                                for (size_t mi = 0; mi < ability_decl->data.ability_decl.method_count; mi++) {
                                    ASTNode *m = ability_decl->data.ability_decl.methods[mi];
                                    if (m != NULL && m->type == AST_FUNC_DECL
                                        && m->data.func_decl.name != NULL
                                        && strcmp(m->data.func_decl.name, method) == 0) {
                                        has_method = true;
                                        break;
                                    }
                                }
                            }
                            if (has_method || ability_name == NULL) {
                                free(ability_name);
                                ability_tag = render_ability_ref_vtable_tag(ab);
                                ability_name = ability_tag;
                            } else {
                                free(ability_tag);
                            }
                            if (has_method)
                                break;
                        }
                        break;
                    }
                }

                if (ability_name != NULL) {
                    CodeBuf *args_buf = codebuf_create();
                    codebuf_write(args_buf, "%s.%s", party_var, slot_name);
                    for (size_t i = 0; i < call->data.call.arg_count; i++) {
                        char *arg = emit_expression(call->data.call.arguments[i], ctx);
                        codebuf_write(args_buf, ", %s", arg);
                        free(arg);
                    }

                    char *result = strdup_fmt("%s.%s_%s_vt->%s(%s)",
                                              party_var, slot_name, ability_name,
                                              method, args_buf->data);
                    codebuf_destroy(args_buf);
                    free(ability_name);
                    return result;
                }
            }
        }

        bool is_slot_method = (strcmp(method, "Write") == 0
                            || strcmp(method, "Read") == 0
                            || strcmp(method, "Release") == 0);

        if (obj != NULL && method != NULL) {
            const char *type_name = transpiler_resolve_nominal_host_expr_type_name(ctx, obj);
            if (type_name != NULL && is_nominal_host_type_name(ctx, type_name)) {
                CodeBuf *args_buf = codebuf_create();
                char stable_type_name[128];
                const char *owned_type_name = type_name;
                bool use_self_cell = is_pointer_self_host_type_name(ctx, owned_type_name);
                bool is_self_ident = (obj->type == AST_IDENTIFIER
                    && obj->data.identifier.name != NULL
                    && strcmp(obj->data.identifier.name, "self") == 0);
                ASTNode *method_decl = find_nominal_host_method_decl(ctx, owned_type_name, method);

                snprintf(stable_type_name, sizeof(stable_type_name), "%s",
                    owned_type_name != NULL ? owned_type_name : "Unknown");
                owned_type_name = stable_type_name;
                use_self_cell = is_pointer_self_host_type_name(ctx, owned_type_name);

                if (is_self_ident && use_self_cell) {
                    codebuf_write(args_buf, "self");
                } else {
                    char *obj_expr = emit_expression(obj, ctx);
                    /* Check if receiver is already a pointer (subject-ref param) */
                    bool already_pointer = false;
                    if (obj->type == AST_IDENTIFIER) {
                        TypedVarEntry *entry = lookup_typed_entry(ctx,
                            obj->data.identifier.name);
                        if (entry != NULL && entry->is_subject_ref)
                            already_pointer = true;
                    }
                    if (use_self_cell && !already_pointer)
                        codebuf_write(args_buf, "&%s", obj_expr);
                    else
                        codebuf_write(args_buf, "%s", obj_expr);
                    free(obj_expr);
                }

                for (size_t i = 0; i < call->data.call.arg_count; i++) {
                    ASTNode *arg_node = call->data.call.arguments[i];
                    char *arg = emit_expression(arg_node, ctx);
                    bool pass_by_ptr = false;
                    if (method_decl != NULL) {
                        size_t param_index = i;
                        if (method_decl->data.func_decl.param_count > 0) {
                            FuncParam *first = method_decl->data.func_decl.params[0];
                            if (first != NULL && first->name != NULL
                                && strcmp(first->name, "self") == 0)
                                param_index++;
                        }
                        if (param_index < method_decl->data.func_decl.param_count) {
                            FuncParam *param = method_decl->data.func_decl.params[param_index];
                            char *ptn = (param != NULL && param->type != NULL)
                                ? render_type_name(param->type)
                                : NULL;
                            if (ptn != NULL && is_pointer_self_host_type_name(ctx, ptn))
                                pass_by_ptr = true;
                            free(ptn);
                        }
                    }
                    if (pass_by_ptr) {
                        bool already_pointer = false;
                        bool is_self_arg = (arg_node != NULL
                            && arg_node->type == AST_IDENTIFIER
                            && arg_node->data.identifier.name != NULL
                            && strcmp(arg_node->data.identifier.name, "self") == 0);
                        if (arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
                            TypedVarEntry *entry = lookup_typed_entry(ctx,
                                arg_node->data.identifier.name);
                            if (entry != NULL && entry->is_subject_ref)
                                already_pointer = true;
                        }
                        if (is_self_arg && current_class_uses_self_cell(ctx))
                            codebuf_write(args_buf, ", self");
                        else if (already_pointer)
                            codebuf_write(args_buf, ", %s", arg);
                        else
                            codebuf_write(args_buf, ", &%s", arg);
                    } else {
                        codebuf_write(args_buf, ", %s", arg);
                    }
                    free(arg);
                }

                {
                    char *result = strdup_fmt("%s_%s(%s)",
                        owned_type_name, method, args_buf->data);
                    const char *source_slot_name =
                        assignment_target_root_slot_name(obj);
                    char *invalidation = NULL;
                    char *action_sync = NULL;
                    char *post_sync = NULL;
                    ASTNode *saved_host_decl = transpiler_current_host_decl_local(ctx);
                    const char *saved_receiver_expr = ctx->current_overlay_receiver_expr;

                    if (saved_host_decl != NULL && saved_host_decl->type == AST_WORLD_DECL) {
                        const char *zone_slot_name = NULL;
                        const char *zone_type_name = NULL;
                        const char *zone_subject_slot_name = NULL;
                        const char *zone_subject_type_name = NULL;

                        if (resolve_world_zone_subject_receiver(ctx, obj,
                                &zone_slot_name, &zone_type_name,
                                &zone_subject_slot_name, &zone_subject_type_name)
                            && zone_slot_name != NULL
                            && zone_type_name != NULL
                            && zone_subject_slot_name != NULL
                            && zone_subject_type_name != NULL
                            && strcmp(zone_subject_type_name, owned_type_name) == 0) {
                            ASTNode *zone_decl = find_zone_decl(ctx, zone_type_name);
                            if (zone_decl != NULL)
                                transpiler_bind_current_host_decl_local(ctx, zone_decl);
                            ctx->current_overlay_receiver_expr =
                                strdup_fmt("(&self->%s)", zone_slot_name);
                            source_slot_name = zone_subject_slot_name;
                        }
                    }

                    invalidation =
                        emit_current_overlay_method_projection_invalidation(
                            ctx, source_slot_name, owned_type_name, method_decl);
                    if (ctx->current_overlay_receiver_expr != NULL
                        && ctx->current_overlay_receiver_expr != saved_receiver_expr) {
                        free((char *)ctx->current_overlay_receiver_expr);
                    }
                    ctx->current_overlay_receiver_expr = saved_receiver_expr;
                    transpiler_bind_current_host_decl_local(ctx, saved_host_decl);
                    action_sync = emit_world_embedded_action_effect_sync(
                        ctx, obj, method_decl);
                    post_sync = emit_world_embedded_receiver_projection_sync(ctx, obj);
                    codebuf_destroy(args_buf);
                    if (invalidation != NULL || action_sync != NULL || post_sync != NULL) {
                        char *ret_type_name = NULL;
                        char *wrapped = NULL;
                        const char *prefix = invalidation != NULL ? invalidation : "";
                        const char *effect_suffix = action_sync != NULL ? action_sync : "";
                        const char *suffix = post_sync != NULL ? post_sync : "";

                        if (method_decl->data.func_decl.return_type != NULL)
                            ret_type_name = render_type_name(
                                method_decl->data.func_decl.return_type);

                        if (ret_type_name != NULL
                            && strcmp(ret_type_name, "Void") != 0) {
                            int tmp_id = ++ctx->tmp_counter;
                            wrapped = strdup_fmt(
                                "({ %s _pgy_call_%d = %s; %s%s%s_pgy_call_%d; })",
                                pergyra_type_to_c(ret_type_name), tmp_id,
                                result, prefix, effect_suffix, suffix, tmp_id);
                        } else {
                            wrapped = strdup_fmt("({ %s; %s%s%s})",
                                result, prefix, effect_suffix, suffix);
                        }

                        free(ret_type_name);
                        free(invalidation);
                        free(action_sync);
                        free(post_sync);
                        free(result);
                        return wrapped;
                    }
                    free(action_sync);
                    free(post_sync);
                    return result;
                }
            }
        }

        if (is_slot_method && obj->type == AST_IDENTIFIER) {
            const char *inner = lookup_slot_type(ctx, obj->data.identifier.name);
            bool is_secure = lookup_slot_is_secure(ctx, obj->data.identifier.name);
            bool saved_suppress = ctx->suppress_slot_auto_read;
            ctx->suppress_slot_auto_read = true;
            char *obj_expr = emit_expression(obj, ctx);
            ctx->suppress_slot_auto_read = saved_suppress;
            if (inner == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for method call on '%s'",
                    obj->data.identifier.name != NULL
                        ? obj->data.identifier.name
                        : "<slot>");
                free(obj_expr);
                return pergyra_strdup("0");
            }
            char *slot_ref = slot_ref_expr(ctx, obj->data.identifier.name, obj_expr);

            if (strcmp(method, "Write") == 0 && call->data.call.arg_count >= 1) {
                char *val_expr = emit_expression(call->data.call.arguments[0], ctx);
                char *result;
                if (is_secure && call->data.call.arg_count >= 2) {
                    char *tok = emit_expression(call->data.call.arguments[1], ctx);
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s)",
                                        inner, slot_ref, val_expr, tok);
                    free(tok);
                } else if (is_secure) {
                    const char *token_name =
                        lookup_slot_token_name(ctx, obj->data.identifier.name);
                    char fallback_token[96];
                    if (token_name == NULL) {
                        snprintf(fallback_token, sizeof(fallback_token),
                                 "%s_token", obj->data.identifier.name);
                        token_name = fallback_token;
                    }
                    result = strdup_fmt("pgy_secure_write_%s(%s, %s, &%s)",
                                        inner, slot_ref, val_expr, token_name);
                } else {
                    result = strdup_fmt("pgy_write_%s(%s, %s)",
                                        inner, slot_ref, val_expr);
                }
                free(val_expr);
                free(slot_ref);
                free(obj_expr);
                return result;
            } else if (strcmp(method, "Read") == 0) {
                char *result;
                if (is_secure) {
                    const char *token_name =
                        lookup_slot_token_name(ctx, obj->data.identifier.name);
                    char fallback_token[96];
                    if (token_name == NULL) {
                        snprintf(fallback_token, sizeof(fallback_token),
                                 "%s_token", obj->data.identifier.name);
                        token_name = fallback_token;
                    }
                    result = strdup_fmt("pgy_secure_read_%s(%s, &%s)",
                                        inner, slot_ref, token_name);
                } else {
                    result = strdup_fmt("pgy_read_%s(%s)", inner, slot_ref);
                }
                free(slot_ref);
                free(obj_expr);
                return result;
            } else if (strcmp(method, "Release") == 0) {
                char *result;
                if (is_secure) {
                    const char *token_name =
                        lookup_slot_token_name(ctx, obj->data.identifier.name);
                    char fallback_token[96];
                    if (token_name == NULL) {
                        snprintf(fallback_token, sizeof(fallback_token),
                                 "%s_token", obj->data.identifier.name);
                        token_name = fallback_token;
                    }
                    result = strdup_fmt("pgy_secure_release_%s(%s, &%s)",
                                        inner, slot_ref, token_name);
                } else {
                    result = strdup_fmt("pgy_release_%s(%s)", inner, slot_ref);
                }
                /* Mark as released */
                for (int ri = 0; ri < ctx->slot_var_count; ri++) {
                    if (strcmp(ctx->slot_vars[ri].name,
                              obj->data.identifier.name) == 0) {
                        ctx->slot_vars[ri].released = true;
                        break;
                    }
                }
                free(slot_ref);
                free(obj_expr);
                return result;
            }
            free(slot_ref);
            free(obj_expr);
        }

        {
            const char *receiver_type = infer_expression_type_name(ctx, obj);
            if (method != NULL
                && strcmp(method, "Slice") == 0
                && receiver_type != NULL
                && (strncmp(receiver_type, "Array<", 6) == 0
                    || strncmp(receiver_type, "Slice<", 6) == 0)
                && call->data.call.arg_count == 2) {
                const char *inner = slot_inner_type_name(receiver_type);
                char *start_expr = emit_expression(call->data.call.arguments[0], ctx);
                char *len_expr = emit_expression(call->data.call.arguments[1], ctx);
                char *result = NULL;
                int tmp_id = ++ctx->tmp_counter;

                if (inner == NULL) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slice element type for receiver '%s'",
                        receiver_type);
                    free(start_expr);
                    free(len_expr);
                    return pergyra_strdup("0");
                }

                if (strncmp(receiver_type, "Array<", 6) == 0) {
                    char *obj_expr = emit_expression(obj, ctx);
                    result = strdup_fmt(
                        "({ PgyArray_%s _pgy_arr_%d = %s; pgy_array_slice_%s(&_pgy_arr_%d, (size_t)(%s), (size_t)(%s)); })",
                        inner, tmp_id, obj_expr, inner, tmp_id, start_expr, len_expr);
                    free(obj_expr);
                } else {
                    char *obj_expr = emit_expression(obj, ctx);
                    result = strdup_fmt(
                        "({ PgySlice_%s _pgy_slice_%d = %s; size_t _pgy_start_%d = (size_t)(%s); size_t _pgy_len_%d = (size_t)(%s); if (_pgy_start_%d + _pgy_len_%d > _pgy_slice_%d.length) PGY_PANIC(\"Slice out of bounds\"); (PgySlice_%s){ _pgy_slice_%d.data + _pgy_start_%d, _pgy_len_%d }; })",
                        inner, tmp_id, obj_expr,
                        tmp_id, start_expr,
                        tmp_id, len_expr,
                        tmp_id, tmp_id, tmp_id,
                        inner, tmp_id, tmp_id, tmp_id);
                    free(obj_expr);
                }

                free(start_expr);
                free(len_expr);
                return result;
            }
        }
    }

    return NULL;
}

static char *
emit_call_user_function(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    /* User function call */
    if (callee->type == AST_IDENTIFIER) {
        ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
        ASTNode *host_method = current_host_method_decl(ctx, callee->data.identifier.name);
        const char *host_name = transpiler_decl_name_local(host_decl);
        if (host_method != NULL && host_name != NULL) {
            CodeBuf *args_buf = codebuf_create();
            codebuf_write(args_buf, "self");
            for (size_t i = 0; i < call->data.call.arg_count; i++) {
                char *arg = emit_expression(call->data.call.arguments[i], ctx);
                codebuf_write(args_buf, ", %s", arg);
                free(arg);
            }
            {
                char *result = strdup_fmt("%s_%s(%s)",
                    host_name, callee->data.identifier.name, args_buf->data);
                codebuf_destroy(args_buf);
                return result;
            }
        }
    }

    char *callee_str = NULL;
    if (callee->type == AST_IDENTIFIER) {
        ASTNode *decl = find_function_decl(ctx, callee->data.identifier.name);
        if (func_has_generic_params(decl)) {
            const char *specialized_name = ensure_generic_specialization(ctx, decl, call);
            if (specialized_name != NULL)
                callee_str = pergyra_strdup(specialized_name);
        }
    }
    if (callee_str == NULL)
        callee_str = emit_expression(callee, ctx);

    /* Build argument list */
    CodeBuf *args_buf = codebuf_create();
    ASTNode *decl = (callee->type == AST_IDENTIFIER)
        ? find_callable_decl(ctx, callee->data.identifier.name) : NULL;
    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        FuncParam *param = (decl != NULL && decl->type == AST_FUNC_DECL
                            && i < decl->data.func_decl.param_count)
            ? decl->data.func_decl.params[i] : NULL;
        ASTNode *intent_param_type = NULL;
        bool handled = false;
        char *arg = NULL;

        if (decl != NULL && decl->type == AST_INTENT_DECL) {
            size_t binding_count = decl->data.intent_decl.binding_count > 0
                ? decl->data.intent_decl.binding_count
                : (decl->data.intent_decl.involve_count
                    + decl->data.intent_decl.value_count);
            if (i < binding_count) {
                ASTNode *binding = decl->data.intent_decl.binding_count > 0
                    ? decl->data.intent_decl.bindings[i]
                    : (i < decl->data.intent_decl.involve_count
                        ? decl->data.intent_decl.involves[i]
                        : decl->data.intent_decl.values[i - decl->data.intent_decl.involve_count]);
                if (binding != NULL && binding->type == AST_INTENT_INVOLVES)
                    intent_param_type = binding->data.intent_involves.subject_type;
                else if (binding != NULL && binding->type == AST_INTENT_VALUE)
                    intent_param_type = binding->data.intent_value.value_type;
            }
        }

        if (param != NULL && param->type != NULL
            && (param->mode == PARAM_MODE_OWN || param->mode == PARAM_MODE_REF)) {
            char *param_type = render_type_name(param->type);
            bool slot_param = param_type != NULL
                && (strncmp(param_type, "Slot<", 5) == 0
                    || strncmp(param_type, "SecureSlot<", 11) == 0);
            bool secure_param = param_type != NULL
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
emit_call(ASTNode *call, TranspilerCtx *ctx)
{
    ASTNode    *callee = call->data.call.callee;
    BuiltinKind bk     = BUILTIN_NOT_BUILTIN;

    if (callee->type == AST_IDENTIFIER) {
        const char *callee_name = callee->data.identifier.name;
        bk = builtin_resolve(callee_name);
        if ((bk == BUILTIN_BOX || bk == BUILTIN_RC_NEW)
            && find_class_decl(ctx, callee_name) != NULL) {
            bk = BUILTIN_NOT_BUILTIN;
        }
    }

    bool handled = false;
    char *result = emit_call_builtin_dispatch(call, bk, ctx, &handled);
    if (handled)
        return result;

    result = emit_call_domain_constructor(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_result_option_builtin(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_stdlib_builtin(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_event_builtin(call, callee, ctx);
    if (result != NULL)
        return result;
    result = emit_call_member_style(call, callee, ctx);
    if (result != NULL)
        return result;
    return emit_call_user_function(call, callee, ctx);
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
