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
        const char *inner = "Int";
        if (expr->data.array_literal.count > 0)
            inner = infer_expression_type_name(ctx, expr->data.array_literal.elements[0]);
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
                if ((strncmp(receiver_type, "Slot<", 5) == 0
                     || strncmp(receiver_type, "SecureSlot<", 11) == 0
                     || strncmp(receiver_type, "ReadView<", 9) == 0
                     || strncmp(receiver_type, "WriteView<", 10) == 0)
                    && strcmp(method_name, "Read") == 0) {
                    return slot_inner_type_name(receiver_type);
                }
                if ((strncmp(receiver_type, "DeviceSlot<", 11) == 0)
                    && strcmp(method_name, "Read") == 0) {
                    return slot_inner_type_name(receiver_type);
                }
                if (((strncmp(receiver_type, "Slot<", 5) == 0
                      || strncmp(receiver_type, "SecureSlot<", 11) == 0
                      || strncmp(receiver_type, "ReadView<", 9) == 0
                      || strncmp(receiver_type, "WriteView<", 10) == 0
                      || strncmp(receiver_type, "DeviceSlot<", 11) == 0))
                    && (strcmp(method_name, "Write") == 0
                        || strcmp(method_name, "Release") == 0)) {
                    return "Void";
                }
                if ((strncmp(receiver_type, "Array<", 6) == 0
                     || strncmp(receiver_type, "Slice<", 6) == 0)
                    && strcmp(method_name, "Slice") == 0) {
                    static char slice_buf[128];
                    const char *inner = slot_inner_type_name(receiver_type);
                    snprintf(slice_buf, sizeof(slice_buf), "Slice<%s>",
                        inner != NULL ? inner : "Int");
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
            if (strcmp(name, "ToInt") == 0)
                return "Int";
            if (strcmp(name, "ToFloat") == 0)
                return "Float";
            if (strcmp(name, "Floor") == 0
                || strcmp(name, "Ceil") == 0
                || strcmp(name, "Sqrt") == 0
                || strcmp(name, "Pow") == 0)
                return "Float";
            if (strcmp(name, "Replace") == 0
                || strcmp(name, "Trim") == 0
                || strcmp(name, "Upper") == 0
                || strcmp(name, "Lower") == 0
                || strcmp(name, "Join") == 0)
                return "String";
            if (strcmp(name, "Split") == 0)
                return "Array<String>";
            if (strcmp(name, "Length") == 0
                || strcmp(name, "Random") == 0)
                return "Int";
            if (strcmp(name, "FileOpen") == 0)
                return "Int";
            if (strcmp(name, "FileRead") == 0)
                return "String";
            if (strcmp(name, "FileWrite") == 0
                || strcmp(name, "FileClose") == 0
                || strcmp(name, "WriteFile") == 0)
                return "Void";
            if (strcmp(name, "ReadFile") == 0)
                return "String";
            if (strcmp(name, "MapNew") == 0)
                return "HashMap";
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
                    snprintf(map_keys_buf, sizeof(map_keys_buf), "Array<%s>",
                        key_buf[0] != '\0' ? key_buf : "Int");
                    return map_keys_buf;
                }
                snprintf(map_keys_buf, sizeof(map_keys_buf), "Array<Int>");
                return map_keys_buf;
            }
            if (strcmp(name, "ToString") == 0)
                return "String";
            if (strcmp(name, "IntentLastTrace") == 0
                || strcmp(name, "IntentLastFailure") == 0
                || strcmp(name, "IntentLastName") == 0
                || strcmp(name, "IntentHistoryStepName") == 0
                || strcmp(name, "IntentHistoryStepZone") == 0
                || strcmp(name, "IntentHistoryStepPhase") == 0
                || strcmp(name, "IntentHistoryStepParticipant") == 0
                || strcmp(name, "IntentHistoryStepSlot") == 0
                || strcmp(name, "IntentHistoryStepFromZone") == 0
                || strcmp(name, "IntentHistoryStepFromSlot") == 0
                || strcmp(name, "IntentHistoryStepToZone") == 0
                || strcmp(name, "IntentHistoryStepToSlot") == 0
                || strcmp(name, "IntentHistoryStepFailure") == 0
                || strcmp(name, "IntentActiveName") == 0
                || strcmp(name, "IntentActiveFailure") == 0
                || strcmp(name, "IntentActiveTrace") == 0
                || strcmp(name, "IntentRecentName") == 0
                || strcmp(name, "IntentRecentTrace") == 0
                || strcmp(name, "IntentRecentFailure") == 0)
                return "String";
            if (strcmp(name, "IntentLastHandle") == 0
                || strcmp(name, "IntentLastTraceId") == 0
                || strcmp(name, "IntentLastStepCount") == 0
                || strcmp(name, "IntentHistoryCount") == 0
                || strcmp(name, "IntentActiveCount") == 0
                || strcmp(name, "IntentActiveHandle") == 0
                || strcmp(name, "IntentActiveParentHandle") == 0
                || strcmp(name, "IntentActiveTraceId") == 0
                || strcmp(name, "IntentActivePriority") == 0
                || strcmp(name, "IntentActiveSubjectCount") == 0
                || strcmp(name, "IntentActiveStepCount") == 0
                || strcmp(name, "IntentRecentCount") == 0
                || strcmp(name, "IntentRecentStepCount") == 0)
                return "Int";
            if (strcmp(name, "ListNew") == 0)
                return "List";
            if (strcmp(name, "ListGet") == 0 && expr->data.call.arg_count >= 1) {
                const char *list_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
                    return slot_inner_type_name(list_type);
                return "Unknown";
            }
            if (strcmp(name, "ListSize") == 0)
                return "Int";
            if (strcmp(name, "SetNew") == 0)
                return "Set";
            if (strcmp(name, "QueueNew") == 0)
                return "Queue";
            if (strcmp(name, "FsmNew") == 0)
                return "Fsm";
            if (strcmp(name, "TimerNew") == 0)
                return "Timer";
            if (strcmp(name, "CooldownNew") == 0)
                return "Cooldown";
            if (strcmp(name, "ClaimQubit") == 0)
                return "QubitSlot";
            if (strcmp(name, "ClaimDeviceSlot") == 0)
                return "DeviceSlot<Int>";
            if (strcmp(name, "ViewRead") == 0 && expr->data.call.arg_count >= 1) {
                static char read_view[128];
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                snprintf(read_view, sizeof(read_view), "ReadView<%s>",
                    slot_inner_type_name(slot_type));
                return read_view;
            }
            if (strcmp(name, "ViewWrite") == 0 && expr->data.call.arg_count >= 1) {
                static char write_view[128];
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                snprintf(write_view, sizeof(write_view), "WriteView<%s>",
                    slot_inner_type_name(slot_type));
                return write_view;
            }
            if (strcmp(name, "Measure") == 0 || strcmp(name, "QubitState") == 0)
                return "Int";
            if (strcmp(name, "Read") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "Slot<", 5) == 0
                    || strncmp(slot_type, "SecureSlot<", 11) == 0
                    || strncmp(slot_type, "ReadView<", 9) == 0
                    || strncmp(slot_type, "WriteView<", 10) == 0
                    || strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    return slot_inner_type_name(slot_type);
                }
            }
            if ((strcmp(name, "Write") == 0 || strcmp(name, "Release") == 0)
                && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "Slot<", 5) == 0
                    || strncmp(slot_type, "SecureSlot<", 11) == 0
                    || strncmp(slot_type, "ReadView<", 9) == 0
                    || strncmp(slot_type, "WriteView<", 10) == 0
                    || strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    return "Void";
                }
            }
            if (strcmp(name, "DeviceRead") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "DeviceSlot<", 11) == 0)
                    return slot_inner_type_name(slot_type);
            }
            if (strcmp(name, "SubmitDeviceRead") == 0 && expr->data.call.arg_count >= 1) {
                const char *slot_type = infer_expression_type_name(ctx,
                    expr->data.call.arguments[0]);
                if (strncmp(slot_type, "DeviceSlot<", 11) == 0) {
                    static char device_future[128];
                    snprintf(device_future, sizeof(device_future), "RemoteFuture<%s>",
                        slot_inner_type_name(slot_type));
                    return device_future;
                }
                return "RemoteFuture<Int>";
            }
            if (strcmp(name, "IsCollapsed") == 0
                || strcmp(name, "IntoClassical") == 0)
                return "Bool";
            if (strcmp(name, "H") == 0
                || strcmp(name, "ArrayPush") == 0
                || strcmp(name, "ArraySet") == 0
                || strcmp(name, "ArrayPop") == 0
                || strcmp(name, "ChannelClose") == 0)
                return "Void";
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
            if (strcmp(name, "TrySend") == 0
                || strcmp(name, "SendTimeout") == 0
                || strcmp(name, "Cancel") == 0
                || strcmp(name, "IsCancelled") == 0
                || strcmp(name, "IntentLastFailed") == 0
                || strcmp(name, "IntentHistoryStepOk") == 0
                || strcmp(name, "IntentActiveConcurrent") == 0
                || strcmp(name, "IntentActiveFailed") == 0
                || strcmp(name, "IntentRecentFailed") == 0
                || strcmp(name, "MapHas") == 0
                || strcmp(name, "HasProjection") == 0
                || strcmp(name, "HasLayer") == 0
                || strcmp(name, "HasState") == 0
                || strcmp(name, "HasZone") == 0
                || strcmp(name, "HasZoneProjection") == 0
                || strcmp(name, "HasZoneLayer") == 0
                || strcmp(name, "HasZoneState") == 0
                || strcmp(name, "ChannelFull") == 0
                || strcmp(name, "ChannelClosed") == 0
                || strcmp(name, "ChannelReady") == 0)
                return "Bool";
            if (strcmp(name, "ChannelLength") == 0
                || strcmp(name, "ChannelCapacity") == 0
                || strcmp(name, "ChannelSpace") == 0)
                return "Int";
            if (strcmp(name, "Some") == 0 && expr->data.call.arg_count == 1) {
                static char opt_buf[128];
                const char *inner = infer_expression_type_name(ctx, expr->data.call.arguments[0]);
                snprintf(opt_buf, sizeof(opt_buf), "Option<%s>", inner);
                return opt_buf;
            }
            if (strcmp(name, "None") == 0)
                return "Option<Int>";
            if ((strcmp(name, "IsSome") == 0 || strcmp(name, "IsNone") == 0)
                && expr->data.call.arg_count == 1)
                return "Bool";
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
