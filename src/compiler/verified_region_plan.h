#ifndef PERGYRA_VERIFIED_REGION_PLAN_H
#define PERGYRA_VERIFIED_REGION_PLAN_H

/*
 * Verified region plan: the third verified-projection artifact (docs/197).
 *
 * The escape pass (region_escape_v1) certifies the allocation sites whose whole
 * expression tree is region-safe; this plan carries the certified disposition
 * per allocation AST site into the backends. Emitters must take the disposition
 * from pgy_verified_region_plan_lookup -- recovering "is this region-safe" from
 * source spelling or AST addresses inside a backend is the drift this artifact removes, exactly
 * as the spawn-lane plan removed source-derived lane guesses.
 *
 * Fail-closed asymmetry (the soundness core, docs/197 §2 #6): a site is REGION
 * only under a certificate; every other site -- and any lookup miss -- resolves
 * to HEAP, which is today's byte-identical emission. Analysis incompleteness can
 * only cost performance, never correctness.
 *
 * Kept in its own file (not folded into verified_projection_plan.{h,c}) so the
 * region artifact has its own owner/consumer boundary, mirroring the house split
 * of the parallel-capture plan.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "air_evidence_certificate.h"

/* Stable identity carried by semantic/HIR/MIR allocation-site facts.  The
 * producer may inspect an AST node while the source is still owned by the
 * driver, but the plan and all backend consumers exchange this value rather
 * than an address that cannot survive serialization or a compiler restart. */
typedef uint32_t PgyRegionAllocationSiteId;

typedef enum PgyRegionDisposition {
    PGY_REGION_DISPOSITION_HEAP = 0, /* default: heap-allocate (today's shape) */
    PGY_REGION_DISPOSITION_REGION    /* certified region-safe: region-allocate */
} PgyRegionDisposition;

typedef struct PgyRegionFactRow {
    PgyRegionAllocationSiteId allocation_site_id;
    PgyRegionDisposition  disposition; /* REGION only under an escape certificate*/
    uint32_t              scope_id;    /* region scope id (same scope = same id) */
    uint32_t              function_syntax_id; /* stable MIR routine owner key */
} PgyRegionFactRow;

typedef struct PgyRegionPlan {
    uint32_t          revision;
    PgyRegionFactRow *rows;      /* owned; released by ..._dispose */
    size_t            row_count;
    bool              verified;
} PgyRegionPlan;

#define PGY_REGION_PLAN_REVISION UINT32_C(1)

/*
 * The escape-analysis result the producer validates into a plan. Owned by the
 * caller (region_escape_v1); the producer copies only what it certifies. Only
 * region-safe sites appear here -- HEAP is the default for everything absent.
 */
typedef struct PgyRegionEscapeSite {
    PgyRegionAllocationSiteId allocation_site_id;
    uint32_t              scope_id;
    uint32_t              function_syntax_id;
} PgyRegionEscapeSite;

typedef struct PgyRegionEscapeResult {
    const PgyRegionEscapeSite *sites;      /* region-safe sites only */
    size_t                     site_count;
} PgyRegionEscapeResult;

/*
 * Fail-closed producer. Requires a certified AIR verification (the whole
 * pipeline verified) before it will trust any escape verdict, refuses a null
 * site, refuses conflicting dispositions for one site, and collapses duplicate
 * rows. Sites absent from the escape result are simply not in the plan, so the
 * lookup returns HEAP for them.
 */
bool pgy_verified_region_plan_from_escape(
    const PgyAirVerification *air,
    const PgyRegionEscapeResult *escape,
    PgyRegionPlan *plan_out,
    const char **error_out);

void pgy_verified_region_plan_dispose(PgyRegionPlan *plan);

/*
 * Per-site disposition lookup. Returns true and fills *disposition_out /
 * *scope_id_out when the stable allocation-site id has a row; returns false
 * (=> caller must treat it as HEAP) when it is absent. Fail-closed: a
 * null/unverified plan or zero id returns false, never REGION.
 */
bool pgy_verified_region_plan_lookup(
    const PgyRegionPlan *plan,
    PgyRegionAllocationSiteId allocation_site_id,
    PgyRegionDisposition *disposition_out,
    uint32_t *scope_id_out);

/* Resolve the one function-scope region owned by a MIR routine.  The backend
 * passes the routine's stable source syntax id, never the AST root; a missing
 * or zero id is a fail-closed HEAP result. */
bool pgy_verified_region_plan_scope_for_function_id(
    const PgyRegionPlan *plan,
    uint32_t function_syntax_id,
    uint32_t *scope_id_out);

/*
 * Row-building core, exposed for standalone unit tests. Validates the escape
 * result into rows (all REGION), refusing zero allocation-site ids and conflicting
 * dispositions, collapsing duplicates. Does NOT apply the AIR certificate gate
 * -- pgy_verified_region_plan_from_escape wraps this with that gate. Production
 * code must call the gated entry point, never this directly.
 */
bool pgy_verified_region_plan_build_rows(
    const PgyRegionEscapeResult *escape,
    PgyRegionPlan *plan_out,
    const char **error_out);

#endif /* PERGYRA_VERIFIED_REGION_PLAN_H */
