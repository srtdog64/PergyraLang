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

/* Distinct dummy AST node identities (only pointer identity is compared). */
static char nodeA, nodeB, nodeC;
#define AST(p) ((const struct ASTNode *)(p))

static void test_empty(void)
{
    PgyRegionEscapeResult esc = { NULL, 0 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(pgy_verified_region_plan_build_rows(&esc, &plan, &err), "empty build");
    CHECK(err == NULL, "empty err");
    CHECK(plan.row_count == 0 && plan.verified, "empty plan verified/0 rows");
    PgyRegionDisposition d; uint32_t s;
    CHECK(!pgy_verified_region_plan_lookup(&plan, AST(&nodeA), &d, &s),
          "empty lookup is HEAP");
    pgy_verified_region_plan_dispose(&plan);
}

static void test_three_sites(void)
{
    PgyRegionEscapeSite sites[3] = {
        { AST(&nodeA), 7 }, { AST(&nodeB), 7 }, { AST(&nodeC), 9 },
    };
    PgyRegionEscapeResult esc = { sites, 3 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(pgy_verified_region_plan_build_rows(&esc, &plan, &err), "3 build");
    CHECK(plan.row_count == 3, "3 rows");
    PgyRegionDisposition d; uint32_t s;
    CHECK(pgy_verified_region_plan_lookup(&plan, AST(&nodeA), &d, &s)
          && d == PGY_REGION_DISPOSITION_REGION && s == 7, "A REGION scope7");
    CHECK(pgy_verified_region_plan_lookup(&plan, AST(&nodeC), &d, &s)
          && s == 9, "C scope9");
    pgy_verified_region_plan_dispose(&plan);
}

static void test_duplicate_collapse(void)
{
    PgyRegionEscapeSite sites[3] = {
        { AST(&nodeA), 4 }, { AST(&nodeA), 4 }, { AST(&nodeB), 4 },
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
    PgyRegionEscapeSite sites[2] = { { AST(&nodeA), 1 }, { AST(&nodeA), 2 } };
    PgyRegionEscapeResult esc = { sites, 2 };
    PgyRegionPlan plan;
    const char *err = NULL;
    CHECK(!pgy_verified_region_plan_build_rows(&esc, &plan, &err),
          "conflicting scope refused");
    CHECK(err != NULL, "conflict err set");
}

static void test_null_site_refused(void)
{
    PgyRegionEscapeSite sites[2] = { { AST(&nodeA), 1 }, { NULL, 1 } };
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
    CHECK(!pgy_verified_region_plan_lookup(NULL, AST(&nodeA), &d, &s),
          "null plan HEAP");
    PgyRegionPlan unver; memset(&unver, 0, sizeof(unver));
    CHECK(!pgy_verified_region_plan_lookup(&unver, AST(&nodeA), &d, &s),
          "unverified plan HEAP");
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
    test_cert_gate();
    if (failures) {
        fprintf(stderr, "%d region-plan checks failed\n", failures);
        return 1;
    }
    printf("region-plan-logic ok\n");
    return 0;
}
