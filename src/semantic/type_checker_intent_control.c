#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

bool
intent_clause_invokes_authority_sensitive_call(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee;

    if (expr == NULL || ctx == NULL || expr->type != AST_CALL)
        return false;

    callee = ast_call_callee(expr);
    if (callee == NULL)
        return false;

    if (callee->type == AST_IDENTIFIER && ast_identifier_name(callee) != NULL) {
        Symbol *sym = scope_lookup(ctx->scope, ast_identifier_name(callee));
        return sym != NULL
            && sym->type != NULL
            && sym->type->kind == TYPE_KIND_FUNCTION
            && type_effect_mask_requires_authority(type_function_effects(sym->type));
    }

    if (callee->type == AST_MEMBER_ACCESS
        && ast_member_object(callee) != NULL
        && ast_member_name(callee) != NULL) {
        Type *object_type = type_check_expression(ast_member_object(callee), ctx);
        ASTNode *host_decl;

        if (object_type == NULL || object_type->name == NULL)
            return false;

        host_decl = semantic_host_decl_for_type(ctx, object_type);
        if (host_decl == NULL)
            return false;

        size_t method_count = 0;
        ASTNode **methods = semantic_host_decl_methods(host_decl, &method_count);
        for (size_t i = 0; i < method_count; i++) {
            ASTNode *method = methods != NULL ? methods[i] : NULL;
            const char *method_name = ast_declaration_name(method);
            if (method == NULL || method->type != AST_FUNC_DECL
                || method_name == NULL) {
                continue;
            }
            if (strcmp(method_name, ast_member_name(callee)) == 0) {
                Type *method_type =
                    expr_host_method_function_type(ctx, host_decl, method_name);
                uint32_t method_effects =
                    method_type != NULL
                        ? type_function_effects(method_type)
                        : declared_effects_from_function_node(method, ctx, NULL);
                if (type_effect_mask_requires_authority(method_effects))
                    return true;
                if (ast_func_is_action(method)
                    && (ast_func_within_zone(method) != NULL
                        || ast_func_causes_effect(method) != NULL)) {
                    return true;
                }
                return false;
            }
        }
    }

    return false;
}

static const char *
intent_forbidden_control_construct(ASTNode *node)
{
    const char *nested = NULL;

    if (node == NULL)
        return NULL;

    switch (node->type) {
        case AST_AWAIT_EXPR:
            return "await";
        case AST_SPAWN_EXPR:
            return "spawn";
        case AST_ASYNC_BLOCK:
            return "async";
        case AST_PARALLEL_BLOCK:
            return "parallel";
        case AST_SELECT_STMT:
            return "select";
        case AST_CHANNEL_SEND:
            return "channel send";
        case AST_CHANNEL_RECV:
            return "channel recv";
        case AST_TASK_GROUP:
            return "task-group";
        case AST_LAMBDA_EXPR:
            if (ast_lambda_is_async(node))
                return "async lambda";
            return intent_forbidden_control_construct(ast_lambda_body(node));
        case AST_BINARY:
            nested = intent_forbidden_control_construct(ast_binary_left(node));
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(ast_binary_right(node));
        case AST_UNARY:
            return intent_forbidden_control_construct(ast_unary_operand(node));
        case AST_CALL:
            nested = intent_forbidden_control_construct(ast_call_callee(node));
            if (nested != NULL)
                return nested;
            for (size_t i = 0; i < ast_call_arg_count(node); i++) {
                nested = intent_forbidden_control_construct(
                    ast_call_argument(node, i));
                if (nested != NULL)
                    return nested;
            }
            return NULL;
        case AST_MEMBER_ACCESS:
            return intent_forbidden_control_construct(ast_member_object(node));
        case AST_ARRAY_ACCESS:
            nested = intent_forbidden_control_construct(
                ast_array_access_array(node));
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(
                ast_array_access_index(node));
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < ast_array_literal_count(node); i++) {
                nested = intent_forbidden_control_construct(
                    ast_array_literal_element(node, i));
                if (nested != NULL)
                    return nested;
            }
            return NULL;
        case AST_ASSIGNMENT:
            nested = intent_forbidden_control_construct(ast_assignment_target(node));
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(ast_assignment_value(node));
        case AST_MATCH_STMT:
            nested = intent_forbidden_control_construct(ast_match_subject(node));
            if (nested != NULL)
                return nested;
            for (size_t i = 0; i < ast_match_case_count(node); i++) {
                nested = intent_forbidden_control_construct(
                    ast_match_case_at(node, i));
                if (nested != NULL)
                    return nested;
            }
            return intent_forbidden_control_construct(
                ast_match_default_body(node));
        case AST_MATCH_CASE:
            nested = intent_forbidden_control_construct(ast_match_case_pattern(node));
            if (nested != NULL)
                return nested;
            nested = intent_forbidden_control_construct(ast_match_case_guard(node));
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(ast_match_case_body(node));
        case AST_IF_STMT:
            nested = intent_forbidden_control_construct(ast_if_condition(node));
            if (nested != NULL)
                return nested;
            nested = intent_forbidden_control_construct(ast_if_then_branch(node));
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(ast_if_else_branch(node));
        case AST_BLOCK:
            for (size_t i = 0; i < ast_block_statement_count(node); i++) {
                nested = intent_forbidden_control_construct(
                    ast_block_statement(node, i));
                if (nested != NULL)
                    return nested;
            }
            return NULL;
        case AST_RETURN:
            return intent_forbidden_control_construct(ast_return_value(node));
        case AST_DEFER_STMT:
            return intent_forbidden_control_construct(ast_defer_body(node));
        default:
            return NULL;
    }
}

bool
intent_clause_rejects_control_transfer(ASTNode *expr,
                                       SemanticContext *ctx,
                                       const char *step_name,
                                       const char *label)
{
    const char *construct = intent_forbidden_control_construct(expr);

    if (construct == NULL)
        return true;

    semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, expr,
        "Intent step '%s' %s clause cannot contain '%s'. "
        "Keep control-transfer constructs out of intent clauses and run them in surrounding intent/action code.",
        step_name != NULL ? step_name : "<step>",
        label != NULL ? label : "expression",
        construct);
    return false;
}
