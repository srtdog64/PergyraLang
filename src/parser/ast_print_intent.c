#include "ast_print_internal.h"

#include <stdio.h>

void
print_intent_step_contract_sources(const ASTNode *node, int indent)
{
    bool printed = false;

    if (node == NULL || node->type != AST_INTENT_STEP)
        return;
    if (!node->data.intent_step.inherited_who_from_action
        && !node->data.intent_step.inherited_where_from_action
        && !node->data.intent_step.inherited_who_from_intent
        && !node->data.intent_step.derived_who_from_on_receiver
        && !node->data.intent_step.derived_who_from_single_participant
        && !node->data.intent_step.inherited_where_from_intent
        && !node->data.intent_step.inherited_requires_from_action
        && !node->data.intent_step.inherited_causes_from_action
        && !node->data.intent_step.inherited_authorized_by_from_action
        && !node->data.intent_step.derived_authorized_by_from_zone
        && !node->data.intent_step.derived_where_from_using
        && !node->data.intent_step.derived_where_from_transfer
        && !node->data.intent_step.derived_using_from_transfer
        && !node->data.intent_step.derived_using_from_where) {
        return;
    }

    ast_print_indent(indent);
    printf("ContractProvenance: ");

    if (node->data.intent_step.inherited_who_from_action) {
        printf("reused who from matching action contract");
        printed = true;
    }
    if (node->data.intent_step.inherited_who_from_intent) {
        printf("%sreused who from intent-level default", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.derived_who_from_on_receiver) {
        printf("%sderived who from on-call receiver", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.derived_who_from_single_participant) {
        printf("%sderived who from single subject participant", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.inherited_where_from_action) {
        printf("%sreused zone from matching action contract", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.inherited_where_from_intent) {
        printf("%sreused zone from intent-level default", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.inherited_requires_from_action) {
        printf("%sreused requires from matching action contract", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.inherited_causes_from_action) {
        printf("%sreused causes from matching action contract", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.inherited_authorized_by_from_action) {
        printf("%sreused authorized by from matching action contract", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.derived_authorized_by_from_zone) {
        printf("%szone-authority approval provenance (legacy field, not who inference)",
            printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.derived_where_from_transfer) {
        printf("%sderived zone from transfer target", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.derived_where_from_using) {
        printf("%sderived zone from using binding", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.derived_using_from_transfer) {
        printf("%sderived using from transfer target", printed ? ", " : "");
        printed = true;
    }
    if (node->data.intent_step.derived_using_from_where) {
        printf("%sderived using from zone type", printed ? ", " : "");
        printed = true;
    }
    printf("\n");
}

bool
ast_print_intent_node(ASTNode *node, int indent)
{
    if (node == NULL)
        return false;

    switch (node->type) {
        case AST_INTENT_DECL:
            printf("Intent: %s\n", node->data.intent_decl.name);
            if (node->data.intent_decl.return_type != NULL) {
                ast_print_indent(indent + 1);
                printf("IntentReturns: ");
                ast_print_inline(node->data.intent_decl.return_type);
                printf("\n");
            }
            if (node->data.intent_decl.is_concurrent) {
                ast_print_indent(indent + 1);
                printf("IntentMode: concurrent\n");
            } else {
                ast_print_indent(indent + 1);
                printf("IntentMode: exclusive\n");
            }
            ast_print_indent(indent + 1);
            printf("IntentRollback: %s\n",
                node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_NONE ? "none" :
                node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_CURRENT ? "current" :
                "full");
            if (node->data.intent_decl.retry_count > 0) {
                ast_print_indent(indent + 1);
                printf("IntentRetry: %d\n",
                    node->data.intent_decl.retry_count);
            }
            if (node->data.intent_decl.priority_expr != NULL) {
                ast_print_indent(indent + 1);
                printf("IntentPriority: ");
                ast_print_inline(node->data.intent_decl.priority_expr);
                printf("\n");
            }
            if (node->data.intent_decl.binding_count > 0) {
                for (size_t i = 0; i < node->data.intent_decl.binding_count; i++) {
                    ast_print(node->data.intent_decl.bindings[i], indent + 1);
                }
            } else {
                for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
                    ast_print(node->data.intent_decl.involves[i], indent + 1);
                }
                for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
                    ast_print(node->data.intent_decl.values[i], indent + 1);
                }
            }
            for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
                ast_print(node->data.intent_decl.steps[i], indent + 1);
            }
            if (node->data.intent_decl.success_expr != NULL) {
                ast_print_indent(indent + 1);
                printf("IntentSuccess: ");
                ast_print_inline(node->data.intent_decl.success_expr);
                printf("\n");
            }
            if (node->data.intent_decl.failure_expr != NULL) {
                ast_print_indent(indent + 1);
                printf("IntentFailure: ");
                ast_print_inline(node->data.intent_decl.failure_expr);
                printf("\n");
            }
            if (node->data.intent_decl.success_terminal.expr != NULL) {
                ast_print_indent(indent + 1);
                printf("IntentTerminalSuccess: %s => ",
                    node->data.intent_decl.success_terminal.step_name);
                ast_print_inline(node->data.intent_decl.success_terminal.expr);
                printf("\n");
            }
            for (size_t i = 0;
                 i < node->data.intent_decl.failure_terminal_count;
                 i++) {
                ast_print_indent(indent + 1);
                printf("IntentTerminalFailure: %s => ",
                    node->data.intent_decl.failure_terminals[i].step_name);
                ast_print_inline(
                    node->data.intent_decl.failure_terminals[i].expr);
                printf("\n");
            }
            break;

        case AST_INTENT_INVOLVES:
            printf("IntentInvolves: %s: ", node->data.intent_involves.alias);
            ast_print_inline(node->data.intent_involves.subject_type);
            printf("\n");
            break;

        case AST_INTENT_VALUE:
            printf("IntentValue: %s: ", node->data.intent_value.alias);
            ast_print_inline(node->data.intent_value.value_type);
            printf("\n");
            break;

        case AST_INTENT_STEP:
            printf("IntentStep: %s", node->data.intent_step.name);
            if (node->data.intent_step.predecessor_step_name != NULL) {
                printf(" after %s",
                    node->data.intent_step.predecessor_step_name);
            }
            if (node->data.intent_step.where_type != NULL) {
                printf(" where ");
                ast_print_inline(node->data.intent_step.where_type);
            }
            if (node->data.intent_step.using_expr != NULL) {
                printf(" using ");
                ast_print_inline(node->data.intent_step.using_expr);
            }
            if (node->data.intent_step.intent_expr != NULL) {
                printf(" intent ");
                ast_print_inline(node->data.intent_step.intent_expr);
            }
            if (node->data.intent_step.transfer_from_alias != NULL
                && node->data.intent_step.transfer_to_alias != NULL) {
                printf(" transfer %s -> %s",
                    node->data.intent_step.transfer_from_alias,
                    node->data.intent_step.transfer_to_alias);
            }
            printf("\n");
            if (node->data.intent_step.who_count > 0) {
                ast_print_indent(indent + 1);
                printf("Who: ");
                for (size_t i = 0; i < node->data.intent_step.who_count; i++) {
                    if (i > 0) printf(", ");
                    printf("%s", node->data.intent_step.who_names[i]);
                }
                printf("\n");
            }
            if (node->data.intent_step.using_expr != NULL) {
                ast_print_indent(indent + 1);
                printf("Using: ");
                ast_print_inline(node->data.intent_step.using_expr);
                printf("\n");
            }
            if (node->data.intent_step.intent_expr != NULL) {
                ast_print_indent(indent + 1);
                printf("Intent: ");
                ast_print_inline(node->data.intent_step.intent_expr);
                printf("\n");
            }
            if (node->data.intent_step.transfer_from_alias != NULL
                && node->data.intent_step.transfer_to_alias != NULL) {
                ast_print_indent(indent + 1);
                printf("Transfer: %s -> %s\n",
                    node->data.intent_step.transfer_from_alias,
                    node->data.intent_step.transfer_to_alias);
            }
            if (node->data.intent_step.on_expr_count > 0) {
                for (size_t i = 0; i < node->data.intent_step.on_expr_count; i++) {
                    ast_print_indent(indent + 1);
                    if (i == 0
                        && node->data.intent_step.outcome_binding_name != NULL) {
                        printf("On %s: ",
                            node->data.intent_step.outcome_binding_name);
                    } else {
                        printf("On: ");
                    }
                    ast_print_inline(node->data.intent_step.on_exprs[i]);
                    printf("\n");
                }
            }
            if (node->data.intent_step.success_branch.variant_name != NULL) {
                ast_print_indent(indent + 1);
                printf("IntentStepSuccess: %s(%s)\n",
                    node->data.intent_step.success_branch.variant_name,
                    node->data.intent_step.success_branch.payload_name);
            }
            if (node->data.intent_step.failure_branch.variant_name != NULL) {
                ast_print_indent(indent + 1);
                printf("IntentStepFailure: %s(%s)\n",
                    node->data.intent_step.failure_branch.variant_name,
                    node->data.intent_step.failure_branch.payload_name);
            }
            if (node->data.intent_step.compensate_expr_count > 0) {
                for (size_t i = 0; i < node->data.intent_step.compensate_expr_count; i++) {
                    ast_print_indent(indent + 1);
                    printf("Compensate: ");
                    ast_print_inline(node->data.intent_step.compensate_exprs[i]);
                    printf("\n");
                }
            }
            if (node->data.intent_step.pre_expr != NULL) {
                ast_print_indent(indent + 1);
                printf("Pre: ");
                ast_print_inline(node->data.intent_step.pre_expr);
                printf("\n");
            }
            if (node->data.intent_step.guard_expr != NULL) {
                ast_print_indent(indent + 1);
                printf("Guard: ");
                ast_print_inline(node->data.intent_step.guard_expr);
                printf("\n");
            }
            if (node->data.intent_step.post_expr != NULL) {
                ast_print_indent(indent + 1);
                printf("Post: ");
                ast_print_inline(node->data.intent_step.post_expr);
                printf("\n");
            }
            if (node->data.intent_step.invariant_expr != NULL) {
                ast_print_indent(indent + 1);
                printf("Invariant: ");
                ast_print_inline(node->data.intent_step.invariant_expr);
                printf("\n");
            }
            if (node->data.intent_step.required_ability_count > 0) {
                ast_print_indent(indent + 1);
                printf("Requires: ");
                for (size_t i = 0; i < node->data.intent_step.required_ability_count; i++) {
                    if (i > 0) printf(", ");
                    ast_print_inline(node->data.intent_step.required_abilities[i]);
                }
                printf("\n");
            }
            if (node->data.intent_step.authorized_by_count > 0) {
                ast_print_indent(indent + 1);
                printf("AuthorizedBy: ");
                for (size_t i = 0; i < node->data.intent_step.authorized_by_count; i++) {
                    if (i > 0) printf(", ");
                    printf("%s", node->data.intent_step.authorized_by[i]);
                }
                printf("\n");
            }
            if (node->data.intent_step.causes_effect != NULL) {
                ast_print_indent(indent + 1);
                printf("Causes: %s\n", node->data.intent_step.causes_effect);
            }
            if (node->data.intent_step.expect_expr != NULL) {
                ast_print_indent(indent + 1);
                printf("Expect: ");
                ast_print_inline(node->data.intent_step.expect_expr);
                printf("\n");
            }
            print_intent_step_contract_sources(node, indent + 1);
            break;

        default:
            return false;
    }

    return true;
}
