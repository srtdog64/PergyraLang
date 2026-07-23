#include "hir.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"

static bool
hir_region_escape_function_exists(const HIRProgram *hir,
                                  uint32_t function_syntax_id)
{
    if (hir == NULL || function_syntax_id == 0)
        return function_syntax_id == 0;
    for (size_t i = 0; i < hir->routine_count; i++) {
        if (hir->routines[i].source_syntax_id == function_syntax_id)
            return true;
    }
    return false;
}

bool
hir_attach_region_escape_facts(HIRProgram *hir,
                               const PgyRegionEscapeFact *facts,
                               size_t fact_count,
                               char **error_message)
{
    PgyRegionEscapeFact *owned = NULL;

    if (error_message != NULL)
        *error_message = NULL;
    if (hir == NULL || (facts == NULL && fact_count != 0)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "HIR region escape fact projection has invalid storage");
        return false;
    }
    if (hir->has_region_escape_facts) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "HIR region escape facts were attached more than once");
        return false;
    }

    for (size_t i = 0; i < fact_count; i++) {
        const PgyRegionEscapeFact *fact = &facts[i];
        if (fact->allocation_site_id == 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR region escape fact has no allocation-site identity");
            return false;
        }
        if (!hir_region_escape_function_exists(
                hir, fact->function_syntax_id)) {
            if (error_message != NULL) {
                char detail[160];
                (void) snprintf(
                    detail, sizeof(detail),
                    "HIR region escape fact references unknown function syntax id %u",
                    fact->function_syntax_id);
                *error_message = pergyra_strdup(detail);
            }
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            if (facts[j].allocation_site_id == fact->allocation_site_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "HIR region escape facts contain duplicate allocation-site identity");
                return false;
            }
        }
    }

    if (fact_count > 0) {
        owned = calloc(fact_count, sizeof(*owned));
        if (owned == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR region escape fact storage allocation failed");
            return false;
        }
        for (size_t i = 0; i < fact_count; i++)
            owned[i] = facts[i];
    }
    hir->region_escape_facts = owned;
    hir->region_escape_fact_count = fact_count;
    hir->has_region_escape_facts = true;
    return true;
}
