#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"

static Type *
expr_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved =
        semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static Type *
expr_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static void
expr_report_unknown_member(SemanticContext *ctx, ASTNode *site,
                           const Type *object_type, const char *field_name)
{
    const char *type_name = type_name_or_unknown(object_type);
    const char *member_name = field_name != NULL ? field_name : "<member>";

    semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL,
        PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, site,
        "Unknown member '%s.%s'.\n"
        "Reason:\n"
        "- the receiver type is known, but no field or overlay member with this name is declared\n"
        "- method calls are resolved through the call path; value-position member access requires a field\n"
        "Fix:\n"
        "- declare field '%s' on '%s'\n"
        "- or call an existing method with parentheses if this was meant to be an action",
        type_name,
        member_name,
        member_name,
        type_name);
}

Type *
type_check_expression(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL)
        return TYPE_VOID;

    switch (expr->type) {
    case AST_NUMBER:
        if (ast_number_is_long(expr))
            return TYPE_LONG;
        return ast_number_value(expr) == (int64_t)ast_number_value(expr)
            ? TYPE_INT
            : TYPE_FLOAT;

    case AST_STRING:
        return TYPE_STRING;

    case AST_BOOLEAN:
        return TYPE_BOOL;

    case AST_LAMBDA_EXPR: {
        size_t param_count = ast_lambda_param_count(expr);
        ASTNode *lambda_body = ast_lambda_body(expr);
        ASTNode *lambda_return_type = ast_lambda_return_type(expr);
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
            ASTNode *param = ast_lambda_param(expr, i);
            const char *param_name = NULL;
            Type *param_type = TYPE_UNKNOWN;

            if (param != NULL && param->type == AST_LET_DECL) {
                param_name = ast_let_name(param);
                if (ast_let_type(param) != NULL)
                    param_type = expr_resolve_type_ref(
                        ast_let_type(param), ctx);
            } else if (param != NULL && param->type == AST_IDENTIFIER) {
                param_name = ast_identifier_name(param);
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

        if (semantic_reject_lambda_unsupported_captures(
                lambda_body, ctx)) {
            scope_exit(&ctx->scope);
            ctx->current_function_effects = saved_effects;
            ctx->current_function_body_summary = saved_body_summary;
            ctx->tracking_function_effects = saved_tracking;
            free(param_types);
            return TYPE_UNKNOWN;
        }

        if (lambda_return_type != NULL) {
            return_type = expr_resolve_type_ref(
                lambda_return_type, ctx);
        } else if (lambda_body != NULL && lambda_body->type != AST_BLOCK) {
            return_type = expr_normalize_type(
                type_check_expression(lambda_body, ctx));
        } else {
            return_type = TYPE_VOID;
        }

        if (lambda_body != NULL && lambda_body->type == AST_BLOCK) {
            bool saved_in_async = ctx->in_async_func;
            Type *saved_return = ctx->current_return;
            ctx->in_async_func = ast_lambda_is_async(expr);
            ctx->current_return = return_type;
            type_check_block(lambda_body, ctx);
            ctx->current_return = saved_return;
            ctx->in_async_func = saved_in_async;
        }
        lambda_effects = ctx->current_function_effects;
        lambda_body_summary = ctx->current_function_body_summary;

        scope_exit(&ctx->scope);
        result = type_create_function(param_types, param_count, return_type);
        if (result != NULL) {
            type_function_set_effects(result, type_effect_mask_closure(lambda_effects));
            type_function_set_body_summary(result, lambda_body_summary);
        }
        ctx->current_function_effects = saved_effects;
        ctx->current_function_body_summary = saved_body_summary;
        ctx->tracking_function_effects = saved_tracking;
        free(param_types);
        return result != NULL ? result : TYPE_UNKNOWN;
    }

    case AST_IDENTIFIER: {
        const char *expr_name = ast_identifier_name(expr);
        /* Special handling for Option value constructors used without parens */
        if (strcmp(expr_name, "None") == 0) {
            return wrap_constructed(TYPE_OPTION, TYPE_UNKNOWN);
        }
        Symbol *sym = scope_lookup(ctx->scope, expr_name);
        if (sym == NULL) {
            Type *field_type = expr_current_host_field_type(
                ctx, expr_name);
            if (field_type != NULL)
                return field_type;
            if (name_looks_qualified(expr_name)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                    "Undefined symbol '%s' (check namespace spelling or export visibility)",
                    expr_name);
            } else {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                    "Undefined symbol '%s'",
                    expr_name);
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
                expr_name,
                expr_name != NULL ? expr_name : "<value>",
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
        size_t n = ast_tuple_literal_count(expr);
        if (n < 2) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                "Tuple literal requires at least 2 elements");
            return TYPE_UNKNOWN;
        }
        Type **elems = calloc(n, sizeof(Type *));
        if (elems == NULL)
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < n; i++)
            elems[i] = expr_normalize_type(type_check_expression(
                ast_tuple_literal_element(expr, i), ctx));
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
            ASTNode *awaited = ast_await_expression(expr);
            Type *future_type = type_check_expression(awaited, ctx);
            if (future_type != NULL
                && (type_equals(type_constructed_constructor(future_type), TYPE_FUTURE)
                    || type_equals(type_constructed_constructor(future_type), TYPE_REMOTE_FUTURE))
                && type_constructed_arg_count(future_type) == 1) {
                Type *inner = type_constructed_arg(future_type, 0);
                /* RemoteFuture<T> -> Result<T>: remote operations can fail
                 * (network partition, timeout, etc.) so the result must be
                 * explicitly handled. Local Future<T> -> T as before. */
                if (type_equals(type_constructed_constructor(future_type), TYPE_REMOTE_FUTURE)) {
                    Type *result_args[1] = { inner };
                    return type_create_constructed(TYPE_RESULT, result_args, 1);
                }
                return inner;
            }
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_AWAIT_NON_FUTURE, PGY_FIX_AWAIT_FUTURE_TYPE,
                awaited,
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
    ASTNode *member_object = ast_member_object(expr);
    const char *member_name = ast_member_name(expr);

    if (expr_member_is_static_access(expr)) {
        char *flat_name = flatten_static_member_access(expr, '_');
        char *display_name = flatten_static_member_access(expr, '.');
        Symbol *sym = flat_name != NULL ? scope_lookup(ctx->scope, flat_name) : NULL;
        if (sym == NULL && member_name != NULL)
            sym = scope_lookup(ctx->scope, member_name);
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

    Type *object_type = type_check_expression(member_object, ctx);

    if ((type_is_constructed_named(object_type, "Array")
         || type_is_constructed_named(object_type, "Slice"))
        && strcmp(member_name, "Length") == 0) {
        return TYPE_INT;
    }

    if (object_type != NULL && object_type->kind == TYPE_KIND_ENUM) {
        Type *variant_payload = expr_type_for_enum_variant_projection(ctx,
            expr, object_type, member_name);
        if (variant_payload != NULL)
            return variant_payload;
    }

    if (object_type != NULL && object_type->kind == TYPE_KIND_CLASS
        && object_type->name != NULL && strchr(object_type->name, '$') != NULL) {
        Type *payload_field = expr_type_for_enum_payload_field(ctx, expr,
            object_type, member_name);
        if (payload_field != NULL)
            return payload_field;
    }

    /* Resolve nominal/domain field types through the shared host-decl seam. */
    if (object_type != NULL
        && (object_type->kind == TYPE_KIND_CLASS
            || object_type->kind == TYPE_KIND_ENUM)
        && object_type->name != NULL && ctx->program_root != NULL) {
        const char *field_name = member_name;
        ASTNode *decl = semantic_host_decl_for_type(ctx, object_type);

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
                size_t roster_count = 0;
                ASTNode **rosters = ast_world_rosters(decl, &roster_count);
                for (size_t i = 0; i < roster_count; i++) {
                    ASTNode *slot = rosters[i];
                    const char *slot_name = ast_world_roster_slot_name(slot);
                    if (slot != NULL && slot_name != NULL
                        && strcmp(slot_name, field_name) == 0) {
                        return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                            ctx, ast_world_roster_type_name(slot), slot);
                    }
                }
                size_t zone_count = 0;
                ASTNode **zones = ast_world_zones(decl, &zone_count);
                for (size_t i = 0; i < zone_count; i++) {
                    ASTNode *slot = zones[i];
                    const char *slot_name = ast_world_zone_slot_name(slot);
                    if (slot != NULL && slot_name != NULL
                        && strcmp(slot_name, field_name) == 0) {
                        return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                            ctx, ast_world_zone_type_name(slot), slot);
                    }
                }
            }
            if (decl->type == AST_ZONE_DECL) {
                size_t layer_slot_count = 0;
                ASTNode **layer_slots = ast_zone_layer_slots(
                    decl, &layer_slot_count);
                for (size_t i = 0; i < layer_slot_count; i++) {
                    ASTNode *slot = layer_slots[i];
                    if (slot != NULL
                        && ast_zone_layer_slot_name(slot) != NULL
                        && strcmp(ast_zone_layer_slot_name(slot),
                                  field_name) == 0) {
                        return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                            ctx, ast_zone_layer_slot_layer_type(slot), slot);
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

        expr_report_unknown_member(ctx, expr, object_type, field_name);
        return TYPE_UNKNOWN;
    }

    if (object_type != NULL
        && (object_type->kind == TYPE_KIND_CLASS
            || object_type->kind == TYPE_KIND_ENUM)) {
        expr_report_unknown_member(ctx, expr, object_type,
            member_name);
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}
