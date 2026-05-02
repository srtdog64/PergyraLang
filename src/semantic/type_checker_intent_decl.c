#include "type_checker_internal.h"
#include "type_checker_ability_match_internal.h"
#include "type_checker_ability_ref_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "type_checker_module_contract_internal.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

bool
type_check_intent_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.intent_decl.name;
    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind != SYMBOL_INTENT) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_INTENT_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of intent '%s'", name);
        return false;
    }
    if (existing != NULL && existing->kind == SYMBOL_INTENT) {
        size_t ipc = node->data.intent_decl.binding_count > 0
            ? node->data.intent_decl.binding_count
            : (node->data.intent_decl.involve_count
                + node->data.intent_decl.value_count);
        Type **ptypes = calloc(ipc > 0 ? ipc : 1, sizeof(Type *));
        Type *ft;
        for (size_t i = 0; i < ipc; i++) {
            ASTNode *binding = node->data.intent_decl.binding_count > 0
                ? node->data.intent_decl.bindings[i]
                : (i < node->data.intent_decl.involve_count
                    ? node->data.intent_decl.involves[i]
                    : node->data.intent_decl.values[i - node->data.intent_decl.involve_count]);
            if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                && binding->data.intent_involves.subject_type != NULL) {
                ptypes[i] = intent_resolve_involves_type(binding, ctx);
            } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                && binding->data.intent_value.value_type != NULL) {
                ptypes[i] = intent_resolve_value_type(binding, ctx);
            } else {
                ptypes[i] = TYPE_UNKNOWN;
            }
        }
        ft = type_create_function(ptypes, ipc, TYPE_BOOL);
        free(ptypes);
        if (ft != NULL) {
            existing->type = ft;
        }
    }

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        (void)intent_resolve_involves_type(involves, ctx);
    }
    for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
        ASTNode *value = node->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        (void)intent_resolve_value_type(value, ctx);
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        Type *subject_type;
        Symbol *participant_sym;

        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;

        subject_type = intent_resolve_involves_type(involves, ctx);
        participant_sym = symbol_create_variable(involves->data.intent_involves.alias,
            subject_type, involves->line, involves->column);
        scope_declare(ctx->scope, participant_sym);
    }
    for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
        ASTNode *value = node->data.intent_decl.values[i];
        Type *value_type;
        Symbol *value_sym;

        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;

        value_type = intent_resolve_value_type(value, ctx);
        value_sym = symbol_create_variable(value->data.intent_value.alias,
            value_type, value->line, value->column);
        scope_declare(ctx->scope, value_sym);
    }

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        bool matched_action = false;
        ASTNode *zone_decl = NULL;
        bool has_subintent = false;
        bool step_requires_authority_flow = false;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        has_subintent = (step->data.intent_step.intent_expr != NULL);
        intent_step_derive_who_from_on_receiver(node, step, ctx);
        intent_step_derive_who_from_action(node, step, ctx);
        intent_step_derive_who_from_single_participant(node, step, ctx);
        intent_step_derive_where_from_on_receiver(node, step, ctx);
        intent_step_inherit_contract_from_on_receiver(node, step, ctx);
        intent_step_inherit_action_contract(node, step, ctx);
        intent_step_derive_transfer_context(node, step, ctx);
        intent_step_derive_zone_binding_context(node, step, ctx);
        intent_step_warn_redundant_action_contract(node, step, ctx);

        if (step->data.intent_step.where_type == NULL && !has_subintent) {
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
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                contract_summary[0] != '\0' ? contract_summary : "",
                contract_summary[0] != '\0' ? "\n" : "");
        } else {
            Type *zone_type = intent_resolve_step_where_type(step, ctx);
            zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                zone_type != NULL ? zone_type->name : NULL);
            if (step->data.intent_step.where_type != NULL && zone_decl == NULL) {
                char contract_summary[512];
                intent_step_format_contract_source_summary(
                    node, step, ctx, contract_summary, sizeof(contract_summary));
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step->data.intent_step.where_type,
                    "Intent step '%s' refers to unknown zone '%s'.\n"
                    "Reason:\n"
                    "- where clauses must resolve to a declared zone\n"
                    "- this where value came from %s\n"
                    "%s%s"
                    "Fix:\n"
                    "- declare zone '%s'\n"
                    "- or change the where/default/derived source to a declared zone",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    zone_type != NULL ? zone_type->name : "<zone>",
                    intent_step_where_source_label(step),
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    zone_type != NULL ? zone_type->name : "<zone>");
            }
        }

        if (step->data.intent_step.using_expr != NULL) {
            Type *using_type = intent_normalize_type(
                type_check_expression(step->data.intent_step.using_expr, ctx));
            Type *zone_type = intent_normalize_type(
                intent_resolve_step_where_type(step, ctx));
            intent_clause_rejects_control_transfer(step->data.intent_step.using_expr, ctx,
                step->data.intent_step.name, "using");
            if (step->data.intent_step.using_expr->type != AST_IDENTIFIER) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step->data.intent_step.using_expr,
                    "Intent step '%s' using clause must reference an intent parameter alias.\n"
                    "Reason:\n"
                    "- compressed using derivation can only bind a named intent participant or value\n"
                    "- expression-valued using clauses would make the derived zone contract ambiguous\n"
                    "Fix:\n"
                    "- replace the using clause with an intent parameter alias\n"
                    "- or write an explicit step where clause instead of deriving it from using",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            } else if (find_intent_involves_local(node,
                    step->data.intent_step.using_expr->data.identifier.name) == NULL
                && find_intent_value_local(node,
                    step->data.intent_step.using_expr->data.identifier.name) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step->data.intent_step.using_expr,
                    "Intent step '%s' using clause refers to unknown binding '%s'.\n"
                    "Reason:\n"
                    "- compressed using derivation only resolves aliases declared by this intent\n"
                    "- no intent participant or value binding named '%s' is visible at this step\n"
                    "Fix:\n"
                    "- declare '%s' in the intent participant/value list\n"
                    "- or change the using clause to an existing binding alias",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>");
            }
            if (using_type != TYPE_UNKNOWN && zone_type != TYPE_UNKNOWN
                && !type_equals(using_type, zone_type)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step->data.intent_step.using_expr,
                    "Intent step '%s' using binding must match zone type '%s', got '%s'.\n"
                    "Reason:\n"
                    "- using derives the step boundary from the same zone contract as where\n"
                    "- using binding points to a different zone than the current where contract\n"
                    "Fix:\n"
                    "- change using to a binding of type '%s'\n"
                    "- or change the step where clause to match the using binding",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    type_name_or_unknown(zone_type),
                    type_name_or_unknown(using_type),
                    type_name_or_unknown(zone_type));
            }
        }

        type_check_intent_step_transfer_contract(node, step, ctx);

        if (step->data.intent_step.who_count == 0 && !has_subintent) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step, "Intent step '%s' requires at least one who participant",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
        }

        if (step->data.intent_step.intent_expr != NULL) {
            Type *intent_type = intent_normalize_type(
                type_check_expression(step->data.intent_step.intent_expr, ctx));
            const char *callee_name = "<callee>";
            intent_clause_rejects_control_transfer(step->data.intent_step.intent_expr, ctx,
                step->data.intent_step.name, "intent");
            if (step->data.intent_step.intent_expr->type != AST_CALL
                || step->data.intent_step.intent_expr->data.call.callee == NULL
                || step->data.intent_step.intent_expr->data.call.callee->type != AST_IDENTIFIER) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step->data.intent_step.intent_expr,
                    "Intent step '%s' intent clause must call a named intent before lowering to Bool-gated orchestration.\n"
                    "Reason:\n"
                    "- compressed intent orchestration needs a stable declared intent target\n"
                    "- anonymous or computed callees cannot carry intent provenance into AIR\n"
                    "Fix:\n"
                    "- call a declared intent by name\n"
                    "- or replace the intent clause with an explicit Bool predicate",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            } else {
                const char *resolved_name =
                    step->data.intent_step.intent_expr->data.call.callee->data.identifier.name;
                callee_name = resolved_name != NULL ? resolved_name : callee_name;
                Symbol *intent_sym = callee_name != NULL
                    ? scope_lookup(ctx->scope, callee_name)
                    : NULL;
                if (intent_sym == NULL || intent_sym->kind != SYMBOL_INTENT) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step->data.intent_step.intent_expr,
                        "Intent step '%s' intent clause target '%s' is not a declared intent (resolved kind: %s).\n"
                        "Reason:\n"
                        "- boolean-gated orchestration requires a declared intent target\n"
                        "- non-intent callees do not carry intent step provenance into AIR\n"
                        "Fix:\n"
                        "- declare '%s' as an intent that returns Bool\n"
                        "- or replace this clause with a Bool-compatible predicate",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        callee_name != NULL ? callee_name : "<callee>",
                        intent_sym != NULL
                            ? semantic_symbol_kind_label(intent_sym->kind)
                            : "unresolved",
                        callee_name != NULL ? callee_name : "<callee>");
                }
            }
            if (intent_type != TYPE_UNKNOWN && !type_equals(intent_type, TYPE_BOOL)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step->data.intent_step.intent_expr,
                    "Intent step '%s' intent clause must return Bool for boolean-orchestration, got '%s'.\n"
                    "Reason:\n"
                    "- intent step orchestration treats the called intent as a Bool gate\n"
                    "- non-Bool results cannot decide whether the step may proceed\n"
                    "Fix:\n"
                    "- adjust callee '%s' to return Bool\n"
                    "- or wrap the call with a Bool predicate",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    type_name_or_unknown(intent_type),
                    callee_name != NULL ? callee_name : "<callee>");
            } else if (intent_type == TYPE_UNKNOWN) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step->data.intent_step.intent_expr,
                    "Intent step '%s' intent clause type could not be resolved as Bool.\n"
                    "Reason:\n"
                    "- boolean-gated orchestration requires the called clause to evaluate to Bool\n"
                    "- unresolved intent clause types cannot be lowered with stable AIR evidence\n"
                    "Fix:\n"
                    "- make '%s' return Bool\n"
                    "- or wrap it in a Bool predicate",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    callee_name != NULL ? callee_name : "<callee>");
            }
        }

        for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
            if (step->data.intent_step.on_exprs[j] != NULL) {
                intent_clause_rejects_control_transfer(step->data.intent_step.on_exprs[j], ctx,
                    step->data.intent_step.name, "on");
                type_check_expression(step->data.intent_step.on_exprs[j], ctx);
                if (intent_clause_invokes_authority_sensitive_call(
                        step->data.intent_step.on_exprs[j], ctx))
                    step_requires_authority_flow = true;
            }
        }
        for (size_t j = 0; j < step->data.intent_step.compensate_expr_count; j++) {
            if (step->data.intent_step.compensate_exprs[j] != NULL) {
                intent_clause_rejects_control_transfer(
                    step->data.intent_step.compensate_exprs[j], ctx,
                    step->data.intent_step.name, "compensate");
                type_check_expression(step->data.intent_step.compensate_exprs[j], ctx);
                if (intent_clause_invokes_authority_sensitive_call(
                        step->data.intent_step.compensate_exprs[j], ctx))
                    step_requires_authority_flow = true;
            }
        }
        if (step->data.intent_step.pre_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.pre_expr, ctx,
                step->data.intent_step.name, "pre");
            intent_condition_is_bool(step->data.intent_step.pre_expr, ctx, "pre");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.pre_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (step->data.intent_step.guard_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.guard_expr, ctx,
                step->data.intent_step.name, "guard");
            intent_condition_is_bool(step->data.intent_step.guard_expr, ctx, "guard");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.guard_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (step->data.intent_step.post_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.post_expr, ctx,
                step->data.intent_step.name, "post");
            intent_condition_is_bool(step->data.intent_step.post_expr, ctx, "post");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.post_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (step->data.intent_step.invariant_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.invariant_expr, ctx,
                step->data.intent_step.name, "invariant");
            intent_condition_is_bool(step->data.intent_step.invariant_expr, ctx, "invariant");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.invariant_expr, ctx))
                step_requires_authority_flow = true;
        }

        type_check_intent_step_participant_contract(
            node, step, zone_decl, &matched_action, ctx);

        for (size_t j = 0; j < step->data.intent_step.required_ability_count; j++) {
            ASTNode *ability_ref = step->data.intent_step.required_abilities[j];
            const char *ability = ability_ref_name(ability_ref);
            char *required_text = ability_ref_display(ability_ref);
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                step,
                step->data.intent_step.name != NULL
                    ? step->data.intent_step.name : "<step>",
                ability_ref,
                "intent step ability consumer lookup");
            if (resolve_required_ability_decl(
                    ability_ref, step, ctx, "Intent step",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>") == NULL) {
                free(required_text);
                continue;
            }

            for (size_t k = 0; k < step->data.intent_step.who_count; k++) {
                const char *alias = step->data.intent_step.who_names[k];
                ASTNode *involves = find_intent_involves_local(node, alias);
                const char *participant_type_name = intent_involves_type_name(involves);
                if (!intent_involves_is_subject_host(ctx->program_root, involves))
                    continue;
                if (participant_type_name != NULL
                    && !subject_type_has_ability(ctx->program_root, participant_type_name, ability_ref)) {
                    ASTNode *actual_impl = subject_type_find_base_ability_impl(
                        ctx->program_root, participant_type_name, ability);
                    char *actual_text = actual_impl != NULL ? ability_ref_display(actual_impl) : NULL;
                    char contract_summary[512];
                    intent_step_format_contract_source_summary(
                        node, step, ctx, contract_summary, sizeof(contract_summary));
                    if (actual_impl != NULL) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                            "Intent step '%s' requires ability '%s', but participant '%s' of type '%s' implements '%s' instead.\n"
                            "Reason:\n"
                            "- participant type '%s' does not satisfy required ability '%s'\n"
                            "- actual implementation is '%s'\n"
                            "%s%s"
                            "Fix:\n"
                            "- implement '%s' for subject type '%s'\n"
                            "- or choose a participant whose subject type satisfies the contract\n"
                            "- or override/remove the inherited step requirement",
                            step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            alias != NULL ? alias : "<participant>",
                            participant_type_name,
                            actual_text,
                            participant_type_name,
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            actual_text,
                            contract_summary[0] != '\0' ? contract_summary : "",
                            contract_summary[0] != '\0' ? "\n" : "",
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            participant_type_name);
                    } else {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                            "Intent step '%s' requires ability '%s', but participant '%s' of type '%s' does not implement it.\n"
                            "Reason:\n"
                            "- participant type '%s' does not satisfy required ability '%s'\n"
                            "- no matching implementation was found for '%s'\n"
                            "%s%s"
                            "Fix:\n"
                            "- implement '%s' for subject type '%s'\n"
                            "- or choose a participant whose subject type satisfies the contract\n"
                            "- or override/remove the inherited step requirement",
                            step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            alias != NULL ? alias : "<participant>",
                            participant_type_name,
                            participant_type_name,
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            contract_summary[0] != '\0' ? contract_summary : "",
                            contract_summary[0] != '\0' ? "\n" : "",
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            participant_type_name);
                    }
                    free(actual_text);
                }
            }
            free(required_text);
        }

        if (step->data.intent_step.causes_effect != NULL
            && find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                step->data.intent_step.causes_effect) == NULL) {
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
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                step->data.intent_step.causes_effect,
                step->data.intent_step.causes_effect != NULL ? step->data.intent_step.causes_effect : "<effect>");
        } else if (step->data.intent_step.causes_effect != NULL
            && zone_decl != NULL
            && !zone_has_effect_layer_type(zone_decl, step->data.intent_step.causes_effect)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID, PGY_CAUSE_INTENT_STEP, PGY_FIX_ALIGN_STEP_WITH_ZONE_ACTION_CONTRACTS, step,
                "Intent step '%s' causes effect '%s', but zone '%s' has no matching effect slot.\n"
                "Reason:\n"
                "- causes requires the current zone to materialize a matching effect layer\n"
                "- zone '%s' does not declare any effect slot of type '%s'\n"
                "Fix:\n"
                "- add an effect slot of type '%s' to zone '%s'\n"
                "- or remove/change the causes clause",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                step->data.intent_step.causes_effect,
                zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                step->data.intent_step.causes_effect,
                step->data.intent_step.causes_effect,
                zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
        }

        type_check_intent_step_authority_contract(
            node, step, zone_decl, has_subintent, step_requires_authority_flow,
            ctx);

        if (step->data.intent_step.expect_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.expect_expr, ctx,
                step->data.intent_step.name, "expect");
            intent_condition_is_bool(step->data.intent_step.expect_expr, ctx, "expect");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.expect_expr, ctx))
                step_requires_authority_flow = true;
        }

        if (!matched_action && step->data.intent_step.on_expr_count == 0
            && step->data.intent_step.intent_expr == NULL) {
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
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                contract_summary[0] != '\0' ? contract_summary : "",
                contract_summary[0] != '\0' ? "\n" : "");
        }
    }

    if (node->data.intent_decl.priority_expr != NULL) {
        Type *priority_type = intent_normalize_type(
            type_check_expression(node->data.intent_decl.priority_expr, ctx));
        intent_clause_rejects_control_transfer(node->data.intent_decl.priority_expr, ctx,
            name, "priority");
        if (priority_type != TYPE_UNKNOWN && !type_equals(priority_type, TYPE_INT)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_INTENT_PRIORITY_NON_INT, PGY_FIX_USE_INT_PRIORITY,
                node->data.intent_decl.priority_expr,
                "Intent priority expression must be Int, got '%s'",
                type_name_or_unknown(priority_type));
        }
    }

    if (node->data.intent_decl.success_expr != NULL) {
        intent_clause_rejects_control_transfer(node->data.intent_decl.success_expr, ctx,
            name, "success");
        intent_condition_is_bool(node->data.intent_decl.success_expr, ctx, "success");
    }
    if (node->data.intent_decl.failure_expr != NULL) {
        intent_clause_rejects_control_transfer(node->data.intent_decl.failure_expr, ctx,
            name, "failure");
        intent_condition_is_bool(node->data.intent_decl.failure_expr, ctx, "failure");
    }

    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx);
