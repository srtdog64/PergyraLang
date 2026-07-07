#ifndef PGY_MIR_ABI_LAYOUT_H
#define PGY_MIR_ABI_LAYOUT_H

#include "mir.h"

typedef enum MIRResourceAbiKind
{
    MIR_RESOURCE_ABI_SLOT,
    MIR_RESOURCE_ABI_SECURE_SLOT,
    MIR_RESOURCE_ABI_DEVICE_SLOT,
} MIRResourceAbiKind;

const MIRTypeLayout *mir_abi_lookup(const char *pergyra_type_name);
const char *mir_abi_resource_runtime_fn(const MIRTypeLayout *layout,
                                        const char *resource_op_name);
const char *mir_abi_resource_runtime_fn_by_type_name(
    const char *abi_type_name,
    const char *resource_op_name);
const char *mir_abi_resource_runtime_fn_by_kind(
    MIRResourceAbiKind kind,
    const char *inner_type_name,
    const char *resource_op_name);
void mir_abi_table_init(void);

#endif /* PGY_MIR_ABI_LAYOUT_H */
