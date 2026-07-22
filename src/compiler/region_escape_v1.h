#ifndef PERGYRA_REGION_ESCAPE_V1_H
#define PERGYRA_REGION_ESCAPE_V1_H

/*
 * Region escape analysis, version 1 (WO-REG-1 REG-1c, docs/197).
 *
 * Certifies the narrowest provably-sound class of region-safe string
 * allocations: a string-concatenation expression that is a DIRECT argument of a
 * Print / PrintLn call. Such a concat's result is handed straight to a builtin
 * that reads it synchronously and returns without retaining it, so the value
 * cannot outlive the enclosing statement -- a function-scope region (destroyed
 * at function exit) trivially outlives every use. It cannot be bound to an
 * escaping name, returned, captured, or stored, because it is only ever the
 * call argument.
 *
 * Soundness is one-directional by construction: the walk is deliberately
 * incomplete (it descends only the containers needed to reach top-level Print
 * statements). A missed site is simply not certified, so it stays HEAP -- the
 * fail-closed default. Over- or under-approximation of "is this a string
 * concat" is harmless too: the backend consults the plan only at its actual
 * StringConcat emission sites, so a spurious row is never read.
 *
 * This module is intentionally free of any driver/context dependency: its
 * output is a plain escape-site array. The driver runs it once while the source
 * AST is owned, converts each certified node to its stable allocation-site id,
 * and feeds the resulting rows to pgy_verified_region_plan_from_escape. No AST
 * address leaves this producer boundary.
 */

#include <stddef.h>

#include "verified_region_plan.h" /* PgyRegionEscapeSite */

struct ASTNode;

/*
 * Walk `root` (an AST program / function / block) and collect every
 * region-safe string-concat site. Allocates *sites_out (caller frees with
 * pgy_region_escape_v1_free); sets it to NULL when nothing is certified.
 * Returns the number of certified sites. Each site carries a function-scope id
 * (all concats in one function share a scope id; distinct functions get
 * distinct ids), while each row carries the stable allocation-site id used by
 * both backend consumers.
 */
size_t pgy_region_escape_v1_collect(const struct ASTNode *root,
                                    PgyRegionEscapeSite **sites_out);

void pgy_region_escape_v1_free(PgyRegionEscapeSite *sites);

#endif /* PERGYRA_REGION_ESCAPE_V1_H */
