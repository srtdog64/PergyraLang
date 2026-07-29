#include "region_retention_summary.h"

#include <string.h>

#include "builtin_kind.h"
#include "type_checker_internal.h"
#include "../parser/ast.h"
#include "../parser/ast_analysis.h"
#include "../parser/ast_api.h"

static bool
region_retention_identifier_matches(const char *candidate, void *userdata)
{
    const char *parameter_name = userdata;

    return candidate != NULL
        && parameter_name != NULL
        && strcmp(candidate, parameter_name) == 0;
}

static bool
region_retention_param_is_direct_identifier(const ASTNode *expr,
                                            const char *parameter_name)
{
    return expr != NULL
        && expr->type == AST_IDENTIFIER
        && ast_identifier_name(expr) != NULL
        && parameter_name != NULL
        && strcmp(ast_identifier_name(expr), parameter_name) == 0;
}

static bool
region_retention_body_is_safe(const ASTNode *node,
                              const char *parameter_name)
{
    if (node == NULL)
        return true;

    switch (node->type) {
    case AST_PROGRAM:
        for (size_t i = 0; i < ast_program_statement_count(node); i++) {
            if (!region_retention_body_is_safe(
                    ast_program_statement(node, i), parameter_name))
                return false;
        }
        return true;
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            if (!region_retention_body_is_safe(
                    ast_block_statement(node, i), parameter_name))
                return false;
        }
        return true;
    case AST_IF_STMT:
        return region_retention_body_is_safe(ast_if_condition(node),
                                             parameter_name)
            && region_retention_body_is_safe(ast_if_then_branch(node),
                                             parameter_name)
            && region_retention_body_is_safe(ast_if_else_branch(node),
                                             parameter_name);
    case AST_WHILE_LOOP:
        return region_retention_body_is_safe(ast_while_condition(node),
                                             parameter_name)
            && region_retention_body_is_safe(ast_while_body(node),
                                             parameter_name);
    case AST_FOR_LOOP:
        return region_retention_body_is_safe(ast_for_range_start(node),
                                             parameter_name)
            && region_retention_body_is_safe(ast_for_range_end(node),
                                             parameter_name)
            && region_retention_body_is_safe(ast_for_iterable(node),
                                             parameter_name)
            && region_retention_body_is_safe(ast_for_body(node),
                                             parameter_name);
    case AST_WITH_STMT:
        return region_retention_body_is_safe(ast_with_slot_type(node),
                                             parameter_name)
            && region_retention_body_is_safe(ast_with_body(node),
                                             parameter_name);
    case AST_CALL:
        if (!region_retention_body_is_safe(ast_call_callee(node),
                                           parameter_name))
            return false;
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            const ASTNode *argument = ast_call_argument(node, i);
            bool uses_parameter = ast_contains_identifier_ref(
                argument,
                region_retention_identifier_matches,
                (void *)parameter_name);
            if (uses_parameter) {
                uint32_t builtin_kind = 0;
                PgyRegionRetentionKind kind =
                    PGY_REGION_RETENTION_UNKNOWN;
                if (!region_retention_param_is_direct_identifier(
                        argument, parameter_name)
                    || !ast_call_semantic_callee_builtin_kind(
                        node, &builtin_kind)
                    || !semantic_region_retention_summary_for_builtin(
                        builtin_kind, i, &kind)
                    || kind != PGY_REGION_RETENTION_BORROWED_FOR_CALL) {
                    return false;
                }
                continue;
            }
            if (!region_retention_body_is_safe(argument, parameter_name))
                return false;
        }
        return true;
    default:
        /* Unlisted shapes are not admitted as a retention proof. */
        return !ast_contains_identifier_ref(
            node,
            region_retention_identifier_matches,
            (void *)parameter_name);
    }
}

bool
semantic_region_retention_summary_for_user_call(
    const struct ASTNode *call,
    size_t argument_index,
    PgyRegionRetentionKind *kind_out,
    void *semantic_context)
{
    SemanticContext *ctx = semantic_context;
    ASTNode *callee_decl;
    const ASTNode *callee_expr;
    FuncParam *parameter;
    uint32_t builtin_kind;
    uint32_t decl_id;
    const char *callee_name;

    if (kind_out != NULL)
        *kind_out = PGY_REGION_RETENTION_UNKNOWN;
    if (call == NULL || call->type != AST_CALL || ctx == NULL)
        return false;

    if (ast_call_semantic_callee_builtin_kind(call, &builtin_kind)
        && builtin_kind != (uint32_t)BUILTIN_NOT_BUILTIN)
        return false;
    decl_id = ast_call_semantic_callee_decl_id(call);
    callee_expr = ast_call_callee(call);
    callee_name = callee_expr != NULL
        ? ast_identifier_name(callee_expr) : NULL;
    if (decl_id == 0 || callee_expr == NULL
        || callee_expr->type != AST_IDENTIFIER || callee_name == NULL)
        return false;

    callee_decl = semantic_find_callable_decl_by_name(ctx, callee_name);
    if (callee_decl == NULL
        || callee_decl->type != AST_FUNC_DECL
        || ast_node_stable_id(callee_decl) != decl_id
        || argument_index >= ast_func_param_count(callee_decl)) {
        return false;
    }
    parameter = ast_func_param(callee_decl, argument_index);
    if (parameter == NULL || parameter->mode != PARAM_MODE_REF
        || parameter->name == NULL
        || ast_func_body(callee_decl) == NULL
        || !region_retention_body_is_safe(
            ast_func_body(callee_decl), parameter->name)) {
        return false;
    }

    if (kind_out != NULL)
        *kind_out = PGY_REGION_RETENTION_BORROWED_FOR_CALL;
    return true;
}
