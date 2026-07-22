/*
 * Standalone unit test for verified_region_plan.c (WO-REG-1 REG-1b, docs/197).
 *
 * Verifies the region plan's pure row-building / lookup / dispose logic and the
 * AIR-certificate fail-closed gate, without the full compiler. A stub
 * certificate stands in for the AIR evidence gate so the translation unit links
 * standalone; the real certificate integration is exercised once the driver
 * wiring lands (blocked while the concurrent session reworks the projection-plan
 * driver path -- docs/197 Appendix A).
 *
 * Built + run by the `region-plan-unit-test-smoke` Make target.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "verified_region_plan.h"

/* Stub the AIR certificate gate: ready iff a non-null AIR handle is passed. */
bool pgy_air_evidence_certificate_ready(const struct AIRProgram *air,
                                        const char **error_out)
{
    if (error_out != NULL) *error_out = NULL;
    return air != NULL;
}

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", (msg)); failures++; } \
} while (0)

/* Stable allocation-site identities are the plan key; no AST addresses cross
 * the plan boundary. */
#define SITE_A UINT32_C(11)
#define SITE_B UINT32_C(12)
#define SITE_C UINT32_C(13)

static void test_empty(void)
{
    PgyRegionEscapeResult esc = { NULL, 0 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(pgy_verified_region_plan_build_rows(&esc, &plan, &err), "empty build");
    CHECK(err == NULL, "empty err");
    CHECK(plan.row_count == 0 && plan.verified, "empty plan verified/0 rows");
    PgyRegionDisposition d; uint32_t s;
    CHECK(!pgy_verified_region_plan_lookup(&plan, SITE_A, &d, &s),
          "empty lookup is HEAP");
    pgy_verified_region_plan_dispose(&plan);
}

static void test_three_sites(void)
{
    PgyRegionEscapeSite sites[3] = {
        { SITE_A, 7, 0 }, { SITE_B, 7, 0 }, { SITE_C, 9, 0 },
    };
    PgyRegionEscapeResult esc = { sites, 3 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(pgy_verified_region_plan_build_rows(&esc, &plan, &err), "3 build");
    CHECK(plan.row_count == 3, "3 rows");
    PgyRegionDisposition d; uint32_t s;
    CHECK(pgy_verified_region_plan_lookup(&plan, SITE_A, &d, &s)
          && d == PGY_REGION_DISPOSITION_REGION && s == 7, "A REGION scope7");
    CHECK(pgy_verified_region_plan_lookup(&plan, SITE_C, &d, &s)
          && s == 9, "C scope9");
    pgy_verified_region_plan_dispose(&plan);
}

static void test_duplicate_collapse(void)
{
    PgyRegionEscapeSite sites[3] = {
        { SITE_A, 4, 0 }, { SITE_A, 4, 0 }, { SITE_B, 4, 0 },
    };
    PgyRegionEscapeResult esc = { sites, 3 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(pgy_verified_region_plan_build_rows(&esc, &plan, &err), "dup build");
    CHECK(plan.row_count == 2, "dup collapse to 2");
    pgy_verified_region_plan_dispose(&plan);
}

static void test_conflict_refused(void)
{
    PgyRegionEscapeSite sites[2] = { { SITE_A, 1, 0 }, { SITE_A, 2, 0 } };
    PgyRegionEscapeResult esc = { sites, 2 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(!pgy_verified_region_plan_build_rows(&esc, &plan, &err),
          "conflicting scope refused");
    CHECK(err != NULL, "conflict err set");
}

static void test_null_site_refused(void)
{
    PgyRegionEscapeSite sites[2] = { { SITE_A, 1, 0 }, { 0, 1, 0 } };
    PgyRegionEscapeResult esc = { sites, 2 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(!pgy_verified_region_plan_build_rows(&esc, &plan, &err),
          "null site refused");
    CHECK(err != NULL, "null site err set");
}

static void test_lookup_failclosed(void)
{
    PgyRegionDisposition d; uint32_t s;
    CHECK(!pgy_verified_region_plan_lookup(NULL, SITE_A, &d, &s),
          "null plan HEAP");
    PgyRegionPlan unver; memset(&unver, 0, sizeof(unver));
    CHECK(!pgy_verified_region_plan_lookup(&unver, SITE_A, &d, &s),
          "unverified plan HEAP");
}

static void test_owner_conflict_refused(void)
{
    /* One site claimed by two different owning functions. The scopes agree, so
       only the owner disagrees -- the escape pass contradicted itself and the
       compile must stop here rather than let one owner win silently. */
    PgyRegionEscapeSite sites[2] = { { SITE_A, 5, 11 }, { SITE_A, 5, 12 } };
    PgyRegionEscapeResult esc = { sites, 2 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(!pgy_verified_region_plan_build_rows(&esc, &plan, &err),
          "conflicting owner refused");
    CHECK(err != NULL, "owner conflict err set");
}

static void test_scope_for_function_id(void)
{
    uint32_t s = 99;
    /* Fail-closed, and the out-param is cleared on every denial so a caller
       that ignores the bool cannot read a stale scope id as a live region. */
    CHECK(!pgy_verified_region_plan_scope_for_function_id(NULL, 11, &s) && s == 0,
          "null plan denies + clears");
    PgyRegionPlan unver; memset(&unver, 0, sizeof(unver));
    s = 99;
    CHECK(!pgy_verified_region_plan_scope_for_function_id(&unver, 11, &s) && s == 0,
          "unverified plan denies + clears");

    PgyRegionEscapeSite sites[3] = {
        { SITE_A, 7, 11 }, { SITE_B, 7, 11 }, { SITE_C, 9, 12 },
    };
    PgyRegionEscapeResult esc = { sites, 3 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(pgy_verified_region_plan_build_rows(&esc, &plan, &err), "owner build");
    s = 0;
    CHECK(pgy_verified_region_plan_scope_for_function_id(&plan, 11, &s) && s == 7,
          "fn 11 owns scope 7");
    s = 0;
    CHECK(pgy_verified_region_plan_scope_for_function_id(&plan, 12, &s) && s == 9,
          "fn 12 owns scope 9");
    s = 99;
    CHECK(!pgy_verified_region_plan_scope_for_function_id(&plan, 13, &s) && s == 0,
          "unknown fn denies");
    s = 99;
    CHECK(!pgy_verified_region_plan_scope_for_function_id(&plan, 0, &s) && s == 0,
          "zero fn id denies");
    pgy_verified_region_plan_dispose(&plan);
}

static void test_owner_row_without_scope_failclosed(void)
{
    /* A row may name an owning function yet carry scope id 0 (no region was
       actually opened). Emitting a function-scope region for it would create a
       region the emitter never opens, so the owner lookup denies. */
    PgyRegionEscapeSite sites[1] = { { SITE_A, 0, 14 } };
    PgyRegionEscapeResult esc = { sites, 1 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(pgy_verified_region_plan_build_rows(&esc, &plan, &err), "scope0 build");
    uint32_t s = 99;
    CHECK(!pgy_verified_region_plan_scope_for_function_id(&plan, 14, &s) && s == 0,
          "owner row with scope 0 denies");
    pgy_verified_region_plan_dispose(&plan);
}

static void test_cert_gate(void)
{
    PgyRegionEscapeResult esc = { NULL, 0 };
    PgyRegionPlan plan;
    const char *err = NULL;
    /* null AIR -> stub says not ready -> refuse (fail-closed) */
    CHECK(!pgy_verified_region_plan_from_escape(NULL, &esc, &plan, &err),
          "null AIR refused by cert gate");
    /* non-null AIR -> stub ready -> build */
    char dummy;
    CHECK(pgy_verified_region_plan_from_escape(
              (const PgyAirVerification *)&dummy, &esc, &plan, &err),
          "ready AIR builds");
    pgy_verified_region_plan_dispose(&plan);
}

int main(void)
{
    test_empty();
    test_three_sites();
    test_duplicate_collapse();
    test_conflict_refused();
    test_null_site_refused();
    test_lookup_failclosed();
    test_owner_conflict_refused();
    test_scope_for_function_id();
    test_owner_row_without_scope_failclosed();
    test_cert_gate();
    if (failures) {
        fprintf(stderr, "%d region-plan checks failed\n", failures);
        return 1;
    }
    printf("region-plan-logic ok\n");
    return 0;
}
