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
            receiver_type = resolve_nominal_host_expr_type_name(ctx, receiver);
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

static char *
infer_spawn_return_type_name(TranspilerCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = spawn_expr != NULL ? spawn_expr->data.spawn_expr.function : NULL;
    const char *function_name = NULL;
    ASTNode *call = NULL;

    if (target == NULL)
        return pergyra_strdup("Void");

    if (target->type == AST_CALL
        && target->data.call.callee != NULL
        && target->data.call.callee->type == AST_IDENTIFIER) {
        call = target;
        function_name = target->data.call.callee->data.identifier.name;
    } else if (target->type == AST_IDENTIFIER) {
        function_name = target->data.identifier.name;
    } else if (target->type == AST_FUNC_DECL) {
        if (target->data.func_decl.return_type != NULL)
            return render_type_name(target->data.func_decl.return_type);
        return pergyra_strdup("Void");
    }

    if (function_name == NULL)
        return pergyra_strdup("Void");

    ASTNode *decl = find_function_decl(ctx, function_name);
    if (decl != NULL && decl->data.func_decl.return_type != NULL) {
        if (call != NULL && func_has_generic_params(decl)) {
            GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
            size_t binding_count = 0;
            if (infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count))
                return render_type_name_with_bindings(ctx, decl->data.func_decl.return_type,
                    bindings, binding_count);
        }
        return render_type_name(decl->data.func_decl.return_type);
    }

    return pergyra_strdup("Void");
}

static bool
is_remote_future_expr(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL) return false;
    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        return type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0;
    }
    return false;
}

static const char *
lookup_future_inner_type(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL)
        return "Void";

    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Future<", 7) == 0)
            return slot_inner_type_name(type_name);
        if (type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0)
            return slot_inner_type_name(type_name);
    }

    if (expr->type == AST_SPAWN_EXPR) {
        char *inner = infer_spawn_return_type_name(ctx, expr);
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s", inner);
        free(inner);
        return buf;
    }

    return "Void";
}

static const char *
pergyra_ast_type_to_c(ASTNode *type_node)
{
    static char mapped[128];
    if (type_node == NULL)
        return "void";

    if (type_node->type == AST_EVENT_HANDLER_TYPE)
        return "void *";

    char *type_name = render_type_name(type_node);
    snprintf(mapped, sizeof(mapped), "%s", pergyra_type_to_c(type_name));
    free(type_name);
    return mapped;
}

static char *
pergyra_ast_typed_declarator(ASTNode *type_node, const char *name)
{
    if (type_node == NULL)
        return strdup_fmt("void %s", name != NULL ? name : "value");

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        char *ret_type = pergyra_strdup("void");
        CodeBuf *params = codebuf_create();
        char *result;

        if (type_node->data.event_handler_type.return_type != NULL) {
            const char *rendered = pergyra_ast_type_to_c(
                type_node->data.event_handler_type.return_type);
            free(ret_type);
            ret_type = pergyra_strdup(rendered);
        }

        if (params == NULL) {
            result = strdup_fmt("%s (*%s)(void)", ret_type,
                name != NULL ? name : "value");
            free(ret_type);
            return result;
        }

        if (type_node->data.event_handler_type.param_count == 0) {
            codebuf_write(params, "void");
        } else {
            for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    codebuf_write(params, ", ");
                codebuf_write(params, "%s",
                    pergyra_ast_type_to_c(
                        type_node->data.event_handler_type.param_types[i]));
            }
        }

        result = strdup_fmt("%s (*%s)(%s)", ret_type,
            name != NULL ? name : "value", params->data);
        codebuf_destroy(params);
        free(ret_type);
        return result;
    }

    return strdup_fmt("%s %s", pergyra_ast_type_to_c(type_node),
        name != NULL ? name : "value");
}

static char *
pergyra_func_pointer_declarator_from_decl(ASTNode *func_decl, const char *name)
{
    CodeBuf *params = NULL;
    char *ret_type = NULL;
    char *result = NULL;

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return strdup_fmt("void (*%s)(void)", name != NULL ? name : "value");

    params = codebuf_create();
    ret_type = pergyra_strdup("void");
    if (func_decl->data.func_decl.return_type != NULL) {
        const char *rendered = pergyra_ast_type_to_c(func_decl->data.func_decl.return_type);
        free(ret_type);
        ret_type = pergyra_strdup(rendered);
    }

    if (params == NULL) {
        result = strdup_fmt("%s (*%s)(void)", ret_type,
            name != NULL ? name : "value");
        free(ret_type);
        return result;
    }

    if (func_decl->data.func_decl.param_count == 0) {
        codebuf_write(params, "void");
    } else {
        for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
            FuncParam *p = func_decl->data.func_decl.params[i];
            if (i > 0)
                codebuf_write(params, ", ");
            codebuf_write(params, "%s",
                p != NULL && p->type != NULL
                    ? pergyra_ast_type_to_c(p->type)
                    : "int32_t");
        }
    }

    result = strdup_fmt("%s (*%s)(%s)", ret_type,
        name != NULL ? name : "value", params->data);
    codebuf_destroy(params);
    free(ret_type);
    return result;
}

static char *
pergyra_func_signature_declarator(ASTNode *return_type, const char *name,
                                  const char *params_sig)
{
    const char *fn_name = name != NULL ? name : "value";
    const char *sig = (params_sig != NULL && params_sig[0] != '\0')
        ? params_sig : "void";

    if (return_type != NULL && return_type->type == AST_EVENT_HANDLER_TYPE) {
        CodeBuf *handler_params = codebuf_create();
        char *ret_type = pergyra_strdup("void");
        char *result;

        if (return_type->data.event_handler_type.return_type != NULL) {
            const char *rendered = pergyra_ast_type_to_c(
                return_type->data.event_handler_type.return_type);
            free(ret_type);
            ret_type = pergyra_strdup(rendered);
        }

        if (handler_params == NULL) {
            result = strdup_fmt("%s (*%s(%s))(void)", ret_type, fn_name, sig);
            free(ret_type);
            return result;
        }

        if (return_type->data.event_handler_type.param_count == 0) {
            codebuf_write(handler_params, "void");
        } else {
            for (size_t i = 0; i < return_type->data.event_handler_type.param_count; i++) {
                if (i > 0)
                    codebuf_write(handler_params, ", ");
                codebuf_write(handler_params, "%s",
                    pergyra_ast_type_to_c(
                        return_type->data.event_handler_type.param_types[i]));
            }
        }

        result = strdup_fmt("%s (*%s(%s))(%s)", ret_type, fn_name, sig,
            handler_params->data);
        codebuf_destroy(handler_params);
        free(ret_type);
        return result;
    }

    return strdup_fmt("%s %s(%s)",
        pergyra_ast_type_to_c(return_type), fn_name, sig);
}

static bool
transpiler_can_forward_declare_type_early(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return true;
    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return true;

    const char *name = type_node->data.type.name;
    if (strcmp(name, "Int") == 0
        || strcmp(name, "Float") == 0
        || strcmp(name, "Bool") == 0
        || strcmp(name, "String") == 0
        || strcmp(name, "Char") == 0
        || strcmp(name, "Byte") == 0
        || strcmp(name, "Void") == 0
        || strcmp(name, "Qubit") == 0)
        return true;

    if (strcmp(name, "Result") == 0
        || strcmp(name, "Option") == 0
        || strcmp(name, "Slot") == 0
        || strcmp(name, "SecureSlot") == 0
        || strcmp(name, "DeviceSlot") == 0
        || strcmp(name, "RemoteFuture") == 0
        || strcmp(name, "Array") == 0
        || strcmp(name, "Slice") == 0
        || strcmp(name, "Channel") == 0
        || strcmp(name, "Box") == 0
        || strcmp(name, "Rc") == 0
        || strcmp(name, "Weak") == 0
        || strcmp(name, "Future") == 0)
        return true;

    return find_class_decl(ctx, name) != NULL;
}

static bool
transpiler_can_forward_declare_func_early(TranspilerCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    if (func->data.func_decl.generic_params != NULL
        && func->data.func_decl.generic_params->count > 0)
        return false;
    if (!transpiler_can_forward_declare_type_early(ctx,
            func->data.func_decl.return_type))
        return false;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *p = func->data.func_decl.params[i];
        if (p == NULL || p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_early(ctx, p->type))
            return false;
    }
    return true;
}

static bool
transpiler_can_forward_declare_type_after_zones(TranspilerCtx *ctx, ASTNode *type_node)
{
    const char *name = NULL;

    if (transpiler_can_forward_declare_type_early(ctx, type_node))
        return true;
    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL)
        return false;

    name = type_node->data.type.name;
    if (find_world_decl(ctx, name) != NULL)
        return false;
    return transpiler_has_known_nominal_type(ctx, name);
}

static bool
transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    if (func->data.func_decl.generic_params != NULL
        && func->data.func_decl.generic_params->count > 0)
        return false;
    if (!transpiler_can_forward_declare_type_after_zones(ctx,
            func->data.func_decl.return_type))
        return false;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *p = func->data.func_decl.params[i];
        if (p == NULL || p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_after_zones(ctx, p->type))
            return false;
    }
    return true;
}

static void
append_mangled_type_name(CodeBuf *buf, const char *type_name)
{
    bool wrote = false;

    for (const unsigned char *p = (const unsigned char *)type_name; *p != '\0'; p++) {
        if ((*p >= 'a' && *p <= 'z')
            || (*p >= 'A' && *p <= 'Z')
            || (*p >= '0' && *p <= '9')) {
            codebuf_write(buf, "%c", *p);
            wrote = true;
        } else if (wrote && buf->len > 0 && buf->data[buf->len - 1] != '_') {
            codebuf_write(buf, "_");
        }
    }

    if (!wrote)
        codebuf_write(buf, "Type");
}

static int
find_generic_param_index(ASTNode *decl, const char *name)
{
    if (!func_has_generic_params(decl) || name == NULL)
        return -1;

    for (size_t i = 0; i < decl->data.func_decl.generic_params->count; i++) {
        GenericParam *param = decl->data.func_decl.generic_params->params[i];
        if (param != NULL && param->name != NULL && strcmp(param->name, name) == 0)
            return (int)i;
    }

    return -1;
}

static bool
