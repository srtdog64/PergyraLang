#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"

static Type *
expr_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_type_ref_or_materialize(ctx, type_ref);
}

Type *
type_check_expression(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL)
        return TYPE_VOID;

    switch (expr->type) {
    case AST_NUMBER:
        if (expr->data.number.is_long)
            return TYPE_LONG;
        return expr->data.number.value == (int64_t)expr->data.number.value
            ? TYPE_INT
            : TYPE_FLOAT;

    case AST_STRING:
        return TYPE_STRING;

    case AST_BOOLEAN:
        return TYPE_BOOL;

    case AST_LAMBDA_EXPR: {
        size_t param_count = expr->data.lambda_expr.param_count;
        Type **param_types = calloc(param_count > 0 ? param_count : 1, sizeof(Type *));
        Type *return_type = TYPE_VOID;
        uint32_t saved_effects = ctx->current_function_effects;
        uint32_t saved_body_summary = ctx->current_function_body_summary;
        bool saved_tracking = ctx->tracking_function_effects;
        uint32_t lambda_effects = EFFECT_NONE;
        uint32_t lambda_body_summary = BODY_SUMMARY_NONE;
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

        ctx->tracking_function_effects = true;
        ctx->current_function_effects = EFFECT_NONE;
        ctx->current_function_body_summary = BODY_SUMMARY_NONE;

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
        lambda_effects = ctx->current_function_effects;
        lambda_body_summary = ctx->current_function_body_summary;

        scope_exit(&ctx->scope);
        result = type_create_function(param_types, param_count, return_type);
        if (result != NULL) {
            result->data.function.effect_mask = type_effect_mask_closure(lambda_effects);
            result->data.function.body_summary_mask = lambda_body_summary;
        }
        ctx->current_function_effects = saved_effects;
        ctx->current_function_body_summary = saved_body_summary;
        ctx->tracking_function_effects = saved_tracking;
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
            Type *field_type = expr_current_host_field_type(
                ctx, expr->data.identifier.name);
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
        if (sym->kind == SYMBOL_SLOT && sym->type != NULL
            && type_is_owned_slot_handle(sym->type)) {
            const char *active_view_name = NULL;
            bool active_is_write = false;
            if (semantic_find_active_slot_view_for_source(ctx->scope,
                    sym->name, &active_view_name, NULL, &active_is_write)
                && active_is_write) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                    PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                    PGY_FIX_SERIALIZE_PIN_ACCESS,
                    expr,
                    "Cannot read slot '%s' while WriteView '%s' is live.\n"
                    "Reason:\n"
                    "- slot identifier use in value position auto-reads the owner slot\n"
                    "- owner reads during a live write view would bypass the view's aliasing contract\n"
                    "Fix:\n"
                    "- end the write view scope before reading '%s'\n"
                    "- or split the operation into a read-only view followed by a write view",
                    sym->name,
                    active_view_name != NULL ? active_view_name : "<view>",
                    sym->name);
                return TYPE_UNKNOWN;
            }
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
        (void)semantic_reject_active_slot_view_boundary(expr, ctx,
            "await suspension boundary",
            "await may suspend and resume after unrelated runtime work changes the slot frontier",
            "move await");
        semantic_record_effect(ctx, EFFECT_REMOTE);
        {
            Type *future_type = type_check_expression(expr->data.await_expr.expression, ctx);
            if (future_type != NULL
                && future_type->kind == TYPE_KIND_CONSTRUCTED
                && (type_equals(future_type->data.constructed.constructor, TYPE_FUTURE)
                    || type_equals(future_type->data.constructed.constructor, TYPE_REMOTE_FUTURE))
                && future_type->data.constructed.arg_count == 1) {
                Type *inner = future_type->data.constructed.args[0];
                /* RemoteFuture<T> ??Result<T>: remote operations can fail
                 * (network partition, timeout, etc.) so the result must be
                 * explicitly handled.  Local Future<T> ??T as before. */
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
type_check_member_access(ASTNode *expr, SemanticContext *ctx)
{
    if (expr_member_is_static_access(expr)) {
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

    /* Unknown member access ??allow without error for class/enum types */
    if (object_type != NULL
        && (object_type->kind == TYPE_KIND_CLASS
            || object_type->kind == TYPE_KIND_ENUM))
        return TYPE_UNKNOWN;

    return TYPE_UNKNOWN;
}
