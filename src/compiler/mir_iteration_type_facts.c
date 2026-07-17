#include "mir.h"

const MIRIterationTypeFact *
mir_routine_iteration_type_fact(const MIRRoutine *routine,
                                uint32_t iteration_syntax_id)
{
    if (routine == NULL || iteration_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < routine->iteration_type_fact_count; i++) {
        const MIRIterationTypeFact *fact =
            &routine->iteration_type_facts[i];
        if (fact->iteration_syntax_id == iteration_syntax_id)
            return fact;
    }
    return NULL;
}
