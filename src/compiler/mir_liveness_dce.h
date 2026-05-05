#ifndef PERGYRA_MIR_LIVENESS_DCE_H
#define PERGYRA_MIR_LIVENESS_DCE_H

#include <stdbool.h>
#include <stddef.h>

#include "mir.h"

void mir_clear_block_name_set(const char ***names,
                              size_t *count,
                              size_t *capacity);
int mir_find_value_summary(const MIRRoutine *routine, const char *name);
bool mir_build_value_summaries(MIRRoutine *routine);
bool mir_compute_liveness(MIRRoutine *routine);

#endif /* PERGYRA_MIR_LIVENESS_DCE_H */
