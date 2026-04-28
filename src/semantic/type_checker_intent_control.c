#include "type_checker_internal.h"
#include "diag_codes.h"

#include <string.h>

bool
intent_clause_invokes_authority_sensitive_call(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee;

    if (expr == NULL || ctx == NULL || expr->type != AST_CALL)
        return false;

    callee = expr->data.call.callee;
    if (callee == NULL)
        return false;

    if (callee->type == AST_IDENTIFIER && callee->data.identifier.name != NULL) {
        Symbol *sym = scope_lookup(ctx->scope, callee->data.identifier.name);
        return sym != NULL
            && sym->type != NULL
            && sym->type->kind == TYPE_KIND_FUNCTION
            && type_effect_mask_requires_authority(type_function_effects(sym->type));
    }

    if (callee->type == AST_MEMBER_ACCESS
        && callee->data.member.object != NULL
        && callee->data.member.name != NULL) {
        Type *object_type = type_check_expression(callee->data.member.object, ctx);
        ASTNode *class_decl;

        if (object_type == NULL || object_type->name == NULL)
            return false;

        class_decl = find_type_decl_by_name(ctx->program_root, object_type->name);
        if (class_decl == NULL || class_decl->type != AST_CLASS_DECL)
            return false;

        for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
            ASTNode *method = class_decl->data.class_decl.methods[i];
            if (method == NULL || method->type != AST_FUNC_DECL
                || method->data.func_decl.name == NULL) {
                continue;
            }
            if (strcmp(method->data.func_decl.name, callee->data.member.name) == 0) {
                uint32_t method_effects =
                    declared_effects_from_function_node(method, ctx, NULL);
                if (type_effect_mask_requires_authority(method_effects))
                    return true;
                if (method->data.func_decl.is_action
                    && (method->data.func_decl.within_zone != NULL
                        || method->data.func_decl.causes_effect != NULL)) {
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
            if (node->data.lambda_expr.is_async)
                return "async lambda";
            return intent_forbidden_control_construct(node->data.lambda_expr.body);
        case AST_BINARY:
            nested = intent_forbidden_control_construct(node->data.binary.left);
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(node->data.binary.right);
        case AST_UNARY:
            return intent_forbidden_control_construct(node->data.unary.operand);
        case AST_CALL:
            nested = intent_forbidden_control_construct(node->data.call.callee);
            if (nested != NULL)
                return nested;
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                nested = intent_forbidden_control_construct(
                    node->data.call.arguments[i]);
                if (nested != NULL)
                    return nested;
            }
            return NULL;
        case AST_MEMBER_ACCESS:
            return intent_forbidden_control_construct(node->data.member.object);
        case AST_ARRAY_ACCESS:
            nested = intent_forbidden_control_construct(
                node->data.array_access.array);
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(
                node->data.array_access.index);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < node->data.array_literal.count; i++) {
                nested = intent_forbidden_control_construct(
                    node->data.array_literal.elements[i]);
                if (nested != NULL)
                    return nested;
            }
            return NULL;
        case AST_ASSIGNMENT:
            nested = intent_forbidden_control_construct(node->data.assignment.target);
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(node->data.assignment.value);
        case AST_MATCH_STMT:
            nested = intent_forbidden_control_construct(node->data.match_stmt.subject);
            if (nested != NULL)
                return nested;
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
                nested = intent_forbidden_control_construct(
                    node->data.match_stmt.cases[i]);
                if (nested != NULL)
                    return nested;
            }
            return intent_forbidden_control_construct(
                node->data.match_stmt.default_body);
        case AST_MATCH_CASE:
            nested = intent_forbidden_control_construct(node->data.match_case.pattern);
            if (nested != NULL)
                return nested;
            nested = intent_forbidden_control_construct(node->data.match_case.guard);
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(node->data.match_case.body);
        case AST_IF_STMT:
            nested = intent_forbidden_control_construct(node->data.if_stmt.condition);
            if (nested != NULL)
                return nested;
            nested = intent_forbidden_control_construct(node->data.if_stmt.then_branch);
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(node->data.if_stmt.else_branch);
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                nested = intent_forbidden_control_construct(
                    node->data.block.statements[i]);
                if (nested != NULL)
                    return nested;
            }
            return NULL;
        case AST_RETURN:
            return intent_forbidden_control_construct(node->data.return_stmt.value);
        case AST_DEFER_STMT:
            return intent_forbidden_control_construct(node->data.defer_stmt.body);
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
