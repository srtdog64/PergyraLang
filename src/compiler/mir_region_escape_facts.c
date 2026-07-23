#include "mir_region_escape_facts.h"

#include <stdint.h>
#include <stdlib.h>

#include "../common/string_compat.h"
#include "mir.h"
#include "mir_base_helpers.h"

static bool
mir_region_escape_function_exists(const MIRProgram *mir,
                                  uint32_t function_syntax_id)
{
    if (mir == NULL || function_syntax_id == 0)
        return function_syntax_id == 0;
    for (size_t i = 0; i < mir->routine_count; i++) {
        if (mir->routines[i].source_syntax_id == function_syntax_id)
            return true;
    }
    return false;
}

void
mir_clear_region_escape_facts(MIRProgram *mir)
{
    if (mir == NULL)
        return;
    free(mir->region_escape_facts);
    mir->region_escape_facts = NULL;
    mir->region_escape_fact_count = 0;
    mir->has_region_escape_facts = false;
}

bool
mir_validate_region_escape_facts(const MIRProgram *mir,
                                 char **error_message)
{
    if (mir == NULL)
        return false;
    if (!mir->has_region_escape_facts) {
        if (mir->region_escape_facts != NULL
            || mir->region_escape_fact_count != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR region escape storage exists without retention marker");
            return false;
        }
        return true;
    }
    if (mir->region_escape_fact_count > 0
        && mir->region_escape_facts == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR region escape retention marker has no fact storage");
        return false;
    }
    for (size_t i = 0; i < mir->region_escape_fact_count; i++) {
        const PgyRegionEscapeFact *fact = &mir->region_escape_facts[i];
        if (fact->allocation_site_id == 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR region escape fact has invalid allocation-site identity");
            return false;
        }
        if (!mir_region_escape_function_exists(
                mir, fact->function_syntax_id)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR region escape fact references unknown function identity");
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            if (mir->region_escape_facts[j].allocation_site_id
                == fact->allocation_site_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "MIR region escape facts duplicate allocation-site identity");
                return false;
            }
        }
    }
    return true;
}

bool
mir_import_region_escape_facts(MIRProgram *mir,
                               const HIRProgram *hir,
                               char **error_message)
{
    PgyRegionEscapeFact *owned = NULL;

    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL || hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR region escape import requires MIR and HIR programs");
        return false;
    }
    if (mir->has_region_escape_facts) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR region escape facts were imported more than once");
        return false;
    }
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
        if (!mir_region_escape_function_exists(
                mir, fact->function_syntax_id)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "HIR region escape fact cannot reach a MIR function identity");
            return false;
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
    if (hir->region_escape_fact_count > 0) {
        owned = calloc(hir->region_escape_fact_count, sizeof(*owned));
        if (owned == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR region escape fact storage allocation failed");
            return false;
        }
        for (size_t i = 0; i < hir->region_escape_fact_count; i++)
            owned[i] = hir->region_escape_facts[i];
    }
    mir->region_escape_facts = owned;
    mir->region_escape_fact_count = hir->region_escape_fact_count;
    mir->has_region_escape_facts = true;
    if (!mir_validate_region_escape_facts(mir, error_message)) {
        mir_clear_region_escape_facts(mir);
        return false;
    }
    return true;
}
