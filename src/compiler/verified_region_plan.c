#include "verified_region_plan.h"

#include <stdlib.h>
#include <string.h>

/*
 * Row-building core. Every certified escape site becomes a REGION row. Refuses
 * a null site (an escape result must not carry a hole), refuses two conflicting
 * dispositions/scopes for one site, and collapses exact duplicates. HEAP is the
 * default for any site not present, so only region-safe sites are recorded.
 */
bool
pgy_verified_region_plan_build_rows(const PgyRegionEscapeResult *escape,
                                    PgyRegionPlan *plan_out,
                                    const char **error_out)
{
    if (error_out != NULL)
        *error_out = NULL;
    if (plan_out == NULL) {
        if (error_out != NULL)
            *error_out = "verified region plan: missing output plan";
        return false;
    }
    memset(plan_out, 0, sizeof(*plan_out));

    size_t site_count = (escape != NULL) ? escape->site_count : 0;
    if (site_count > 0) {
        if (escape->sites == NULL) {
            if (error_out != NULL)
                *error_out =
                    "verified region plan: escape result has a null site array";
            return false;
        }
        plan_out->rows = calloc(site_count, sizeof(*plan_out->rows));
        if (plan_out->rows == NULL) {
            if (error_out != NULL)
                *error_out = "verified region plan: row allocation failed";
            return false;
        }
    }

    for (size_t i = 0; i < site_count; i++) {
        const PgyRegionEscapeSite *site = &escape->sites[i];
        bool duplicate = false;

        if (site->site == NULL) {
            pgy_verified_region_plan_dispose(plan_out);
            if (error_out != NULL)
                *error_out =
                    "verified region plan: escape result carries a null site";
            return false;
        }
        for (size_t j = 0; j < plan_out->row_count; j++) {
            if (plan_out->rows[j].site != site->site)
                continue;
            /* A site certified twice must agree on scope; a disagreement means
               the escape pass produced contradictory evidence and must fail the
               compile here, observably, not silently pick one. */
            if (plan_out->rows[j].scope_id != site->scope_id) {
                pgy_verified_region_plan_dispose(plan_out);
                if (error_out != NULL)
                    *error_out =
                        "verified region plan: conflicting scopes for one site";
                return false;
            }
            duplicate = true;
            break;
        }
        if (duplicate)
            continue;
        plan_out->rows[plan_out->row_count].site = site->site;
        plan_out->rows[plan_out->row_count].disposition =
            PGY_REGION_DISPOSITION_REGION;
        plan_out->rows[plan_out->row_count].scope_id = site->scope_id;
        plan_out->row_count++;
    }

    plan_out->revision = PGY_REGION_PLAN_REVISION;
    plan_out->verified = true;
    return true;
}

bool
pgy_verified_region_plan_from_escape(const PgyAirVerification *air,
                                     const PgyRegionEscapeResult *escape,
                                     PgyRegionPlan *plan_out,
                                     const char **error_out)
{
    const char *certificate_error = NULL;

    if (error_out != NULL)
        *error_out = NULL;
    if (plan_out == NULL) {
        if (error_out != NULL)
            *error_out = "verified region plan: missing output plan";
        return false;
    }
    memset(plan_out, 0, sizeof(*plan_out));
    /* Same admission boundary as the spawn-lane plan: without a certified AIR
       verification the pipeline is not proven, so no escape verdict is
       trusted and every site stays HEAP (fail-closed). */
    if (!pgy_air_evidence_certificate_ready(air, &certificate_error)) {
        if (error_out != NULL)
            *error_out = certificate_error != NULL
                ? certificate_error
                : "verified region plan: AIR evidence certificate is missing";
        return false;
    }
    return pgy_verified_region_plan_build_rows(escape, plan_out, error_out);
}

void
pgy_verified_region_plan_dispose(PgyRegionPlan *plan)
{
    if (plan == NULL)
        return;
    free(plan->rows);
    memset(plan, 0, sizeof(*plan));
}

bool
pgy_verified_region_plan_lookup(const PgyRegionPlan *plan,
                                const struct ASTNode *site,
                                PgyRegionDisposition *disposition_out,
                                uint32_t *scope_id_out)
{
    if (plan == NULL || !plan->verified || site == NULL)
        return false;
    for (size_t i = 0; i < plan->row_count; i++) {
        if (plan->rows[i].site != site)
            continue;
        if (disposition_out != NULL)
            *disposition_out = plan->rows[i].disposition;
        if (scope_id_out != NULL)
            *scope_id_out = plan->rows[i].scope_id;
        return true;
    }
    return false;
}
