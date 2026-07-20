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
const MIRResourceRuntimeRow *mir_abi_resource_runtime_row_at(size_t index);
const MIRResourceRuntimeRow *mir_abi_resource_runtime_row_by_type_name(
    const char *abi_type_name,
    const char *resource_op_name);
/* Resolve a canonical Slot/SecureSlot/DeviceSlot ABI type name.  Static rows
 * are returned directly; an unknown nominal payload may receive a temporary
 * constructed row owned by this ABI module.  MIR lowering must copy that row
 * into its arena before the pointer is allowed to escape the call. */
const MIRResourceRuntimeRow *mir_abi_resource_runtime_row_for_type_name(
    const char *abi_type_name,
    const char *resource_op_name);
const MIRResourceRuntimeRow *mir_abi_resource_runtime_row_by_kind(
    MIRResourceAbiKind kind,
    const char *inner_type_name,
    const char *resource_op_name);
size_t mir_abi_resource_runtime_row_count(void);
const MIRTextBuilderRuntimeRow *mir_text_builder_runtime_row(
    const char *operation);
const MIRTextBuilderRuntimeRow *mir_text_builder_runtime_row_by_source_name(
    const char *source_name);
const MIRTextBuilderRuntimeRow *mir_text_builder_runtime_row_at(size_t index);
size_t mir_text_builder_runtime_row_count(void);
const char *mir_text_builder_call_shape_name(MIRTextBuilderCallShape shape);
const char *mir_abi_resource_runtime_row_domain(size_t index);
const char *mir_abi_resource_runtime_row_type_name(size_t index);
const char *mir_abi_resource_runtime_row_operation(size_t index);
const char *mir_abi_resource_runtime_row_symbol(size_t index);
const char *mir_abi_resource_runtime_row_target_kind(size_t index);
const char *mir_abi_resource_runtime_row_materialization(size_t index);
const char *mir_abi_resource_runtime_row_call_shape(size_t index);
void mir_abi_table_init(void);

#endif /* PGY_MIR_ABI_LAYOUT_H */
