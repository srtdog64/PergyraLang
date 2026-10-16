#ifndef PGY_MIR_BRANCH_SOURCE_FACTS_INTERNAL_H
#define PGY_MIR_BRANCH_SOURCE_FACTS_INTERNAL_H

#include "mir_branch_source_facts.h"

const char *mir_collection_binding_type_name(const MIRRoutine *routine,
                                             uint32_t binding_syntax_id);
bool mir_collection_binding_move_source_value_matches(
    const MIRRoutine *routine, const MIRInstruction *destination,
    uint32_t source_binding_syntax_id);

#endif
