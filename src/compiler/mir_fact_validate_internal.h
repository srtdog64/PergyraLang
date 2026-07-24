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

/* Resource-surface fact classification owner
 * (mir_fact_surface_validate_resource.c). */
bool mir_claim_abi_type_is_slot_family(const MIRInstruction *inst);
bool mir_resource_kind_consumes_view(const MIRInstruction *inst);
const char *mir_prior_borrow_source_for_view(const MIRRoutine *routine,
                                             size_t before_block,
                                             size_t before_inst,
                                             const char *view_name);
bool mir_resource_owner_layout_is_slot_family(const MIRInstruction *inst);
bool mir_resource_runtime_fact_requires_row(const MIRInstruction *inst);
const MIRInstruction *mir_resource_runtime_fact_source_for_consumer(
    const MIRBasicBlock *block,
    const MIRInstruction *consumer);

#endif /* PERGYRA_MIR_FACT_VALIDATE_INTERNAL_H */
