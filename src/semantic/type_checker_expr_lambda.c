#include <stdlib.h>
#include "type_checker_internal.h"
#include "diag_codes.h"

static Type *
lambda_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved =
        semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static Type *
lambda_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static void
lambda_restore_context(SemanticContext *ctx,
                       uint32_t saved_effects,
                       uint32_t saved_capabilities,
                       uint32_t saved_body_summary,
                       bool saved_tracking)
{
    ctx->current_function_effects = saved_effects;
    ctx->current_function_capabilities = saved_capabilities;
    ctx->current_function_body_summary = saved_body_summary;
    ctx->tracking_function_effects = saved_tracking;
}

Type *
type_check_lambda_expression(ASTNode *expr, SemanticContext *ctx)
{
    size_t param_count = ast_lambda_param_count(expr);
    ASTNode *lambda_body = ast_lambda_body(expr);
    ASTNode *lambda_return_type = ast_lambda_return_type(expr);
    Type **param_types = calloc(param_count > 0 ? param_count : 1,
                                sizeof(Type *));
    Type *return_type = TYPE_VOID;
    Type *expected_lambda_type = ctx->expected_lambda_type;
    uint32_t saved_effects = ctx->current_function_effects;
    uint32_t saved_capabilities = ctx->current_function_capabilities;
    uint32_t saved_body_summary = ctx->current_function_body_summary;
    bool saved_tracking = ctx->tracking_function_effects;
    uint32_t lambda_effects = EFFECT_NONE;
    uint32_t lambda_capabilities = 0u;
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
                param_type = lambda_resolve_type_ref(ast_let_type(param), ctx);
        } else if (param != NULL && param->type == AST_IDENTIFIER) {
            param_name = ast_identifier_name(param);
        }
        if (param_type == TYPE_UNKNOWN
            && expected_lambda_type != NULL
            && expected_lambda_type->kind == TYPE_KIND_FUNCTION
            && type_function_param_count(expected_lambda_type)
                == param_count) {
            Type *expected_param =
                type_function_param_type(expected_lambda_type, i);
            if (expected_param != NULL)
                param_type = expected_param;
        }

        if (param_name != NULL) {
            Symbol *param_sym = symbol_create_variable(
                param_name, param_type, expr->line, expr->column);
            if (param_sym != NULL) {
                param_sym->is_parameter = true;
                param_sym->param_mode = PARAM_MODE_DEFAULT;
                if (semantic_type_is_future_handle(param_type)) {
                    semantic_error_with_hints(ctx,
                        PGY_CODE_SEM_TASK_LIFECYCLE,
                        PGY_CAUSE_TASK_LIFECYCLE,
                        PGY_FIX_AWAIT_TASK_BEFORE_EXIT,
                        param,
                        "Lambda Future parameters are not supported in the beta structured-spawn contract.\n"
                        "Reason:\n"
                        "- lambda parameters have no explicit own transfer mode\n"
                        "- default carriage would alias one completion handle\n"
                        "Fix:\n"
                        "- use a named function with an 'own Future<T>' parameter");
                }
                scope_declare(ctx->scope, param_sym);
            }
        }
        param_types[i] = param_type;
    }

    ctx->tracking_function_effects = true;
    ctx->current_function_effects = EFFECT_NONE;
    ctx->current_function_capabilities = 0u;
    ctx->current_function_body_summary = BODY_SUMMARY_NONE;

    bool allow_copy_capture = ctx->capture_allowed_let_init;
    ctx->capture_allowed_let_init = false; /* body lambdas are escaping */
    if (semantic_reject_lambda_unsupported_captures(
            expr, ctx, allow_copy_capture)) {
        scope_exit(&ctx->scope);
        lambda_restore_context(ctx, saved_effects, saved_capabilities,
                               saved_body_summary, saved_tracking);
        free(param_types);
        return TYPE_UNKNOWN;
    }

    Type *body_expr_type = NULL;
    if (lambda_body != NULL && lambda_body->type != AST_BLOCK) {
        body_expr_type =
            lambda_normalize_type(type_check_expression(lambda_body, ctx));
    }

    if (lambda_return_type != NULL) {
        return_type = lambda_resolve_type_ref(lambda_return_type, ctx);
    } else if (expected_lambda_type != NULL
               && expected_lambda_type->kind == TYPE_KIND_FUNCTION
               && type_function_param_count(expected_lambda_type)
                    == param_count
               && type_function_return_type(expected_lambda_type) != NULL) {
        return_type = type_function_return_type(expected_lambda_type);
    } else if (body_expr_type != NULL) {
        return_type = body_expr_type;
    } else {
        return_type = TYPE_VOID;
    }

    if (lambda_body != NULL && lambda_body->type == AST_BLOCK) {
        bool saved_in_async = ctx->in_async_func;
        Type *saved_return = ctx->current_return;
        SemanticBodyFlowSummary flow_summary = {0};
        ctx->in_async_func = ast_lambda_is_async(expr);
        ctx->current_return = return_type;
        semantic_check_body_flow_summary(lambda_body, ctx, &flow_summary);
        if (flow_summary.has_fallthrough) {
            semantic_future_require_scope_retired(
                ctx->scope, lambda_body, ctx, "lambda fallthrough");
        }
        ctx->current_return = saved_return;
        ctx->in_async_func = saved_in_async;
    }
    lambda_effects = ctx->current_function_effects;
    lambda_capabilities = ctx->current_function_capabilities;
    lambda_body_summary = ctx->current_function_body_summary;

    scope_exit(&ctx->scope);
    result = type_create_function(param_types, param_count, return_type);
    if (result != NULL) {
        type_function_set_effects(result,
            type_effect_mask_closure(lambda_effects));
        type_function_set_capabilities(result, lambda_capabilities);
        type_function_set_body_summary(result, lambda_body_summary);
    }
    lambda_restore_context(ctx, saved_effects, saved_capabilities,
                           saved_body_summary, saved_tracking);
    free(param_types);
    return result != NULL ? result : TYPE_UNKNOWN;
}
