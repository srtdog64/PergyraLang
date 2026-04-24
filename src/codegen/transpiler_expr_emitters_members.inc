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
            const char *type_name = resolve_nominal_host_expr_type_name(ctx, obj);
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
