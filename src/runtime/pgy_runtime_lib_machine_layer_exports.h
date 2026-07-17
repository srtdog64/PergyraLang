/* LLVM runtime twin for the generated C machine-layer bind entrypoint. */
#ifndef PGY_RUNTIME_LIB_MACHINE_LAYER_EXPORTS_H
#define PGY_RUNTIME_LIB_MACHINE_LAYER_EXPORTS_H

#include <stdint.h>

typedef int32_t (*PgyMachineLayerRuntimeMappingProvider)(
    uint64_t grant_base, uint64_t grant_size, uint32_t grant_mode,
    void *context);

static uint64_t pgy_machine_layer_bound_manifest_fingerprint;
static uint64_t pgy_machine_layer_bound_physical_fingerprint;
static uint64_t pgy_machine_layer_bound_grant_base;
static uint64_t pgy_machine_layer_bound_grant_size;
static uint32_t pgy_machine_layer_bound_grant_mode;
static PgyMachineLayerRuntimeMappingProvider
    pgy_machine_layer_mapping_provider;
static void *pgy_machine_layer_mapping_provider_context;

int32_t
pgy_machine_layer_runtime_provider_bind_export(
    PgyMachineLayerRuntimeMappingProvider provider, void *context)
{
    if (provider == NULL)
        return 0;
    if (pgy_machine_layer_mapping_provider != NULL)
        return pgy_machine_layer_mapping_provider == provider
            && pgy_machine_layer_mapping_provider_context == context;
    if (pgy_machine_layer_bound_grant_size != 0
        && provider(pgy_machine_layer_bound_grant_base,
                    pgy_machine_layer_bound_grant_size,
                    pgy_machine_layer_bound_grant_mode, context) != 1)
        return 0;
    pgy_machine_layer_mapping_provider = provider;
    pgy_machine_layer_mapping_provider_context = context;
    return 1;
}

int32_t
pgy_machine_layer_runtime_bind_export(uint64_t manifest_fingerprint,
                                      uint64_t physical_fingerprint)
{
    if (manifest_fingerprint == 0 || physical_fingerprint == 0)
        return 0;
    if (pgy_machine_layer_bound_manifest_fingerprint != 0
        || pgy_machine_layer_bound_physical_fingerprint != 0) {
        return pgy_machine_layer_bound_manifest_fingerprint
                   == manifest_fingerprint
               && pgy_machine_layer_bound_physical_fingerprint
                   == physical_fingerprint;
    }
    pgy_machine_layer_bound_manifest_fingerprint = manifest_fingerprint;
    pgy_machine_layer_bound_physical_fingerprint = physical_fingerprint;
    return 1;
}

/* LLVM runtime twin of the declaration-level mapping guard. */
int32_t
pgy_machine_layer_runtime_bind_mapping_export(
    uint64_t manifest_fingerprint,
    uint64_t physical_fingerprint,
    uint64_t grant_base,
    uint64_t grant_size,
    uint32_t grant_mode,
    uint32_t provider_required)
{
    if (!pgy_machine_layer_runtime_bind_export(
            manifest_fingerprint, physical_fingerprint)
        || grant_size == 0
        || grant_mode != 1u)
        return 0;
    if (provider_required != 0 && pgy_machine_layer_mapping_provider == NULL)
        return 0;
    if (pgy_machine_layer_mapping_provider != NULL
        && pgy_machine_layer_mapping_provider(
               grant_base, grant_size, grant_mode,
               pgy_machine_layer_mapping_provider_context) != 1)
        return 0;
    if (pgy_machine_layer_bound_grant_size != 0) {
        return pgy_machine_layer_bound_grant_base == grant_base
            && pgy_machine_layer_bound_grant_size == grant_size
            && pgy_machine_layer_bound_grant_mode == grant_mode;
    }
    pgy_machine_layer_bound_grant_base = grant_base;
    pgy_machine_layer_bound_grant_size = grant_size;
    pgy_machine_layer_bound_grant_mode = grant_mode;
    return 1;
}

#endif /* PGY_RUNTIME_LIB_MACHINE_LAYER_EXPORTS_H */
