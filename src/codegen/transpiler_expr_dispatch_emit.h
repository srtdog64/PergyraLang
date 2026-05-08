#ifndef PGY_TRANSPILER_EXPR_DISPATCH_EMIT_H
#define PGY_TRANSPILER_EXPR_DISPATCH_EMIT_H

#include "../common/string_compat.h"

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
        /* None is target-typed; without contextual Option<T> semantic should
         * already reject it, and the backend keeps a hard guard. */
        if (strcmp(id_name, "None") == 0) {
            return transpiler_emit_none_with_context(ctx, node);
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
            && transpiler_current_world_has_field(ctx, id_name)) {
            return strdup_fmt("self->%s", id_name);
        }
        const char *ssa_name = transpiler_resolve_active_ssa_name(ctx, id_name);
        if (ssa_name != NULL) {
            char *c_ssa_name = transpiler_make_c_ssa_name(ctx, ssa_name);
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
                if (inner == NULL || inner[0] == '\0'
                    || strcmp(inner, "Unknown") == 0) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C slot SSA auto-read requires concrete Slot<T> payload metadata");
                    free(c_ssa_name);
                    return pergyra_strdup("0");
                }
                const char *token_name = secure
                    ? require_slot_token_name(ctx, id_name, "SecureSlot SSA auto-read")
                    : NULL;
                if (secure && token_name == NULL) {
                    free(c_ssa_name);
                    return pergyra_strdup("0");
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
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for auto-read of '%s'",
                    id_name != NULL ? id_name : "<slot>");
                return pergyra_strdup("0");
            }
            char *slot_ref = slot_ref_expr(ctx, id_name, id_name);
            const char *token_name = secure
                ? require_slot_token_name(ctx, id_name, "SecureSlot auto-read")
                : NULL;
            if (secure && token_name == NULL) {
                free(slot_ref);
                return pergyra_strdup("0");
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
        if (array_type != NULL && strncmp(array_type, "Array<", 6) == 0) {
            const char *inner = slot_inner_type_name(array_type);
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C array access requires concrete Array<T> element metadata");
                free(array);
                free(index);
                return pergyra_strdup("0");
            }
            int tmp_id = ++ctx->tmp_counter;
            result = strdup_fmt(
                "({ PgyArray_%s _pgy_arr_get_%d = %s; "
                "pgy_array_get_%s(&_pgy_arr_get_%d, %s); })",
                inner, tmp_id, array, inner, tmp_id, index);
        } else if (array_type != NULL && strncmp(array_type, "Slice<", 6) == 0) {
            const char *inner = slot_inner_type_name(array_type);
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C slice access requires concrete Slice<T> element metadata");
                free(array);
                free(index);
                return pergyra_strdup("0");
            }
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
        /* Determine tuple type name from concrete context. Priority:
         *   1) ctx->expected_type (let binding with annotation)
         *   2) ctx->current_return_type (inside `return` in tuple-returning fn)
         *   3) Build from concrete element type names.
         * All three must start with '(' for tuple classification. */
        char tuple_name_buf[256];
        const char *tuple_name = NULL;
        if (ctx->expected_type != NULL && ctx->expected_type[0] == '(')
            tuple_name = ctx->expected_type;
        else if (ctx->current_return_type[0] == '(')
            tuple_name = ctx->current_return_type;
        if (tuple_name == NULL) {
            size_t off = 0;
            tuple_name_buf[0] = '\0';
            off = pergyra_str_append(tuple_name_buf, sizeof(tuple_name_buf), "(");
            for (size_t i = 0; i < node->data.tuple_literal.count; i++) {
                const char *et =
                    infer_expression_type_name(ctx, node->data.tuple_literal.elements[i]);
                if (et == NULL || et[0] == '\0'
                    || strcmp(et, "Unknown") == 0) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C backend: tuple literal requires concrete element type metadata");
                    return pergyra_strdup("0");
                }
                if (i > 0) {
                    off = pergyra_str_append(tuple_name_buf,
                        sizeof(tuple_name_buf), ", ");
                }
                off = pergyra_str_append(tuple_name_buf,
                    sizeof(tuple_name_buf), et);
            }
            (void)off;
            (void)pergyra_str_append(tuple_name_buf, sizeof(tuple_name_buf), ")");
            tuple_name = tuple_name_buf;
        }
        const char *ctype = pergyra_type_to_c(tuple_name);
        if (ctype == NULL || ctype[0] == '\0'
            || strcmp(ctype, "Unknown") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C tuple literal requires concrete tuple layout metadata");
            return pergyra_strdup("0");
        }
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
        if (array_type == NULL || strncmp(array_type, "Array<", 6) != 0
            || inner == NULL || inner[0] == '\0'
            || strcmp(inner, "Unknown") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C array literal requires concrete Array<T> element metadata");
            return pergyra_strdup("0");
        }
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
                if (inner == NULL || inner[0] == '\0'
                    || strcmp(inner, "Unknown") == 0) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine slot payload type for assignment to '%s'",
                        tgt_name != NULL ? tgt_name : "<slot>");
                    return pergyra_strdup("0");
                }
                char *value = emit_expression(node->data.assignment.value, ctx);
                char *slot_ref = slot_ref_expr(ctx, tgt_name, tgt_name);
                char *result;
                if (secure) {
                    const char *token_name = require_slot_token_name(
                        ctx, tgt_name, "SecureSlot assignment");
                    if (token_name == NULL) {
                        free(slot_ref);
                        free(value);
                        return pergyra_strdup("0");
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
            if (expr == NULL
                || inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C await expression requires concrete Future<T> result metadata");
                free(expr);
                return pergyra_strdup("0");
            }
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

#endif /* PGY_TRANSPILER_EXPR_DISPATCH_EMIT_H */
