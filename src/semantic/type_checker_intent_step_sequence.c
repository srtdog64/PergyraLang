#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "type_checker_module_contract_internal.h"
#include "type_checker_intent_step_sequence_internal.h"
#include "diag_codes.h"

#include <string.h>

static bool
type_check_intent_step_bind_outcome(ASTNode *intent_decl,
                                    ASTNode *step,
                                    Type *outcome_type,
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

    action_decl = intent_step_resolve_single_on_action_decl(
        intent_decl, step, ctx, NULL);
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL
        || !ast_func_is_action(action_decl)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP,
            PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS,
            step,
            "Intent step '%s' outcome binding '%s' requires one exactly resolved subject action call.\n"
            "Reason:\n"
            "- ordinary functions and computed callees do not carry action authority identity\n"
            "Fix:\n"
            "- bind the result of '<participant>.<Action>(...)'\n"
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

void
type_check_intent_step_sequence(
    ASTNode *node,
    SemanticContext *ctx,
    Type **typed_success_payload_types,
    Type **typed_failure_payload_types,
    size_t *typed_success_scope_count_out)
{
    ASTNode **steps = NULL;
    size_t step_count = 0;
    bool typed_result = ast_intent_decl_has_typed_result(node);
    size_t typed_success_scope_count = 0;

    steps = ast_intent_decl_steps(node, &step_count);
    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = steps[i];
        bool matched_action = false;
        ASTNode *zone_decl = NULL;
        bool has_subintent = false;
        bool step_requires_authority_flow = false;
        bool on_action_zone_conflict = false;
        bool outcome_scope_entered = false;
        Type *bound_outcome_type = NULL;
        const char *step_name;
        ASTNode *where_type;
        ASTNode *using_expr;
        ASTNode *intent_expr;
        ASTNode **on_exprs;
        size_t on_expr_count;
        ASTNode **compensate_exprs;
        size_t compensate_expr_count;
        ASTNode *pre_expr;
        ASTNode *guard_expr;
        ASTNode *post_expr;
        ASTNode *invariant_expr;
        ASTNode *expect_expr;
        const char *causes_effect;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        intent_step_derive_who_from_on_receiver(node, step, ctx);
        intent_step_derive_who_from_action(node, step, ctx);
        intent_step_derive_who_from_single_participant(node, step, ctx);
        intent_step_derive_where_from_on_receiver(node, step, ctx);
        on_action_zone_conflict =
            intent_step_report_on_action_zone_conflict(node, step, ctx);
        intent_step_inherit_contract_from_on_receiver(node, step, ctx);
        intent_step_inherit_action_contract(node, step, ctx);
        intent_step_derive_transfer_context(node, step, ctx);
        intent_step_derive_zone_binding_context(node, step, ctx);
        intent_step_warn_redundant_action_contract(node, step, ctx);

        step_name = ast_intent_step_name(step);
        where_type = ast_intent_step_where_type(step);
        using_expr = ast_intent_step_using_expr(step);
        intent_expr = ast_intent_step_intent_expr(step);
        on_exprs = ast_intent_step_on_exprs(step, &on_expr_count);
        compensate_exprs = ast_intent_step_compensate_exprs(
            step, &compensate_expr_count);
        pre_expr = ast_intent_step_pre_expr(step);
        guard_expr = ast_intent_step_guard_expr(step);
        post_expr = ast_intent_step_post_expr(step);
        invariant_expr = ast_intent_step_invariant_expr(step);
        expect_expr = ast_intent_step_expect_expr(step);
        causes_effect = ast_intent_step_causes_effect(step);
        has_subintent = (intent_expr != NULL);

        if (where_type == NULL
            && !has_subintent
            && !on_action_zone_conflict) {
            char contract_summary[512];
            intent_step_format_contract_source_summary(
                node, step, ctx, contract_summary, sizeof(contract_summary));
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' requires a where zone type.\n"
                "Reason:\n"
                "- no local, reused, or derived zone contract is available for this step\n"
                "%s%s"
                "Fix:\n"
                "- add 'where: <Zone>;' to the step\n"
                "- or add 'within <Zone>' to the matching action contract\n"
                "- or add a transfer/using binding that can derive the zone",
                step_name != NULL ? step_name : "<step>",
                contract_summary[0] != '\0' ? contract_summary : "",
                contract_summary[0] != '\0' ? "\n" : "");
        } else {
            Type *zone_type = NULL;
            zone_decl = intent_resolve_step_where_zone_decl(
                step, ctx, &zone_type);
            if (where_type != NULL && zone_decl == NULL) {
                char contract_summary[512];
                intent_step_format_contract_source_summary(
                    node, step, ctx, contract_summary, sizeof(contract_summary));
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, where_type,
                    "Intent step '%s' refers to unknown zone '%s'.\n"
                    "Reason:\n"
                    "- where clauses must resolve to a declared zone\n"
                    "- this where value came from %s\n"
                    "%s%s"
                    "Fix:\n"
                    "- declare zone '%s'\n"
                    "- or change the where/default/derived source to a declared zone",
                    step_name != NULL ? step_name : "<step>",
                    zone_type != NULL ? zone_type->name : "<zone>",
                    intent_step_where_source_label(step),
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    zone_type != NULL ? zone_type->name : "<zone>");
            }
        }

        if (using_expr != NULL) {
            Type *using_type = intent_normalize_type(
                type_check_expression(using_expr, ctx));
            Type *zone_type = intent_normalize_type(
                intent_resolve_step_where_type(step, ctx));
            intent_clause_rejects_control_transfer(using_expr, ctx,
                step_name, "using");
            if (using_expr->type != AST_IDENTIFIER) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, using_expr,
                    "Intent step '%s' using clause must reference an intent parameter alias.\n"
                    "Reason:\n"
                    "- compressed using derivation can only bind a named intent participant or value\n"
                    "- expression-valued using clauses would make the derived zone contract ambiguous\n"
                    "Fix:\n"
                    "- replace the using clause with an intent parameter alias\n"
                    "- or write an explicit step where clause instead of deriving it from using",
                    step_name != NULL ? step_name : "<step>");
            } else if (find_intent_involves_local(node,
                    ast_identifier_name(using_expr)) == NULL
                && find_intent_value_local(node,
                    ast_identifier_name(using_expr)) == NULL) {
                const char *using_name = ast_identifier_name(using_expr);
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, using_expr,
                    "Intent step '%s' using clause refers to unknown binding '%s'.\n"
                    "Reason:\n"
                    "- compressed using derivation only resolves aliases declared by this intent\n"
                    "- no intent participant or value binding named '%s' is visible at this step\n"
                    "Fix:\n"
                    "- declare '%s' in the intent participant/value list\n"
                    "- or change the using clause to an existing binding alias",
                    step_name != NULL ? step_name : "<step>",
                    using_name != NULL ? using_name : "<binding>",
                    using_name != NULL ? using_name : "<binding>",
                    using_name != NULL ? using_name : "<binding>");
            }
            if (using_type != TYPE_UNKNOWN && zone_type != TYPE_UNKNOWN
                && !type_equals(using_type, zone_type)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, using_expr,
                    "Intent step '%s' using binding must match zone type '%s', got '%s'.\n"
                    "Reason:\n"
                    "- using derives the step boundary from the same zone contract as where\n"
                    "- using binding points to a different zone than the current where contract\n"
                    "Fix:\n"
                    "- change using to a binding of type '%s'\n"
                    "- or change the step where clause to match the using binding",
                    step_name != NULL ? step_name : "<step>",
                    type_name_or_unknown(zone_type),
                    type_name_or_unknown(using_type),
                    type_name_or_unknown(zone_type));
            }
        }

        type_check_intent_step_transfer_contract(node, step, ctx);

        if (ast_intent_step_who_count(step) == 0 && !has_subintent) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step, "Intent step '%s' requires at least one who participant",
                step_name != NULL ? step_name : "<step>");
        }

        if (intent_expr != NULL) {
            Type *intent_type = intent_normalize_type(
                type_check_expression(intent_expr, ctx));
            const char *callee_name = "<callee>";
            intent_clause_rejects_control_transfer(intent_expr, ctx,
                step_name, "intent");
            if (intent_expr->type != AST_CALL
                || ast_call_callee(intent_expr) == NULL
                || ast_call_callee(intent_expr)->type != AST_IDENTIFIER) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, intent_expr,
                    "Intent step '%s' intent clause must call a named intent before lowering to Bool-gated orchestration.\n"
                    "Reason:\n"
                    "- compressed intent orchestration needs a stable declared intent target\n"
                    "- anonymous or computed callees cannot carry intent provenance into AIR\n"
                    "Fix:\n"
                    "- call a declared intent by name\n"
                    "- or replace the intent clause with an explicit Bool predicate",
                    step_name != NULL ? step_name : "<step>");
            } else {
                const char *resolved_name =
                    ast_identifier_name(ast_call_callee(intent_expr));
                callee_name = resolved_name != NULL ? resolved_name : callee_name;
                Symbol *intent_sym = callee_name != NULL
                    ? scope_lookup(ctx->scope, callee_name)
                    : NULL;
                if (intent_sym == NULL || intent_sym->kind != SYMBOL_INTENT) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, intent_expr,
                        "Intent step '%s' intent clause target '%s' is not a declared intent (resolved kind: %s).\n"
                        "Reason:\n"
                        "- boolean-gated orchestration requires a declared intent target\n"
                        "- non-intent callees do not carry intent step provenance into AIR\n"
                        "Fix:\n"
                        "- declare '%s' as an intent that returns Bool\n"
                        "- or replace this clause with a Bool-compatible predicate",
                        step_name != NULL ? step_name : "<step>",
                        callee_name != NULL ? callee_name : "<callee>",
                        intent_sym != NULL
                            ? semantic_symbol_kind_label(intent_sym->kind)
                            : "unresolved",
                        callee_name != NULL ? callee_name : "<callee>");
                }
            }
            if (intent_type != TYPE_UNKNOWN && !type_equals(intent_type, TYPE_BOOL)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, intent_expr,
                    "Intent step '%s' intent clause must return Bool for boolean-orchestration, got '%s'.\n"
                    "Reason:\n"
                    "- intent step orchestration treats the called intent as a Bool gate\n"
                    "- non-Bool results cannot decide whether the step may proceed\n"
                    "Fix:\n"
                    "- adjust callee '%s' to return Bool\n"
                    "- or wrap the call with a Bool predicate",
                    step_name != NULL ? step_name : "<step>",
                    type_name_or_unknown(intent_type),
                    callee_name != NULL ? callee_name : "<callee>");
            } else if (intent_type == TYPE_UNKNOWN) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, intent_expr,
                    "Intent step '%s' intent clause type could not be resolved as Bool.\n"
                    "Reason:\n"
                    "- boolean-gated orchestration requires the called clause to evaluate to Bool\n"
                    "- unresolved intent clause types cannot be lowered with stable AIR evidence\n"
                    "Fix:\n"
                    "- make '%s' return Bool\n"
                    "- or wrap it in a Bool predicate",
                    step_name != NULL ? step_name : "<step>",
                    callee_name != NULL ? callee_name : "<callee>");
            }
        }

        /* Preconditions run before an action result exists.  Keeping them in
           the intent parameter scope makes an early use of the optional
           outcome name fail closed as an undefined symbol. */
        if (pre_expr != NULL) {
            intent_clause_rejects_control_transfer(pre_expr, ctx,
                step_name, "pre");
            intent_condition_is_bool(pre_expr, ctx, "pre");
            if (intent_clause_invokes_authority_sensitive_call(
                    pre_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (guard_expr != NULL) {
            intent_clause_rejects_control_transfer(guard_expr, ctx,
                step_name, "guard");
            intent_condition_is_bool(guard_expr, ctx, "guard");
            if (intent_clause_invokes_authority_sensitive_call(
                    guard_expr, ctx))
                step_requires_authority_flow = true;
        }

        for (size_t j = 0; j < on_expr_count; j++) {
            if (on_exprs[j] != NULL) {
                Type *on_type;
                intent_clause_rejects_control_transfer(on_exprs[j], ctx,
                    step_name, "on");
                on_type = intent_normalize_type(
                    type_check_expression(on_exprs[j], ctx));
                if (j == 0)
                    bound_outcome_type = on_type;
                if (intent_clause_invokes_authority_sensitive_call(
                        on_exprs[j], ctx))
                    step_requires_authority_flow = true;
            }
        }
        (void)type_check_intent_step_bind_outcome(
            node, step, bound_outcome_type, ctx, &outcome_scope_entered);
        if (typed_result && outcome_scope_entered) {
            if (intent_typed_resolve_step_branches(
                    step, bound_outcome_type, ctx,
                    &typed_success_payload_types[i],
                    &typed_failure_payload_types[i])) {
                (void)intent_typed_declare_payload_binding(
                    step, true, typed_success_payload_types[i], ctx);
            }
        }

        for (size_t j = 0; j < compensate_expr_count; j++) {
            if (compensate_exprs[j] != NULL) {
                intent_clause_rejects_control_transfer(
                    compensate_exprs[j], ctx,
                    step_name, "compensate");
                type_check_expression(compensate_exprs[j], ctx);
                if (intent_clause_invokes_authority_sensitive_call(
                        compensate_exprs[j], ctx))
                    step_requires_authority_flow = true;
            }
        }
        if (post_expr != NULL) {
            intent_clause_rejects_control_transfer(post_expr, ctx,
                step_name, "post");
            intent_condition_is_bool(post_expr, ctx, "post");
            if (intent_clause_invokes_authority_sensitive_call(
                    post_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (invariant_expr != NULL) {
            intent_clause_rejects_control_transfer(invariant_expr, ctx,
                step_name, "invariant");
            intent_condition_is_bool(invariant_expr, ctx, "invariant");
            if (intent_clause_invokes_authority_sensitive_call(
                    invariant_expr, ctx))
                step_requires_authority_flow = true;
        }

        type_check_intent_step_participant_contract(
            node, step, zone_decl, &matched_action, ctx);

        type_check_intent_step_ability_contract(node, step, ctx);

        if (expect_expr != NULL) {
            intent_clause_rejects_control_transfer(expect_expr, ctx,
                step_name, "expect");
            intent_condition_is_bool(expect_expr, ctx, "expect");
            if (intent_clause_invokes_authority_sensitive_call(
                    expect_expr, ctx))
                step_requires_authority_flow = true;
        }

        if (causes_effect != NULL
            && intent_find_effect_decl_by_name(causes_effect, ctx) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' causes unknown effect '%s'.\n"
                "Reason:\n"
                "- causes requires an effect declaration visible to this intent contract\n"
                "- compile-time contract validation cannot be repaired by runtime sync\n"
                "Contract source:\n"
                "- intent step causes clause\n"
                "Fix:\n"
                "- declare/export effect '%s'\n"
                "- or change/remove the causes clause",
                step_name != NULL ? step_name : "<step>",
                causes_effect,
                causes_effect != NULL ? causes_effect : "<effect>");
        } else if (causes_effect != NULL
            && zone_decl != NULL
            && !zone_has_effect_layer_type(zone_decl, causes_effect)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' causes effect '%s', but zone '%s' has no matching effect slot.\n"
                "Reason:\n"
                "- causes requires the current zone to materialize a matching effect layer\n"
                "- zone '%s' does not declare any effect slot of type '%s'\n"
                "Fix:\n"
                "- add an effect slot of type '%s' to zone '%s'\n"
                "- or remove/change the causes clause",
                step_name != NULL ? step_name : "<step>",
                causes_effect,
                ast_zone_name(zone_decl) != NULL ? ast_zone_name(zone_decl) : "<zone>",
                ast_zone_name(zone_decl) != NULL ? ast_zone_name(zone_decl) : "<zone>",
                causes_effect,
                causes_effect,
                ast_zone_name(zone_decl) != NULL ? ast_zone_name(zone_decl) : "<zone>");
        }

        type_check_intent_step_authority_contract(
            node, step, zone_decl, has_subintent, step_requires_authority_flow,
            ctx);

        if (!matched_action && on_expr_count == 0 && intent_expr == NULL) {
            char contract_summary[512];
            intent_step_format_contract_source_summary(
                node, step, ctx, contract_summary, sizeof(contract_summary));
            semantic_warning(ctx, step,
                "Intent step '%s' does not currently match a subject action of the same name; intent is declarative only.\n"
                "Reason:\n"
                "- matching action contract reuse only applies when a unique matching subject action exists\n"
                "%s%s"
                "Fix:\n"
                "- align the step name and who participants with a subject action if you want reused contracts\n"
                "- or keep the step declarative and spell out who/where/requires/authorized by explicitly",
                step_name != NULL ? step_name : "<step>",
                contract_summary[0] != '\0' ? contract_summary : "",
                contract_summary[0] != '\0' ? "\n" : "");
        }

        if (outcome_scope_entered)
            scope_exit(&ctx->scope);
        if (typed_result && typed_success_payload_types[i] != NULL) {
            Scope *parent_scope = ctx->scope;
            scope_enter(&ctx->scope, SCOPE_BLOCK);
            if (ctx->scope == parent_scope
                || !intent_typed_declare_payload_binding(
                    step, true, typed_success_payload_types[i], ctx)) {
                if (ctx->scope != parent_scope)
                    scope_exit(&ctx->scope);
            } else {
                typed_success_scope_count++;
            }
        }
    }

    if (typed_success_scope_count_out != NULL)
        *typed_success_scope_count_out = typed_success_scope_count;
}
