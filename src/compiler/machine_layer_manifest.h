#ifndef PERGYRA_MACHINE_LAYER_MANIFEST_H
#define PERGYRA_MACHINE_LAYER_MANIFEST_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "rir.h"

/* This is intentionally an abstract runtime manifest.  It names the current
 * DeviceSlot contract without claiming that the host has proven a physical
 * MMIO device or a silicon-specific memory model. */
#define PGY_MACHINE_LAYER_MANIFEST_ID "pergyra.abstract-device-slot.v1"
#define PGY_MACHINE_LAYER_PHYSICAL_MANIFEST_ID "pergyra.machine-declaration.host-sim.v1"
#define PGY_MACHINE_LAYER_PHYSICAL_MANIFEST_PREFIX "pergyra.machine-declaration."

/* A target declaration is the native counterpart of the formal
 * MachineDeclaration.  It is deliberately a data record: validation proves
 * shape, bounds, non-overlap, and mode/adequacy consistency, but does not
 * claim that a bootloader or a real board agrees with it. */
typedef enum
{
    PGY_MACHINE_LAYER_ACCESS_PLAIN = 0,
    PGY_MACHINE_LAYER_ACCESS_VOLATILE = 1,
    PGY_MACHINE_LAYER_ACCESS_ATOMIC = 2
} PgyMachineLayerPhysicalAccessMode;

typedef struct PgyMachineLayerPhysicalManifest
{
    const char *grant_id;
    uint64_t base;
    uint64_t size;
    PgyMachineLayerPhysicalAccessMode mode;
    bool hardware_adequate;
} PgyMachineLayerPhysicalGrant;

typedef struct
{
    const char *manifest_id;
    const char *target_kind;
    /* Provenance rows identify the board/boot/linker declaration boundary.
     * They are declaration metadata, not a claim that live hardware agrees. */
    const char *board_id;
    const char *boot_contract;
    const char *linker_contract;
    uint64_t address_limit;
    const char *device_grant_id;
    const PgyMachineLayerPhysicalGrant *grants;
    size_t grant_count;
    bool hardware_adequate;
} PgyMachineLayerPhysicalManifest;

typedef struct
{
    RIRMachineContactKind operation;
    const char *contact_name;
    const char *runtime_operation;
    bool requires_authority;
    bool requires_live_lease;
} PgyMachineLayerOperationManifest;

/* The semantic contact contract is shared, but its physical representation
 * is deliberately target-specific.  These rows are owned by the abstract
 * machine manifest so C, LLVM, and self-hosted projections cannot silently
 * invent a representation or fall back to a target default. */
typedef struct
{
    const char *projection;
    const char *physical_representation;
    const char *lowering_contract;
} PgyMachineLayerProjectionManifest;

typedef struct
{
    const char *manifest_id;
    const char *target_kind;
    unsigned supported_operations;
    bool hardware_adequate;
    const PgyMachineLayerOperationManifest *operations;
    size_t operation_count;
    const PgyMachineLayerProjectionManifest *projections;
    size_t projection_count;
} PgyMachineLayerTargetManifest;

/* AIR/MIR site validation view. The manifest owns the contact-to-runtime
 * mapping and proof requirements; downstream evidence only supplies the row
 * it observed. */
typedef struct
{
    const char *manifest_id;
    const char *contact_name;
    const char *physical_grant_id;
    uint64_t physical_base;
    uint64_t physical_size;
    PgyMachineLayerPhysicalAccessMode physical_mode;
    const char *runtime_operation;
    bool hardware_adequate;
    bool authority_required;
    bool live_lease_required;
} PgyMachineLayerSiteFactView;

const PgyMachineLayerTargetManifest *
pgy_machine_layer_target_manifest(void);
const PgyMachineLayerPhysicalManifest *
pgy_machine_layer_physical_manifest(void);
/* Bind one immutable target declaration before planning/code generation.
 * The provider owns the pointed-to record and grant table for the remainder
 * of the process; the compiler never copies or reconstructs those rows. */
bool pgy_machine_layer_physical_manifest_bind(
    const PgyMachineLayerPhysicalManifest *manifest,
    const char **error_out);
const PgyMachineLayerPhysicalGrant *
pgy_machine_layer_physical_manifest_grant(
    const PgyMachineLayerPhysicalManifest *manifest,
    const char *grant_id);
const char *
pgy_machine_layer_physical_access_mode_name(
    PgyMachineLayerPhysicalAccessMode mode);
/* Stable identity for the abstract machine contract.  Projection planners
 * bind this once; backends must consume the derived plan row instead of
 * recomputing or inventing a machine manifest. */
uint64_t pgy_machine_layer_manifest_fingerprint(
    const PgyMachineLayerTargetManifest *manifest);
uint64_t pgy_machine_layer_physical_manifest_fingerprint(
    const PgyMachineLayerPhysicalManifest *manifest);
bool pgy_machine_layer_manifest_supports(
    const PgyMachineLayerTargetManifest *manifest,
    RIRMachineContactKind operation);
const PgyMachineLayerOperationManifest *
pgy_machine_layer_manifest_operation(
    const PgyMachineLayerTargetManifest *manifest,
    RIRMachineContactKind operation);
size_t pgy_machine_layer_manifest_operation_count(
    const PgyMachineLayerTargetManifest *manifest);
const PgyMachineLayerOperationManifest *
pgy_machine_layer_manifest_operation_at(
    const PgyMachineLayerTargetManifest *manifest,
    size_t index);
const PgyMachineLayerProjectionManifest *
pgy_machine_layer_manifest_projection(
    const PgyMachineLayerTargetManifest *manifest,
    const char *projection);
bool pgy_machine_layer_projection_validate(
    const PgyMachineLayerTargetManifest *manifest,
    const char *projection,
    const char **error_out);
bool pgy_machine_layer_projection_ready_for_backend(
    const char *projection,
    const char **error_out);
bool pgy_machine_layer_physical_manifest_validate(
    const PgyMachineLayerPhysicalManifest *manifest,
    const char **error_out);
bool pgy_machine_layer_physical_projection_ready_for_backend(
    const char *projection,
    const char **error_out);
bool pgy_machine_layer_manifest_validate_site(
    const PgyMachineLayerTargetManifest *manifest,
    const PgyMachineLayerSiteFactView *site,
    const char **error_out);

/* Emit the native declaration as the single-consumer handoff artifact for
 * self-hosted and external target tooling. The serializer exposes the same
 * owner rows used by validation/planning; it does not manufacture a second
 * physical declaration. */
void pgy_machine_layer_manifest_dump_json(FILE *out);

#endif
