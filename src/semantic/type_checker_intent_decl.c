#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "type_checker_module_contract_internal.h"
#include "diag_codes.h"

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
    Symbol *existing = scope_lookup_current(ctx->scope, name);

    steps = ast_intent_decl_steps(node, &step_count);
    priority_expr = ast_intent_decl_priority_expr(node);
    success_expr = ast_intent_decl_success_expr(node);
    failure_expr = ast_intent_decl_failure_expr(node);

    if (existing != NULL && existing->kind != SYMBOL_INTENT) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_INTENT_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of intent '%s'", name);
        return false;
    }
    if (existing != NULL && existing->kind == SYMBOL_INTENT) {
        if (!type_check_intent_update_existing_signature(node, existing, ctx))
            return false;
    }

    type_check_intent_resolve_binding_types(node, ctx);

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    type_check_intent_declare_binding_symbols(node, ctx);

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = steps[i];
        bool matched_action = false;
        ASTNode *zone_decl = NULL;
        bool has_subintent = false;
        bool step_requires_authority_flow = false;
        bool on_action_zone_conflict = false;
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

        for (size_t j = 0; j < on_expr_count; j++) {
            if (on_exprs[j] != NULL) {
                intent_clause_rejects_control_transfer(on_exprs[j], ctx,
                    step_name, "on");
                type_check_expression(on_exprs[j], ctx);
                if (intent_clause_invokes_authority_sensitive_call(
                        on_exprs[j], ctx))
                    step_requires_authority_flow = true;
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
    }

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

    if (success_expr != NULL) {
        intent_clause_rejects_control_transfer(success_expr, ctx,
            name, "success");
        intent_condition_is_bool(success_expr, ctx, "success");
    }
    if (failure_expr != NULL) {
        intent_clause_rejects_control_transfer(failure_expr, ctx,
            name, "failure");
        intent_condition_is_bool(failure_expr, ctx, "failure");
    }

    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx);
