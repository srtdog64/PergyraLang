#ifndef PERGYRA_MIR_FACT_VALIDATE_INTERNAL_H
#define PERGYRA_MIR_FACT_VALIDATE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "mir.h"

char *mir_fact_strdup_fmt(const char *fmt, ...);

bool mir_validate_instruction_inventory_shape(const MIRRoutine *routine,
                                              const MIRBasicBlock *block,
                                              size_t block_index,
                                              const char *validator,
                                              char **error_message);
bool mir_validate_instruction_surface_usage(const MIRRoutine *routine,
                                            const MIRBasicBlock *block,
                                            size_t block_index,
                                            char **error_message);

#endif /* PERGYRA_MIR_FACT_VALIDATE_INTERNAL_H */
