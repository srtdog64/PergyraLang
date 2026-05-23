#include "type_checker_internal.h"
#include "diag_codes.h"

static const char *
semantic_event_expr_name(ASTNode *expr)
{
    if (expr == NULL)
        return "<event>";
    if (expr->type == AST_IDENTIFIER && ast_identifier_name(expr) != NULL)
        return ast_identifier_name(expr);
    if (expr->type == AST_MEMBER_ACCESS && ast_member_name(expr) != NULL)
        return ast_member_name(expr);
    return "<event>";
}

static Type *
semantic_event_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static Type *
semantic_event_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    if (type_ref == NULL || ctx == NULL)
        return TYPE_UNKNOWN;
    return type_check_signature_resolve_type_ref(type_ref, ctx);
}

bool
type_check_event_decl(ASTNode *node, SemanticContext *ctx)
{
    bool ok = true;
    const char *event_name;

    if (node == NULL || ctx == NULL || node->type != AST_EVENT_DECL)
        return false;

    event_name = ast_event_name(node);
    for (size_t i = 0; i < ast_event_param_count(node); i++) {
        ASTNode *param = ast_event_param(node, i);
        if (param == NULL)
            continue;

        if (param->type != AST_LET_DECL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, param,
                "Event '%s' parameter %llu must be a typed binding",
                event_name != NULL ? event_name : "<event>",
                (unsigned long long)(i + 1));
            ok = false;
            continue;
        }

        if (ast_let_type(param) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, param,
                "Event '%s' parameter '%s' requires an explicit type",
                event_name != NULL ? event_name : "<event>",
                ast_let_name(param) != NULL
                    ? ast_let_name(param) : "<param>");
            ok = false;
            continue;
        }

        if (semantic_event_resolve_type_ref(ast_let_type(param), ctx) == NULL)
            ok = false;
    }

    if (ast_event_return_type(node) != NULL) {
        Type *return_type = semantic_event_resolve_type_ref(
            ast_event_return_type(node), ctx);
        if (return_type != NULL && !type_equals(return_type, TYPE_VOID)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
                ast_event_return_type(node),
                "Event '%s' must return Void, got '%s'",
                event_name != NULL ? event_name : "<event>",
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
    ASTNode *event_node;
    ASTNode *handler_node;

    if (node == NULL || ctx == NULL)
        return false;

    event_node = ast_event_op_event(node);
    handler_node = ast_event_op_handler(node);

    if (handler_node != NULL
        && handler_node->type == AST_LAMBDA_EXPR
        && semantic_reject_active_slot_view_boundary(node, ctx,
            "event callback boundary",
            "event lambda handlers may run after the current synchronous frame advances",
            "move event subscription")) {
        return false;
    }

    event_type = semantic_event_normalize_type(
        type_check_expression(event_node, ctx));
    handler_type = semantic_event_normalize_type(
        semantic_event_handler_signature(handler_node, ctx));
    event_name = semantic_event_expr_name(event_node);

    if (event_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            event_node,
            "Event %s target '%s' must be an event-compatible callable",
            op_name != NULL ? op_name : "operation",
            event_name);
        return false;
    }

    if (type_function_return_type(event_type) != NULL
        && !type_equals(type_function_return_type(event_type), TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            event_node,
            "Event '%s' must return Void to support %s",
            event_name, op_name != NULL ? op_name : "subscription");
        ok = false;
    }

    if (handler_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            handler_node,
            "Event %s handler for '%s' must be a function or typed lambda",
            op_name != NULL ? op_name : "operation",
            event_name);
        return false;
    }

    if (type_function_return_type(handler_type) != NULL
        && !type_equals(type_function_return_type(handler_type), TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            handler_node,
            "Event %s handler for '%s' must return Void, got '%s'",
            op_name != NULL ? op_name : "operation",
            event_name,
            type_name_or_unknown(type_function_return_type(handler_type)));
        ok = false;
    }

    if (type_function_param_count(event_type) != type_function_param_count(handler_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            handler_node,
            "Event %s handler for '%s' has parameter count mismatch: expected %llu, got %llu",
            op_name != NULL ? op_name : "operation",
            event_name,
            (unsigned long long)type_function_param_count(event_type),
            (unsigned long long)type_function_param_count(handler_type));
        return false;
    }

    for (size_t i = 0; i < type_function_param_count(event_type); i++) {
        Type *expected = type_function_param_type(event_type, i);
        Type *actual = type_function_param_type(handler_type, i);

        if (expected == NULL || actual == NULL
            || expected == TYPE_UNKNOWN || actual == TYPE_UNKNOWN) {
            continue;
        }

        if (!type_equals(expected, actual)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
                handler_node,
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
    ASTNode *event_node;

    if (node == NULL || ctx == NULL || node->type != AST_EVENT_INVOKE)
        return false;

    event_node = ast_event_invoke_event(node);
    event_type = semantic_event_normalize_type(
        type_check_expression(event_node, ctx));
    event_name = semantic_event_expr_name(event_node);

    if (event_type->kind != TYPE_KIND_FUNCTION) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
            event_node,
            "Event invoke target '%s' must be an event-compatible callable",
            event_name);
        return false;
    }

    if (type_function_param_count(event_type) != ast_event_invoke_arg_count(node)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
            PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE, node,
            "Event '%s' invoke argument count mismatch: expected %llu, got %llu",
            event_name,
            (unsigned long long)type_function_param_count(event_type),
            (unsigned long long)ast_event_invoke_arg_count(node));
        return false;
    }

    for (size_t i = 0; i < ast_event_invoke_arg_count(node); i++) {
        Type *expected = type_function_param_type(event_type, i);
        Type *actual = semantic_event_normalize_type(
            type_check_expression(ast_event_invoke_argument(node, i), ctx));

        if (expected == NULL || actual == NULL
            || expected == TYPE_UNKNOWN || actual == TYPE_UNKNOWN) {
            continue;
        }

        if (!type_is_assignable(actual, expected)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EVENT_CONTRACT_INVALID,
                PGY_CAUSE_EVENT_SIGNATURE, PGY_FIX_ALIGN_EVENT_SIGNATURE,
                ast_event_invoke_argument(node, i),
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
