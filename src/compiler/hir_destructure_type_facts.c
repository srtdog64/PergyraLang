#include "hir.h"

#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
hir_append_destructure_type_fact(HIRRoutine *routine,
                                 const PgyDestructureTypeFact *fact,
                                 char **error_message)
{
    size_t next_capacity;
    HIRDestructureTypeFact *grown;
    HIRDestructureTypeFact *copy;

    if (routine == NULL || fact == NULL
        || fact->function_syntax_id != routine->source_syntax_id
        || fact->destructure_syntax_id == 0
        || fact->binding_count == 0
        || fact->binding_index >= fact->binding_count
        || fact->binding_type_name == NULL
        || fact->binding_type_name[0] == '\0')
        return false;
    for (size_t i = 0; i < routine->destructure_type_fact_count; i++) {
        const HIRDestructureTypeFact *existing =
            &routine->destructure_type_facts[i];
        if (existing->destructure_syntax_id == fact->destructure_syntax_id
            && existing->binding_index == fact->binding_index) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "duplicate HIR destructure type fact identity");
            return false;
        }
    }
    if (routine->destructure_type_fact_count
        == routine->destructure_type_fact_capacity) {
        next_capacity = routine->destructure_type_fact_capacity == 0
            ? 8
            : routine->destructure_type_fact_capacity * 2;
        if (next_capacity < routine->destructure_type_fact_capacity
            || next_capacity > SIZE_MAX / sizeof(*grown))
            return false;
        grown = realloc(routine->destructure_type_facts,
                        next_capacity * sizeof(*grown));
        if (grown == NULL)
            return false;
        routine->destructure_type_facts = grown;
        routine->destructure_type_fact_capacity = next_capacity;
    }
    copy = &routine->destructure_type_facts[
        routine->destructure_type_fact_count];
    *copy = *fact;
    copy->binding_type_name = pergyra_strdup(fact->binding_type_name);
    if (copy->binding_type_name == NULL)
        return false;
    routine->destructure_type_fact_count++;
    return true;
}

bool
hir_attach_destructure_type_facts(HIRProgram *hir,
                                  const PgyDestructureTypeFact *facts,
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
                    "Destructure type fact references an unknown HIR routine");
            return false;
        }
        if (!hir_append_destructure_type_fact(routine, &facts[i],
                error_message)) {
            if (error_message != NULL && *error_message == NULL)
                *error_message = pergyra_strdup(
                    "Invalid or unallocatable HIR destructure type fact");
            return false;
        }
    }
    hir->has_destructure_type_facts = fact_count != 0;
    return true;
}
