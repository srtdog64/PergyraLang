#ifndef PERGYRA_MIR_REGION_ESCAPE_FACTS_H
#define PERGYRA_MIR_REGION_ESCAPE_FACTS_H

#include <stdbool.h>

#include "mir_program.h"

/*
 * MIR owns the retained copy of the HIR region rows.  The import boundary is
 * deliberately separate from plan construction: a missing or malformed HIR
 * carrier fails during MIR lowering, before a backend can recover the fact.
 */
bool mir_import_region_escape_facts(
    MIRProgram *mir,
    const HIRProgram *hir,
    char **error_message);

bool mir_validate_region_escape_facts(
    const MIRProgram *mir,
    char **error_message);

void mir_clear_region_escape_facts(MIRProgram *mir);

#endif /* PERGYRA_MIR_REGION_ESCAPE_FACTS_H */
