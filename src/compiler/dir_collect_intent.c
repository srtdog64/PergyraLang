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
        step.ast = step_node;
        step.where_type_name = type_name(dir, ast_intent_step_where_type(step_node));
        {
            ssize_t to = dir_find_zone_node_by_name(dir, step.where_type_name);
            step.where_type_node_id = to >= 0 ? (size_t)to : SIZE_MAX;
        }
        step.using_alias = ast_intent_step_using_expr(step_node) != NULL
            && ast_intent_step_using_expr(step_node)->type == AST_IDENTIFIER
            ? ast_intent_step_using_expr(step_node)->data.identifier.name
            : NULL;
        step.predecessor_step_name = (i > 0 && steps[i - 1] != NULL)
            ? ast_intent_step_name(steps[i - 1])
            : NULL;
        step.predecessor_step_index = i > 0 ? (i - 1) : SIZE_MAX;
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
            const char *ability_name = (ability_ref != NULL && ability_ref->type == AST_TYPE)
                ? ability_ref->data.type.name : NULL;
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

    if (!append_intent_info(&dir->intents, &dir->intent_count, &dir->intent_capacity, info))
        goto oom;
    return true;

oom:
    free(info.participants);
    if (info.steps != NULL) {
        for (size_t i = 0; i < info.step_count; i++) {
            clear_intent_step_names(&info.steps[i]);
        }
    }
    free(info.steps);
    return false;
}
