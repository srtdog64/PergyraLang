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
append_intent_info(DIRIntentInfo **items, size_t *count, size_t *capacity, DIRIntentInfo info)
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
append_intent_step(DIRIntentStep **items, size_t *count, size_t *capacity, DIRIntentStep step)
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

static ASTNode *
dir_find_ability_decl_ast(ASTNode *program, const char *name)
{
    if (program == NULL || name == NULL || program->type != AST_PROGRAM)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt != NULL
            && stmt->type == AST_ABILITY_DECL
            && stmt->data.ability_decl.name != NULL
            && strcmp(stmt->data.ability_decl.name, name) == 0) {
            return stmt;
        }
    }
    return NULL;
}

static bool
dir_impl_has_method_named(ASTNode *impl, const char *method_name)
{
    if (impl == NULL || impl->type != AST_IMPL_ABILITY || method_name == NULL)
        return false;

    for (size_t i = 0; i < impl->data.impl_ability.method_count; i++) {
        ASTNode *method = impl->data.impl_ability.methods[i];
        if (method != NULL
            && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return true;
        }
    }
    return false;
}

bool
dir_collect_nodes(DIRProgram *dir, ASTNode *program)
{
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *node = program->data.program.statements[i];
        switch (node->type) {
            case AST_CLASS_DECL:
                if (!dir_add_node(dir, DIR_NODE_TYPE, node->data.class_decl.name, node))
                    return false;
                break;
            case AST_TYPE_ALIAS:
                if (!dir_add_node(dir, DIR_NODE_TYPE, node->data.type_alias.name, node))
                    return false;
                break;
            case AST_ENUM_DECL:
                if (!dir_add_node(dir, DIR_NODE_TYPE, node->data.enum_decl.name, node))
                    return false;
                break;
            case AST_ABILITY_DECL:
                if (!dir_add_node(dir, DIR_NODE_ABILITY, node->data.ability_decl.name, node))
                    return false;
                break;
            case AST_ROLE_DECL:
                if (!dir_add_node(dir, DIR_NODE_ROLE, node->data.role_decl.name, node))
                    return false;
                break;
            case AST_PARTY_DECL:
                if (!dir_add_node(dir, DIR_NODE_PARTY, node->data.party_decl.name, node))
                    return false;
                break;
            case AST_ROSTER_DECL:
                if (!dir_add_node(dir, DIR_NODE_SYSTEMIC, node->data.roster_decl.name, node))
                    return false;
                break;
            case AST_WORLD_DECL:
                if (!dir_add_node(dir, DIR_NODE_WORLD, node->data.world_decl.name, node))
                    return false;
                break;
            case AST_RELATION_DECL:
                if (!dir_add_node(dir, DIR_NODE_RELATION, node->data.relation_decl.name, node))
                    return false;
                break;
            case AST_EFFECT_DECL:
                if (!dir_add_node(dir, DIR_NODE_EFFECT, node->data.effect_decl.name, node))
                    return false;
                break;
            case AST_ZONE_DECL:
                if (!dir_add_node(dir, DIR_NODE_ZONE, node->data.zone_decl.name, node))
                    return false;
                break;
            case AST_INTENT_DECL:
                if (!dir_add_node(dir, DIR_NODE_INTENT, node->data.intent_decl.name, node))
                    return false;
                break;
            default:
                break;
        }
    }

    return true;
}

static bool
dir_collect_role_edges(DIRProgram *dir, ASTNode *program, size_t from_id, ASTNode *node)
{
    const char *for_type = type_name(dir, node->data.role_decl.for_type);
    if (for_type != NULL) {
        ssize_t to = dir_find_type_node_by_name(dir, for_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_ROLE_FOR_TYPE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX, "for", for_type))
            return false;
    }

    for (size_t i = 0; i < node->data.role_decl.include_count; i++) {
        ASTNode *inc = node->data.role_decl.includes[i];
        if (inc == NULL)
            continue;
        ssize_t to = dir_find_role_node_by_name(dir, inc->data.include_stmt.role_name);
        if (!dir_add_named_edge(dir, DIR_EDGE_ROLE_INCLUDE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                "include",
                                inc->data.include_stmt.role_name))
            return false;
    }

    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];
        ASTNode *ability_decl = NULL;
        if (impl == NULL)
            continue;
        const char *ability_name =
            (impl->data.impl_ability.ability_ref != NULL
             && impl->data.impl_ability.ability_ref->type == AST_TYPE)
            ? impl->data.impl_ability.ability_ref->data.type.name : NULL;
        ssize_t to = dir_find_ability_node_by_name(dir, ability_name);
        if (!dir_add_named_edge(dir, DIR_EDGE_ROLE_IMPL_ABILITY, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                "impl",
                                ability_name))
            return false;

        ability_decl = dir_find_ability_decl_ast(program, ability_name);
        if (ability_decl != NULL) {
            bool complete = true;
            for (size_t j = 0; j < ability_decl->data.ability_decl.method_count; j++) {
                ASTNode *ability_method = ability_decl->data.ability_decl.methods[j];
                const char *method_name = ability_method != NULL
                    ? ability_method->data.func_decl.name
                    : NULL;
                if (method_name == NULL)
                    continue;
                if (!dir_impl_has_method_named(impl, method_name)) {
                    complete = false;
                    if (!dir_add_named_edge(dir,
                                            DIR_EDGE_ROLE_MISSING_ABILITY_METHOD,
                                            from_id,
                                            to >= 0 ? (size_t)to : SIZE_MAX,
                                            ability_name,
                                            method_name))
                        return false;
                }
            }
            if (complete) {
                if (!dir_add_named_edge(dir,
                                        DIR_EDGE_ROLE_COMPLETES_ABILITY,
                                        from_id,
                                        to >= 0 ? (size_t)to : SIZE_MAX,
                                        "complete",
                                        ability_name))
                    return false;
            }
        }
    }
    return true;
}

static bool
dir_collect_party_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *slot = node->data.party_decl.role_slots[i];
        ssize_t slot_id;
        if (slot == NULL)
            continue;
        slot_id = dir_ensure_qualified_slot_node(dir,
                                                 DIR_NODE_PARTY_SLOT,
                                                 node->data.party_decl.name,
                                                 slot->data.role_slot.slot_name,
                                                 slot);
        if (slot_id < 0)
            return false;
        if (!dir_add_named_edge(dir,
                                DIR_EDGE_PARTY_HAS_SLOT,
                                from_id,
                                (size_t)slot_id,
                                slot->data.role_slot.slot_name,
                                dir->nodes[(size_t)slot_id].name))
            return false;
        for (size_t j = 0; j < slot->data.role_slot.ability_count; j++) {
            ASTNode *ability = slot->data.role_slot.required_abilities[j];
            const char *ability_name = type_name(dir, ability);
            ssize_t to = dir_find_ability_node_by_name(dir, ability_name);
            if (!dir_add_named_edge(dir, DIR_EDGE_PARTY_SLOT_ABILITY, from_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    slot->data.role_slot.slot_name,
                                    ability_name))
                return false;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_PARTY_SLOT_ABILITY,
                                    (size_t)slot_id,
                                    to >= 0 ? (size_t)to : SIZE_MAX,
                                    slot->data.role_slot.slot_name,
                                    ability_name))
                return false;
        }
    }
    return true;
}

static bool
dir_collect_roster_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    for (size_t i = 0; i < node->data.roster_decl.party_count; i++) {
        ASTNode *slot = node->data.roster_decl.party_slots[i];
        ssize_t to = dir_find_party_node_by_name(dir, slot->data.roster_slot.party_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_SYSTEMIC_PARTY, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot->data.roster_slot.slot_name,
                                slot->data.roster_slot.party_type))
            return false;
    }
    return true;
}

static bool
dir_collect_world_edges(DIRProgram *dir, size_t from_id, ASTNode *node)
{
    for (size_t i = 0; i < node->data.world_decl.roster_count; i++) {
        ASTNode *slot = node->data.world_decl.rosters[i];
        ssize_t to = dir_find_roster_node_by_name(dir, slot->data.world_roster.roster_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_WORLD_SYSTEMIC, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot->data.world_roster.slot_name,
                                slot->data.world_roster.roster_type))
            return false;
    }
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *slot = node->data.world_decl.zones[i];
        ssize_t to = dir_find_zone_node_by_name(dir, slot->data.world_zone.zone_type);
        if (!dir_add_named_edge(dir, DIR_EDGE_WORLD_ZONE, from_id,
                                to >= 0 ? (size_t)to : SIZE_MAX,
                                slot->data.world_zone.slot_name,
                                slot->data.world_zone.zone_type))
            return false;
    }
    return true;
}

static bool
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
            if (ability_name == NULL)
                continue;
            if (!append_name(&step.required_abilities,
                             &step.required_ability_count,
                             &step.required_ability_capacity,
                             ability_name))
                goto oom;
            if (!dir_add_named_edge(dir,
                                    DIR_EDGE_INTENT_STEP_REQUIRES,
                                    from_id,
                                    dir_find_ability_node_by_name(dir, ability_name) >= 0
                                        ? (size_t)dir_find_ability_node_by_name(dir, ability_name)
                                        : SIZE_MAX,
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

bool
dir_collect_edges_and_intents(DIRProgram *dir, ASTNode *program)
{
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *node = program->data.program.statements[i];
        ssize_t from = -1;
        switch (node->type) {
            case AST_ROLE_DECL:
                from = dir_find_role_node_by_name(dir, node->data.role_decl.name);
                if (from >= 0 && !dir_collect_role_edges(dir, program, (size_t)from, node))
                    return false;
                break;
            case AST_PARTY_DECL:
                from = dir_find_node_by_name_kind(dir, node->data.party_decl.name, DIR_NODE_PARTY);
                if (from >= 0 && !dir_collect_party_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_ROSTER_DECL:
                from = dir_find_roster_node_by_name(dir, node->data.roster_decl.name);
                if (from >= 0 && !dir_collect_roster_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_WORLD_DECL:
                from = dir_find_node_by_name_kind(dir, node->data.world_decl.name, DIR_NODE_WORLD);
                if (from >= 0 && !dir_collect_world_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_RELATION_DECL:
                from = dir_find_relation_node_by_name(dir, node->data.relation_decl.name);
                if (from >= 0
                    && !dir_collect_relation_effect_slot_edges(dir,
                                                               (size_t)from,
                                                               node->data.relation_decl.name,
                                                               node->data.relation_decl.slots,
                                                               node->data.relation_decl.slot_count,
                                                               node->data.relation_decl.refreshes,
                                                               node->data.relation_decl.refresh_count))
                    return false;
                break;
            case AST_EFFECT_DECL:
                from = dir_find_effect_node_by_name(dir, node->data.effect_decl.name);
                if (from >= 0
                    && !dir_collect_relation_effect_slot_edges(dir,
                                                               (size_t)from,
                                                               node->data.effect_decl.name,
                                                               node->data.effect_decl.slots,
                                                               node->data.effect_decl.slot_count,
                                                               node->data.effect_decl.refreshes,
                                                               node->data.effect_decl.refresh_count))
                    return false;
                break;
            case AST_ZONE_DECL:
                from = dir_find_zone_node_by_name(dir, node->data.zone_decl.name);
                if (from >= 0 && !dir_collect_zone_edges(dir, (size_t)from, node))
                    return false;
                break;
            case AST_INTENT_DECL:
                from = dir_find_node_by_name_kind(dir, node->data.intent_decl.name, DIR_NODE_INTENT);
                if (from >= 0 && !dir_collect_intent_info(dir, (size_t)from, node))
                    return false;
                break;
            default:
                break;
        }
    }
    return true;
}
