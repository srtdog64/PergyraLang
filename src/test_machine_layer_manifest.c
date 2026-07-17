#include "compiler/machine_layer_manifest.h"
#include "runtime/pgy_runtime_machine_layer_inline.h"

#include <stdio.h>
#include <string.h>

static int
require_projection(const PgyMachineLayerTargetManifest *manifest,
                   const char *name,
                   const char *representation)
{
    const PgyMachineLayerProjectionManifest *row =
        pgy_machine_layer_manifest_projection(manifest, name);
    const char *error = NULL;

    if (row == NULL || !pgy_machine_layer_projection_validate(
            manifest, name, &error)) {
        fprintf(stderr, "projection %s rejected: %s\n", name,
                error != NULL ? error : "missing row");
        return 1;
    }
    if (row->physical_representation == NULL
        || strcmp(row->physical_representation, representation) != 0) {
        fprintf(stderr, "projection %s has unexpected physical representation\n",
                name);
        return 1;
    }
    return 0;
}

static int32_t
machine_layer_mapping_provider(uint64_t base, uint64_t size, uint32_t mode,
                               void *context)
{
    const uint64_t *expected = (const uint64_t *)context;
    return expected != NULL && base == expected[0] && size == expected[1]
        && mode == (uint32_t)expected[2];
}

int
main(void)
{
    const PgyMachineLayerTargetManifest *manifest =
        pgy_machine_layer_target_manifest();
    const PgyMachineLayerProjectionManifest *c_projection;
    const PgyMachineLayerProjectionManifest *llvm_projection;
    const PgyMachineLayerPhysicalManifest *physical;
    PgyMachineLayerTargetManifest broken;
    PgyMachineLayerSiteFactView site;
    const char *error = NULL;
    uint64_t fingerprint;
    uint64_t physical_fingerprint;

    if (manifest == NULL || manifest->operation_count != 5
        || manifest->projection_count != 3)
        return 1;
    fingerprint = pgy_machine_layer_manifest_fingerprint(manifest);
    if (fingerprint == 0)
        return 2;
    physical = pgy_machine_layer_physical_manifest();
    physical_fingerprint = pgy_machine_layer_physical_manifest_fingerprint(physical);
    if (physical == NULL || physical_fingerprint == 0
        || !pgy_machine_layer_physical_manifest_validate(physical, &error))
        return 2;
    if (pgy_machine_layer_runtime_bind_export(0, physical_fingerprint) != 0
        || pgy_machine_layer_runtime_bind_export(fingerprint, 0) != 0
        || pgy_machine_layer_runtime_bind_export(
               fingerprint, physical_fingerprint) != 1
        || pgy_machine_layer_runtime_bind_export(
               fingerprint, physical_fingerprint) != 1
        || pgy_machine_layer_runtime_bind_export(
               fingerprint + 1, physical_fingerprint) != 0) {
        fprintf(stderr, "runtime machine-layer bind did not fail closed\n");
        return 2;
    }
    if (pgy_machine_layer_runtime_bind_mapping_export(
            fingerprint, physical_fingerprint,
            physical->grants[0].base, physical->grants[0].size,
            (uint32_t)physical->grants[0].mode, 0) != 1
        || pgy_machine_layer_runtime_bind_mapping_export(
               fingerprint, physical_fingerprint,
               physical->grants[0].base, physical->grants[0].size,
               (uint32_t)physical->grants[0].mode, 0) != 1
        || pgy_machine_layer_runtime_bind_mapping_export(
               fingerprint, physical_fingerprint,
               physical->grants[0].base + 1, physical->grants[0].size,
               (uint32_t)physical->grants[0].mode, 0) != 0
        || pgy_machine_layer_runtime_bind_mapping_export(
               fingerprint, physical_fingerprint,
               physical->grants[0].base, physical->grants[0].size,
               (uint32_t)PGY_MACHINE_LAYER_ACCESS_PLAIN, 0) != 0) {
        fprintf(stderr, "runtime machine-layer mapping did not fail closed\n");
        return 2;
    }
    {
        const uint64_t expected[] = {
            physical->grants[0].base, physical->grants[0].size,
            (uint64_t)physical->grants[0].mode
        };
        const uint64_t wrong[] = { expected[0] + 1, expected[1], expected[2] };
        if (pgy_machine_layer_runtime_provider_bind_export(
                machine_layer_mapping_provider, (void *)wrong) != 0
            || pgy_machine_layer_runtime_provider_bind_export(
                   machine_layer_mapping_provider, (void *)expected) != 1
            || pgy_machine_layer_runtime_bind_mapping_export(
                   fingerprint, physical_fingerprint,
                   expected[0], expected[1], (uint32_t)expected[2], 0) != 1
            || pgy_machine_layer_runtime_bind_mapping_export(
                   fingerprint, physical_fingerprint,
                   expected[0] + 1, expected[1], (uint32_t)expected[2], 0) != 0) {
            fprintf(stderr, "runtime machine-layer provider did not fail closed\n");
            return 2;
        }
    }
    if (require_projection(manifest, "cpu-c", "c-abi-runtime-handle") != 0
        || require_projection(manifest, "cpu-llvm",
                              "llvm-ssa-address-space") != 0
        || require_projection(manifest, "self-hosted",
                              "owner-fact-artifact") != 0)
        return 1;

    c_projection = pgy_machine_layer_manifest_projection(manifest, "cpu-c");
    llvm_projection = pgy_machine_layer_manifest_projection(manifest, "cpu-llvm");
    if (c_projection == NULL || llvm_projection == NULL
        || strcmp(c_projection->physical_representation,
                  llvm_projection->physical_representation) == 0) {
        fprintf(stderr, "C and LLVM projections must remain physically distinct\n");
        return 1;
    }
    if (pgy_machine_layer_projection_ready_for_backend("unknown", &error)
        || error == NULL) {
        fprintf(stderr, "unknown machine projection did not fail closed\n");
        return 1;
    }
    if (!pgy_machine_layer_physical_projection_ready_for_backend("cpu-c", &error)
        || !pgy_machine_layer_physical_projection_ready_for_backend("cpu-llvm", &error)) {
        fprintf(stderr, "physical machine declaration projection rejected: %s\n",
                error != NULL ? error : "missing error");
        return 1;
    }

    site.manifest_id = manifest->manifest_id;
    site.contact_name = "read";
    site.physical_grant_id = physical->device_grant_id;
    site.physical_base = physical->grants[0].base;
    site.physical_size = physical->grants[0].size;
    site.physical_mode = physical->grants[0].mode;
    site.runtime_operation = "Read";
    site.hardware_adequate = true;
    site.authority_required = true;
    site.live_lease_required = true;
    if (!pgy_machine_layer_manifest_validate_site(
            manifest, &site, &error)) {
        fprintf(stderr, "valid machine site rejected: %s\n",
                error != NULL ? error : "missing error");
        return 1;
    }
    site.runtime_operation = "Write";
    if (pgy_machine_layer_manifest_validate_site(
            manifest, &site, &error) || error == NULL) {
        fprintf(stderr, "machine site runtime mismatch did not fail closed\n");
        return 1;
    }
    site.runtime_operation = "Read";
    site.authority_required = false;
    if (pgy_machine_layer_manifest_validate_site(
            manifest, &site, &error) || error == NULL) {
        fprintf(stderr, "machine site authority mismatch did not fail closed\n");
        return 1;
    }
    site.authority_required = true;
    site.physical_base += 1;
    if (pgy_machine_layer_manifest_validate_site(
            manifest, &site, &error) || error == NULL) {
        fprintf(stderr, "machine site physical shape mismatch did not fail closed\n");
        return 1;
    }

    broken = *manifest;
    broken.manifest_id = "mutated-machine-layer-manifest";
    if (pgy_machine_layer_manifest_fingerprint(&broken) == fingerprint) {
        fprintf(stderr, "machine manifest mutation kept the same fingerprint\n");
        return 3;
    }
    broken = *manifest;
    broken.hardware_adequate = false;
    if (pgy_machine_layer_projection_validate(&broken, "cpu-c", &error)
        || error == NULL) {
        fprintf(stderr, "unproven machine adequacy did not fail closed\n");
        return 1;
    }

    {
        PgyMachineLayerPhysicalManifest broken_provenance = *physical;
        broken_provenance.board_id = "mutated-board";
        if (pgy_machine_layer_physical_manifest_fingerprint(
                &broken_provenance) == physical_fingerprint) {
            fprintf(stderr, "physical provenance mutation kept the same fingerprint\n");
            return 3;
        }
    }

    {
        PgyMachineLayerPhysicalManifest broken_physical = *physical;
        PgyMachineLayerPhysicalGrant broken_grant = physical->grants[0];
        broken_grant.base = physical->address_limit;
        broken_physical.grants = &broken_grant;
        if (pgy_machine_layer_physical_manifest_validate(
                &broken_physical, &error) || error == NULL) {
            fprintf(stderr, "out-of-range physical grant did not fail closed\n");
            return 1;
        }
        broken_grant.base = physical->grants[0].base;
        broken_grant.mode = PGY_MACHINE_LAYER_ACCESS_PLAIN;
        if (pgy_machine_layer_physical_manifest_validate(
                &broken_physical, &error) || error == NULL) {
            fprintf(stderr, "non-volatile device grant did not fail closed\n");
            return 1;
        }
    }

    {
        static const PgyMachineLayerPhysicalGrant provider_grants[] = {
            { "board-device0", UINT64_C(0x20000000), UINT64_C(0x2000),
              PGY_MACHINE_LAYER_ACCESS_VOLATILE, true },
        };
        static const PgyMachineLayerPhysicalManifest provider_manifest = {
            "pergyra.machine-declaration.board-v1",
            "board-device",
            "board-01",
            "boot-01.v1",
            "linker-01.v1",
            UINT64_C(0x7fffffff),
            "board-device0",
            provider_grants,
            1,
            true
        };
        if (!pgy_machine_layer_physical_manifest_bind(
                &provider_manifest, &error)
            || pgy_machine_layer_physical_manifest() != &provider_manifest
            || pgy_machine_layer_physical_manifest_fingerprint(
                   &provider_manifest) == physical_fingerprint) {
            fprintf(stderr, "physical declaration provider did not bind\n");
            return 2;
        }
    }

    puts("machine-layer manifest probe: C/LLVM distinct projections, physical declaration provider, runtime fingerprint/mapping bind, and fail-closed validation ok");
    return 0;
}
