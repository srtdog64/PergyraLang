#include "hir_region_escape_validate.h"

#include "../common/string_compat.h"

bool
hir_validate_region_escape_facts(const HIRProgram *hir,
                                 const HIRRoutineInventory *inventory,
                                 char **error_message)
{
    if (hir == NULL || inventory == NULL)
        return false;
    if (!hir->has_region_escape_facts) {
        if (hir->region_escape_facts != NULL
            || hir->region_escape_fact_count != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR region escape storage exists without projection marker");
            return false;
        }
        return true;
    }
    if (hir->region_escape_fact_count > 0
        && hir->region_escape_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "HIR region escape projection marker has no fact storage");
        return false;
    }
    for (size_t i = 0; i < hir->region_escape_fact_count; i++) {
        const PgyRegionEscapeFact *fact = &hir->region_escape_facts[i];
        if (fact->allocation_site_id == 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR region escape fact has invalid allocation-site identity");
            return false;
        }
        if (fact->function_syntax_id != 0) {
            bool known = false;
            for (size_t r = 0; r < inventory->count; r++) {
                const HIRRoutine *routine =
                    hir_routine_inventory_get(inventory, r);
                if (routine != NULL && routine->source_syntax_id
                    == fact->function_syntax_id) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "HIR region escape fact references unknown function identity");
                return false;
            }
        }
        for (size_t j = 0; j < i; j++) {
            if (hir->region_escape_facts[j].allocation_site_id
                == fact->allocation_site_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "HIR region escape facts duplicate allocation-site identity");
                return false;
            }
        }
    }
    return true;
}
