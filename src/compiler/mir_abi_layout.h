#ifndef PGY_MIR_ABI_LAYOUT_H
#define PGY_MIR_ABI_LAYOUT_H

#include <stddef.h>

#include "mir.h"

typedef enum MIRResourceAbiKind
{
    MIR_RESOURCE_ABI_SLOT,
    MIR_RESOURCE_ABI_SECURE_SLOT,
    MIR_RESOURCE_ABI_DEVICE_SLOT,
} MIRResourceAbiKind;

typedef struct MIRAbiTargetPolicy
{
    const char *abi_name;
    const char *projection_set;
    const char *required_facts;
    const char *fallback_reasons;
} MIRAbiTargetPolicy;

const MIRTypeLayout *mir_abi_lookup(const char *pergyra_type_name);
const MIRAbiTargetPolicy *mir_abi_target_policy(const char *abi_name);
const char *mir_abi_resource_runtime_fn(const MIRTypeLayout *layout,
                                        const char *resource_op_name);
const char *mir_abi_resource_runtime_fn_by_type_name(
    const char *abi_type_name,
    const char *resource_op_name);
const char *mir_abi_resource_runtime_fn_by_kind(
    MIRResourceAbiKind kind,
    const char *inner_type_name,
    const char *resource_op_name);
size_t mir_abi_resource_runtime_row_count(void);
const char *mir_abi_resource_runtime_row_domain(size_t index);
const char *mir_abi_resource_runtime_row_type_name(size_t index);
const char *mir_abi_resource_runtime_row_operation(size_t index);
const char *mir_abi_resource_runtime_row_symbol(size_t index);
const char *mir_abi_resource_runtime_row_target_kind(size_t index);
const char *mir_abi_resource_runtime_row_materialization(size_t index);
void mir_abi_table_init(void);

#endif /* PGY_MIR_ABI_LAYOUT_H */
