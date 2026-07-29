/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * DIR intent participant, step, outcome, and terminal validation.
 */

#include "dir_validate_internal.h"

#include <string.h>

#include "../parser/ast_api.h"

static bool
dir_nullable_string_equal(const char *left, const char *right)
{
    if (left == NULL || right == NULL)
        return left == right;
    return strcmp(left, right) == 0;
}

static bool
dir_intent_branch_matches_ast(const DIRIntentOutcomeBranch *branch,
                              const ASTNode *step,
                              bool success)
{
    const char *variant = success
        ? ast_intent_step_success_variant_name(step)
        : ast_intent_step_failure_variant_name(step);
    const char *payload = success
        ? ast_intent_step_success_payload_name(step)
        : ast_intent_step_failure_payload_name(step);
    const char *payload_type = success
        ? ast_intent_step_success_payload_type_name(step)
        : ast_intent_step_failure_payload_type_name(step);
    size_t variant_index = success
        ? ast_intent_step_success_variant_index(step)
        : ast_intent_step_failure_variant_index(step);

    return branch != NULL
        && dir_nullable_string_equal(branch->variant_name, variant)
        && branch->variant_index == variant_index
        && dir_nullable_string_equal(branch->payload_name, payload)
        && dir_nullable_string_equal(
            branch->payload_type_name, payload_type)
        && branch->payload_decl_syntax_id
            == (success
                ? ast_intent_step_success_payload_decl_syntax_id(step)
                : ast_intent_step_failure_payload_decl_syntax_id(step))
        && dir_nullable_string_equal(
            branch->enum_type_name,
            ast_intent_step_outcome_enum_type_name(step))
        && branch->enum_decl_syntax_id
            == ast_intent_step_outcome_enum_decl_syntax_id(step);
}

static bool
dir_intent_terminal_matches_ast(const DIRIntentTerminal *terminal,
                                const ASTNode *intent,
                                bool success,
                                size_t failure_index)
{
    return terminal != NULL
        && dir_nullable_string_equal(
            terminal->result_type_name,
            ast_intent_decl_terminal_result_type_name(
                intent, success, failure_index))
        && terminal->result_enum_decl_syntax_id
            == ast_intent_decl_terminal_result_enum_decl_syntax_id(
                intent, success, failure_index)
        && terminal->result_variant_index
            == ast_intent_decl_terminal_result_variant_index(
                intent, success, failure_index)
        && dir_nullable_string_equal(
            terminal->result_variant_name,
            ast_intent_decl_terminal_result_variant_name(
                intent, success, failure_index))
        && dir_nullable_string_equal(
            terminal->result_payload_name,
            ast_intent_decl_terminal_result_payload_name(
                intent, success, failure_index))
        && dir_nullable_string_equal(
            terminal->result_payload_type_name,
            ast_intent_decl_terminal_result_payload_type_name(
                intent, success, failure_index))
        && terminal->result_payload_decl_syntax_id
            == ast_intent_decl_terminal_result_payload_decl_syntax_id(
                intent, success, failure_index);
}

static bool
dir_intent_terminal_matches_source(
    const DIRIntentTerminal *terminal,
    const DIRIntentOutcomeBranch *source_branch,
    const char *return_type_name,
    uint32_t result_enum_decl_syntax_id)
{
    return terminal != NULL && source_branch != NULL
        && terminal->result_enum_decl_syntax_id != 0
        && terminal->result_enum_decl_syntax_id
            == result_enum_decl_syntax_id
        && terminal->result_payload_decl_syntax_id != 0
        && terminal->result_payload_decl_syntax_id
            == source_branch->payload_decl_syntax_id
        && terminal->result_variant_index != SIZE_MAX
        && terminal->result_variant_name != NULL
        && terminal->result_variant_name[0] != '\0'
        && dir_nullable_string_equal(
            terminal->result_type_name, return_type_name)
        && dir_nullable_string_equal(
            terminal->result_payload_name, source_branch->payload_name)
        && dir_nullable_string_equal(
            terminal->result_payload_type_name,
            source_branch->payload_type_name);
}

bool
dir_validate_intents(const DIRProgram *dir, char **error_message)
{
    for (size_t i = 0; i < dir->intent_count; i++) {
        const DIRIntentInfo *intent = &dir->intents[i];
        if (intent->node_id >= dir->node_count) {
            if (error_message != NULL) {
                *error_message = dir_validate_strdup_fmt(
                    "DIR intent[%llu] has invalid node id",
                    (unsigned long long)i);
            }
            return false;
        }
        if (intent->return_type_name == NULL
            || intent->return_type_name[0] == '\0') {
            if (error_message != NULL) {
                *error_message = dir_validate_strdup_fmt(
                    "DIR intent[%llu] has no explicit result carrier",
                    (unsigned long long)i);
            }
            return false;
        }
        for (size_t j = 0; j < intent->participant_count; j++) {
            const DIRIntentParticipant *participant = &intent->participants[j];
            if (!participant->is_value_binding
                && (participant->subject_type_node_id == SIZE_MAX
                    || participant->subject_type_node_id >= dir->node_count)) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] participant '%s' is unresolved",
                        (unsigned long long)i,
                        participant->alias != NULL ? participant->alias : "-");
                }
                return false;
            }
        }
        for (size_t j = 0; j < intent->step_count; j++) {
            const DIRIntentStep *step = &intent->steps[j];
            const char *ast_outcome_name;
            const char *ast_outcome_type;
            uint32_t ast_action_id;
            size_t ast_on_count;
            if (step->index != j) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step[%llu] has unstable index",
                        (unsigned long long)i,
                        (unsigned long long)j);
                }
                return false;
            }
            if (step->ast == NULL || step->ast->type != AST_INTENT_STEP) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step '%s' has no exact AST step owner",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if (step->syntax_id == 0
                || step->syntax_id != ast_node_stable_id(step->ast)) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step '%s' stable identity drifted",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            ast_outcome_name =
                ast_intent_step_outcome_binding_name(step->ast);
            ast_outcome_type =
                ast_intent_step_outcome_binding_type_name(step->ast);
            ast_action_id =
                ast_intent_step_outcome_action_decl_syntax_id(step->ast);
            ast_on_count = ast_intent_step_on_expr_count(step->ast);
            if (step->on_expr_count != ast_on_count) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step '%s' on expression count drifted from its AST owner",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if ((step->outcome_binding_name == NULL)
                    != (ast_outcome_name == NULL)
                || (step->outcome_binding_type_name == NULL)
                    != (ast_outcome_type == NULL)
                || (step->outcome_binding_name != NULL
                    && strcmp(step->outcome_binding_name,
                              ast_outcome_name) != 0)
                || (step->outcome_binding_type_name != NULL
                    && strcmp(step->outcome_binding_type_name,
                              ast_outcome_type) != 0)
                || step->outcome_action_decl_syntax_id != ast_action_id) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step '%s' outcome binding metadata drifted from its semantic AST owner",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if (step->outcome_binding_name != NULL
                && (step->outcome_binding_name[0] == '\0'
                    || step->outcome_binding_type_name == NULL
                    || step->outcome_binding_type_name[0] == '\0'
                    || strcmp(step->outcome_binding_type_name, "Void") == 0
                    || step->outcome_action_decl_syntax_id == 0
                    || step->on_expr_count != 1)) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step '%s' has incomplete outcome binding name/type/action identity or non-single on expression",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if (step->outcome_binding_name == NULL
                && (step->outcome_binding_type_name != NULL
                    || step->outcome_action_decl_syntax_id != 0)) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step '%s' has outcome type/action identity without a binding",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if (step->outcome_binding_name != NULL) {
                for (size_t k = 0; k < j; k++) {
                    const char *prior_name =
                        intent->steps[k].outcome_binding_name;
                    if (prior_name != NULL
                        && strcmp(prior_name,
                                  step->outcome_binding_name) == 0) {
                        if (error_message != NULL) {
                            *error_message = dir_validate_strdup_fmt(
                                "DIR intent[%llu] outcome binding '%s' is duplicated across steps",
                                (unsigned long long)i,
                                step->outcome_binding_name);
                        }
                        return false;
                    }
                }
            }
            if (step->where_type_name != NULL
                && (step->where_type_node_id == SIZE_MAX
                    || step->where_type_node_id >= dir->node_count)) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step '%s' has unresolved where zone",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if ((step->where_inherited_from_action
                 || step->where_inherited_from_intent
                 || step->where_derived_from_using
                 || step->where_derived_from_transfer)
                && step->where_type_name == NULL) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step '%s' has zone provenance without a zone",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if ((step->using_derived_from_where
                 || step->using_derived_from_transfer)
                && step->using_alias == NULL) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step '%s' has using provenance without a binding",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if (step->predecessor_step_name != NULL && j == 0) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] first step '%s' cannot have predecessor",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if (step->predecessor_step_name != NULL
                && step->predecessor_step_index >= j) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step '%s' has invalid predecessor index",
                        (unsigned long long)i,
                        step->name != NULL ? step->name : "-");
                }
                return false;
            }
            if (intent->has_typed_result) {
                if ((j == 0
                        && (step->predecessor_step_name != NULL
                            || step->predecessor_step_index != SIZE_MAX
                            || step->predecessor_step_syntax_id != 0))
                    || (j > 0
                        && (step->predecessor_step_name == NULL
                            || step->predecessor_step_index + 1 != j
                            || step->predecessor_step_syntax_id
                                != intent->steps[j - 1].syntax_id
                            || !dir_nullable_string_equal(
                                step->predecessor_step_name,
                                intent->steps[j - 1].name)))) {
                    if (error_message != NULL) {
                        *error_message = dir_validate_strdup_fmt(
                            "DIR typed intent[%llu] step '%s' explicit predecessor identity drifted",
                            (unsigned long long)i,
                            step->name != NULL ? step->name : "-");
                    }
                    return false;
                }
                if (!dir_intent_branch_matches_ast(
                        &step->success_branch, step->ast, true)
                    || !dir_intent_branch_matches_ast(
                        &step->failure_branch, step->ast, false)
                    || step->success_branch.enum_decl_syntax_id == 0
                    || step->success_branch.enum_decl_syntax_id
                        != step->failure_branch.enum_decl_syntax_id
                    || step->success_branch.variant_index == SIZE_MAX
                    || step->failure_branch.variant_index == SIZE_MAX
                    || step->success_branch.variant_index
                        == step->failure_branch.variant_index) {
                    if (error_message != NULL) {
                        *error_message = dir_validate_strdup_fmt(
                            "DIR typed intent[%llu] step '%s' outcome branch seal drifted",
                            (unsigned long long)i,
                            step->name != NULL ? step->name : "-");
                    }
                    return false;
                }
            }
        }
        if (intent->has_typed_result) {
            if (intent->step_count == 0
                || intent->success_terminal.step_index + 1
                    != intent->step_count
                || intent->success_terminal.step_syntax_id
                    != intent->steps[intent->step_count - 1].syntax_id
                || !dir_nullable_string_equal(
                    intent->success_terminal.step_name,
                    intent->steps[intent->step_count - 1].name)
                || intent->success_terminal.expr == NULL
                || !dir_intent_terminal_matches_ast(
                    &intent->success_terminal,
                    intent->ast,
                    true, 0)
                || !dir_intent_terminal_matches_source(
                    &intent->success_terminal,
                    &intent->steps[intent->step_count - 1].success_branch,
                    intent->return_type_name,
                    intent->success_terminal.result_enum_decl_syntax_id)
                || intent->failure_terminal_count != intent->step_count
                || (intent->failure_terminal_count > 0
                    && intent->failure_terminals == NULL)) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR typed intent[%llu] terminal coverage drifted",
                        (unsigned long long)i);
                }
                return false;
            }
            for (size_t j = 0; j < intent->failure_terminal_count; j++) {
                const DIRIntentTerminal *terminal =
                    &intent->failure_terminals[j];
                if (terminal->step_index >= intent->step_count
                    || terminal->step_syntax_id
                        != intent->steps[terminal->step_index].syntax_id
                    || !dir_nullable_string_equal(
                        terminal->step_name,
                        intent->steps[terminal->step_index].name)
                    || terminal->expr == NULL
                    || !dir_intent_terminal_matches_ast(
                        terminal,
                        intent->ast,
                        false, j)
                    || !dir_intent_terminal_matches_source(
                        terminal,
                        &intent->steps[terminal->step_index].failure_branch,
                        intent->return_type_name,
                        intent->success_terminal.result_enum_decl_syntax_id)) {
                    if (error_message != NULL) {
                        *error_message = dir_validate_strdup_fmt(
                            "DIR typed intent[%llu] failure terminal[%llu] identity drifted",
                            (unsigned long long)i,
                            (unsigned long long)j);
                    }
                    return false;
                }
            }
            for (size_t step_index = 0;
                 step_index < intent->step_count;
                 step_index++) {
                size_t matching_terminal_count = 0;

                for (size_t terminal_index = 0;
                     terminal_index < intent->failure_terminal_count;
                     terminal_index++) {
                    if (intent->failure_terminals[terminal_index].step_syntax_id
                        == intent->steps[step_index].syntax_id) {
                        matching_terminal_count++;
                    }
                }
                if (matching_terminal_count != 1) {
                    if (error_message != NULL) {
                        *error_message = dir_validate_strdup_fmt(
                            "DIR typed intent[%llu] step '%s' failure terminal coverage drifted",
                            (unsigned long long)i,
                            intent->steps[step_index].name != NULL
                                ? intent->steps[step_index].name : "-");
                    }
                    return false;
                }
            }
        } else if (intent->failure_terminal_count != 0
                   || intent->failure_terminals != NULL
                   || intent->success_terminal.expr != NULL) {
            if (error_message != NULL) {
                *error_message = dir_validate_strdup_fmt(
                    "DIR legacy intent[%llu] contains typed terminal facts",
                    (unsigned long long)i);
            }
            return false;
        }
    }
    return true;
}
