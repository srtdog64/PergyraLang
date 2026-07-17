#include "air_validate_machine_layer.h"

#include "air_internal.h"
#include "machine_layer_manifest.h"

#include <string.h>

bool
air_validate_machine_layer_site_inventory(const AIRProgram *air,
                                          char **error_message)
{
    if (air->machine_layer_site_count > 0
        && air->machine_layer_sites == NULL) {
        air_set_invariant_error(error_message,
                                "AIR has machine-layer site count without array");
        return false;
    }
    for (size_t i = 0; i < air_machine_layer_site_count(air); i++) {
        const AIRMachineLayerSite *site =
            air_machine_layer_site_at(air, i);
        const PgyMachineLayerTargetManifest *manifest =
            pgy_machine_layer_target_manifest();
        PgyMachineLayerSiteFactView site_view;
        const char *site_error = NULL;
        if (site == NULL || air_name_is_empty(site->slot)
            || air_name_is_empty(site->operation)
            || air_name_is_empty(site->manifest_id)
            || air_name_is_empty(site->physical_grant_id)
            || air_name_is_empty(site->physical_mode)
            || air_name_is_empty(site->runtime_operation)
            || air_name_is_empty(site->routine)
            || !site->hardware_adequate
            || !site->authority_required
            || !site->live_lease_required) {
            air_set_invariant_error(error_message,
                                    "AIR machine-layer site %zu is incomplete",
                                    i);
            return false;
        }
        site_view.manifest_id = site->manifest_id;
        site_view.contact_name = site->operation;
        site_view.physical_grant_id = site->physical_grant_id;
        site_view.physical_base = site->physical_base;
        site_view.physical_size = site->physical_size;
        site_view.physical_mode = site->physical_mode != NULL
            && strcmp(site->physical_mode, "plain") == 0
            ? PGY_MACHINE_LAYER_ACCESS_PLAIN
            : site->physical_mode != NULL
                && strcmp(site->physical_mode, "volatile") == 0
                ? PGY_MACHINE_LAYER_ACCESS_VOLATILE
                : site->physical_mode != NULL
                    && strcmp(site->physical_mode, "atomic") == 0
                    ? PGY_MACHINE_LAYER_ACCESS_ATOMIC
                    : (PgyMachineLayerPhysicalAccessMode)-1;
        site_view.runtime_operation = site->runtime_operation;
        site_view.hardware_adequate = site->hardware_adequate;
        site_view.authority_required = site->authority_required;
        site_view.live_lease_required = site->live_lease_required;
        if (!pgy_machine_layer_manifest_validate_site(
                manifest, &site_view, &site_error)) {
            air_set_invariant_error(error_message,
                                    "AIR machine-layer site %zu is invalid: %s",
                                    i,
                                    site_error != NULL
                                        ? site_error
                                        : "manifest validation failed");
            return false;
        }
    }
    return true;
}
