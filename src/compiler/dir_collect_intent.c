#include "dir_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    size_t next;

    if (capacity == NULL || elem_size == 0)
        return false;
    if (*capacity == 0) {
        next = initial;
    } else {
        if (*capacity > SIZE_MAX / 2)
            return false;
        next = *capacity * 2;
    }
    if (next > SIZE_MAX / elem_size)
        return false;
    *capacity = next;
    return true;
}

static void
clear_intent_step_names(DIRIntentStep *step)
{
    if (step == NULL)
        return;
    free((void *)step->who_names);
    free((void *)step->required_abilities);
    free((void *)step->authorized_by);
    step->who_names = NULL;
    step->required_abilities = NULL;
    step->authorized_by = NULL;
    step->who_count = 0;
    step->required_ability_count = 0;
    step->authorized_by_count = 0;
}

static bool
append_name(const char ***items, size_t *count, size_t *capacity, const char *name)
{
    const char **grown;
    if (name == NULL)
        return true;
    if (*count == *capacity) {
        size_t grown_capacity = *capacity;
        if (!next_capacity(&grown_capacity, 4, sizeof(const char *)))
            return false;
        grown = realloc((void *)*items, grown_capacity * sizeof(const char *));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = grown_capacity;
    }
    (*items)[*count] = name;
    (*count)++;
    return true;
}

static bool
append_intent_info(DIRIntentInfo **items,
                   size_t *count,
                   size_t *capacity,
                   DIRIntentInfo info)
{
    if (*count == *capacity) {
        size_t grown_capacity = *capacity;
        if (!next_capacity(&grown_capacity, 4, sizeof(DIRIntentInfo)))
            return false;
        DIRIntentInfo *grown = realloc(*items, grown_capacity * sizeof(DIRIntentInfo));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = grown_capacity;
    }
    (*items)[*count] = info;
    (*count)++;
    return true;
}

static bool
append_intent_participant(DIRIntentParticipant **items,
                          size_t *count,
                          size_t *capacity,
                          DIRIntentParticipant participant)
{
    if (*count == *capacity) {
        size_t grown_capacity = *capacity;
        if (!next_capacity(&grown_capacity, 4, sizeof(DIRIntentParticipant)))
            return false;
        DIRIntentParticipant *grown = realloc(*items, grown_capacity * sizeof(DIRIntentParticipant));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = grown_capacity;
    }
    (*items)[*count] = participant;
    (*count)++;
    return true;
}

static bool
append_intent_step(DIRIntentStep **items,
                   size_t *count,
                   size_t *capacity,
                   DIRIntentStep step)
{
    if (*count == *capacity) {
        size_t grown_capacity = *capacity;
        if (!next_capacity(&grown_capacity, 4, sizeof(DIRIntentStep)))
            return false;
        DIRIntentStep *grown = realloc(*items, grown_capacity * sizeof(DIRIntentStep));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = grown_capacity;
    }
    (*items)[*count] = step;
    (*count)++;
    return true;
}

static size_t
intent_step_index_for_syntax_id(ASTNode **steps,
                                size_t step_count,
                                uint32_t syntax_id)
{
    if (syntax_id == 0)
        return SIZE_MAX;
    for (size_t i = 0; i < step_count; i++) {
        if (ast_node_stable_id(steps[i]) == syntax_id)
            return i;
    }
    return SIZE_MAX;
}

bool
dir_collect_intent_info(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    DIRIntentInfo info;
    ASTNode **involves_nodes;
    ASTNode **values;
    ASTNode **steps;
    size_t involve_count;
    size_t value_count;
    size_t step_count;
    memset(&info, 0, sizeof(info));
    info.node_id = from_id;
    info.ast = node;
    info.has_typed_result = ast_intent_decl_has_typed_result(node);
    info.return_type_name = info.has_typed_result
        ? type_name(dir, ast_intent_decl_return_type(node))
        : "Bool";

    involves_nodes = ast_intent_decl_involves(node, &involve_count);
    values = ast_intent_decl_values(node, &value_count);
    steps = ast_intent_decl_steps(node, &step_count);

    for (size_t i = 0; i < involve_count; i++) {
        ASTNode *inv = involves_nodes[i];
        DIRIntentParticipant participant;
        memset(&participant, 0, sizeof(participant));
        participant.alias = ast_intent_involves_alias(inv);
        participant.subject_type_name = type_name(
            dir, ast_intent_involves_subject_type(inv));
        {
            ssize_t to = dir_find_any_node_by_name(dir, participant.subject_type_name);
            participant.subject_type_node_id = to >= 0 ? (size_t)to : SIZE_MAX;
        }
        if (!append_intent_participant(&info.participants,
                                       &info.participant_count,
                                       &info.participant_capacity,
                                       participant))
            goto oom;
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_INTENT_PARTICIPANT_TYPE,
                                from_id,
                                participant.subject_type_node_id,
                                participant.alias,
                                participant.subject_type_name))
            goto oom;
    }

    for (size_t i = 0; i < value_count; i++) {
        ASTNode *value = values[i];
        DIRIntentParticipant participant;
        memset(&participant, 0, sizeof(participant));
        participant.alias = ast_intent_value_alias(value);
        participant.subject_type_name = type_name(
            dir, ast_intent_value_type(value));
        participant.is_value_binding = true;
        {
            ssize_t to = dir_find_any_node_by_name(dir, participant.subject_type_name);
            participant.subject_type_node_id = to >= 0 ? (size_t)to : SIZE_MAX;
        }
        if (!append_intent_participant(&info.participants,
                                       &info.participant_count,
                                       &info.participant_capacity,
                                       participant))
            goto oom;
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_INTENT_PARTICIPANT_TYPE,
                                from_id,
                                participant.subject_type_node_id,
                                participant.alias,
                                participant.subject_type_name))
            goto oom;
    }

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step_node = steps[i];
        DIRIntentStep step;
        memset(&step, 0, sizeof(step));
        step.index = i;
        step.name = ast_intent_step_name(step_node);
        step.syntax_id = ast_node_stable_id(step_node);
        step.ast = step_node;
        step.where_type_name = type_name(dir, ast_intent_step_where_type(step_node));
        {
            ssize_t to = dir_find_zone_node_by_name(dir, step.where_type_name);
            step.where_type_node_id = to >= 0 ? (size_t)to : SIZE_MAX;
        }
        step.using_alias = ast_intent_step_using_expr(step_node) != NULL
            && ast_intent_step_using_expr(step_node)->type == AST_IDENTIFIER
            ? ast_identifier_name(ast_intent_step_using_expr(step_node))
            : NULL;
        step.predecessor_step_name = info.has_typed_result
            ? ast_intent_step_predecessor_name(step_node)
            : (i > 0 ? ast_intent_step_name(steps[i - 1]) : NULL);
        step.predecessor_step_syntax_id = info.has_typed_result
            ? ast_intent_step_predecessor_syntax_id(step_node)
            : (i > 0 ? ast_node_stable_id(steps[i - 1]) : 0);
        step.predecessor_step_index = intent_step_index_for_syntax_id(
            steps, step_count, step.predecessor_step_syntax_id);
        step.transfer_from_alias = ast_intent_step_transfer_from_alias(step_node);
        step.transfer_to_alias = ast_intent_step_transfer_to_alias(step_node);
        step.who_inherited_from_intent =
            ast_intent_step_inherited_who_from_intent(step_node);
        step.who_inherited_from_action =
            ast_intent_step_inherited_who_from_action(step_node);
        step.who_derived_from_on_receiver =
            ast_intent_step_derived_who_from_on_receiver(step_node);
        step.who_derived_from_single_participant =
            ast_intent_step_derived_who_from_single_participant(step_node);
        step.where_inherited_from_intent =
            ast_intent_step_inherited_where_from_intent(step_node);
        step.where_inherited_from_action =
            ast_intent_step_inherited_where_from_action(step_node);
        step.where_derived_from_using =
            ast_intent_step_derived_where_from_using(step_node);
        step.where_derived_from_transfer =
            ast_intent_step_derived_where_from_transfer(step_node);
        step.requires_inherited_from_action =
            ast_intent_step_inherited_requires_from_action(step_node);
        step.causes_inherited_from_action =
            ast_intent_step_inherited_causes_from_action(step_node);
        step.authorized_by_derived_from_zone =
            ast_intent_step_derived_authorized_by_from_zone(step_node);
        step.authorized_by_inherited_from_action =
            ast_intent_step_inherited_authorized_by_from_action(step_node);
        step.using_derived_from_transfer =
            ast_intent_step_derived_using_from_transfer(step_node);
        step.using_derived_from_where =
            ast_intent_step_derived_using_from_where(step_node);
        step.causes_effect_name = ast_intent_step_causes_effect(step_node);
        step.outcome_binding_name =
            ast_intent_step_outcome_binding_name(step_node);
        step.outcome_binding_type_name =
            ast_intent_step_outcome_binding_type_name(step_node);
        step.outcome_action_decl_syntax_id =
            ast_intent_step_outcome_action_decl_syntax_id(step_node);
        step.success_branch.variant_name =
            ast_intent_step_success_variant_name(step_node);
        step.success_branch.variant_index =
            ast_intent_step_success_variant_index(step_node);
        step.success_branch.payload_name =
            ast_intent_step_success_payload_name(step_node);
        step.success_branch.payload_type_name =
            ast_intent_step_success_payload_type_name(step_node);
        step.success_branch.enum_type_name =
            ast_intent_step_outcome_enum_type_name(step_node);
        step.success_branch.enum_decl_syntax_id =
            ast_intent_step_outcome_enum_decl_syntax_id(step_node);
        step.failure_branch.variant_name =
            ast_intent_step_failure_variant_name(step_node);
        step.failure_branch.variant_index =
            ast_intent_step_failure_variant_index(step_node);
        step.failure_branch.payload_name =
            ast_intent_step_failure_payload_name(step_node);
        step.failure_branch.payload_type_name =
            ast_intent_step_failure_payload_type_name(step_node);
        step.failure_branch.enum_type_name =
            ast_intent_step_outcome_enum_type_name(step_node);
        step.failure_branch.enum_decl_syntax_id =
            ast_intent_step_outcome_enum_decl_syntax_id(step_node);
        step.on_expr_count = ast_intent_step_on_expr_count(step_node);
        {
            ssize_t to = dir_find_effect_node_by_name(dir, step.causes_effect_name);
            step.causes_effect_node_id = to >= 0 ? (size_t)to : SIZE_MAX;
        }
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_INTENT_STEP_ZONE,
                                from_id,
                                step.where_type_node_id,
                                step.name,
                                step.where_type_name))
            goto step_oom;
        if (step.causes_effect_name != NULL) {
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_CAUSES,
                                    from_id,
                                    step.causes_effect_node_id,
                                    step.name,
                                    step.causes_effect_name))
                goto step_oom;
        }
        if (step.predecessor_step_name != NULL) {
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_DEPENDS_ON,
                                    from_id,
                                    from_id,
                                    step.predecessor_step_name,
                                    step.name))
                goto step_oom;
        }
        for (size_t j = 0; j < ast_intent_step_who_count(step_node); j++) {
            if (!append_name(&step.who_names,
                             &step.who_count,
                             &step.who_capacity,
                             ast_intent_step_who_names(step_node, NULL)[j]))
                goto step_oom;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_WHO,
                                    from_id,
                                    from_id,
                                    step.name,
                                    ast_intent_step_who_names(step_node, NULL)[j]))
                goto step_oom;
        }
        for (size_t j = 0; j < ast_intent_step_required_ability_count(step_node); j++) {
            ASTNode *ability_ref = ast_intent_step_required_abilities(step_node, NULL)[j];
            const char *ability_name = ast_type_name(ability_ref);
            ssize_t ability_id;
            if (ability_name == NULL)
                continue;
            ability_id = dir_find_ability_node_by_name(dir, ability_name);
            if (!append_name(&step.required_abilities,
                             &step.required_ability_count,
                             &step.required_ability_capacity,
                             ability_name))
                goto step_oom;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_REQUIRES,
                                    from_id,
                                    ability_id >= 0 ? (size_t)ability_id : SIZE_MAX,
                                    step.name,
                                    ability_name))
                goto step_oom;
        }
        for (size_t j = 0; j < ast_intent_step_authorized_by_count(step_node); j++) {
            if (!append_name(&step.authorized_by,
                             &step.authorized_by_count,
                             &step.authorized_by_capacity,
                             ast_intent_step_authorized_by(step_node, NULL)[j]))
                goto step_oom;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_AUTHORIZED_BY,
                                    from_id,
                                    from_id,
                                    step.name,
                                    ast_intent_step_authorized_by(step_node, NULL)[j]))
                goto step_oom;
        }
        if (!append_intent_step(&info.steps, &info.step_count, &info.step_capacity, step))
            goto step_oom;
        continue;

step_oom:
        clear_intent_step_names(&step);
        goto oom;
    }

    if (info.has_typed_result) {
        info.success_terminal.step_name =
            ast_intent_decl_success_terminal_step(node);
        info.success_terminal.step_syntax_id =
            ast_intent_decl_success_terminal_step_syntax_id(node);
        info.success_terminal.step_index = intent_step_index_for_syntax_id(
            steps, step_count, info.success_terminal.step_syntax_id);
        info.success_terminal.expr =
            ast_intent_decl_success_terminal_expr(node);
        info.success_terminal.result_type_name =
            ast_intent_decl_terminal_result_type_name(node, true, 0);
        info.success_terminal.result_enum_decl_syntax_id =
            ast_intent_decl_terminal_result_enum_decl_syntax_id(
                node, true, 0);
        info.success_terminal.result_variant_index =
            ast_intent_decl_terminal_result_variant_index(node, true, 0);
        info.success_terminal.result_variant_name =
            ast_intent_decl_terminal_result_variant_name(node, true, 0);
        info.success_terminal.result_payload_name =
            ast_intent_decl_terminal_result_payload_name(node, true, 0);
        info.success_terminal.result_payload_type_name =
            ast_intent_decl_terminal_result_payload_type_name(node, true, 0);
        info.failure_terminal_count =
            ast_intent_decl_failure_terminal_count(node);
        info.failure_terminals = calloc(
            info.failure_terminal_count, sizeof(DIRIntentTerminal));
        if (info.failure_terminal_count > 0
            && info.failure_terminals == NULL) {
            goto oom;
        }
        for (size_t i = 0; i < info.failure_terminal_count; i++) {
            DIRIntentTerminal *terminal = &info.failure_terminals[i];
            terminal->step_name =
                ast_intent_decl_failure_terminal_step(node, i);
            terminal->step_syntax_id =
                ast_intent_decl_failure_terminal_step_syntax_id(node, i);
            terminal->step_index = intent_step_index_for_syntax_id(
                steps, step_count, terminal->step_syntax_id);
            terminal->expr = ast_intent_decl_failure_terminal_expr(node, i);
            terminal->result_type_name =
                ast_intent_decl_terminal_result_type_name(node, false, i);
            terminal->result_enum_decl_syntax_id =
                ast_intent_decl_terminal_result_enum_decl_syntax_id(
                    node, false, i);
            terminal->result_variant_index =
                ast_intent_decl_terminal_result_variant_index(node, false, i);
            terminal->result_variant_name =
                ast_intent_decl_terminal_result_variant_name(node, false, i);
            terminal->result_payload_name =
                ast_intent_decl_terminal_result_payload_name(node, false, i);
            terminal->result_payload_type_name =
                ast_intent_decl_terminal_result_payload_type_name(
                    node, false, i);
        }
    }

    if (!append_intent_info(&dir->intents, &dir->intent_count, &dir->intent_capacity, info))
        goto oom;
    return true;

oom:
    free(info.participants);
    free(info.failure_terminals);
    if (info.steps != NULL) {
        for (size_t i = 0; i < info.step_count; i++) {
            clear_intent_step_names(&info.steps[i]);
        }
    }
    free(info.steps);
    return false;
}
