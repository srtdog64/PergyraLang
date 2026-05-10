#include <stdlib.h>

#include "type_checker_internal.h"
#include "diag_codes.h"

static const char *
semantic_event_expr_name(ASTNode *expr)
{
    if (expr == NULL)
        return "<event>";
    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL)
        return expr->data.identifier.name;
    if (expr->type == AST_MEMBER_ACCESS && expr->data.member.name != NULL)
        return expr->data.member.name;
    return "<event>";
}

static Type *
semantic_event_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_type_ref_or_materialize(ctx, type_ref);
}

static Type *
semantic_event_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

bool
type_check_event_decl(ASTNode *node, SemanticContext *ctx)
{
    bool ok = true;

    if (node == NULL || ctx == NULL || node->type != AST_EVENT_DECL)
        return false;

    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        if (param == NULL)
            continue;

        if (param->type != AST_LET_DECL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, param,
                "Event '%s' parameter %llu must be a typed binding",
                node->data.event_decl.name != NULL
                    ? node->data.event_decl.name : "<event>",
                (unsigned long long)(i + 1));
            ok = false;
            continue;
        }

        if (param->data.let_decl.type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, param,
                "Event '%s' parameter '%s' requires an explicit type",
                node->data.event_decl.name != NULL
                    ? node->data.event_decl.name : "<event>",
                param->data.let_decl.name != NULL
                    ? param->data.let_decl.name : "<param>");
            ok = false;
            continue;
        }

        if (semantic_event_resolve_type_ref(param->data.let_decl.type, ctx) == NULL)
            ok = false;
    }

    if (node->data.event_decl.return_type != NULL) {
        Type *return_type = semantic_event_resolve_type_ref(
            node->data.event_decl.return_type, ctx);
        if (return_type != NULL && !type_equals(return_type, TYPE_VOID)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
                node->data.event_decl.return_type,
                "Event '%s' must return Void, got '%s'",
                node->data.event_decl.name != NULL
                    ? node->data.event_decl.name : "<event>",
                type_name_or_unknown(return_type));
            ok = false;
        }
    }

    return ok && !ctx->has_error;
}

static Type *
semantic_event_handler_signature(ASTNode *handler, SemanticContext *ctx)
{
    if (handler == NULL || ctx == NULL)
        return NULL;

    if (handler->type == AST_LAMBDA_EXPR) {
        size_t param_count = handler->data.lambda_expr.param_count;
        Type **param_types = calloc(param_count > 0 ? param_count : 1,
            sizeof(Type *));
        Type *return_type = TYPE_VOID;
        Type *lambda_type;

        if (param_types == NULL)
            return TYPE_UNKNOWN;

        for (size_t i = 0; i < param_count; i++) {
            ASTNode *param = handler->data.lambda_expr.params[i];
            if (param != NULL
                && param->type == AST_LET_DECL
                && param->data.let_decl.type != NULL) {
                param_types[i] = semantic_event_resolve_type_ref(
                    param->data.let_decl.type, ctx);
            } else {
                param_types[i] = TYPE_UNKNOWN;
            }
        }

        if (handler->data.lambda_expr.return_type != NULL) {
            Type *resolved = semantic_event_resolve_type_ref(
                handler->data.lambda_expr.return_type, ctx);
            if (resolved != NULL)
                return_type = resolved;
        }

        lambda_type = type_create_function(param_types, param_count, return_type);
        free(param_types);
        return lambda_type != NULL ? lambda_type : TYPE_UNKNOWN;
    }

    return semantic_event_normalize_type(type_check_expression(handler, ctx));
}

bool
type_check_event_subscription(ASTNode *node, SemanticContext *ctx,
                              const char *op_name)
{
    Type *event_type;
    Type *handler_type;
    const char *event_name;
    bool ok = true;

    if (node == NULL || ctx == NULL)
        return false;

    if (node->data.event_op.handler != NULL
        && node->data.event_op.handler->type == AST_LAMBDA_EXPR
        && semantic_reject_active_slot_view_boundary(node, ctx,
            "event callback boundary",
            "event lambda handlers may run after the current synchronous frame advances",
            "move event subscription")) {
        return false;
    }

    event_type = semantic_event_normalize_type(
        type_check_expression(node->data.event_op.event, ctx));
    handler_type = semantic_event_normalize_type(
        semantic_event_handler_signature(node->data.event_op.handler, ctx));
    event_name = semantic_event_expr_name(node->data.event_op.event);

    if (event_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_op.event,
            "Event %s target '%s' must be an event-compatible callable",
            op_name != NULL ? op_name : "operation",
            event_name);
        return false;
    }

    if (event_type->data.function.return_type != NULL
        && !type_equals(event_type->data.function.return_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_op.event,
            "Event '%s' must return Void to support %s",
            event_name, op_name != NULL ? op_name : "subscription");
        ok = false;
    }

    if (handler_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_op.handler,
            "Event %s handler for '%s' must be a function or typed lambda",
            op_name != NULL ? op_name : "operation",
            event_name);
        return false;
    }

    if (handler_type->data.function.return_type != NULL
        && !type_equals(handler_type->data.function.return_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_op.handler,
            "Event %s handler for '%s' must return Void, got '%s'",
            op_name != NULL ? op_name : "operation",
            event_name,
            type_name_or_unknown(handler_type->data.function.return_type));
        ok = false;
    }

    if (event_type->data.function.param_count != handler_type->data.function.param_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_op.handler,
            "Event %s handler for '%s' has parameter count mismatch: expected %llu, got %llu",
            op_name != NULL ? op_name : "operation",
            event_name,
            (unsigned long long)event_type->data.function.param_count,
            (unsigned long long)handler_type->data.function.param_count);
        return false;
    }

    for (size_t i = 0; i < event_type->data.function.param_count; i++) {
        Type *expected = event_type->data.function.param_types[i];
        Type *actual = handler_type->data.function.param_types[i];

        if (expected == NULL || actual == NULL
            || expected == TYPE_UNKNOWN || actual == TYPE_UNKNOWN) {
            continue;
        }

        if (!type_equals(expected, actual)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
                node->data.event_op.handler,
                "Event %s handler for '%s' parameter %llu mismatch: expected '%s', got '%s'",
                op_name != NULL ? op_name : "operation",
                event_name,
                (unsigned long long)(i + 1),
                type_name_or_unknown(expected),
                type_name_or_unknown(actual));
            ok = false;
        }
    }

    return ok && !ctx->has_error;
}

bool
type_check_event_invoke_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *event_type;
    const char *event_name;
    bool ok = true;

    if (node == NULL || ctx == NULL || node->type != AST_EVENT_INVOKE)
        return false;

    event_type = semantic_event_normalize_type(
        type_check_expression(node->data.event_invoke.event, ctx));
    event_name = semantic_event_expr_name(node->data.event_invoke.event);

    if (event_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            node->data.event_invoke.event,
            "Event invoke target '%s' must be an event-compatible callable",
            event_name);
        return false;
    }

    if (event_type->data.function.param_count != node->data.event_invoke.arg_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, node,
            "Event '%s' invoke argument count mismatch: expected %llu, got %llu",
            event_name,
            (unsigned long long)event_type->data.function.param_count,
            (unsigned long long)node->data.event_invoke.arg_count);
        return false;
    }

    for (size_t i = 0; i < node->data.event_invoke.arg_count; i++) {
        Type *expected = event_type->data.function.param_types[i];
        Type *actual = semantic_event_normalize_type(
            type_check_expression(node->data.event_invoke.arguments[i], ctx));

        if (expected == NULL || actual == NULL
            || expected == TYPE_UNKNOWN || actual == TYPE_UNKNOWN) {
            continue;
        }

        if (!type_is_assignable(actual, expected)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
                node->data.event_invoke.arguments[i],
                "Event '%s' invoke argument %llu mismatch: expected '%s', got '%s'",
                event_name,
                (unsigned long long)(i + 1),
                type_name_or_unknown(expected),
                type_name_or_unknown(actual));
            ok = false;
        }
    }

    return ok && !ctx->has_error;
}
