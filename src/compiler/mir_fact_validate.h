#ifndef PERGYRA_MIR_FACT_VALIDATE_H
#define PERGYRA_MIR_FACT_VALIDATE_H

#include <stdbool.h>
#include <stddef.h>

#include "mir.h"

bool mir_validate_inventory_surface_usage(const MIRProgram *mir,
                                          char **error_message);
bool mir_validate_decl_header_metadata(const MIRProgram *mir,
                                       char **error_message);
bool mir_validate_statement_inventory(const MIRRoutine *routine,
                                      const MIRBasicBlock *block,
                                      size_t block_index,
                                      char **error_message);
bool mir_validate_instruction_surface_usage(const MIRRoutine *routine,
                                            const MIRBasicBlock *block,
                                            size_t block_index,
                                            char **error_message);
bool mir_validate_terminator_provenance(const MIRRoutine *routine,
                                        const MIRBasicBlock *block,
                                        size_t block_index,
                                        char **error_message);

#endif /* PERGYRA_MIR_FACT_VALIDATE_H */
