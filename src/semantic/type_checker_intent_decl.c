#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "type_checker_intent_step_sequence_internal.h"
#include "type_checker_module_contract_internal.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

bool
type_check_intent_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = ast_intent_decl_name(node);
    ASTNode **steps = NULL;
    size_t step_count = 0;
    ASTNode *priority_expr;
    ASTNode *success_expr;
    ASTNode *failure_expr;
    bool typed_result = ast_intent_decl_has_typed_result(node);
    Type *typed_return_type = TYPE_BOOL;
    Type **typed_success_payload_types = NULL;
    Type **typed_failure_payload_types = NULL;
    size_t typed_success_scope_count = 0;
    Symbol *existing = scope_lookup_current(ctx->scope, name);

    steps = ast_intent_decl_steps(node, &step_count);
    priority_expr = ast_intent_decl_priority_expr(node);
    success_expr = ast_intent_decl_success_expr(node);
    failure_expr = ast_intent_decl_failure_expr(node);

    if (ast_intent_decl_retry_count(node) > 0) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP,
            PGY_FIX_CHECK_INTENT_STEP_LOWERING,
            node,
            "Intent retry(%d) is parsed and carried by MIR, but execution lowering is not implemented.\n"
            "Reason:\n"
            "- C and LLVM must wrap the same MIR intent body before retry is executable\n"
            "Fix:\n"
            "- remove the retry modifier until backend retry lowering lands",
            ast_intent_decl_retry_count(node));
        return false;
    }

    if (existing != NULL
        && !symbol_is_forward_declaration_for(existing,
            SYMBOL_INTENT, ast_node_stable_id(node))) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_INTENT_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of intent '%s'", name);
        return false;
    }
    if (existing != NULL) {
        if (!type_check_intent_update_existing_signature(node, existing, ctx))
            return false;
        symbol_complete_forward_declaration(existing);
    }

    type_check_intent_resolve_binding_types(node, ctx);

    if (!intent_typed_validate_topology(node, ctx))
        return false;
    if (typed_result) {
        ASTNode *return_enum;
        typed_return_type = intent_normalize_type(
            intent_resolve_type_ref(ast_intent_decl_return_type(node), ctx));
        return_enum = typed_return_type != TYPE_UNKNOWN
                && typed_return_type->name != NULL
            ? semantic_find_enum_decl_by_name(ctx, typed_return_type->name)
            : NULL;
        if (typed_return_type == TYPE_UNKNOWN || return_enum == NULL) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_INTENT_STEP_INVALID,
                PGY_CAUSE_INTENT_STEP,
                PGY_FIX_CHECK_INTENT_STEP_LOWERING,
                node,
                "Typed intent '%s' return type must resolve to an enum declaration.",
                name != NULL ? name : "<intent>");
            return false;
        }
        typed_success_payload_types = calloc(
            step_count, sizeof(Type *));
        typed_failure_payload_types = calloc(
            step_count, sizeof(Type *));
        if (typed_success_payload_types == NULL
            || typed_failure_payload_types == NULL) {
            free(typed_success_payload_types);
            free(typed_failure_payload_types);
            semantic_error(ctx, node,
                "Out of memory while validating typed intent outcomes");
            return false;
        }
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    type_check_intent_declare_binding_symbols(node, ctx);

    type_check_intent_step_sequence(
        node, ctx, typed_success_payload_types,
        typed_failure_payload_types, &typed_success_scope_count);

    if (priority_expr != NULL) {
        Type *priority_type = intent_normalize_type(
            type_check_expression(priority_expr, ctx));
        intent_clause_rejects_control_transfer(priority_expr, ctx,
            name, "priority");
        if (priority_type != TYPE_UNKNOWN && !type_equals(priority_type, TYPE_INT)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_INTENT_PRIORITY_NON_INT, PGY_FIX_USE_INT_PRIORITY,
                priority_expr,
                "Intent priority expression must be Int, got '%s'",
                type_name_or_unknown(priority_type));
        }
    }

    if (typed_result) {
        ASTNode *terminal = ast_intent_decl_success_terminal_expr(node);
        Type *terminal_type = terminal != NULL
            ? intent_normalize_type(type_check_expression(terminal, ctx))
            : TYPE_UNKNOWN;
        if (terminal != NULL)
            intent_clause_rejects_control_transfer(
                terminal, ctx, name, "typed success terminal");
        if (terminal_type != TYPE_UNKNOWN
            && !type_equals(terminal_type, typed_return_type)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_INTENT_STEP,
                PGY_FIX_CHECK_INTENT_STEP_LOWERING,
                terminal,
                "Typed intent '%s' success terminal must return '%s', got '%s'.",
                name != NULL ? name : "<intent>",
                type_name_or_unknown(typed_return_type),
                type_name_or_unknown(terminal_type));
        }
        if (step_count > 0) {
            (void)intent_typed_resolve_terminal_result(
                node, true, 0, steps[step_count - 1], typed_return_type,
                typed_success_payload_types[step_count - 1], ctx);
        }
        while (typed_success_scope_count > 0) {
            scope_exit(&ctx->scope);
            typed_success_scope_count--;
        }
        for (size_t i = 0;
             i < ast_intent_decl_failure_terminal_count(node);
             i++) {
            ASTNode *step = intent_typed_step_for_failure_terminal(node, i);
            ASTNode *terminal_expr =
                ast_intent_decl_failure_terminal_expr(node, i);
            Type *payload_type = NULL;
            Type *terminal_type;
            Scope *parent_scope;
            for (size_t j = 0; j < step_count; j++) {
                if (steps[j] == step) {
                    payload_type = typed_failure_payload_types[j];
                    break;
                }
            }
            parent_scope = ctx->scope;
            scope_enter(&ctx->scope, SCOPE_BLOCK);
            if (ctx->scope == parent_scope) {
                semantic_error(ctx, terminal_expr,
                    "Out of memory while opening typed intent failure terminal scope");
                continue;
            }
            if (payload_type == NULL
                || !intent_typed_declare_payload_binding(
                    step, false, payload_type, ctx)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_INTENT_STEP_INVALID,
                    PGY_CAUSE_INTENT_STEP,
                    PGY_FIX_CHECK_INTENT_STEP_LOWERING,
                    terminal_expr,
                    "Typed intent failure terminal for step '%s' has no sealed failure payload.",
                    ast_intent_step_name(step));
            }
            terminal_type = terminal_expr != NULL
                ? intent_normalize_type(
                    type_check_expression(terminal_expr, ctx))
                : TYPE_UNKNOWN;
            if (terminal_expr != NULL)
                intent_clause_rejects_control_transfer(
                    terminal_expr, ctx, name, "typed failure terminal");
            if (terminal_type != TYPE_UNKNOWN
                && !type_equals(terminal_type, typed_return_type)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_INTENT_STEP,
                    PGY_FIX_CHECK_INTENT_STEP_LOWERING,
                    terminal_expr,
                    "Typed intent '%s' failure terminal for step '%s' must return '%s', got '%s'.",
                    name != NULL ? name : "<intent>",
                    ast_intent_step_name(step),
                    type_name_or_unknown(typed_return_type),
                    type_name_or_unknown(terminal_type));
            }
            (void)intent_typed_resolve_terminal_result(
                node, false, i, step, typed_return_type, payload_type, ctx);
            scope_exit(&ctx->scope);
        }
    } else if (success_expr != NULL) {
        intent_clause_rejects_control_transfer(success_expr, ctx,
            name, "success");
        intent_condition_is_bool(success_expr, ctx, "success");
    }
    if (!typed_result && failure_expr != NULL) {
        intent_clause_rejects_control_transfer(failure_expr, ctx,
            name, "failure");
        intent_condition_is_bool(failure_expr, ctx, "failure");
    }

    scope_exit(&ctx->scope);
    free(typed_success_payload_types);
    free(typed_failure_payload_types);
    return !ctx->has_error;
}

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx);
