#include "mir_domain_runtime.h"

#include "dir.h"
#include "mir_program.h"

#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

static bool
mir_domain_runtime_text_present(const char *text)
{
    return text != NULL && text[0] != '\0';
}

static char *
mir_domain_runtime_copy_text(const char *text)
{
    return text != NULL ? pergyra_strdup(text) : NULL;
}

static void
mir_domain_runtime_set_error(char **error_message, const char *message)
{
    if (error_message != NULL)
        *error_message = pergyra_strdup(message);
}

const char *
mir_domain_participant_role_name(PgyDomainParticipantRole role)
{
    switch (role) {
    case PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER:
        return "effect-bearer";
    case PGY_DOMAIN_PARTICIPANT_RELATION_SOURCE:
        return "relation-source";
    case PGY_DOMAIN_PARTICIPANT_RELATION_TARGET:
        return "relation-target";
    default:
        return NULL;
    }
}

const char *
mir_domain_projection_operation_name(PgyDomainProjectionOperation operation)
{
    switch (operation) {
    case PGY_DOMAIN_PROJECTION_REFRESH:
        return "refresh";
    case PGY_DOMAIN_PROJECTION_PUBLISH:
        return "publish";
    case PGY_DOMAIN_PROJECTION_BIND:
        return "bind";
    default:
        return NULL;
    }
}

void
mir_domain_runtime_clear(MIRProgram *mir)
{
    if (mir == NULL)
        return;
    pgy_domain_participant_role_facts_destroy(
        mir->domain_participant_role_facts,
        mir->domain_participant_role_fact_count);
    pgy_domain_projection_member_assignment_facts_destroy(
        mir->domain_projection_member_assignment_facts,
        mir->domain_projection_member_assignment_fact_count);
    mir->domain_participant_role_facts = NULL;
    mir->domain_participant_role_fact_count = 0;
    mir->domain_projection_member_assignment_facts = NULL;
    mir->domain_projection_member_assignment_fact_count = 0;
    mir->domain_runtime_program_syntax_id = 0;
    mir->has_domain_runtime_facts = false;
}

static bool
mir_domain_runtime_copy_participant_role(
    PgyDomainParticipantRoleFact *target,
    const PgyDomainParticipantRoleFact *source)
{
    if (target == NULL || source == NULL)
        return false;
    target->program_syntax_id = source->program_syntax_id;
    target->owner_syntax_id = source->owner_syntax_id;
    target->role = source->role;
    target->field_syntax_id = source->field_syntax_id;
    target->owner_name = mir_domain_runtime_copy_text(source->owner_name);
    target->field_name = mir_domain_runtime_copy_text(source->field_name);
    target->field_type_name =
        mir_domain_runtime_copy_text(source->field_type_name);
    return target->owner_name != NULL
        && target->field_name != NULL
        && target->field_type_name != NULL;
}

static bool
mir_domain_runtime_copy_path_segments(
    PgyDomainProjectionMemberAssignmentFact *target,
    const PgyDomainProjectionMemberAssignmentFact *source)
{
    size_t count;

    if (target == NULL || source == NULL)
        return false;
    count = source->source_path_segment_count;
    target->source_path_segment_count = count;
    if (count == 0)
        return true;
    if (source->source_path_segments == NULL
        || count > SIZE_MAX / sizeof(*target->source_path_segments)) {
        return false;
    }
    target->source_path_segments = calloc(
        count, sizeof(*target->source_path_segments));
    if (target->source_path_segments == NULL)
        return false;
    for (size_t i = 0; i < count; i++) {
        const PgyDomainProjectionPathSegmentFact *source_segment =
            &source->source_path_segments[i];
        PgyDomainProjectionPathSegmentFact *target_segment =
            &target->source_path_segments[i];
        target_segment->field_syntax_id = source_segment->field_syntax_id;
        target_segment->field_name =
            mir_domain_runtime_copy_text(source_segment->field_name);
        target_segment->field_type_name =
            mir_domain_runtime_copy_text(source_segment->field_type_name);
        if (target_segment->field_name == NULL
            || target_segment->field_type_name == NULL) {
            return false;
        }
    }
    return true;
}

static bool
mir_domain_runtime_copy_projection_member(
    PgyDomainProjectionMemberAssignmentFact *target,
    const PgyDomainProjectionMemberAssignmentFact *source)
{
    if (target == NULL || source == NULL)
        return false;
    target->program_syntax_id = source->program_syntax_id;
    target->owner_syntax_id = source->owner_syntax_id;
    target->directive_syntax_id = source->directive_syntax_id;
    target->operation = source->operation;
    target->projection_slot_syntax_id = source->projection_slot_syntax_id;
    target->source_slot_syntax_id = source->source_slot_syntax_id;
    target->target_decl_syntax_id = source->target_decl_syntax_id;
    target->target_field_syntax_id = source->target_field_syntax_id;
    target->source_decl_syntax_id = source->source_decl_syntax_id;
    target->explicit_map = source->explicit_map;
    target->owner_name = mir_domain_runtime_copy_text(source->owner_name);
    target->projection_slot_name =
        mir_domain_runtime_copy_text(source->projection_slot_name);
    target->source_slot_name =
        mir_domain_runtime_copy_text(source->source_slot_name);
    target->target_field_name =
        mir_domain_runtime_copy_text(source->target_field_name);
    target->target_field_type_name =
        mir_domain_runtime_copy_text(source->target_field_type_name);
    target->source_path = mir_domain_runtime_copy_text(source->source_path);
    target->source_leaf_type_name =
        mir_domain_runtime_copy_text(source->source_leaf_type_name);
    return target->owner_name != NULL
        && target->projection_slot_name != NULL
        && target->source_slot_name != NULL
        && target->target_field_name != NULL
        && target->target_field_type_name != NULL
        && target->source_path != NULL
        && target->source_leaf_type_name != NULL
        && mir_domain_runtime_copy_path_segments(target, source);
}

static bool
mir_domain_runtime_copy_from_dir(MIRProgram *mir, const DIRProgram *dir)
{
    size_t role_count = dir->domain_participant_role_fact_count;
    size_t assignment_count =
        dir->domain_projection_member_assignment_fact_count;

    if (((role_count == 0)
            != (dir->domain_participant_role_facts == NULL))
        || ((assignment_count == 0)
            != (dir->domain_projection_member_assignment_facts == NULL))
        || role_count > SIZE_MAX / sizeof(*mir->domain_participant_role_facts)
        || assignment_count
            > SIZE_MAX
                / sizeof(*mir->domain_projection_member_assignment_facts)) {
        return false;
    }
    if (role_count > 0) {
        mir->domain_participant_role_facts = calloc(
            role_count, sizeof(*mir->domain_participant_role_facts));
        if (mir->domain_participant_role_facts == NULL)
            return false;
        mir->domain_participant_role_fact_count = role_count;
        for (size_t i = 0; i < role_count; i++) {
            if (!mir_domain_runtime_copy_participant_role(
                    &mir->domain_participant_role_facts[i],
                    &dir->domain_participant_role_facts[i])) {
                return false;
            }
        }
    }
    if (assignment_count > 0) {
        mir->domain_projection_member_assignment_facts = calloc(
            assignment_count,
            sizeof(*mir->domain_projection_member_assignment_facts));
        if (mir->domain_projection_member_assignment_facts == NULL)
            return false;
        mir->domain_projection_member_assignment_fact_count = assignment_count;
        for (size_t i = 0; i < assignment_count; i++) {
            if (!mir_domain_runtime_copy_projection_member(
                    &mir->domain_projection_member_assignment_facts[i],
                    &dir->domain_projection_member_assignment_facts[i])) {
                return false;
            }
        }
    }
    return true;
}

static bool
mir_domain_runtime_participant_role_valid(
    const PgyDomainParticipantRoleFact *fact)
{
    return fact != NULL
        && fact->program_syntax_id != 0
        && fact->owner_syntax_id != 0
        && fact->field_syntax_id != 0
        && mir_domain_participant_role_name(fact->role) != NULL
        && mir_domain_runtime_text_present(fact->owner_name)
        && mir_domain_runtime_text_present(fact->field_name)
        && mir_domain_runtime_text_present(fact->field_type_name);
}

static bool
mir_domain_runtime_projection_member_valid(
    const PgyDomainProjectionMemberAssignmentFact *fact)
{
    if (fact == NULL
        || fact->program_syntax_id == 0
        || fact->owner_syntax_id == 0
        || fact->directive_syntax_id == 0
        || fact->projection_slot_syntax_id == 0
        || fact->source_slot_syntax_id == 0
        || fact->target_decl_syntax_id == 0
        || fact->target_field_syntax_id == 0
        || fact->source_decl_syntax_id == 0
        || mir_domain_projection_operation_name(fact->operation) == NULL
        || !mir_domain_runtime_text_present(fact->owner_name)
        || !mir_domain_runtime_text_present(fact->projection_slot_name)
        || !mir_domain_runtime_text_present(fact->source_slot_name)
        || !mir_domain_runtime_text_present(fact->target_field_name)
        || !mir_domain_runtime_text_present(fact->target_field_type_name)
        || !mir_domain_runtime_text_present(fact->source_path)
        || !mir_domain_runtime_text_present(fact->source_leaf_type_name)
        || fact->source_path_segment_count == 0
        || fact->source_path_segments == NULL) {
        return false;
    }
    /* Semantic analysis owns assignability; MIR preserves both exact type
     * names and must not reinterpret that verdict as string equality. */
    for (size_t i = 0; i < fact->source_path_segment_count; i++) {
        const PgyDomainProjectionPathSegmentFact *segment =
            &fact->source_path_segments[i];
        if (segment->field_syntax_id == 0
            || !mir_domain_runtime_text_present(segment->field_name)
            || !mir_domain_runtime_text_present(segment->field_type_name)) {
            return false;
        }
    }
    return strcmp(
        fact->source_path_segments[fact->source_path_segment_count - 1]
            .field_type_name,
        fact->source_leaf_type_name) == 0;
}

bool
mir_domain_runtime_validate(const MIRProgram *mir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL
        || (!mir->has_domain_runtime_facts
            && (mir->domain_runtime_program_syntax_id != 0
                || mir->domain_participant_role_fact_count > 0
                || mir->domain_participant_role_facts != NULL
                || mir->domain_projection_member_assignment_fact_count > 0
                || mir->domain_projection_member_assignment_facts != NULL))
        || ((mir->domain_participant_role_fact_count == 0)
            != (mir->domain_participant_role_facts == NULL))
        || ((mir->domain_projection_member_assignment_fact_count == 0)
            != (mir->domain_projection_member_assignment_facts == NULL))) {
        mir_domain_runtime_set_error(
            error_message,
            "MIR domain runtime assignments are missing their owned facts");
        return false;
    }
    if (!mir->has_domain_runtime_facts)
        return true;
    if (mir->domain_runtime_program_syntax_id == 0) {
        mir_domain_runtime_set_error(
            error_message,
            "MIR domain runtime assignments are missing their program identity epoch");
        return false;
    }
    for (size_t i = 0; i < mir->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &mir->domain_participant_role_facts[i];
        if (!mir_domain_runtime_participant_role_valid(fact)) {
            mir_domain_runtime_set_error(
                error_message,
                "MIR domain participant role has incomplete identity or type");
            return false;
        }
        if (fact->program_syntax_id
            != mir->domain_runtime_program_syntax_id) {
            mir_domain_runtime_set_error(
                error_message,
                "MIR domain participant roles cross program identity epochs");
            return false;
        }
        for (size_t j = i + 1;
             j < mir->domain_participant_role_fact_count; j++) {
            const PgyDomainParticipantRoleFact *other =
                &mir->domain_participant_role_facts[j];
            if ((fact->program_syntax_id == other->program_syntax_id
                 && fact->owner_syntax_id == other->owner_syntax_id
                 && fact->role == other->role)
                || fact->field_syntax_id == other->field_syntax_id) {
                mir_domain_runtime_set_error(
                    error_message,
                    "MIR domain participant role identity is duplicated");
                return false;
            }
            if ((fact->owner_syntax_id == other->owner_syntax_id)
                != (strcmp(fact->owner_name, other->owner_name) == 0)) {
                mir_domain_runtime_set_error(
                    error_message,
                    "MIR domain participant role owner name and identity disagree");
                return false;
            }
        }
    }
    for (size_t i = 0;
         i < mir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &mir->domain_projection_member_assignment_facts[i];
        if (!mir_domain_runtime_projection_member_valid(fact)) {
            mir_domain_runtime_set_error(
                error_message,
                "MIR domain projection member has incomplete identity or path segment");
            return false;
        }
        if (fact->program_syntax_id
            != mir->domain_runtime_program_syntax_id) {
            mir_domain_runtime_set_error(
                error_message,
                "MIR domain runtime assignments cross program identity epochs");
            return false;
        }
        for (size_t j = i + 1;
             j < mir->domain_projection_member_assignment_fact_count; j++) {
            const PgyDomainProjectionMemberAssignmentFact *other =
                &mir->domain_projection_member_assignment_facts[j];
            if (fact->program_syntax_id == other->program_syntax_id
                && fact->directive_syntax_id == other->directive_syntax_id
                && fact->target_field_syntax_id
                    == other->target_field_syntax_id) {
                mir_domain_runtime_set_error(
                    error_message,
                    "MIR domain projection directive target identity is duplicated");
                return false;
            }
            if ((fact->owner_syntax_id == other->owner_syntax_id)
                != (strcmp(fact->owner_name, other->owner_name) == 0)) {
                mir_domain_runtime_set_error(
                    error_message,
                    "MIR domain projection owner name and identity disagree");
                return false;
            }
        }
    }
    return true;
}

bool
mir_domain_runtime_project_from_dir(MIRProgram *mir,
                                    const DIRProgram *dir,
                                    char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL || dir == NULL || dir->source_program_syntax_id == 0) {
        mir_domain_runtime_set_error(
            error_message,
            "MIR domain runtime projection requires anchored DIR input");
        return false;
    }
    mir_domain_runtime_clear(mir);
    mir->has_domain_runtime_facts = dir->has_domain_runtime_facts;
    if (dir->has_domain_runtime_facts)
        mir->domain_runtime_program_syntax_id =
            dir->source_program_syntax_id;
    if (!dir->has_domain_runtime_facts
        && (dir->domain_participant_role_fact_count > 0
            || dir->domain_participant_role_facts != NULL
            || dir->domain_projection_member_assignment_fact_count > 0
            || dir->domain_projection_member_assignment_facts != NULL)) {
        mir_domain_runtime_set_error(
            error_message,
            "DIR domain runtime assignments disagree with presence state");
        mir_domain_runtime_clear(mir);
        return false;
    }
    if (!mir_domain_runtime_copy_from_dir(mir, dir)) {
        mir_domain_runtime_set_error(
            error_message,
            "MIR could not deep-copy DIR domain runtime assignments");
        mir_domain_runtime_clear(mir);
        return false;
    }
    if (!mir_domain_runtime_validate(mir, error_message)) {
        mir_domain_runtime_clear(mir);
        return false;
    }
    for (size_t i = 0; i < mir->domain_participant_role_fact_count; i++) {
        if (mir->domain_participant_role_facts[i].program_syntax_id
            != dir->source_program_syntax_id) {
            mir_domain_runtime_set_error(
                error_message,
                "MIR domain participant role belongs to a different source program");
            mir_domain_runtime_clear(mir);
            return false;
        }
    }
    for (size_t i = 0;
         i < mir->domain_projection_member_assignment_fact_count; i++) {
        if (mir->domain_projection_member_assignment_facts[i]
                .program_syntax_id != dir->source_program_syntax_id) {
            mir_domain_runtime_set_error(
                error_message,
                "MIR domain projection member belongs to a different source program");
            mir_domain_runtime_clear(mir);
            return false;
        }
    }
    return true;
}
