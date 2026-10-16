#include "type_checker_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "type_checker_intent_step_sequence_internal.h"
#include "diag_codes.h"

#include <string.h>

bool
type_check_intent_step_bind_outcome(ASTNode *intent_decl,
                                    ASTNode *step,
                                    Type *outcome_type,
                                    ASTNode *child_intent,
                                    SemanticContext *ctx,
                                    bool *scope_entered_out)
{
    const char *binding_name = ast_intent_step_outcome_binding_name(step);
    ASTNode *action_decl;
    Symbol *binding;
    Scope *parent_scope;
    uint32_t action_syntax_id;

    if (scope_entered_out != NULL)
        *scope_entered_out = false;
    if (binding_name == NULL)
        return true;

    if (binding_name[0] == '\0'
        || ast_intent_step_outcome_binding_length(step) == 0) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP,
            PGY_FIX_CHECK_INTENT_STEP_LOWERING,
            step,
            "Intent step '%s' has an empty action outcome binding.",
            ast_intent_step_name(step) != NULL
                ? ast_intent_step_name(step) : "<step>");
        return false;
    }
    {
        size_t intent_step_count = 0;
        ASTNode **intent_steps = ast_intent_decl_steps(
            intent_decl, &intent_step_count);
        for (size_t i = 0; i < intent_step_count; i++) {
            const char *prior_name;
            if (intent_steps[i] == step)
                break;
            prior_name = ast_intent_step_outcome_binding_name(
                intent_steps[i]);
            if (prior_name != NULL
                && strcmp(prior_name, binding_name) == 0) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_REDECLARATION,
                    PGY_CAUSE_INTENT_DUPLICATE_NAME,
                    PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
                    step,
                    "Intent outcome binding '%s' is already used by an earlier step.\n"
                    "Reason:\n"
                    "- outcome values remain available to the intent rollback tail\n"
                    "Fix:\n"
                    "- give every bound action outcome in this intent a distinct name",
                    binding_name);
                return false;
            }
        }
    }
    if (ast_intent_step_on_expr_count(step) != 1) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP,
            PGY_FIX_CHECK_INTENT_STEP_LOWERING,
            step,
            "Intent step '%s' outcome binding '%s' requires exactly one on-call.\n"
            "Reason:\n"
            "- one immutable binding cannot identify more than one action result\n"
            "Fix:\n"
            "- keep one 'on %s: <action-call>;' in this step\n"
            "- move additional actions into separate ordered steps",
            ast_intent_step_name(step) != NULL
                ? ast_intent_step_name(step) : "<step>",
            binding_name, binding_name);
        return false;
    }

    action_decl = child_intent != NULL
        ? child_intent
        : intent_step_resolve_single_on_action_decl(
            intent_decl, step, ctx, NULL);
    if (action_decl == NULL
        || (child_intent == NULL
            && (action_decl->type != AST_FUNC_DECL
                || !ast_func_is_action(action_decl)))) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP,
            PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS,
            step,
            "Intent step '%s' outcome binding '%s' requires one exactly resolved subject action call or declared intent call.\n"
            "Reason:\n"
            "- ordinary functions and computed callees do not carry action authority identity\n"
            "Fix:\n"
            "- bind the result of '<participant>.<Action>(...)' or of a typed '<Intent>(...)'\n"
            "- keep an unbound legacy 'on: <expr>;' for non-action expressions",
            ast_intent_step_name(step) != NULL
                ? ast_intent_step_name(step) : "<step>",
            binding_name);
        return false;
    }
    if (outcome_type == NULL || outcome_type == TYPE_UNKNOWN) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP,
            PGY_FIX_CHECK_INTENT_STEP_LOWERING,
            step,
            "Intent step '%s' outcome binding '%s' has no exact inferred action return type.",
            ast_intent_step_name(step) != NULL
                ? ast_intent_step_name(step) : "<step>",
            binding_name);
        return false;
    }
    if (type_equals(outcome_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP,
            PGY_FIX_CHECK_INTENT_STEP_LOWERING,
            step,
            "Intent step '%s' cannot bind Void action result as '%s'.\n"
            "Fix:\n"
            "- return an explicit typed outcome from the action\n"
            "- or use legacy 'on: <action-call>;' when no result is consumed",
            ast_intent_step_name(step) != NULL
                ? ast_intent_step_name(step) : "<step>",
            binding_name);
        return false;
    }
    if (scope_lookup_current(ctx->scope, binding_name) != NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_INTENT_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            step,
            "Intent step '%s' outcome binding '%s' conflicts with an existing intent binding.",
            ast_intent_step_name(step) != NULL
                ? ast_intent_step_name(step) : "<step>",
            binding_name);
        return false;
    }

    action_syntax_id = ast_node_stable_id(action_decl);
    if (action_syntax_id == 0 || outcome_type->name == NULL
        || outcome_type->name[0] == '\0'
        || !ast_intent_step_set_outcome_resolution_copy(
            step, outcome_type->name, action_syntax_id)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP,
            PGY_FIX_CHECK_INTENT_STEP_LOWERING,
            step,
            "Intent step '%s' could not preserve the exact action outcome owner seam.",
            ast_intent_step_name(step) != NULL
                ? ast_intent_step_name(step) : "<step>");
        return false;
    }

    parent_scope = ctx->scope;
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    if (ctx->scope == parent_scope) {
        semantic_error(ctx, step,
            "Out of memory while opening intent step outcome scope");
        return false;
    }
    binding = symbol_create_variable(
        binding_name, outcome_type,
        ast_intent_step_outcome_binding_line(step),
        ast_intent_step_outcome_binding_column(step));
    if (binding == NULL || !scope_declare(ctx->scope, binding)) {
        symbol_destroy(binding);
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_INTENT_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            step,
            "Intent step '%s' could not declare immutable outcome binding '%s'.",
            ast_intent_step_name(step) != NULL
                ? ast_intent_step_name(step) : "<step>",
            binding_name);
        scope_exit(&ctx->scope);
        return false;
    }
    binding->is_mut_binding = false;
    if (scope_entered_out != NULL)
        *scope_entered_out = true;
    return true;
}
