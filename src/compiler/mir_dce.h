#ifndef PERGYRA_MIR_DCE_H
#define PERGYRA_MIR_DCE_H

#include <stdbool.h>

#include "mir.h"

void mir_reset_routine_analysis(MIRRoutine *routine);
bool mir_recompute_analysis(MIRRoutine *routine);
bool mir_run_dce_on_routine(MIRRoutine *routine, bool *changed_out);

#endif /* PERGYRA_MIR_DCE_H */
