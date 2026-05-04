#include "dir_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
append_name(const char ***items, size_t *count, size_t *capacity, const char *name)
{
    const char **grown;
    if (name == NULL)
        return true;
    if (*count == *capacity) {
        size_t next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        grown = realloc((void *)*items, next_capacity * sizeof(const char *));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
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
        size_t next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        DIRIntentInfo *grown = realloc(*items, next_capacity * sizeof(DIRIntentInfo));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
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
        size_t next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        DIRIntentParticipant *grown = realloc(*items, next_capacity * sizeof(DIRIntentParticipant));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
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
        size_t next_capacity = *capacity == 0 ? 4 : *capacity * 2;
        DIRIntentStep *grown = realloc(*items, next_capacity * sizeof(DIRIntentStep));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
    }
    (*items)[*count] = step;
    (*count)++;
    return true;
}

bool
dir_collect_intent_info(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    DIRIntentInfo info;
    memset(&info, 0, sizeof(info));
    info.node_id = from_id;

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *inv = node->data.intent_decl.involves[i];
        DIRIntentParticipant participant;
        memset(&participant, 0, sizeof(participant));
        participant.alias = inv->data.intent_involves.alias;
        participant.subject_type_name = type_name(dir, inv->data.intent_involves.subject_type);
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

    for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
        ASTNode *value = node->data.intent_decl.values[i];
        DIRIntentParticipant participant;
        memset(&participant, 0, sizeof(participant));
        participant.alias = value->data.intent_value.alias;
        participant.subject_type_name = type_name(dir, value->data.intent_value.value_type);
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

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step_node = node->data.intent_decl.steps[i];
        DIRIntentStep step;
        memset(&step, 0, sizeof(step));
        step.index = i;
        step.name = step_node->data.intent_step.name;
        step.ast = step_node;
        step.where_type_name = type_name(dir, step_node->data.intent_step.where_type);
        {
            ssize_t to = dir_find_zone_node_by_name(dir, step.where_type_name);
            step.where_type_node_id = to >= 0 ? (size_t)to : SIZE_MAX;
        }
        step.using_alias = step_node->data.intent_step.using_expr != NULL
            && step_node->data.intent_step.using_expr->type == AST_IDENTIFIER
            ? step_node->data.intent_step.using_expr->data.identifier.name
            : NULL;
        step.predecessor_step_name = (i > 0 && node->data.intent_decl.steps[i - 1] != NULL)
            ? node->data.intent_decl.steps[i - 1]->data.intent_step.name
            : NULL;
        step.predecessor_step_index = i > 0 ? (i - 1) : SIZE_MAX;
        step.transfer_from_alias = step_node->data.intent_step.transfer_from_alias;
        step.transfer_to_alias = step_node->data.intent_step.transfer_to_alias;
        step.who_inherited_from_intent =
            step_node->data.intent_step.inherited_who_from_intent;
        step.who_inherited_from_action =
            step_node->data.intent_step.inherited_who_from_action;
        step.who_derived_from_on_receiver =
            step_node->data.intent_step.derived_who_from_on_receiver;
        step.who_derived_from_single_participant =
            step_node->data.intent_step.derived_who_from_single_participant;
        step.where_inherited_from_intent =
            step_node->data.intent_step.inherited_where_from_intent;
        step.where_inherited_from_action =
            step_node->data.intent_step.inherited_where_from_action;
        step.where_derived_from_using =
            step_node->data.intent_step.derived_where_from_using;
        step.where_derived_from_transfer =
            step_node->data.intent_step.derived_where_from_transfer;
        step.requires_inherited_from_action =
            step_node->data.intent_step.inherited_requires_from_action;
        step.causes_inherited_from_action =
            step_node->data.intent_step.inherited_causes_from_action;
        step.authorized_by_derived_from_zone =
            step_node->data.intent_step.derived_authorized_by_from_zone;
        step.authorized_by_inherited_from_action =
            step_node->data.intent_step.inherited_authorized_by_from_action;
        step.using_derived_from_transfer =
            step_node->data.intent_step.derived_using_from_transfer;
        step.using_derived_from_where =
            step_node->data.intent_step.derived_using_from_where;
        step.causes_effect_name = step_node->data.intent_step.causes_effect;
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
            goto oom;
        if (step.causes_effect_name != NULL) {
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_CAUSES,
                                    from_id,
                                    step.causes_effect_node_id,
                                    step.name,
                                    step.causes_effect_name))
                goto oom;
        }
        if (step.predecessor_step_name != NULL) {
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_DEPENDS_ON,
                                    from_id,
                                    from_id,
                                    step.predecessor_step_name,
                                    step.name))
                goto oom;
        }
        for (size_t j = 0; j < step_node->data.intent_step.who_count; j++) {
            if (!append_name(&step.who_names,
                             &step.who_count,
                             &step.who_capacity,
                             step_node->data.intent_step.who_names[j]))
                goto oom;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_WHO,
                                    from_id,
                                    from_id,
                                    step.name,
                                    step_node->data.intent_step.who_names[j]))
                goto oom;
        }
        for (size_t j = 0; j < step_node->data.intent_step.required_ability_count; j++) {
            ASTNode *ability_ref = step_node->data.intent_step.required_abilities[j];
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
                goto oom;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_REQUIRES,
                                    from_id,
                                    ability_id >= 0 ? (size_t)ability_id : SIZE_MAX,
                                    step.name,
                                    ability_name))
                goto oom;
        }
        for (size_t j = 0; j < step_node->data.intent_step.authorized_by_count; j++) {
            if (!append_name(&step.authorized_by,
                             &step.authorized_by_count,
                             &step.authorized_by_capacity,
                             step_node->data.intent_step.authorized_by[j]))
                goto oom;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_AUTHORIZED_BY,
                                    from_id,
                                    from_id,
                                    step.name,
                                    step_node->data.intent_step.authorized_by[j]))
                goto oom;
        }
        if (!append_intent_step(&info.steps, &info.step_count, &info.step_capacity, step))
            goto oom;
    }

    if (!append_intent_info(&dir->intents, &dir->intent_count, &dir->intent_capacity, info))
        goto oom;
    return true;

oom:
    free(info.participants);
    if (info.steps != NULL) {
        for (size_t i = 0; i < info.step_count; i++) {
            free((void *)info.steps[i].who_names);
            free((void *)info.steps[i].required_abilities);
            free((void *)info.steps[i].authorized_by);
        }
    }
    free(info.steps);
    return false;
}
