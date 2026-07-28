#include "hir.h"

#include "../common/string_compat.h"
#include "../semantic/semantic.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
hir_domain_runtime_text_present(const char *text)
{
    return text != NULL && text[0] != '\0';
}

static bool
hir_validate_domain_participant_role_source(
    const HIRProgram *hir,
    const PgyDomainParticipantRoleFact *facts,
    size_t fact_count,
    char **error_message)
{
    if (facts == NULL && fact_count != 0) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "Semantic domain participant-role count has no fact array");
        return false;
    }
    for (size_t i = 0; i < fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact = &facts[i];
        if (fact->program_syntax_id == 0
            || fact->program_syntax_id != hir->source_program_syntax_id
            || fact->owner_syntax_id == 0
            || fact->field_syntax_id == 0
            || (unsigned)fact->role
                > (unsigned)PGY_DOMAIN_PARTICIPANT_RELATION_TARGET
            || !hir_domain_runtime_text_present(fact->owner_name)
            || !hir_domain_runtime_text_present(fact->field_name)
            || !hir_domain_runtime_text_present(fact->field_type_name)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "Semantic domain participant-role fact has incomplete exact identity, name, or type");
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainParticipantRoleFact *prior = &facts[j];
            if ((prior->program_syntax_id == fact->program_syntax_id
                 && prior->owner_syntax_id == fact->owner_syntax_id
                 && prior->role == fact->role)
                || prior->field_syntax_id == fact->field_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "Semantic domain participant-role facts duplicate stable identity");
                return false;
            }
        }
    }
    return true;
}

static bool
hir_validate_domain_projection_assignment_source(
    const HIRProgram *hir,
    const PgyDomainProjectionMemberAssignmentFact *facts,
    size_t fact_count,
    char **error_message)
{
    if (facts == NULL && fact_count != 0) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "Semantic domain projection assignment count has no fact array");
        return false;
    }
    for (size_t i = 0; i < fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact = &facts[i];
        if (fact->program_syntax_id == 0
            || fact->program_syntax_id != hir->source_program_syntax_id
            || fact->owner_syntax_id == 0
            || fact->directive_syntax_id == 0
            || fact->projection_slot_syntax_id == 0
            || fact->source_slot_syntax_id == 0
            || fact->target_decl_syntax_id == 0
            || fact->target_field_syntax_id == 0
            || fact->source_decl_syntax_id == 0
            || (unsigned)fact->operation
                > (unsigned)PGY_DOMAIN_PROJECTION_BIND
            || !hir_domain_runtime_text_present(fact->owner_name)
            || !hir_domain_runtime_text_present(
                fact->projection_slot_name)
            || !hir_domain_runtime_text_present(fact->source_slot_name)
            || !hir_domain_runtime_text_present(fact->target_field_name)
            || !hir_domain_runtime_text_present(
                fact->target_field_type_name)
            || !hir_domain_runtime_text_present(fact->source_path)
            || !hir_domain_runtime_text_present(
                fact->source_leaf_type_name)
            || fact->source_path_segment_count == 0
            || fact->source_path_segments == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "Semantic domain projection assignment has incomplete exact identity, name, type, or path");
            return false;
        }
        for (size_t s = 0; s < fact->source_path_segment_count; s++) {
            const PgyDomainProjectionPathSegmentFact *segment =
                &fact->source_path_segments[s];
            if (segment->field_syntax_id == 0
                || !hir_domain_runtime_text_present(segment->field_name)
                || !hir_domain_runtime_text_present(
                    segment->field_type_name)) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "Semantic domain projection assignment has an incomplete source-path segment");
                return false;
            }
        }
        if (strcmp(fact->source_path_segments[
                       fact->source_path_segment_count - 1]
                       .field_type_name,
                   fact->source_leaf_type_name) != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "Semantic domain projection assignment leaf type disagrees with its exact source path");
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainProjectionMemberAssignmentFact *prior =
                &facts[j];
            if (prior->program_syntax_id == fact->program_syntax_id
                && prior->owner_syntax_id == fact->owner_syntax_id
                && prior->directive_syntax_id == fact->directive_syntax_id
                && prior->projection_slot_syntax_id
                    == fact->projection_slot_syntax_id
                && prior->target_field_syntax_id
                    == fact->target_field_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "Semantic domain projection assignments duplicate stable member identity");
                return false;
            }
        }
    }
    return true;
}

static char *
hir_domain_runtime_strdup(const char *source)
{
    return source != NULL ? pergyra_strdup(source) : NULL;
}

static bool
hir_attach_domain_runtime_facts(
    HIRProgram *hir,
    const PgyDomainParticipantRoleFact *role_facts,
    size_t role_fact_count,
    const PgyDomainProjectionMemberAssignmentFact *assignment_facts,
    size_t assignment_fact_count,
    char **error_message)
{
    if (hir == NULL
        || !hir_validate_domain_participant_role_source(
            hir, role_facts, role_fact_count, error_message)
        || !hir_validate_domain_projection_assignment_source(
            hir, assignment_facts, assignment_fact_count, error_message)) {
        return false;
    }

    hir->has_domain_runtime_facts = true;
    if (role_fact_count != 0) {
        hir->domain_participant_role_facts = calloc(
            role_fact_count, sizeof(*hir->domain_participant_role_facts));
        if (hir->domain_participant_role_facts == NULL)
            goto out_of_memory;
        hir->domain_participant_role_fact_count = role_fact_count;
        for (size_t i = 0; i < role_fact_count; i++) {
            PgyDomainParticipantRoleFact *copy =
                &hir->domain_participant_role_facts[i];
            *copy = role_facts[i];
            copy->owner_name = hir_domain_runtime_strdup(
                role_facts[i].owner_name);
            copy->field_name = hir_domain_runtime_strdup(
                role_facts[i].field_name);
            copy->field_type_name = hir_domain_runtime_strdup(
                role_facts[i].field_type_name);
            if (copy->owner_name == NULL || copy->field_name == NULL
                || copy->field_type_name == NULL) {
                goto out_of_memory;
            }
        }
    }

    if (assignment_fact_count != 0) {
        hir->domain_projection_member_assignment_facts = calloc(
            assignment_fact_count,
            sizeof(*hir->domain_projection_member_assignment_facts));
        if (hir->domain_projection_member_assignment_facts == NULL)
            goto out_of_memory;
        hir->domain_projection_member_assignment_fact_count =
            assignment_fact_count;
        for (size_t i = 0; i < assignment_fact_count; i++) {
            const PgyDomainProjectionMemberAssignmentFact *source =
                &assignment_facts[i];
            PgyDomainProjectionMemberAssignmentFact *copy =
                &hir->domain_projection_member_assignment_facts[i];
            *copy = *source;
            copy->owner_name = hir_domain_runtime_strdup(source->owner_name);
            copy->projection_slot_name = hir_domain_runtime_strdup(
                source->projection_slot_name);
            copy->source_slot_name = hir_domain_runtime_strdup(
                source->source_slot_name);
            copy->target_field_name = hir_domain_runtime_strdup(
                source->target_field_name);
            copy->target_field_type_name = hir_domain_runtime_strdup(
                source->target_field_type_name);
            copy->source_path = hir_domain_runtime_strdup(
                source->source_path);
            copy->source_leaf_type_name = hir_domain_runtime_strdup(
                source->source_leaf_type_name);
            copy->source_path_segments = calloc(
                source->source_path_segment_count,
                sizeof(*copy->source_path_segments));
            if (copy->owner_name == NULL
                || copy->projection_slot_name == NULL
                || copy->source_slot_name == NULL
                || copy->target_field_name == NULL
                || copy->target_field_type_name == NULL
                || copy->source_path == NULL
                || copy->source_leaf_type_name == NULL
                || copy->source_path_segments == NULL) {
                goto out_of_memory;
            }
            for (size_t s = 0; s < source->source_path_segment_count; s++) {
                copy->source_path_segments[s] =
                    source->source_path_segments[s];
                copy->source_path_segments[s].field_name =
                    hir_domain_runtime_strdup(
                        source->source_path_segments[s].field_name);
                copy->source_path_segments[s].field_type_name =
                    hir_domain_runtime_strdup(
                        source->source_path_segments[s].field_type_name);
                if (copy->source_path_segments[s].field_name == NULL
                    || copy->source_path_segments[s].field_type_name == NULL) {
                    goto out_of_memory;
                }
            }
        }
    }
    return true;

out_of_memory:
    if (error_message != NULL && *error_message == NULL)
        *error_message = pergyra_strdup(
            "Out of memory while copying semantic domain runtime facts into HIR");
    return false;
}

static bool
hir_append_match_binding_type_fact(HIRRoutine *routine,
                                   const PgyMatchBindingTypeFact *fact,
                                   char **error_message)
{
    size_t next_capacity;
    HIRMatchBindingTypeFact *grown;
    HIRMatchBindingTypeFact *copy;

    if (routine == NULL || fact == NULL
        || fact->function_syntax_id != routine->source_syntax_id
        || fact->match_case_syntax_id == 0 || fact->binding_count == 0
        || fact->binding_index >= fact->binding_count
        || fact->binding_type_name == NULL
        || fact->binding_type_name[0] == '\0')
        return false;
    for (size_t i = 0; i < routine->match_binding_type_fact_count; i++) {
        const HIRMatchBindingTypeFact *existing =
            &routine->match_binding_type_facts[i];
        if (existing->match_case_syntax_id == fact->match_case_syntax_id
            && existing->binding_index == fact->binding_index) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "duplicate HIR match binding type fact identity");
            return false;
        }
    }
    if (routine->match_binding_type_fact_count
        == routine->match_binding_type_fact_capacity) {
        next_capacity = routine->match_binding_type_fact_capacity == 0
            ? 8
            : routine->match_binding_type_fact_capacity * 2;
        if (next_capacity < routine->match_binding_type_fact_capacity
            || next_capacity > SIZE_MAX / sizeof(*grown))
            return false;
        grown = realloc(routine->match_binding_type_facts,
                        next_capacity * sizeof(*grown));
        if (grown == NULL)
            return false;
        routine->match_binding_type_facts = grown;
        routine->match_binding_type_fact_capacity = next_capacity;
    }
    copy = &routine->match_binding_type_facts[
        routine->match_binding_type_fact_count];
    *copy = *fact;
    copy->binding_type_name = pergyra_strdup(fact->binding_type_name);
    if (copy->binding_type_name == NULL)
        return false;
    routine->match_binding_type_fact_count++;
    return true;
}

bool
hir_attach_match_binding_type_facts(HIRProgram *hir,
                                    const PgyMatchBindingTypeFact *facts,
                                    size_t fact_count,
                                    char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (hir == NULL || (facts == NULL && fact_count != 0))
        return false;
    for (size_t i = 0; i < fact_count; i++) {
        HIRRoutine *routine = NULL;
        for (size_t r = 0; r < hir->routine_count; r++) {
            if (hir->routines[r].source_syntax_id
                == facts[i].function_syntax_id) {
                routine = &hir->routines[r];
                break;
            }
        }
        if (routine == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "Match binding type fact references an unknown HIR routine");
            return false;
        }
        if (!hir_append_match_binding_type_fact(
                routine, &facts[i], error_message)) {
            if (error_message != NULL && *error_message == NULL)
                *error_message = pergyra_strdup(
                    "Invalid or unallocatable HIR match binding type fact");
            return false;
        }
    }
    hir->has_match_binding_type_facts = fact_count != 0;
    return true;
}

HIRProgram *
hir_lower_with_semantic_facts(const SemanticResult *semantic,
                              HIRSemanticProjectionFailure *failure,
                              char **error_message)
{
    HIRProgram *hir;

    if (failure != NULL)
        *failure = HIR_SEMANTIC_PROJECTION_NONE;
    if (error_message != NULL)
        *error_message = NULL;
    if (semantic == NULL || semantic->annotated_ast == NULL) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_LOWER;
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "HIR semantic fact projection requires semantic facts");
        return NULL;
    }

    hir = hir_lower_with_resource_and_param_flow_facts(
        semantic->annotated_ast,
        semantic->resource_flow_facts,
        semantic->resource_flow_fact_count,
        semantic->function_param_flow_facts,
        semantic->function_param_flow_fact_count,
        error_message);
    if (hir == NULL) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_LOWER;
        return NULL;
    }
    if (!hir_attach_loop_flow_facts(
            hir,
            semantic->loop_flow_summary_facts,
            semantic->loop_flow_summary_fact_count,
            semantic->loop_flow_state_facts,
            semantic->loop_flow_state_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_LOOP_FLOW;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_attach_iteration_type_facts(
            hir,
            semantic->iteration_type_facts,
            semantic->iteration_type_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_ITERATION_TYPE;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_attach_destructure_type_facts(
            hir,
            semantic->destructure_type_facts,
            semantic->destructure_type_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_DESTRUCTURE_TYPE;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_attach_match_binding_type_facts(
            hir,
            semantic->match_binding_type_facts,
            semantic->match_binding_type_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_MATCH_BINDING_TYPE;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_attach_region_escape_facts(
            hir,
            semantic->region_escape_facts,
            semantic->region_escape_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_REGION_ESCAPE;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_attach_domain_runtime_facts(
            hir,
            semantic->domain_participant_role_facts,
            semantic->domain_participant_role_fact_count,
            semantic->domain_projection_member_assignment_facts,
            semantic->domain_projection_member_assignment_fact_count,
            error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_DOMAIN_RUNTIME_FACT;
        hir_destroy(hir);
        return NULL;
    }
    if (!hir_validate(hir, error_message)) {
        if (failure != NULL)
            *failure = HIR_SEMANTIC_PROJECTION_VALIDATE;
        hir_destroy(hir);
        return NULL;
    }
    return hir;
}
