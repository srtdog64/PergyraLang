#ifndef PERGYRA_COMPILER_MIR_DESTRUCTURE_TYPE_FACTS_H
#define PERGYRA_COMPILER_MIR_DESTRUCTURE_TYPE_FACTS_H

#include "mir.h"

bool mir_copy_destructure_type_facts(MIRRoutine *routine,
                                     const HIRRoutine *hir_routine,
                                     char **error_message);
void mir_free_destructure_type_facts(MIRRoutine *routine);
const MIRDestructureTypeFact *mir_routine_destructure_type_fact(
    const MIRRoutine *routine,
    uint32_t destructure_syntax_id,
    size_t binding_index);

#endif
