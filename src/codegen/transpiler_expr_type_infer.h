#include "transpiler_builtin_type_table.h"

#include "codegen_slot_type_policy.h"

static const char *
infer_expression_type_name(TranspilerCtx *ctx, ASTNode *expr)
{
    static char derived_call_type[128];

    if (expr == NULL)
        return "Unknown";

    switch (expr->type) {
    case AST_NUMBER:
        if (expr->data.number.is_long)
            return "Long";
        return expr->data.number.value == (int64_t)expr->data.number.value
            ? "Int"
            : "Float";
    case AST_STRING:
        return "String";
    case AST_BOOLEAN:
        return "Bool";
    case AST_ARRAY_LITERAL: {
        const char *inner = NULL;
        if (expr->data.array_literal.count > 0) {
            inner = infer_expression_type_name(ctx, expr->data.array_literal.elements[0]);
        } else if (ctx != NULL
                   && ctx->expected_type != NULL
                   && strncmp(ctx->expected_type, "Array<", 6) == 0) {
            inner = slot_inner_type_name(ctx->expected_type);
        }
        if (inner == NULL || inner[0] == '\0')
            inner = "Unknown";
        static char buf[128];
        snprintf(buf, sizeof(buf), "Array<%s>", inner);
        return buf;
    }
    case AST_ARRAY_ACCESS: {
        const char *array_type = infer_expression_type_name(ctx, expr->data.array_access.array);
        if (strncmp(array_type, "Array<", 6) == 0 || strncmp(array_type, "Slice<", 6) == 0)
            return slot_inner_type_name(array_type);
        return "Unknown";
    }
    case AST_IDENTIFIER: {
        if (strcmp(expr->data.identifier.name, "None") == 0) {
            const char *context_type = transpiler_contextual_option_type_name(ctx);
            return context_type != NULL ? context_type : "Option<Unknown>";
        }
        ASTNode *alias_expr = lookup_alias_expr(ctx, expr->data.identifier.name);
        if (alias_expr != NULL)
            return infer_expression_type_name(ctx, alias_expr);
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL)
            return type_name;
        type_name = transpiler_current_field_type_name(ctx, expr->data.identifier.name);
        if (type_name != NULL)
            return type_name;
        {
            const char *enum_variant = lookup_enum_variant_qualified_name(ctx,
                expr->data.identifier.name);
            if (enum_variant != NULL) {
                size_t len = strcspn(enum_variant, "_");
                static char enum_name[128];
                if (len >= sizeof(enum_name))
                    len = sizeof(enum_name) - 1;
                memcpy(enum_name, enum_variant, len);
                enum_name[len] = '\0';
                return enum_name;
            }
        }
        return "Unknown";
    }
    case AST_CHANNEL_RECV: {
        ASTNode *channel = expr->data.channel_recv.channel;
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
            if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                return slot_inner_type_name(type_name);
        }
        return "Unknown";
    }
    case AST_MEMBER_ACCESS: {
        const char *resolved = transpiler_resolve_nominal_host_expr_type_name(ctx, expr);
        if (resolved != NULL)
            return resolved;
        if (expr->data.member.object != NULL && expr->data.member.name != NULL) {
            const char *obj_type = infer_expression_type_name(ctx, expr->data.member.object);
            if (obj_type != NULL) {

                ASTNode *obj_decl = find_class_decl(ctx, obj_type);
                if (obj_decl != NULL) {
                    for (size_t fi = 0; fi < obj_decl->data.class_decl.field_count; fi++) {
                        ClassField *f = obj_decl->data.class_decl.fields[fi];
                        if (f != NULL && f->name != NULL && f->type != NULL
                            && strcmp(f->name, expr->data.member.name) == 0) {
                            char *ft = render_type_name(f->type);
                            if (ft != NULL) {
                                static char mbuf[128];
                                snprintf(mbuf, sizeof(mbuf), "%s", ft);
                                free(ft);
                                return mbuf;
                            }
                        }
                    }
                }
            }
        }
        return "Unknown";
    }
    case AST_BINARY: {
        PgyTokenType op = expr->data.binary.op.type;
        const char *left_type = infer_expression_type_name(ctx, expr->data.binary.left);
        const char *right_type = infer_expression_type_name(ctx, expr->data.binary.right);
        if (op == TOKEN_PLUS) {
            if ((left_type != NULL && strcmp(left_type, "String") == 0)
                || (right_type != NULL && strcmp(right_type, "String") == 0)) {
                return "String";
            }
            {
                ASTNode *cursor = expr->data.binary.left;
                while (cursor != NULL && cursor->type == AST_BINARY
                       && cursor->data.binary.op.type == TOKEN_PLUS) {
                    cursor = cursor->data.binary.left;
                }
                if (cursor != NULL) {
                    const char *leaf_type = infer_expression_type_name(ctx, cursor);
                    if (leaf_type != NULL && strcmp(leaf_type, "String") == 0)
                        return "String";
                }
            }
            return "Int";
        }
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL
            || op == TOKEN_AND || op == TOKEN_OR) {
            return "Bool";
        }
        if (op == TOKEN_MINUS || op == TOKEN_STAR
            || op == TOKEN_SLASH || op == TOKEN_PERCENT) {
            if ((left_type != NULL && strcmp(left_type, "Float") == 0)
                || (right_type != NULL && strcmp(right_type, "Float") == 0))
                return "Float";
            if ((left_type != NULL && strcmp(left_type, "Long") == 0)
                || (right_type != NULL && strcmp(right_type, "Long") == 0))
                return "Long";
            return "Int";
        }
        return "Unknown";
    }
    case AST_CALL:
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_MEMBER_ACCESS
            && expr->data.call.callee->data.member.name != NULL) {
            ASTNode *receiver = expr->data.call.callee->data.member.object;
            const char *method_name = expr->data.call.callee->data.member.name;
            const char *receiver_type = infer_expression_type_name(ctx, receiver);
            if (receiver_type != NULL && method_name != NULL) {
                if (pgy_codegen_type_name_is_slot_or_view(receiver_type)
                    && strcmp(method_name, "Read") == 0) {
                    return slot_inner_type_name(receiver_type);
                }
                if (pgy_codegen_type_name_is_device_slot(receiver_type)
                    && strcmp(method_name, "Read") == 0) {
                    return slot_inner_type_name(receiver_type);
                }
                if (pgy_codegen_type_name_is_slot_family(receiver_type)
                    && (strcmp(method_name, "Write") == 0
                        || strcmp(method_name, "Release") == 0)) {
                    return "Void";
                }
                if ((strncmp(receiver_type, "Array<", 6) == 0
                     || strncmp(receiver_type, "Slice<", 6) == 0)
                    && strcmp(method_name, "Slice") == 0) {
                    static char slice_buf[128];
                    const char *inner = slot_inner_type_name(receiver_type);
                    if (inner == NULL || inner[0] == '\0')
                        return "Unknown";
                    snprintf(slice_buf, sizeof(slice_buf), "Slice<%s>",
                        inner);
                    return slice_buf;
                }
            }
            ASTNode *method_decl = NULL;
            receiver_type = transpiler_resolve_nominal_host_expr_type_name(ctx, receiver);
            if (receiver_type != NULL) {
                method_decl = find_nominal_host_method_decl(ctx, receiver_type,
                    method_name);
            }
            if (method_decl != NULL && method_decl->type == AST_FUNC_DECL
                && method_decl->data.func_decl.return_type != NULL) {
                char *resolved = render_type_name(method_decl->data.func_decl.return_type);
                snprintf(derived_call_type, sizeof(derived_call_type), "%s", resolved);
                free(resolved);
                return derived_call_type;
            }
        }
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_IDENTIFIER
            && expr->data.call.callee->data.identifier.name != NULL) {
            const char *name = expr->data.call.callee->data.identifier.name;
            const char *simple_type = NULL;
            if (strcmp(name, "Min") == 0
                || strcmp(name, "Max") == 0
                || strcmp(name, "Abs") == 0
                || strcmp(name, "Clone") == 0) {
                if (expr->data.call.arg_count >= 1) {
                    const char *arg_type = infer_expression_type_name(ctx,
                        expr->data.call.arguments[0]);
                    if (arg_type != NULL && strcmp(arg_type, "Unknown") != 0)
                        return arg_type;
                }
                return "Int";
            }
            if (strcmp(name, "MapGet") == 0 && expr->data.call.arg_count >= 1) {
                const char *map_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0)
                    return constructed_arg_name_at(map_type, 1);
                return "Unknown";
            }
            if (strcmp(name, "MapKeys") == 0 && expr->data.call.arg_count >= 1) {
                static char map_keys_buf[128];
                const char *map_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
                    char key_buf[64];
                    copy_constructed_arg_name_at(map_type, 0,
                        key_buf, sizeof(key_buf));
                    if (key_buf[0] == '\0')
                        return "Unknown";
                    snprintf(map_keys_buf, sizeof(map_keys_buf), "Array<%s>",
                        key_buf);
                    return map_keys_buf;
                }
                return "Unknown";
            }
            if (strcmp(name, "ListGet") == 0 && expr->data.call.arg_count >= 1) {
                const char *list_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                    return slot_inner_type_name(list_type);
                return "Unknown";
            }
            if (strcmp(name, "ClaimDeviceSlot") == 0) {
                if (ctx != NULL
                    && ctx->active_type_hint != NULL
                    && strncmp(ctx->active_type_hint, "DeviceSlot<", 11) == 0) {
                    return ctx->active_type_hint;
                }
                return "Unknown";
            }
            if (strcmp(name, "ViewRead") == 0 && expr->data.call.arg_count >= 1) {
                static char read_view[128];
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                const char *inner = slot_inner_type_name(slot_type);
                if (inner == NULL || inner[0] == '\0')
                    return "Unknown";
                snprintf(read_view, sizeof(read_view), "ReadView<%s>",
                    inner);
                return read_view;
            }
            if (strcmp(name, "ViewWrite") == 0 && expr->data.call.arg_count >= 1) {
                static char write_view[128];
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                const char *inner = slot_inner_type_name(slot_type);
                if (inner == NULL || inner[0] == '\0')
                    return "Unknown";
                snprintf(write_view, sizeof(write_view), "WriteView<%s>",
                    inner);
                return write_view;
            }
            if (strcmp(name, "Measure") == 0 || strcmp(name, "QubitState") == 0)
                return "Int";
            if (strcmp(name, "Read") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (pgy_codegen_type_name_is_slot_family(slot_type)) {
                    const char *inner = slot_inner_type_name(slot_type);
                    return (inner != NULL && inner[0] != '\0') ? inner : "Unknown";
                }
            }
            if ((strcmp(name, "Write") == 0 || strcmp(name, "Release") == 0)
                && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (pgy_codegen_type_name_is_slot_family(slot_type)) {
                    return "Void";
                }
            }
            if (strcmp(name, "DeviceRead") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    const char *inner = slot_inner_type_name(slot_type);
                    return (inner != NULL && inner[0] != '\0') ? inner : "Unknown";
                }
            }
            if (strcmp(name, "SubmitDeviceRead") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    static char device_future[128];
                    const char *inner = slot_inner_type_name(slot_type);
                    if (inner == NULL || inner[0] == '\0')
                        return "Unknown";
                    snprintf(device_future, sizeof(device_future), "RemoteFuture<%s>",
                        inner);
                    return device_future;
                }
                return "Unknown";
            }
            if ((strcmp(name, "TryRecv") == 0
                 || strcmp(name, "RecvTimeout") == 0
                 || strcmp(name, "TrySendStatus") == 0
                 || strcmp(name, "SendTimeoutStatus") == 0)
                && expr->data.call.arg_count >= 1) {
                static char opt_buf[128];
                if (strcmp(name, "TrySendStatus") == 0
                    || strcmp(name, "SendTimeoutStatus") == 0) {
                    snprintf(opt_buf, sizeof(opt_buf), "Option<Bool>");
                } else {
                    const char *inner = channel_inner_type_name(ctx,
                        expr->data.call.arguments[0]);
                    snprintf(opt_buf, sizeof(opt_buf), "Option<%s>", inner);
                }
                return opt_buf;
            }
            if (strcmp(name, "Some") == 0 && expr->data.call.arg_count == 1) {
                static char opt_buf[128];
                const char *inner = infer_expression_type_name(ctx, expr->data.call.arguments[0]);
                snprintf(opt_buf, sizeof(opt_buf), "Option<%s>", inner);
                return opt_buf;
            }
            if (strcmp(name, "None") == 0) {
                const char *context_type = transpiler_contextual_option_type_name(ctx);
                return context_type != NULL ? context_type : "Option<Unknown>";
            }
            if ((strcmp(name, "IsSome") == 0 || strcmp(name, "IsNone") == 0)
                && expr->data.call.arg_count == 1)
                return "Bool";
            simple_type = pgy_builtin_simple_return_type(name);
            if (simple_type != NULL)
                return simple_type;
            if (strcmp(name, "UnwrapOption") == 0 && expr->data.call.arg_count == 1) {
                const char *opt_type = infer_expression_type_name(ctx, expr->data.call.arguments[0]);
                if (strncmp(opt_type, "Option<", 7) == 0)
                    return slot_inner_type_name(opt_type);
            }
            if (strcmp(name, "ToTObject") == 0 && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER
                && expr->data.call.arguments[0]->data.identifier.name != NULL) {
                return expr->data.call.arguments[0]->data.identifier.name;
            }
            if (strcmp(name, "ToObject") == 0 && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER
                && expr->data.call.arguments[0]->data.identifier.name != NULL) {
                return expr->data.call.arguments[0]->data.identifier.name;
            }
            if (find_class_decl(ctx, name) != NULL
                || find_subject_host_decl(ctx, name) != NULL
                || find_relation_decl(ctx, name) != NULL
                || find_effect_decl(ctx, name) != NULL
                || find_zone_decl(ctx, name) != NULL
                || find_world_decl(ctx, name) != NULL) {
                return name;
            }
            if (find_intent_decl(ctx, name) != NULL)
                return "Bool";

            {
                ASTNode *host_method = current_host_method_decl(ctx, name);
                if (host_method != NULL
                    && host_method->type == AST_FUNC_DECL
                    && host_method->data.func_decl.return_type != NULL) {
                    char *resolved = render_type_name(host_method->data.func_decl.return_type);
                    snprintf(derived_call_type, sizeof(derived_call_type), "%s", resolved);
                    free(resolved);
                    return derived_call_type;
                }
            }

            {
                ASTNode *decl = find_function_decl(ctx, name);
                if (decl != NULL && decl->data.func_decl.return_type != NULL) {
                    char *resolved = NULL;
                    if (func_has_generic_params(decl)) {
                        GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
                        size_t binding_count = 0;
                        if (infer_generic_call_bindings(ctx, decl, expr, bindings, &binding_count))
                            resolved = render_type_name_with_bindings(ctx,
                                decl->data.func_decl.return_type, bindings, binding_count);
                    }
                    if (resolved == NULL)
                        resolved = render_type_name(decl->data.func_decl.return_type);
                    snprintf(derived_call_type, sizeof(derived_call_type), "%s", resolved);
                    free(resolved);
                    return derived_call_type;
                }
            }
        }
        return "Unknown";
    case AST_UNARY:
        if (expr->data.unary.op.type == TOKEN_NOT)
            return "Bool";
        return infer_expression_type_name(ctx, expr->data.unary.operand);
    default:
        return "Unknown";
    }
}
