#ifndef PGY_MIR_ABI_LAYOUT_H
#define PGY_MIR_ABI_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

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
/* Stable identity for a complete static MIR ABI layout row.  Returns zero for
 * an incomplete row; the value is independent of table index and pointer. */
uint32_t mir_abi_layout_id(const MIRTypeLayout *layout);
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
/* Stable logical identity for a runtime-call row.  The identity is keyed by
 * owner domain, canonical ABI type, and logical operation; it never depends
 * on table position or target-specific symbol spelling. */
uint32_t mir_abi_resource_runtime_row_id(
    const MIRResourceRuntimeRow *row);
bool mir_abi_resource_runtime_row_matches_owner(
    const MIRResourceRuntimeRow *row);
/* Constructed nominal Slot<T>/SecureSlot<T>/DeviceSlot<T> rows do not have a
 * static pgy_abi_spec.h layout row yet. Their runtime-call row is the MIR
 * owner for this compatibility edge; backends may accept the absent static
 * MIRTypeLayout only when this owner explicitly marks the row as constructed.
 */
bool mir_abi_resource_runtime_row_is_constructed_nominal(
    const MIRResourceRuntimeRow *row);
const MIRInstruction *mir_abi_resource_runtime_instruction_for_source(
    const MIRRoutine *routine,
    uint32_t source_stable_id);
/* Resolve a non-source consumer (for example MIR's synthetic slot auto-read)
 * to an existing same-type/resource-operation owner row.  This is a MIR fact
 * lookup, not a backend ABI-table fallback; absence remains an error. */
const MIRInstruction *mir_abi_resource_runtime_instruction_for_abi(
    const MIRRoutine *routine,
    MIRResourceAbiKind kind,
    const char *inner_type_name,
    const char *resource_op_name);
/* Return the existing same-type MIR resource fact that authorizes a
 * synthetic consumer.  This is the layout/evidence owner for consumers that
 * have no source resource instruction of their own. */
const MIRInstruction *mir_abi_resource_runtime_owner_for_mir_abi(
    const MIRRoutine *routine,
    MIRResourceAbiKind kind,
    const char *inner_type_name);
/* Derive a synthetic consumer row (such as MIR's slot auto-read) from an
 * existing same-type MIR resource owner. The returned TLS row is valid until
 * the next call on the same thread; no row is produced when the routine has
 * no same-type owner fact. */
const MIRResourceRuntimeRow *mir_abi_resource_runtime_row_for_mir_abi(
    const MIRRoutine *routine,
    MIRResourceAbiKind kind,
    const char *inner_type_name,
    const char *resource_op_name);
/* Return the MIR resource instruction that authorizes a synthetic pin
 * enter/exit runtime row for this ABI type.  Pin emission has no source call
 * instruction of its own, so layout validation must follow this owner fact
 * instead of inspecting the backend's current instruction cursor. */
const MIRInstruction *mir_abi_resource_runtime_pin_owner_for_mir(
    const MIRRoutine *routine,
    MIRResourceAbiKind kind,
    const char *inner_type_name);
/* Pin enter/exit rows are derived only after MIR has supplied a concrete
 * resource row for the same ABI type.  This keeps the pin runtime spelling in
 * the ABI owner without allowing a backend to synthesize it from a slot name. */
const MIRResourceRuntimeRow *mir_abi_resource_runtime_pin_row_for_mir(
    const MIRRoutine *routine,
    MIRResourceAbiKind kind,
    const char *inner_type_name,
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
