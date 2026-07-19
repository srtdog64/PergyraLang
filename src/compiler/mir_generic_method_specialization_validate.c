#include "mir_generic_method_specialization.h"

#include <ctype.h>
#include <string.h>

#include "mir.h"
#include "mir_base_helpers.h"
#include "../common/string_compat.h"

static bool
mir_type_name_retains_formal(const char *type_name,
                             char *const *generic_param_names,
                             size_t generic_param_count,
                             const char **retained_formal)
{
    const char *cursor = type_name;

    while (*cursor != '\0') {
        const char *start;
        size_t length;

        if (!isalpha((unsigned char)*cursor) && *cursor != '_') {
            cursor++;
            continue;
        }
        start = cursor++;
        while (isalnum((unsigned char)*cursor) || *cursor == '_')
            cursor++;
        length = (size_t)(cursor - start);
        for (size_t i = 0; i < generic_param_count; i++) {
            const char *formal = generic_param_names[i];
            if (formal != NULL && strlen(formal) == length
                && strncmp(start, formal, length) == 0) {
                if (retained_formal != NULL)
                    *retained_formal = formal;
                return true;
            }
        }
    }
    return false;
}

bool
mir_generic_method_specializations_validate(const MIRProgram *mir,
                                            char **error_message)
{
    if (mir == NULL)
        return false;
    if (mir->generic_method_specialization_count > 0
        && mir->generic_method_specializations == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR generic method specialization inventory is missing");
        return false;
    }
    for (size_t i = 0; i < mir->generic_method_specialization_count; i++) {
        const MIRGenericMethodSpecializationFact *fact =
            &mir->generic_method_specializations[i];
        if (fact->source_call_syntax_id == 0
            || fact->caller_routine_index >= mir->routine_count
            || fact->method_routine_index >= mir->routine_count
            || fact->owner_name == NULL || fact->method_name == NULL
            || fact->specialized_name == NULL || fact->binding_count == 0
            || fact->generic_param_names == NULL
            || fact->actual_type_names == NULL
            || mir->routines[fact->method_routine_index].generic_param_count
                != fact->binding_count) {
            if (error_message != NULL)
                *error_message = mir_strdup_fmt(
                    "MIR generic method specialization[%zu] has invalid shape",
                    i);
            return false;
        }
        for (size_t j = 0; j < fact->binding_count; j++) {
            const char *retained_formal = NULL;
            if (fact->generic_param_names[j] == NULL
                || fact->generic_param_names[j][0] == '\0'
                || fact->actual_type_names[j] == NULL
                || fact->actual_type_names[j][0] == '\0'
                || strcmp(fact->generic_param_names[j],
                    mir->routines[fact->method_routine_index]
                        .generic_param_names[j]) != 0) {
                if (error_message != NULL)
                    *error_message = mir_strdup_fmt(
                        "MIR generic method specialization[%zu] binding[%zu] is invalid",
                        i, j);
                return false;
            }
            if (mir_type_name_retains_formal(fact->actual_type_names[j],
                    fact->generic_param_names, fact->binding_count,
                    &retained_formal)) {
                if (error_message != NULL)
                    *error_message = mir_strdup_fmt(
                        "MIR generic method specialization[%zu] actual type '%s' retains unresolved formal '%s'",
                        i, fact->actual_type_names[j], retained_formal);
                return false;
            }
        }
        for (size_t j = 0; j < i; j++) {
            if (mir->generic_method_specializations[j].source_call_syntax_id
                == fact->source_call_syntax_id) {
                if (error_message != NULL)
                    *error_message = mir_strdup_fmt(
                        "MIR generic method specialization[%zu] duplicates call id %u",
                        i, fact->source_call_syntax_id);
                return false;
            }
        }
    }
    return true;
}

size_t
mir_generic_method_specialization_count(const MIRProgram *mir)
{
    return mir != NULL ? mir->generic_method_specialization_count : 0;
}

const MIRGenericMethodSpecializationFact *
mir_generic_method_specialization_at(const MIRProgram *mir, size_t index)
{
    if (mir == NULL || index >= mir->generic_method_specialization_count)
        return NULL;
    return &mir->generic_method_specializations[index];
}

const MIRGenericMethodSpecializationFact *
mir_generic_method_specialization_for_call(const MIRProgram *mir,
                                           uint32_t source_call_syntax_id)
{
    if (mir == NULL || source_call_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < mir->generic_method_specialization_count; i++) {
        if (mir->generic_method_specializations[i].source_call_syntax_id
            == source_call_syntax_id)
            return &mir->generic_method_specializations[i];
    }
    return NULL;
}
