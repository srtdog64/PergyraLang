#ifndef PGY_MIR_SIGNATURE_METADATA_H
#define PGY_MIR_SIGNATURE_METADATA_H

#include "mir.h"

void mir_routine_signature_type_names_clear(MIRRoutine *routine);
bool mir_routine_signature_type_names_capture(MIRRoutine *routine);

#endif
