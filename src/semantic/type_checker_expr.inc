#include "type_checker_resolve.inc"

static Type *
expr_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    if (type_ref == NULL)
        return NULL;
    return resolve_type_node(type_ref, ctx);
}

Type *
type_check_expression(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL)
        return TYPE_VOID;

    switch (expr->type) {
    case AST_NUMBER:
        return expr->data.number.is_long ? TYPE_LONG : TYPE_INT;

    case AST_STRING:
        return TYPE_STRING;

    case AST_BOOLEAN:
        return TYPE_BOOL;

    case AST_LAMBDA_EXPR: {
        size_t param_count = expr->data.lambda_expr.param_count;
        Type **param_types = calloc(param_count > 0 ? param_count : 1, sizeof(Type *));
        Type *return_type = TYPE_VOID;
        Type *result;

        if (param_types == NULL)
            return TYPE_UNKNOWN;

        scope_enter(&ctx->scope, SCOPE_FUNCTION);
        for (size_t i = 0; i < param_count; i++) {
            ASTNode *param = expr->data.lambda_expr.params[i];
            const char *param_name = NULL;
            Type *param_type = TYPE_UNKNOWN;

            if (param != NULL && param->type == AST_LET_DECL) {
                param_name = param->data.let_decl.name;
                if (param->data.let_decl.type != NULL)
                    param_type = expr_resolve_type_ref(
                        param->data.let_decl.type, ctx);
            } else if (param != NULL && param->type == AST_IDENTIFIER) {
                param_name = param->data.identifier.name;
            }

            if (param_name != NULL) {
                Symbol *param_sym = symbol_create_variable(
                    param_name, param_type, expr->line, expr->column);
                if (param_sym != NULL)
                    scope_declare(ctx->scope, param_sym);
            }
            param_types[i] = param_type;
        }

        if (expr->data.lambda_expr.return_type != NULL) {
            return_type = expr_resolve_type_ref(
                expr->data.lambda_expr.return_type, ctx);
        } else if (expr->data.lambda_expr.body != NULL
                   && expr->data.lambda_expr.body->type != AST_BLOCK) {
            return_type = type_check_expression(expr->data.lambda_expr.body, ctx);
        } else {
            return_type = TYPE_VOID;
        }

        if (expr->data.lambda_expr.body != NULL
            && expr->data.lambda_expr.body->type == AST_BLOCK) {
            bool saved_in_async = ctx->in_async_func;
            Type *saved_return = ctx->current_return;
            ctx->in_async_func = expr->data.lambda_expr.is_async;
            ctx->current_return = return_type;
            type_check_block(expr->data.lambda_expr.body, ctx);
            ctx->current_return = saved_return;
            ctx->in_async_func = saved_in_async;
        }

        scope_exit(&ctx->scope);
        result = type_create_function(param_types, param_count, return_type);
        free(param_types);
        return result != NULL ? result : TYPE_UNKNOWN;
    }

    case AST_IDENTIFIER: {
        /* Special handling for Option value constructors used without parens */
        if (strcmp(expr->data.identifier.name, "None") == 0) {
            return wrap_constructed(TYPE_OPTION, TYPE_UNKNOWN);
        }
        Symbol *sym = scope_lookup(ctx->scope,
                                    expr->data.identifier.name);
        if (sym == NULL) {
            Type *field_type = current_host_field_type(ctx, expr->data.identifier.name);
            if (field_type != NULL)
                return field_type;
            if (name_looks_qualified(expr->data.identifier.name)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                    "Undefined symbol '%s' (check namespace spelling or export visibility)",
                    expr->data.identifier.name);
            } else {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                    "Undefined symbol '%s'",
                    expr->data.identifier.name);
            }
            return TYPE_UNKNOWN;
        }
        if ((type_is_general_boundary_type(sym->type, ctx)
             || type_is_move_token(sym->type))
            && sym->is_consumed) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED,
                PGY_CAUSE_MOVE_FROM_RELEASED, PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
                expr,
                "%s '%s' was moved or released and cannot be used again.\n"
                "Reason:\n"
                "- value '%s' was already consumed by an ownership transfer or release path\n"
                "- ownership-bearing boundary values cannot be reused after consumption\n"
                "Fix:\n"
                "- create/acquire a fresh %s value\n"
                "- or keep ownership in one binding and avoid the earlier move",
                type_is_subject_type(sym->type, ctx)
                    ? type_name_or_unknown(sym->type)
                    : resource_handle_display_name(sym->type),
                expr->data.identifier.name,
                expr->data.identifier.name != NULL ? expr->data.identifier.name : "<value>",
                type_is_subject_type(sym->type, ctx)
                    ? type_name_or_unknown(sym->type)
                    : resource_handle_display_name(sym->type));
            return TYPE_UNKNOWN;
        }
        sym->is_used = true;
        return sym->type;
    }

    case AST_BINARY:
        return type_check_binary(expr, ctx);

    case AST_UNARY:
        return type_check_unary(expr, ctx);

    case AST_CALL:
        return type_check_call(expr, ctx);

    case AST_MEMBER_ACCESS:
        return type_check_member_access(expr, ctx);

    case AST_ARRAY_ACCESS:
        return type_check_array_access(expr, ctx);

    case AST_ARRAY_LITERAL:
        return type_check_array_literal(expr, ctx);

    case AST_TUPLE_LITERAL: {
        size_t n = expr->data.tuple_literal.count;
        if (n < 2) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                "Tuple literal requires at least 2 elements");
            return TYPE_UNKNOWN;
        }
        Type **elems = calloc(n, sizeof(Type *));
        if (elems == NULL)
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < n; i++)
            elems[i] = type_check_expression(
                expr->data.tuple_literal.elements[i], ctx);
        Type *tup = type_create_tuple(elems, n);
        free(elems);
        return tup != NULL ? tup : TYPE_UNKNOWN;
    }

    case AST_ASSIGNMENT:
        return type_check_assignment(expr, ctx);

    case AST_AWAIT_EXPR:
        if (!ctx->in_async_func) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_ASYNC_CONTEXT_REQUIRED, PGY_FIX_MOVE_INTO_ASYNC_FUNCTION,
                expr,
                "'await' used outside of async function");
        }
        semantic_record_effect(ctx, EFFECT_REMOTE);
        {
            Type *future_type = type_check_expression(expr->data.await_expr.expression, ctx);
            if (future_type != NULL
                && future_type->kind == TYPE_KIND_CONSTRUCTED
                && (type_equals(future_type->data.constructed.constructor, TYPE_FUTURE)
                    || type_equals(future_type->data.constructed.constructor, TYPE_REMOTE_FUTURE))
                && future_type->data.constructed.arg_count == 1) {
                Type *inner = future_type->data.constructed.args[0];
                /* RemoteFuture<T> → Result<T>: remote operations can fail
                 * (network partition, timeout, etc.) so the result must be
                 * explicitly handled.  Local Future<T> → T as before. */
                if (type_equals(future_type->data.constructed.constructor, TYPE_REMOTE_FUTURE)) {
                    Type *result_args[1] = { inner };
                    return type_create_constructed(TYPE_RESULT, result_args, 1);
                }
                return inner;
            }
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_AWAIT_NON_FUTURE, PGY_FIX_AWAIT_FUTURE_TYPE,
                expr->data.await_expr.expression,
                "'await' requires Future<T> or RemoteFuture<T>");
            return TYPE_UNKNOWN;
        }

    case AST_SPAWN_EXPR:
        return type_check_spawn_expr(expr, ctx);

    case AST_CHANNEL_SEND:
        return type_check_channel_send(expr, ctx);

    case AST_CHANNEL_RECV:
        return type_check_channel_recv(expr, ctx);

    default:
        return TYPE_UNKNOWN;
    }
}

Type *
type_check_call(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee = expr->data.call.callee;

    if (callee->type == AST_IDENTIFIER) {
        const char *name = callee->data.identifier.name;
        BuiltinKind bk   = builtin_resolve(name);
        if (bk != BUILTIN_NOT_BUILTIN)
            return type_check_builtin_call(expr, bk, ctx);

        {
            ASTNode *host_method = current_host_method_decl(ctx, name);
            if (host_method != NULL)
                return type_check_host_method_call(expr, host_method, ctx);
        }

        /* Channel(capacity) is a built-in constructor that the transpiler
         * handles; let it pass without a symbol table entry. The actual
         * type is resolved from the annotation in emit_let_decl. */
        if (strcmp(name, "Channel") == 0)
            return TYPE_UNKNOWN;  /* type inferred from let annotation */

        /* Result built-in functions — Ok/Err still return TYPE_UNKNOWN
         * (type from let annotation); others go to stdlib_call */
        if (strcmp(name, "Ok") == 0 || strcmp(name, "Err") == 0)
            return TYPE_UNKNOWN;

        /* Standard library built-in functions */
        {
            Type *stdlib_type = type_check_stdlib_call(expr, name, ctx);
            if (stdlib_type != NULL)
                return stdlib_type;
        }

        Symbol *sym = scope_lookup(ctx->scope, name);
        return type_check_function_symbol_call(expr, sym, name, ctx);
    }

    /* Callee is a member access (method call) */
    if (callee->type == AST_MEMBER_ACCESS) {
        ASTNode *object = callee->data.member.object;
        const char *method_name = callee->data.member.name;

        if (object != NULL && method_name != NULL
            && (strcmp(method_name, "Write") == 0
                || strcmp(method_name, "Read") == 0
                || strcmp(method_name, "Release") == 0)) {
            Type *slot_type = type_check_expression(object, ctx);
            if (slot_type != NULL && slot_type->kind == TYPE_KIND_SLOT) {
                size_t orig_argc = expr->data.call.arg_count;
                bool inject_token = false;
                char token_name_buf[256];
                const char *token_name = NULL;
                ASTNode token_arg;
                ASTNode *synthetic_args[4];
                ASTNode fake_call;
                size_t new_argc = 1 + orig_argc;

                memset(&token_arg, 0, sizeof(token_arg));
                memset(&fake_call, 0, sizeof(fake_call));

                if (slot_type->data.slot.is_secure
                    && object->type == AST_IDENTIFIER
                    && object->data.identifier.name != NULL) {
                    Symbol *slot_sym =
                        scope_lookup(ctx->scope, object->data.identifier.name);
                    if (slot_sym != NULL
                        && slot_sym->kind == SYMBOL_SLOT
                        && slot_sym->slot_info.paired_token_name != NULL) {
                        token_name = slot_sym->slot_info.paired_token_name;
                    } else {
                        snprintf(token_name_buf, sizeof(token_name_buf), "%s_token",
                            object->data.identifier.name);
                        token_name = token_name_buf;
                    }
                    if ((strcmp(method_name, "Write") == 0 && orig_argc < 2)
                        || ((strcmp(method_name, "Read") == 0
                             || strcmp(method_name, "Release") == 0)
                            && orig_argc < 1)) {
                        inject_token = true;
                        new_argc++;
                        token_arg.type = AST_IDENTIFIER;
                        token_arg.data.identifier.name = (char *)token_name;
                    }
                }

                if (new_argc <= sizeof(synthetic_args) / sizeof(synthetic_args[0])) {
                    synthetic_args[0] = object;
                    for (size_t i = 0; i < orig_argc; i++)
                        synthetic_args[i + 1] = expr->data.call.arguments[i];
                    if (inject_token)
                        synthetic_args[new_argc - 1] = &token_arg;

                    fake_call.type = AST_CALL;
                    fake_call.line = expr->line;
                    fake_call.column = expr->column;
                    fake_call.data.call.callee = callee;
                    fake_call.data.call.arguments = synthetic_args;
                    fake_call.data.call.arg_count = new_argc;

                    if (strcmp(method_name, "Write") == 0) {
                        (void)type_check_write_slot(&fake_call, ctx);
                        return TYPE_VOID;
                    }
                    if (strcmp(method_name, "Read") == 0)
                        return type_check_read_slot(&fake_call, ctx);

                    (void)type_check_release_slot(&fake_call, ctx);
                    return TYPE_VOID;
                }
            }
        }

        if (expr_is_static_member_access(callee)) {
            char *flat_name = flatten_static_member_access(callee, '_');
            char *display_name = flatten_static_member_access(callee, '.');
            Symbol *sym = flat_name != NULL
                ? scope_lookup(ctx->scope, flat_name)
                : NULL;
            if (sym == NULL && callee->data.member.name != NULL) {
                sym = scope_lookup(ctx->scope, callee->data.member.name);
            }
            Type *result = type_check_function_symbol_call(
                expr, sym, display_name != NULL ? display_name : "<member>", ctx);
            free(flat_name);
            free(display_name);
            return result;
        }

        if (!(object != NULL
              && object->type == AST_IDENTIFIER
              && object->data.identifier.name != NULL
              && object->data.identifier.name[0] >= 'A'
              && object->data.identifier.name[0] <= 'Z')) {
            reject_if_embedded_world_zone_mutation(ctx, expr, object, "hosted func/action call");
            /* Resolve object type for normal method calls.
             * Namespace/static-style calls like Math.Add are lowered later. */
            Type *object_type = type_check_expression(object, ctx);
            if (object_type != NULL
                && method_name != NULL
                && strcmp(method_name, "Slice") == 0
                && (type_is_constructed_named(object_type, "Array")
                    || type_is_constructed_named(object_type, "Slice"))) {
                if (expr->data.call.arg_count != 2) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                        "%s.%s(start, len) requires exactly two Int arguments",
                        type_is_constructed_named(object_type, "Array")
                            ? "Array<T>" : "Slice<T>",
                        method_name);
                    return TYPE_UNKNOWN;
                }

                require_assignable(
                    type_check_expression(expr->data.call.arguments[0], ctx),
                    TYPE_INT, expr->data.call.arguments[0], ctx);
                require_assignable(
                    type_check_expression(expr->data.call.arguments[1], ctx),
                    TYPE_INT, expr->data.call.arguments[1], ctx);

                if (object_type->kind == TYPE_KIND_CONSTRUCTED
                    && object_type->data.constructed.arg_count >= 1
                    && object_type->data.constructed.args[0] != NULL) {
                    Type *slice_args[1] = {
                        object_type->data.constructed.args[0]
                    };
                    return type_create_constructed(TYPE_SLICE, slice_args, 1);
                }
                return TYPE_UNKNOWN;
            }
            if (object_type != NULL
                && object_type->kind == TYPE_KIND_SLOT
                && method_name != NULL) {
                Symbol *sym = NULL;
                Symbol *owner = NULL;
                if (object->type == AST_IDENTIFIER)
                    sym = scope_lookup(ctx->scope, object->data.identifier.name);

                if (strcmp(method_name, "Write") == 0) {
                    if (expr->data.call.arg_count < 1) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                            "slot.Write(value) requires a value argument");
                        return TYPE_UNKNOWN;
                    }
                    if (type_is_read_view(object_type)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_VIEW_KIND_MISMATCH, PGY_CAUSE_VIEW_KIND_OP_MISMATCH, PGY_FIX_ACQUIRE_MATCHING_VIEW_OR_USE_SLOT, object,
                            "Cannot write through ReadView<T>; create a WriteView(slot) or keep the owning Slot<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (type_is_move_token(object_type)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_TOKEN_MISUSE, PGY_CAUSE_MOVE_TOKEN_DIRECT_ACCESS, PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT, object,
                            "Cannot write through MoveToken<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (object_type->data.slot.is_secure)
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    if (sym != NULL && sym->kind == SYMBOL_SLOT) {
                        if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_LIFECYCLE_WRITE_AFTER_RELEASE,
                                PGY_FIX_RECLAIM_BEFORE_USE,
                                object,
                                "Cannot write to released slot '%s'",
                                sym->name);
                            return TYPE_UNKNOWN;
                        }
                    } else if (sym != NULL && type_is_write_view(sym->type)
                               && sym->slot_info.paired_slot_name != NULL) {
                        owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
                        if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_VIEW_WRITE_THROUGH_RELEASED_OWNER,
                                PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW,
                                object,
                                "Cannot write through WriteView '%s' because source slot '%s' was released",
                                sym->name, owner->name);
                            return TYPE_UNKNOWN;
                        }
                        if (owner != NULL && owner->slot_info.is_secure)
                            semantic_record_effect(ctx, EFFECT_SECURE);
                    }
                    require_assignable(
                        type_check_expression(expr->data.call.arguments[0], ctx),
                        object_type->data.slot.inner_type,
                        expr->data.call.arguments[0], ctx);
                    return TYPE_VOID;
                }

                if (strcmp(method_name, "Read") == 0) {
                    if (type_is_write_view(object_type)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_VIEW_KIND_MISMATCH, PGY_CAUSE_VIEW_KIND_OP_MISMATCH, PGY_FIX_ACQUIRE_MATCHING_VIEW_OR_USE_SLOT, object,
                            "Cannot read through WriteView<T>; create a ReadView(slot) or keep the owning Slot<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (type_is_move_token(object_type)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_TOKEN_MISUSE, PGY_CAUSE_MOVE_TOKEN_DIRECT_ACCESS, PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT, object,
                            "Cannot read through MoveToken<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (object_type->data.slot.is_secure)
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    if (sym != NULL && sym->kind == SYMBOL_SLOT) {
                        if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_LIFECYCLE_READ_AFTER_RELEASE,
                                PGY_FIX_RECLAIM_BEFORE_USE,
                                object,
                                "Cannot read from released slot '%s'",
                                sym->name);
                            return TYPE_UNKNOWN;
                        }
                    } else if (sym != NULL && type_is_read_view(sym->type)
                               && sym->slot_info.paired_slot_name != NULL) {
                        owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
                        if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_VIEW_READ_THROUGH_RELEASED_OWNER,
                                PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW,
                                object,
                                "Cannot read through ReadView '%s' because source slot '%s' was released",
                                sym->name, owner->name);
                            return TYPE_UNKNOWN;
                        }
                        if (owner != NULL && owner->slot_info.is_secure)
                            semantic_record_effect(ctx, EFFECT_SECURE);
                    }
                    return object_type->data.slot.inner_type;
                }

                if (strcmp(method_name, "Release") == 0) {
                    if (object->type != AST_IDENTIFIER || sym == NULL || sym->kind != SYMBOL_SLOT) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_RELEASE_REQUIRES_OWNER, PGY_CAUSE_RELEASE_NON_OWNING_RECEIVER, PGY_FIX_RELEASE_OWNING_SLOT_NOT_VIEW, object,
                            "slot.Release() requires an owning slot identifier");
                        return TYPE_UNKNOWN;
                    }
                    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_DOUBLE_RELEASE, PGY_CAUSE_RELEASE_DOUBLE, PGY_FIX_REMOVE_REDUNDANT_RELEASE, object,
                            "Slot '%s' has already been released", sym->name);
                        return TYPE_UNKNOWN;
                    }
                    if (sym->slot_info.is_secure)
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    scope_release_slot(ctx->scope, sym->name);
                    return TYPE_VOID;
                }
            }
            ASTNode *class_decl;

            if (type_is_nominal_host_type(object_type, ctx)
                && object_type->name != NULL
                && method_name != NULL) {
                class_decl = find_type_decl_by_name(ctx->program_root,
                    object_type->name);
                if (class_decl != NULL) {
                    ASTNode **methods = NULL;
                    size_t method_count = 0;
                    if (class_decl->type == AST_CLASS_DECL) {
                        methods = class_decl->data.class_decl.methods;
                        method_count = class_decl->data.class_decl.method_count;
                    } else if (class_decl->type == AST_ENUM_DECL) {
                        methods = class_decl->data.enum_decl.methods;
                        method_count = class_decl->data.enum_decl.method_count;
                    }
                    for (size_t i = 0; i < method_count; i++) {
                        ASTNode *method = methods[i];
                        if (method == NULL || method->type != AST_FUNC_DECL
                            || method->data.func_decl.name == NULL)
                            continue;
                        if (strcmp(method->data.func_decl.name, method_name) == 0) {
                            uint32_t method_effects =
                                declared_effects_from_function_node(method, ctx, NULL);
                            if (!explicit_member_access_allowed(class_decl,
                                    object_type,
                                    method->data.func_decl.access,
                                    method->data.func_decl.has_explicit_access,
                                    ctx)) {
                                semantic_error_with_hints(ctx, PGY_CODE_SEM_VISIBILITY_BOUNDARY, PGY_CAUSE_VISIBILITY_BOUNDARY_CROSS, PGY_FIX_WIDEN_VISIBILITY_OR_MOVE_CALLER, expr,
                                    "Member '%s.%s' is not accessible across the current visibility boundary",
                                    object_type->name,
                                    method_name);
                                return TYPE_UNKNOWN;
                            }
                            semantic_record_effect(ctx, method_effects);
                            if (ctx->in_parallel
                                && type_effect_mask_has(method_effects, EFFECT_SECURE)) {
                                semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
                                    "Parallel context does not permit calling secure-effect method '%s.%s'; serialize authority-bearing operations outside the parallel block",
                                    object_type->name,
                                    method_name);
                                return TYPE_UNKNOWN;
                            }
                            if (method->data.func_decl.return_type != NULL)
                                return expr_resolve_type_ref(
                                    method->data.func_decl.return_type, ctx);
                            return TYPE_VOID;
                        }
                    }
                }
            }
        }
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}

Type *
type_check_member_access(ASTNode *expr, SemanticContext *ctx)
{
    if (expr_is_static_member_access(expr)) {
        char *flat_name = flatten_static_member_access(expr, '_');
        char *display_name = flatten_static_member_access(expr, '.');
        Symbol *sym = flat_name != NULL ? scope_lookup(ctx->scope, flat_name) : NULL;
        if (sym == NULL && expr->data.member.name != NULL)
            sym = scope_lookup(ctx->scope, expr->data.member.name);
        if (sym != NULL) {
            sym->is_used = true;
            free(flat_name);
            free(display_name);
            return sym->type;
        }
        semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
            "Undefined symbol '%s' (check namespace spelling or export visibility)",
            display_name != NULL ? display_name : "<member>");
        free(flat_name);
        free(display_name);
        return TYPE_UNKNOWN;
    }

    Type *object_type = type_check_expression(expr->data.member.object, ctx);

    if ((type_is_constructed_named(object_type, "Array")
         || type_is_constructed_named(object_type, "Slice"))
        && strcmp(expr->data.member.name, "Length") == 0) {
        return TYPE_INT;
    }

    /* Resolve nominal/domain field types by looking up the declaration AST. */
    if (object_type != NULL && object_type->kind == TYPE_KIND_CLASS
        && object_type->name != NULL && ctx->program_root != NULL) {
        const char *field_name = expr->data.member.name;
        ASTNode *decl = find_type_decl_by_name(ctx->program_root, object_type->name);

        if (decl == NULL)
            decl = find_domain_decl_by_name(ctx->program_root, AST_ROSTER_DECL,
                object_type->name);
        if (decl == NULL)
            decl = find_domain_decl_by_name(ctx->program_root, AST_WORLD_DECL,
                object_type->name);
        if (decl == NULL)
            decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                object_type->name);
        if (decl == NULL)
            decl = find_domain_decl_by_name(ctx->program_root, AST_RELATION_DECL,
                object_type->name);
        if (decl == NULL)
            decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                object_type->name);

        if (decl != NULL && decl->type == AST_CLASS_DECL) {
            size_t field_count = projection_source_field_count(decl);
            for (size_t fi = 0; fi < field_count; fi++) {
                ClassField *cf = projection_source_field_at(decl, fi);
                if (cf == NULL || cf->name == NULL)
                    continue;
                if (strcmp(cf->name, field_name) == 0) {
                    if (!explicit_member_access_allowed(decl,
                            object_type,
                            cf->access,
                            cf->has_explicit_access,
                            ctx)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_VISIBILITY_BOUNDARY, PGY_CAUSE_VISIBILITY_BOUNDARY_CROSS, PGY_FIX_WIDEN_VISIBILITY_OR_MOVE_CALLER, expr,
                            "Member '%s.%s' is not accessible across the current visibility boundary",
                            object_type->name,
                            field_name);
                        return TYPE_UNKNOWN;
                    }
                    return expr_resolve_type_ref(cf->type, ctx);
                }
            }
        } else if (decl != NULL) {
            if (decl->type == AST_WORLD_DECL) {
                for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
                    ASTNode *slot = decl->data.world_decl.rosters[i];
                    if (slot != NULL && slot->data.world_roster.slot_name != NULL
                        && strcmp(slot->data.world_roster.slot_name, field_name) == 0) {
                        return resolve_named_type(slot->data.world_roster.roster_type,
                            ctx, slot);
                    }
                }
                for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
                    ASTNode *slot = decl->data.world_decl.zones[i];
                    if (slot != NULL && slot->data.world_zone.slot_name != NULL
                        && strcmp(slot->data.world_zone.slot_name, field_name) == 0) {
                        return resolve_named_type(slot->data.world_zone.zone_type,
                            ctx, slot);
                    }
                }
            }

            for (size_t fi = 0; fi < overlay_field_count(decl); fi++) {
                const char *overlay_field_name = NULL;
                ASTNode *field_type_node = overlay_field_decl_at(decl, fi,
                    &overlay_field_name);
                if (overlay_field_name != NULL
                    && strcmp(overlay_field_name, field_name) == 0) {
                    return expr_resolve_type_ref(field_type_node, ctx);
                }
            }
        }

        /* Accept any remaining field access on nominal/domain types. */
        return TYPE_UNKNOWN;
    }

    /* Unknown member access — allow without error for class/enum types */
    if (object_type != NULL
        && (object_type->kind == TYPE_KIND_CLASS
            || object_type->kind == TYPE_KIND_ENUM))
        return TYPE_UNKNOWN;

    return TYPE_UNKNOWN;
}

#include "type_checker_ownership_assign.inc"

Type *
type_check_assignment(ASTNode *expr, SemanticContext *ctx)
{
    reject_if_embedded_world_zone_mutation(ctx, expr,
        expr->data.assignment.target, "assignment");
    Type *value_type  = type_check_expression(expr->data.assignment.value,  ctx);
    Type *target_type = type_check_expression(expr->data.assignment.target, ctx);

    if (type_is_slot_handle(target_type)
        && target_type->data.slot.inner_type != NULL
        && !type_is_resource_handle(value_type)
        && type_is_assignable(value_type, target_type->data.slot.inner_type)) {
        return target_type;
    }

    if (semantic_check_assignment_borrow_rebind(
            expr, ctx, target_type, value_type)) {
        return target_type;
    }

    if (type_is_class_object_type(target_type, ctx)
        || type_is_class_object_type(value_type, ctx)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
            PGY_CAUSE_SUBJECT_REBIND_FORBIDDEN,
            PGY_FIX_MUTATE_FIELD_OR_USE_METHOD,
            expr,
            "Subject assignment is not allowed; subjects are identity-bearing active hosts. Mutate fields or methods on the existing subject instead of rebinding it with '='");
        return target_type;
    }

    /* Reject field assignment on object/tobject — they are read-only
     * after construction.  Zone-internal projection sync (refresh/publish)
     * uses a separate code path and is not affected by this check. */
    if (expr->data.assignment.target != NULL
        && expr->data.assignment.target->type == AST_MEMBER_ACCESS) {
        ASTNode *obj_node = expr->data.assignment.target->data.member.object;
        if (obj_node != NULL && obj_node->type == AST_IDENTIFIER) {
            const char *var_name = obj_node->data.identifier.name;
            Symbol *sym = scope_lookup(ctx->scope, var_name);
            if (sym != NULL && sym->type != NULL
                && sym->type->kind == TYPE_KIND_CLASS
                && sym->type->name != NULL) {
                ASTNode *decl = find_type_decl_by_name(ctx->program_root,
                                                        sym->type->name);
                if (decl != NULL && decl->type == AST_CLASS_DECL) {
                    NominalDeclKind nk = decl->data.class_decl.nominal_kind;
                    if (nk == NOMINAL_DECL_OBJECT) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE, PGY_CAUSE_IMMUTABLE_FIELD_WRITE, PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND, expr,
                            "object '%s' fields are read-only after construction.\n"
                            "Reason:\n"
                            "- object is an internal projection contract\n"
                            "- projection state must be refreshed from its source, not mutated directly\n"
                            "Fix:\n"
                            "- update the source subject/value and refresh the object slot\n"
                            "- or construct a new object projection",
                            var_name);
                    } else if (nk == NOMINAL_DECL_TOBJECT) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE, PGY_CAUSE_IMMUTABLE_FIELD_WRITE, PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND, expr,
                            "tobject '%s' fields are immutable.\n"
                            "Reason:\n"
                            "- tobject is a boundary transfer contract\n"
                            "- transfer snapshots must be republished from their source, not mutated in place\n"
                            "Fix:\n"
                            "- update the source subject/value and publish a new tobject\n"
                            "- or construct a new transfer snapshot",
                            var_name);
                    }
                }
            }
        }
    }

    require_assignable(value_type, target_type, expr, ctx);
    return target_type;
}

/* -----------------------------------------------------------------
 * Statement checkers
 * ----------------------------------------------------------------- */
