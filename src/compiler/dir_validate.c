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
        && dir_nullable_string_equal(
            branch->enum_type_name,
            ast_intent_step_outcome_enum_type_name(step))
        && branch->enum_decl_syntax_id
            == ast_intent_step_outcome_enum_decl_syntax_id(step);
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

static bool
dir_domain_runtime_text_present(const char *text)
{
    return text != NULL && text[0] != '\0';
}

static const DIRNode *
dir_domain_runtime_find_node_by_source(const DIRProgram *dir,
                                       uint32_t source_syntax_id)
{
    if (dir == NULL || source_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < dir->node_count; i++) {
        if (dir->nodes[i].source_syntax_id == source_syntax_id)
            return &dir->nodes[i];
    }
    return NULL;
}

static const DIRNode *
dir_domain_runtime_find_slot(const DIRProgram *dir,
                             uint32_t owner_syntax_id,
                             uint32_t slot_syntax_id)
{
    const DIRNode *slot = dir_domain_runtime_find_node_by_source(
        dir, slot_syntax_id);
    if (slot == NULL || slot->owner_source_syntax_id != owner_syntax_id
        || (slot->kind != DIR_NODE_ZONE_SLOT
            && slot->kind != DIR_NODE_PROJECTION_SLOT)) {
        return NULL;
    }
    return slot;
}

static bool
dir_domain_runtime_slot_type_matches(const DIRProgram *dir,
                                     const DIRNode *slot,
                                     const char *field_name,
                                     const char *type_name,
                                     uint32_t type_decl_syntax_id)
{
    if (dir == NULL || slot == NULL
        || !dir_domain_runtime_text_present(field_name)
        || !dir_domain_runtime_text_present(type_name)) {
        return false;
    }
    for (size_t i = 0; i < dir->edge_count; i++) {
        const DIREdge *edge = &dir->edges[i];
        if (edge->from_node_id != slot->id
            || (edge->kind != DIR_EDGE_ZONE_SLOT_TYPE
                && edge->kind != DIR_EDGE_PROJECTION_SLOT_TYPE)
            || edge->label == NULL || strcmp(edge->label, field_name) != 0
            || edge->target_name == NULL
            || strcmp(edge->target_name, type_name) != 0) {
            continue;
        }
        if (type_decl_syntax_id == 0)
            return true;
        return edge->to_node_id < dir->node_count
            && dir->nodes[edge->to_node_id].source_syntax_id
                == type_decl_syntax_id;
    }
    return false;
}

static const DIRNode *
dir_domain_runtime_layer_type(const DIRProgram *dir,
                              const DIRDomainTopologyRow *row)
{
    if (dir == NULL || row == NULL || row->owner_node_id >= dir->node_count
        || !dir_domain_runtime_text_present(row->layer_slot_name)) {
        return NULL;
    }
    for (size_t i = 0; i < dir->edge_count; i++) {
        const DIREdge *edge = &dir->edges[i];
        if (edge->kind == DIR_EDGE_ZONE_LAYER_TYPE
            && edge->from_node_id == row->owner_node_id
            && edge->label != NULL
            && strcmp(edge->label, row->layer_slot_name) == 0
            && edge->to_node_id < dir->node_count) {
            return &dir->nodes[edge->to_node_id];
        }
    }
    return NULL;
}

static const PgyDomainParticipantRoleFact *
dir_domain_runtime_find_role(
    const DIRProgram *dir,
    uint32_t owner_syntax_id,
    PgyDomainParticipantRole role)
{
    if (dir == NULL || owner_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < dir->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &dir->domain_participant_role_facts[i];
        if (fact->owner_syntax_id == owner_syntax_id
            && fact->role == role) {
            return fact;
        }
    }
    return NULL;
}

static PgyDomainProjectionOperation
dir_domain_runtime_projection_operation(DIRDomainTopologyKind kind)
{
    if (kind == DIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH)
        return PGY_DOMAIN_PROJECTION_PUBLISH;
    if (kind == DIR_DOMAIN_TOPOLOGY_PROJECTION_BIND)
        return PGY_DOMAIN_PROJECTION_BIND;
    return PGY_DOMAIN_PROJECTION_REFRESH;
}

static const DIRDomainTopologyRow *
dir_domain_runtime_find_projection_row(
    const DIRProgram *dir,
    const PgyDomainProjectionMemberAssignmentFact *fact)
{
    for (size_t i = 0; i < dir->domain_topology_row_count; i++) {
        const DIRDomainTopologyRow *row = &dir->domain_topology_rows[i];
        if (dir_domain_topology_is_projection(row->kind)
            && row->owner_source_syntax_id == fact->owner_syntax_id
            && row->source_syntax_id == fact->directive_syntax_id
            && row->projection_slot_source_syntax_id
                == fact->projection_slot_syntax_id
            && row->source_slot_source_syntax_id
                == fact->source_slot_syntax_id
            && row->projection_slot_name != NULL
            && strcmp(row->projection_slot_name,
                      fact->projection_slot_name) == 0
            && row->source_slot_name != NULL
            && strcmp(row->source_slot_name, fact->source_slot_name) == 0
            && dir_domain_runtime_projection_operation(row->kind)
                == fact->operation) {
            return row;
        }
    }
    return NULL;
}

static bool
dir_validate_domain_runtime_role_facts(const DIRProgram *dir,
                                       char **error_message)
{
    for (size_t i = 0; i < dir->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &dir->domain_participant_role_facts[i];
        const DIRNode *owner;
        const DIRNode *field;
        DIRNodeKind expected_owner_kind;

        if (fact->program_syntax_id == 0
            || fact->program_syntax_id != dir->source_program_syntax_id
            || fact->owner_syntax_id == 0 || fact->field_syntax_id == 0
            || (unsigned)fact->role
                > (unsigned)PGY_DOMAIN_PARTICIPANT_RELATION_TARGET
            || !dir_domain_runtime_text_present(fact->owner_name)
            || !dir_domain_runtime_text_present(fact->field_name)
            || !dir_domain_runtime_text_present(fact->field_type_name)) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain participant-role fact[%llu] has incomplete exact identity, name, or type",
                    (unsigned long long)i);
            return false;
        }
        expected_owner_kind =
            fact->role == PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER
                ? DIR_NODE_EFFECT : DIR_NODE_RELATION;
        owner = dir_domain_runtime_find_node_by_source(
            dir, fact->owner_syntax_id);
        field = dir_domain_runtime_find_slot(
            dir, fact->owner_syntax_id, fact->field_syntax_id);
        if (owner == NULL || owner->kind != expected_owner_kind
            || owner->name == NULL
            || strcmp(owner->name, fact->owner_name) != 0
            || field == NULL
            || !dir_domain_runtime_slot_type_matches(
                dir, field, fact->field_name, fact->field_type_name, 0)) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain participant-role fact[%llu] does not match its exact owner/field declaration",
                    (unsigned long long)i);
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainParticipantRoleFact *prior =
                &dir->domain_participant_role_facts[j];
            if ((prior->program_syntax_id == fact->program_syntax_id
                 && prior->owner_syntax_id == fact->owner_syntax_id
                 && prior->role == fact->role)
                || prior->field_syntax_id == fact->field_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "DIR domain participant-role facts duplicate stable identity");
                return false;
            }
        }
    }
    return true;
}

static bool
dir_validate_domain_runtime_projection_facts(const DIRProgram *dir,
                                             char **error_message)
{
    for (size_t i = 0;
         i < dir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &dir->domain_projection_member_assignment_facts[i];
        const DIRDomainTopologyRow *row;
        const DIRNode *owner;
        const DIRNode *projection_slot;
        const DIRNode *source_slot;
        const DIRNode *target_decl;

        if (fact->program_syntax_id == 0
            || fact->program_syntax_id != dir->source_program_syntax_id
            || fact->owner_syntax_id == 0
            || fact->directive_syntax_id == 0
            || fact->projection_slot_syntax_id == 0
            || fact->source_slot_syntax_id == 0
            || fact->target_decl_syntax_id == 0
            || fact->target_field_syntax_id == 0
            || fact->source_decl_syntax_id == 0
            || (unsigned)fact->operation
                > (unsigned)PGY_DOMAIN_PROJECTION_BIND
            || !dir_domain_runtime_text_present(fact->owner_name)
            || !dir_domain_runtime_text_present(fact->projection_slot_name)
            || !dir_domain_runtime_text_present(fact->source_slot_name)
            || !dir_domain_runtime_text_present(fact->target_field_name)
            || !dir_domain_runtime_text_present(
                fact->target_field_type_name)
            || !dir_domain_runtime_text_present(fact->source_path)
            || !dir_domain_runtime_text_present(fact->source_leaf_type_name)
            || fact->source_path_segment_count == 0
            || fact->source_path_segments == NULL) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain projection assignment[%llu] has incomplete exact identity, name, type, or path",
                    (unsigned long long)i);
            return false;
        }
        for (size_t s = 0; s < fact->source_path_segment_count; s++) {
            const PgyDomainProjectionPathSegmentFact *segment =
                &fact->source_path_segments[s];
            if (segment->field_syntax_id == 0
                || !dir_domain_runtime_text_present(segment->field_name)
                || !dir_domain_runtime_text_present(
                    segment->field_type_name)) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR domain projection assignment[%llu] has an incomplete source-path segment",
                        (unsigned long long)i);
                return false;
            }
        }
        if (strcmp(fact->source_path_segments[
                       fact->source_path_segment_count - 1]
                       .field_type_name,
                   fact->source_leaf_type_name) != 0) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain projection assignment[%llu] has exact type drift",
                    (unsigned long long)i);
            return false;
        }

        row = dir_domain_runtime_find_projection_row(dir, fact);
        owner = dir_domain_runtime_find_node_by_source(
            dir, fact->owner_syntax_id);
        projection_slot = dir_domain_runtime_find_slot(
            dir, fact->owner_syntax_id,
            fact->projection_slot_syntax_id);
        source_slot = dir_domain_runtime_find_slot(
            dir, fact->owner_syntax_id, fact->source_slot_syntax_id);
        target_decl = dir_domain_runtime_find_node_by_source(
            dir, fact->target_decl_syntax_id);
        if (row == NULL || owner == NULL || owner->name == NULL
            || strcmp(owner->name, fact->owner_name) != 0
            || projection_slot == NULL
            || projection_slot->kind != DIR_NODE_PROJECTION_SLOT
            || source_slot == NULL || target_decl == NULL
            || target_decl->name == NULL
            || !dir_domain_runtime_slot_type_matches(
                dir, projection_slot, fact->projection_slot_name,
                target_decl->name,
                fact->target_decl_syntax_id)) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain projection assignment[%llu] does not match its topology owner/target declaration",
                    (unsigned long long)i);
            return false;
        }
        {
            const DIRNode *source_decl =
                dir_domain_runtime_find_node_by_source(
                    dir, fact->source_decl_syntax_id);
            if (source_decl == NULL || source_decl->name == NULL
                || !dir_domain_runtime_slot_type_matches(
                    dir, source_slot, fact->source_slot_name,
                    source_decl->name, fact->source_decl_syntax_id)) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR domain projection assignment[%llu] does not match its exact source declaration",
                        (unsigned long long)i);
                return false;
            }
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainProjectionMemberAssignmentFact *prior =
                &dir->domain_projection_member_assignment_facts[j];
            if (prior->program_syntax_id == fact->program_syntax_id
                && prior->owner_syntax_id == fact->owner_syntax_id
                && prior->directive_syntax_id == fact->directive_syntax_id
                && prior->projection_slot_syntax_id
                    == fact->projection_slot_syntax_id
                && prior->target_field_syntax_id
                    == fact->target_field_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "DIR domain projection assignments duplicate stable member identity");
                return false;
            }
        }
    }
    return true;
}

static bool
dir_validate_domain_runtime_topology_coverage(const DIRProgram *dir,
                                              char **error_message)
{
    for (size_t i = 0; i < dir->domain_topology_row_count; i++) {
        const DIRDomainTopologyRow *row = &dir->domain_topology_rows[i];
        if (dir_domain_topology_is_projection(row->kind)) {
            bool found = false;
            for (size_t j = 0;
                 j < dir->domain_projection_member_assignment_fact_count;
                 j++) {
                const PgyDomainProjectionMemberAssignmentFact *fact =
                    &dir->domain_projection_member_assignment_facts[j];
                if (fact->owner_syntax_id == row->owner_source_syntax_id
                    && fact->directive_syntax_id == row->source_syntax_id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR projection topology directive %u has no semantic member-assignment facts",
                        (unsigned)row->source_syntax_id);
                return false;
            }
            continue;
        }

        const DIRNode *layer_type =
            dir_domain_runtime_layer_type(dir, row);
        if (layer_type == NULL) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR layer topology directive %u has no exact layer type",
                    (unsigned)row->source_syntax_id);
            return false;
        }
        if (row->kind == DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT
            || row->kind == DIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT) {
            const PgyDomainParticipantRoleFact *bearer =
                dir_domain_runtime_find_role(
                    dir, layer_type->source_syntax_id,
                    PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER);
            const DIRNode *target_slot = dir_domain_runtime_find_slot(
                dir, row->owner_source_syntax_id,
                row->target_slot_source_syntax_id);
            if (layer_type->kind != DIR_NODE_EFFECT || bearer == NULL
                || target_slot == NULL
                || !dir_domain_runtime_slot_type_matches(
                    dir, target_slot, row->target_slot_name,
                    bearer->field_type_name, 0)) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR effect topology directive %u has no exact semantic bearer role",
                        (unsigned)row->source_syntax_id);
                return false;
            }
        } else if (row->kind == DIR_DOMAIN_TOPOLOGY_LINK_RELATION) {
            const PgyDomainParticipantRoleFact *source =
                dir_domain_runtime_find_role(
                    dir, layer_type->source_syntax_id,
                    PGY_DOMAIN_PARTICIPANT_RELATION_SOURCE);
            const PgyDomainParticipantRoleFact *target =
                dir_domain_runtime_find_role(
                    dir, layer_type->source_syntax_id,
                    PGY_DOMAIN_PARTICIPANT_RELATION_TARGET);
            const DIRNode *left_slot = dir_domain_runtime_find_slot(
                dir, row->owner_source_syntax_id,
                row->left_slot_source_syntax_id);
            const DIRNode *right_slot = dir_domain_runtime_find_slot(
                dir, row->owner_source_syntax_id,
                row->right_slot_source_syntax_id);
            if (layer_type->kind != DIR_NODE_RELATION || source == NULL
                || target == NULL || left_slot == NULL || right_slot == NULL
                || !dir_domain_runtime_slot_type_matches(
                    dir, left_slot, row->left_slot_name,
                    source->field_type_name, 0)
                || !dir_domain_runtime_slot_type_matches(
                    dir, right_slot, row->right_slot_name,
                    target->field_type_name, 0)) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR relation topology directive %u has no exact semantic source/target roles",
                        (unsigned)row->source_syntax_id);
                return false;
            }
        }
    }
    return true;
}

static bool
dir_validate_domain_runtime_facts(const DIRProgram *dir,
                                  char **error_message)
{
    if (!dir->has_domain_runtime_facts) {
        if (dir->domain_participant_role_facts != NULL
            || dir->domain_participant_role_fact_count != 0
            || dir->domain_projection_member_assignment_facts != NULL
            || dir->domain_projection_member_assignment_fact_count != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "DIR domain runtime storage exists without HIR projection marker");
            return false;
        }
        /* Compatibility-only legacy entry points (dir_lower and the LSP's
         * syntax probe) do not claim a semantic snapshot.  The production
         * dir_lower_with_hir_* path sets the marker even for an empty table,
         * so topology there still fails closed through coverage below. */
        return true;
    }
    if ((dir->domain_participant_role_fact_count == 0
         && dir->domain_participant_role_facts != NULL)
        || (dir->domain_participant_role_fact_count != 0
            && dir->domain_participant_role_facts == NULL)
        || (dir->domain_projection_member_assignment_fact_count == 0
            && dir->domain_projection_member_assignment_facts != NULL)
        || (dir->domain_projection_member_assignment_fact_count != 0
            && dir->domain_projection_member_assignment_facts == NULL)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "DIR domain runtime semantic snapshot has incomplete storage");
        return false;
    }
    return dir_validate_domain_runtime_role_facts(dir, error_message)
        && dir_validate_domain_runtime_projection_facts(dir, error_message)
        && dir_validate_domain_runtime_topology_coverage(dir, error_message);
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
    if (!dir_validate_domain_runtime_facts(dir, error_message))
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
                    || terminal->expr == NULL) {
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
    fprintf(out, "  domain_participant_role_facts: %zu\n",
            dir->domain_participant_role_fact_count);
    fprintf(out, "  domain_projection_member_assignment_facts: %zu\n",
            dir->domain_projection_member_assignment_fact_count);
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
