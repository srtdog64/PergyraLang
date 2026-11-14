#ifndef PGY_MIR_ABILITY_REF_H
#define PGY_MIR_ABILITY_REF_H

#include <stdbool.h>

#include "mir_decl.h"

bool mir_ability_ref_capture(MIRAbilityRef *ref, ASTNode *ability);
void mir_ability_ref_clear(MIRAbilityRef *ref);

#endif /* PGY_MIR_ABILITY_REF_H */
