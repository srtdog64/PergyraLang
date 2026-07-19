#include "mir_destructure_type_facts.h"

#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

const MIRDestructureTypeFact *
mir_routine_destructure_type_fact(const MIRRoutine *routine,
                                  uint32_t destructure_syntax_id,
                                  size_t binding_index)
{
    if (routine == NULL || destructure_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < routine->destructure_type_fact_count; i++) {
        const MIRDestructureTypeFact *fact =
            &routine->destructure_type_facts[i];
        if (fact->destructure_syntax_id == destructure_syntax_id
            && fact->binding_index == binding_index)
            return fact;
    }
    return NULL;
}

bool
mir_copy_destructure_type_facts(MIRRoutine *routine,
                                const HIRRoutine *hir_routine,
                                char **error_message)
{
    size_t count;

    if (routine == NULL || hir_routine == NULL)
        return false;
    count = hir_routine->destructure_type_fact_count;
    if (count == 0)
        return true;
    if (routine->source_syntax_id == 0
        || hir_routine->source_syntax_id != routine->source_syntax_id
        || hir_routine->destructure_type_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR destructure type facts have incomplete routine identity or storage");
        return false;
    }
    routine->destructure_type_facts = calloc(
        count, sizeof(*routine->destructure_type_facts));
    if (routine->destructure_type_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return false;
    }
    routine->destructure_type_fact_capacity = count;
    for (size_t i = 0; i < count; i++) {
        const HIRDestructureTypeFact *source =
            &hir_routine->destructure_type_facts[i];
        MIRDestructureTypeFact *target = &routine->destructure_type_facts[i];
        if (source->function_syntax_id != routine->source_syntax_id
            || source->destructure_syntax_id == 0
            || source->binding_count == 0
            || source->binding_index >= source->binding_count
            || source->binding_type_name == NULL
            || source->binding_type_name[0] == '\0'
            || mir_routine_destructure_type_fact(routine,
                source->destructure_syntax_id, source->binding_index) != NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR destructure type facts have invalid or duplicate identity");
            goto fail;
        }
        *target = *source;
        target->binding_type_name = pergyra_strdup(source->binding_type_name);
        if (target->binding_type_name == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            goto fail;
        }
        routine->destructure_type_fact_count++;
    }
    return true;

fail:
    mir_free_destructure_type_facts(routine);
    return false;
}

void
mir_free_destructure_type_facts(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    for (size_t i = 0; i < routine->destructure_type_fact_count; i++)
        free(routine->destructure_type_facts[i].binding_type_name);
    free(routine->destructure_type_facts);
    routine->destructure_type_facts = NULL;
    routine->destructure_type_fact_count = 0;
    routine->destructure_type_fact_capacity = 0;
}
