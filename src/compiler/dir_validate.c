#include "dir.h"

#include <stdarg.h>
#include <stdlib.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static char *
dir_validate_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    return result;
}

const char *
dir_node_kind_name(DIRNodeKind kind)
{
    switch (kind) {
        case DIR_NODE_TYPE: return "type";
        case DIR_NODE_ABILITY: return "ability";
        case DIR_NODE_ROLE: return "role";
        case DIR_NODE_PARTY: return "party";
        case DIR_NODE_PARTY_SLOT: return "party-slot";
        case DIR_NODE_SYSTEMIC: return "roster";
        case DIR_NODE_WORLD: return "world";
        case DIR_NODE_RELATION: return "relation";
        case DIR_NODE_EFFECT: return "effect";
        case DIR_NODE_ZONE: return "zone";
        case DIR_NODE_ZONE_SLOT: return "zone-slot";
        case DIR_NODE_PROJECTION_SLOT: return "projection-slot";
        case DIR_NODE_AUTHORITY_SLOT: return "authority-slot";
        case DIR_NODE_INTENT: return "intent";
        default: return "unknown";
    }
}

const char *
dir_edge_kind_name(DIREdgeKind kind)
{
    switch (kind) {
        case DIR_EDGE_ROLE_FOR_TYPE: return "role-for";
        case DIR_EDGE_ROLE_INCLUDE: return "role-include";
        case DIR_EDGE_ROLE_IMPL_ABILITY: return "role-impl";
        case DIR_EDGE_ROLE_COMPLETES_ABILITY: return "role-complete";
        case DIR_EDGE_ROLE_MISSING_ABILITY_METHOD: return "role-missing-method";
        case DIR_EDGE_PARTY_HAS_SLOT: return "party-has-slot";
        case DIR_EDGE_PARTY_SLOT_ABILITY: return "party-slot";
        case DIR_EDGE_SYSTEMIC_PARTY: return "roster-party";
        case DIR_EDGE_WORLD_SYSTEMIC: return "world-roster";
        case DIR_EDGE_WORLD_ZONE: return "world-zone";
        case DIR_EDGE_ZONE_HAS_SLOT: return "zone-has-slot";
        case DIR_EDGE_ZONE_SLOT_TYPE: return "zone-slot";
        case DIR_EDGE_OWNER_HAS_PROJECTION_SLOT: return "owner-has-projection-slot";
        case DIR_EDGE_PROJECTION_SLOT_TYPE: return "projection-slot-type";
        case DIR_EDGE_PROJECTION_SLOT_SOURCE: return "projection-slot-source";
        case DIR_EDGE_ZONE_HAS_AUTHORITY_SLOT: return "zone-has-authority-slot";
        case DIR_EDGE_AUTHORITY_SLOT_SUBJECT: return "authority-slot-subject";
        case DIR_EDGE_ZONE_LAYER_TYPE: return "zone-layer";
        case DIR_EDGE_ZONE_AUTHORITY_ABILITY: return "zone-authority";
        case DIR_EDGE_ZONE_STATE_LAYER: return "zone-state";
        case DIR_EDGE_INTENT_PARTICIPANT_TYPE: return "intent-participant";
        case DIR_EDGE_INTENT_STEP_ZONE: return "intent-step-zone";
        case DIR_EDGE_INTENT_STEP_WHO: return "intent-step-who";
        case DIR_EDGE_INTENT_STEP_REQUIRES: return "intent-step-requires";
        case DIR_EDGE_INTENT_STEP_AUTHORIZED_BY: return "intent-step-authorized-by";
        case DIR_EDGE_INTENT_STEP_CAUSES: return "intent-step-causes";
        case DIR_EDGE_INTENT_STEP_DEPENDS_ON: return "intent-step-depends-on";
        default: return "unknown";
    }
}

const char *
dir_domain_topology_kind_name(DIRDomainTopologyKind kind)
{
    switch (kind) {
    case DIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH: return "refresh";
    case DIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH: return "publish";
    case DIR_DOMAIN_TOPOLOGY_PROJECTION_BIND: return "bind";
    case DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT: return "apply-effect";
    case DIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT: return "maintain-effect";
    case DIR_DOMAIN_TOPOLOGY_LINK_RELATION: return "link-relation";
    default: return "unknown";
    }
}

static bool
dir_domain_topology_is_projection(DIRDomainTopologyKind kind)
{
    return kind == DIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH
        || kind == DIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH
        || kind == DIR_DOMAIN_TOPOLOGY_PROJECTION_BIND;
}

static bool
dir_domain_topology_slot_matches(const DIRNode *owner,
                                 const char *name,
                                 uint32_t syntax_id,
                                 bool layer)
{
    ASTNode **slots = NULL;
    size_t count = 0;

    if (owner == NULL || owner->ast == NULL || name == NULL
        || syntax_id == 0)
        return false;
    if (layer) {
        if (owner->kind != DIR_NODE_ZONE)
            return false;
        slots = ast_zone_layer_slots(owner->ast, &count);
        for (size_t i = 0; slots != NULL && i < count; i++) {
            if (slots[i] != NULL
                && ast_node_stable_id(slots[i]) == syntax_id
                && ast_zone_layer_slot_name(slots[i]) != NULL
                && strcmp(ast_zone_layer_slot_name(slots[i]), name) == 0) {
                return true;
            }
        }
        return false;
    }

    switch (owner->kind) {
    case DIR_NODE_ZONE:
        slots = ast_zone_slots(owner->ast, &count);
        break;
    case DIR_NODE_RELATION:
        slots = ast_relation_slots(owner->ast, &count);
        break;
    case DIR_NODE_EFFECT:
        slots = ast_effect_slots(owner->ast, &count);
        break;
    default:
        return false;
    }
    for (size_t i = 0; slots != NULL && i < count; i++) {
        if (slots[i] != NULL
            && ast_node_stable_id(slots[i]) == syntax_id
            && ast_domain_slot_name(slots[i]) != NULL
            && strcmp(ast_domain_slot_name(slots[i]), name) == 0) {
            return true;
        }
    }
    return false;
}

static size_t
dir_domain_topology_expected_count(const DIRProgram *dir)
{
    size_t expected = 0;

    if (dir == NULL)
        return 0;
    for (size_t i = 0; i < dir->node_count; i++) {
        const DIRNode *node = &dir->nodes[i];
        size_t count = 0;
        if (node->ast == NULL)
            continue;
        switch (node->kind) {
        case DIR_NODE_ZONE:
            (void)ast_zone_refreshes(node->ast, &count);
            expected += count;
            (void)ast_zone_applies(node->ast, &count);
            expected += count;
            (void)ast_zone_maintained_effects(node->ast, &count);
            expected += count;
            (void)ast_zone_links(node->ast, &count);
            expected += count;
            break;
        case DIR_NODE_RELATION:
            (void)ast_relation_refreshes(node->ast, &count);
            expected += count;
            break;
        case DIR_NODE_EFFECT:
            (void)ast_effect_refreshes(node->ast, &count);
            expected += count;
            break;
        default:
            break;
        }
    }
    return expected;
}

static bool
dir_domain_topology_effect_directive_matches(const DIRNode *owner,
                                             const DIRDomainTopologyRow *row,
                                             bool apply)
{
    ASTNode **directives = NULL;
    size_t count = 0;

    if (owner == NULL || row == NULL || owner->kind != DIR_NODE_ZONE
        || owner->ast == NULL || row->source_syntax_id == 0) {
        return false;
    }
    directives = apply
        ? ast_zone_applies(owner->ast, &count)
        : ast_zone_maintained_effects(owner->ast, &count);
    for (size_t i = 0; directives != NULL && i < count; i++) {
        ASTNode *directive = directives[i];
        const char *layer = ast_zone_effect_slot_name(directive);
        const char *target = ast_zone_effect_target_slot_name(directive);
        const char *participant =
            ast_zone_directive_participant_slot_name(directive);
        bool participant_matches =
            participant == NULL && row->participant_slot_name == NULL;
        if (participant != NULL && row->participant_slot_name != NULL) {
            participant_matches = strcmp(
                participant, row->participant_slot_name) == 0;
        }
        if (directive != NULL
            && ast_node_stable_id(directive) == row->source_syntax_id
            && layer != NULL && row->layer_slot_name != NULL
            && strcmp(layer, row->layer_slot_name) == 0
            && target != NULL && row->target_slot_name != NULL
            && strcmp(target, row->target_slot_name) == 0
            && participant_matches) {
            return true;
        }
    }
    return false;
}

static bool
dir_validate_domain_topology(const DIRProgram *dir, char **error_message)
{
    size_t expected;

    if (dir == NULL)
        return false;
    expected = dir_domain_topology_expected_count(dir);
    if (dir->domain_topology_row_count != expected
        || (dir->domain_topology_row_count > 0
            && dir->domain_topology_rows == NULL)) {
        if (error_message != NULL) {
            *error_message = dir_validate_strdup_fmt(
                "DIR domain topology row count %llu does not match source-owned count %llu",
                (unsigned long long)dir->domain_topology_row_count,
                (unsigned long long)expected);
        }
        return false;
    }

    for (size_t i = 0; i < dir->domain_topology_row_count; i++) {
        const DIRDomainTopologyRow *row = &dir->domain_topology_rows[i];
        const DIRNode *owner;
        bool shape_ok = false;

        if (row->owner_node_id >= dir->node_count
            || row->owner_source_syntax_id == 0
            || row->source_syntax_id == 0) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain topology row[%llu] has incomplete stable identity",
                    (unsigned long long)i);
            return false;
        }
        owner = &dir->nodes[row->owner_node_id];
        if (owner->source_syntax_id != row->owner_source_syntax_id
            || (owner->kind != DIR_NODE_ZONE
                && owner->kind != DIR_NODE_RELATION
                && owner->kind != DIR_NODE_EFFECT)) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain topology row[%llu] has owner identity drift",
                    (unsigned long long)i);
            return false;
        }

        if (dir_domain_topology_is_projection(row->kind)) {
            shape_ok = dir_domain_topology_slot_matches(
                    owner, row->projection_slot_name,
                    row->projection_slot_source_syntax_id, false)
                && dir_domain_topology_slot_matches(
                    owner, row->source_slot_name,
                    row->source_slot_source_syntax_id, false)
                && row->layer_slot_name == NULL
                && row->target_slot_name == NULL
                && row->left_slot_name == NULL
                && row->right_slot_name == NULL;
        } else if (row->kind == DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT
                   || row->kind == DIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT) {
            shape_ok = owner->kind == DIR_NODE_ZONE
                && dir_domain_topology_effect_directive_matches(
                    owner, row,
                    row->kind == DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT)
                && dir_domain_topology_slot_matches(
                    owner, row->layer_slot_name,
                    row->layer_slot_source_syntax_id, true)
                && dir_domain_topology_slot_matches(
                    owner, row->target_slot_name,
                    row->target_slot_source_syntax_id, false)
                && row->projection_slot_name == NULL
                && row->source_slot_name == NULL
                && row->left_slot_name == NULL
                && row->right_slot_name == NULL;
        } else if (row->kind == DIR_DOMAIN_TOPOLOGY_LINK_RELATION) {
            shape_ok = owner->kind == DIR_NODE_ZONE
                && dir_domain_topology_slot_matches(
                    owner, row->layer_slot_name,
                    row->layer_slot_source_syntax_id, true)
                && dir_domain_topology_slot_matches(
                    owner, row->left_slot_name,
                    row->left_slot_source_syntax_id, false)
                && dir_domain_topology_slot_matches(
                    owner, row->right_slot_name,
                    row->right_slot_source_syntax_id, false)
                && row->projection_slot_name == NULL
                && row->source_slot_name == NULL
                && row->target_slot_name == NULL;
        }
        if (row->participant_slot_name != NULL) {
            shape_ok = shape_ok && dir_domain_topology_slot_matches(
                owner, row->participant_slot_name,
                row->participant_slot_source_syntax_id, false);
        } else if (row->participant_slot_source_syntax_id != 0) {
            shape_ok = false;
        }
        if (!shape_ok) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain topology row[%llu] has invalid %s shape",
                    (unsigned long long)i,
                    dir_domain_topology_kind_name(row->kind));
            return false;
        }
        for (size_t j = i + 1; j < dir->domain_topology_row_count; j++) {
            if (dir->domain_topology_rows[j].source_syntax_id
                == row->source_syntax_id) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR domain topology rows duplicate source identity %u",
                        (unsigned)row->source_syntax_id);
                return false;
            }
        }
    }
    return true;
}

bool
dir_validate(const DIRProgram *dir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (dir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("DIR program is null");
        return false;
    }

    if (dir->source_program_syntax_id == 0 || dir->domain_graph_id == 0) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "DIR domain graph is missing its anchored source identity");
        return false;
    }

    if ((dir->resource_flow_fact_count != 0
         && dir->resource_flow_facts == NULL)
        || (dir->has_resource_flow_facts
            && dir->resource_flow_fact_count == 0)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "DIR ResourceFlowUniverse snapshot is incomplete");
        return false;
    }
    for (size_t i = 0; i < dir->resource_flow_fact_count; i++) {
        const PgyResourceFlowFact *fact = &dir->resource_flow_facts[i];
        if (fact->function_syntax_id == 0
            || fact->stable_index == SIZE_MAX
            || fact->name == NULL
            || fact->name[0] == '\0') {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR ResourceFlowUniverse fact[%llu] has incomplete stable identity",
                    (unsigned long long)i);
            return false;
        }
        for (size_t j = i + 1; j < dir->resource_flow_fact_count; j++) {
            const PgyResourceFlowFact *other =
                &dir->resource_flow_facts[j];
            if (fact->function_syntax_id == other->function_syntax_id
                && fact->stable_index == other->stable_index) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR ResourceFlowUniverse facts duplicate function %u stable index %llu",
                        (unsigned)fact->function_syntax_id,
                        (unsigned long long)fact->stable_index);
                return false;
            }
        }
    }

    for (size_t i = 0; i < dir->node_count; i++) {
        const DIRNode *node = &dir->nodes[i];
        const bool is_domain_owner =
            node->kind == DIR_NODE_ZONE
            || node->kind == DIR_NODE_RELATION
            || node->kind == DIR_NODE_EFFECT;
        const bool is_qualified_slot =
            node->kind == DIR_NODE_ZONE_SLOT
            || node->kind == DIR_NODE_PROJECTION_SLOT
            || node->kind == DIR_NODE_AUTHORITY_SLOT;

        if (is_domain_owner && node->ast != NULL
            && node->source_syntax_id == 0) {
            if (error_message != NULL) {
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain node '%s' is missing source syntax identity",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }
        if (is_qualified_slot && node->ast != NULL
            && (node->source_syntax_id == 0
                || node->owner_source_syntax_id == 0)) {
            if (error_message != NULL) {
                *error_message = dir_validate_strdup_fmt(
                    "DIR slot-contract node '%s' is missing source or owner syntax identity",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }
        if (node->source_syntax_id != 0) {
            for (size_t j = i + 1; j < dir->node_count; j++) {
                if (dir->nodes[j].source_syntax_id == node->source_syntax_id) {
                    if (error_message != NULL) {
                        *error_message = dir_validate_strdup_fmt(
                            "DIR nodes '%s' and '%s' duplicate source syntax identity %u",
                            node->name != NULL ? node->name : "(unnamed)",
                            dir->nodes[j].name != NULL
                                ? dir->nodes[j].name : "(unnamed)",
                            (unsigned)node->source_syntax_id);
                    }
                    return false;
                }
            }
        }
    }

    for (size_t i = 0; i < dir->edge_count; i++) {
        const DIREdge *edge = &dir->edges[i];
        if (edge->from_node_id >= dir->node_count) {
            if (error_message != NULL) {
                *error_message = dir_validate_strdup_fmt(
                    "DIR edge[%llu] has invalid from_node_id",
                    (unsigned long long)i);
            }
            return false;
        }
        if (edge->to_node_id != SIZE_MAX && edge->to_node_id >= dir->node_count) {
            if (error_message != NULL) {
                *error_message = dir_validate_strdup_fmt(
                    "DIR edge[%llu] has invalid to_node_id",
                    (unsigned long long)i);
            }
            return false;
        }
    }

    if (!dir_validate_domain_topology(dir, error_message))
        return false;

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
            if (step->index != j) {
                if (error_message != NULL) {
                    *error_message = dir_validate_strdup_fmt(
                        "DIR intent[%llu] step[%llu] has unstable index",
                        (unsigned long long)i,
                        (unsigned long long)j);
                }
                return false;
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
        }
    }

    return true;
}

static void
dir_dump_resolved_id(FILE *out, size_t node_id)
{
    if (node_id == SIZE_MAX)
        fputc('-', out);
    else
        fprintf(out, "%zu", node_id);
}

void
dir_dump(const DIRProgram *dir, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (dir == NULL) {
        fprintf(out, "DIR: (null)\n");
        return;
    }

    fprintf(out, "DIR Program\n  nodes: %zu\n  edges: %zu\n  intents: %zu\n",
            dir->node_count, dir->edge_count, dir->intent_count);
    fprintf(out, "  resource_flow_facts: %zu\n", dir->resource_flow_fact_count);
    fprintf(out, "  domain_topology_rows: %zu\n",
            dir->domain_topology_row_count);

    for (size_t i = 0; i < dir->node_count; i++) {
        fprintf(out, "  node[%02zu] %-8s %s source=%u owner_source=%u\n",
                i,
                dir_node_kind_name(dir->nodes[i].kind),
                dir->nodes[i].name != NULL ? dir->nodes[i].name : "(anonymous)",
                (unsigned)dir->nodes[i].source_syntax_id,
                (unsigned)dir->nodes[i].owner_source_syntax_id);
    }

    for (size_t i = 0; i < dir->edge_count; i++) {
        const DIREdge *edge = &dir->edges[i];
        fprintf(out, "  edge[%02zu] %-14s from=%zu label=%s target=%s resolved=",
                i,
                dir_edge_kind_name(edge->kind),
                edge->from_node_id,
                edge->label != NULL ? edge->label : "-",
                edge->target_name != NULL ? edge->target_name : "-");
        dir_dump_resolved_id(out, edge->to_node_id);
        fputc('\n', out);
    }

    for (size_t i = 0; i < dir->domain_topology_row_count; i++) {
        const DIRDomainTopologyRow *row = &dir->domain_topology_rows[i];
        fprintf(out,
                "  topology[%02zu] %-16s owner=%zu source=%u projection=%s source-slot=%s layer=%s target=%s left=%s right=%s participant=%s\n",
                i,
                dir_domain_topology_kind_name(row->kind),
                row->owner_node_id,
                (unsigned)row->source_syntax_id,
                row->projection_slot_name != NULL
                    ? row->projection_slot_name : "-",
                row->source_slot_name != NULL ? row->source_slot_name : "-",
                row->layer_slot_name != NULL ? row->layer_slot_name : "-",
                row->target_slot_name != NULL ? row->target_slot_name : "-",
                row->left_slot_name != NULL ? row->left_slot_name : "-",
                row->right_slot_name != NULL ? row->right_slot_name : "-",
                row->participant_slot_name != NULL
                    ? row->participant_slot_name : "-");
    }

    for (size_t i = 0; i < dir->intent_count; i++) {
        const DIRIntentInfo *intent = &dir->intents[i];
        const DIRNode *node = &dir->nodes[intent->node_id];
        fprintf(out, "  intent[%02zu] %s participants=%zu steps=%zu\n",
                i,
                node->name != NULL ? node->name : "(anonymous)",
                intent->participant_count,
                intent->step_count);
        for (size_t j = 0; j < intent->participant_count; j++) {
            const DIRIntentParticipant *p = &intent->participants[j];
            fprintf(out, "    %s %-12s type=%s resolved=",
                    p->is_value_binding ? "value      " : "participant",
                    p->alias != NULL ? p->alias : "-",
                    p->subject_type_name != NULL ? p->subject_type_name : "-");
            dir_dump_resolved_id(out, p->subject_type_node_id);
            fputc('\n', out);
        }
        for (size_t j = 0; j < intent->step_count; j++) {
            const DIRIntentStep *step = &intent->steps[j];
            fprintf(out, "    step[%02zu] %-12s where=%s resolved=",
                    step->index,
                    step->name != NULL ? step->name : "-",
                    step->where_type_name != NULL ? step->where_type_name : "-");
            dir_dump_resolved_id(out, step->where_type_node_id);
            fprintf(out, " using=%s causes=%s",
                    step->using_alias != NULL ? step->using_alias : "-",
                    step->causes_effect_name != NULL ? step->causes_effect_name : "-");
            if (step->predecessor_step_name != NULL)
                fprintf(out, " depends-on=%s", step->predecessor_step_name);
            if (step->transfer_from_alias != NULL || step->transfer_to_alias != NULL) {
                fprintf(out, " transfer=%s->%s",
                        step->transfer_from_alias != NULL ? step->transfer_from_alias : "-",
                        step->transfer_to_alias != NULL ? step->transfer_to_alias : "-");
            }
            if (step->who_inherited_from_intent)
                fputs(" who-default=intent", out);
            if (step->who_inherited_from_action)
                fputs(" who-default=action", out);
            if (step->who_derived_from_on_receiver)
                fputs(" who-derived=on-receiver", out);
            if (step->who_derived_from_single_participant)
                fputs(" who-derived=single-participant", out);
            if (step->where_inherited_from_intent)
                fputs(" where-default=intent", out);
            if (step->where_inherited_from_action)
                fputs(" where-default=action", out);
            if (step->where_derived_from_using)
                fputs(" where-derived=using", out);
            if (step->where_derived_from_transfer)
                fputs(" where-derived=transfer", out);
            if (step->using_derived_from_transfer)
                fputs(" using-derived=transfer", out);
            if (step->using_derived_from_where)
                fputs(" using-derived=where", out);
            if (step->requires_inherited_from_action)
                fputs(" requires-default=action", out);
            if (step->causes_inherited_from_action)
                fputs(" causes-default=action", out);
            fputc('\n', out);
            for (size_t k = 0; k < step->who_count; k++)
                fprintf(out, "      who[%zu] %s\n", k, step->who_names[k]);
            for (size_t k = 0; k < step->required_ability_count; k++)
                fprintf(out, "      requires[%zu] %s\n", k, step->required_abilities[k]);
            for (size_t k = 0; k < step->authorized_by_count; k++)
                fprintf(out, "      authorized_by[%zu] %s\n", k, step->authorized_by[k]);
            if (step->authorized_by_derived_from_zone)
                fputs("      authorized_by_provenance legacy-zone-field\n", out);
            if (step->authorized_by_inherited_from_action)
                fputs("      authorized_by_provenance action-inherited\n", out);
        }
    }
}
