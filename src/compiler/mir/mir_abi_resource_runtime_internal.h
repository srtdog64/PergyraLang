#ifndef PGY_MIR_ABI_RESOURCE_RUNTIME_INTERNAL_H
#define PGY_MIR_ABI_RESOURCE_RUNTIME_INTERNAL_H

#include "mir_abi_layout.h"

/* Spell the runtime-call row for a nominal Slot/SecureSlot/DeviceSlot payload
 * that has no static row. Only mir_abi_resource_runtime_row_for_type_name may
 * call this, after the static table missed; no consumer outside the ABI owner
 * builds a row from a resource kind. */
const MIRResourceRuntimeRow *mir_abi_resource_runtime_constructed_row(
    MIRResourceAbiKind kind,
    const char *inner_type_name,
    const char *resource_op_name);

#endif /* PGY_MIR_ABI_RESOURCE_RUNTIME_INTERNAL_H */
